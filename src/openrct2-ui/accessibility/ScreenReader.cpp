/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "ScreenReader.h"

#include <algorithm>
#include <cmath>
#include <openrct2/audio/Audio.h>
#include <openrct2/audio/AudioMixer.h>
#include <openrct2/config/Config.h>
#include <openrct2/core/Path.hpp>
#include <openrct2/platform/Platform.h>
#include <openrct2/world/Location.hpp>
#include <string>
#include <vector>

namespace
{
    // Removes OpenRCT2 formatting tokens (e.g. "{BABYBLUE}") from text so the screen
    // reader doesn't read them aloud. Newline tokens become spaces.
    [[maybe_unused]] std::string StripFormatCodes(std::string_view text)
    {
        std::string out;
        out.reserve(text.size());
        for (size_t i = 0; i < text.size();)
        {
            if (text[i] == '{')
            {
                const auto end = text.find('}', i);
                if (end == std::string_view::npos)
                {
                    out.append(text.substr(i));
                    break;
                }
                const auto token = text.substr(i + 1, end - i - 1);
                if (token.rfind("NEWLINE", 0) == 0)
                    out.push_back(' ');
                // Any other token (colours, fonts, etc.) is dropped.
                i = end + 1;
            }
            else
            {
                out.push_back(text[i]);
                i++;
            }
        }
        return out;
    }
} // namespace

#ifdef _WIN32

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#elif defined(__APPLE__) || defined(__unix__)
    #include <dlfcn.h>
#endif

#if defined(_WIN32) || defined(__APPLE__) || defined(__unix__)

namespace OpenRCT2::Ui::Accessibility
{
    // Opaque Prism types plus the PrismError values this bridge relies on, matching the C ABI
    // published in prism.h from https://github.com/ethindp/prism. The library is loaded
    // dynamically (like the old NVDA controller client), so no build-time dependency exists.
    using PrismError = int32_t;
    constexpr PrismError kPrismErrorBackendNotAvailable = 16;

    struct PrismContext;
    struct PrismBackend;
    struct PrismConfig;

    // On x64 there is a single calling convention, so the function pointers can be declared
    // without decoration (Prism uses __cdecl on Windows).
    using PrismInit = PrismContext* (*)(PrismConfig*);
    using PrismShutdown = void (*)(PrismContext*);
    using PrismCreateBest = PrismBackend* (*)(PrismContext*);
    using PrismSpeak = PrismError (*)(PrismBackend*, const char*, bool);
    using PrismStop = PrismError (*)(PrismBackend*);
    using PrismFree = void (*)(PrismBackend*);

    // Platform wrapper around loading the Prism library and resolving symbols. On Windows
    // LoadLibrary finds prism.dll in the application directory automatically; the POSIX
    // loader does not search the executable's own directory (or an app bundle's Frameworks
    // folder), so those are tried explicitly before falling back to the normal search path.
#ifdef _WIN32
    using LibraryHandle = HMODULE;

    static LibraryHandle LoadPrismLibrary()
    {
        return LoadLibraryW(L"prism.dll");
    }

    static void* ResolvePrismSymbol(LibraryHandle handle, const char* name)
    {
        return reinterpret_cast<void*>(GetProcAddress(handle, name));
    }

    static void FreePrismLibrary(LibraryHandle handle)
    {
        FreeLibrary(handle);
    }
#else
    #if defined(__APPLE__)
        static constexpr const char* kPrismLibraryName = "libprism.dylib";
    #else
        static constexpr const char* kPrismLibraryName = "libprism.so";
    #endif

    using LibraryHandle = void*;

    static LibraryHandle LoadPrismLibrary()
    {
        const auto exePath = Platform::GetCurrentExecutablePath();
        if (!exePath.empty())
        {
            const auto exeDir = Path::GetDirectory(exePath);
    #if defined(__APPLE__)
            // Inside the .app bundle the library can sit next to the executable (Contents/MacOS)
            // or in the bundle's Frameworks folder (Contents/Frameworks).
            const char* const candidates[] = { "libprism.dylib", "../Frameworks/libprism.dylib" };
    #else
            const char* const candidates[] = { "libprism.so" };
    #endif
            for (const char* candidate : candidates)
            {
                const auto fullPath = Path::Combine(exeDir, candidate);
                if (LibraryHandle handle = dlopen(fullPath.c_str(), RTLD_LAZY))
                    return handle;
            }
        }
        return dlopen(kPrismLibraryName, RTLD_LAZY);
    }

    static void* ResolvePrismSymbol(LibraryHandle handle, const char* name)
    {
        return dlsym(handle, name);
    }

    static void FreePrismLibrary(LibraryHandle handle)
    {
        dlclose(handle);
    }
#endif

    static LibraryHandle _prismLibrary = nullptr;
    static PrismContext* _context = nullptr;
    static PrismBackend* _backend = nullptr;

    static PrismInit _prismInit = nullptr;
    static PrismShutdown _prismShutdown = nullptr;
    static PrismCreateBest _prismCreateBest = nullptr;
    static PrismSpeak _prismSpeak = nullptr;
    static PrismStop _prismStop = nullptr;
    static PrismFree _prismFree = nullptr;

    // Picks the highest-priority backend that works (a running screen reader such as NVDA or JAWS
    // first, then a platform TTS engine). Done lazily so a screen reader that starts after the
    // game can still be picked up. Returns true once speech is available.
    static bool EnsureBackend()
    {
        if (_backend != nullptr)
            return true;
        if (_context == nullptr || _prismCreateBest == nullptr)
            return false;
        _backend = _prismCreateBest(_context);
        return _backend != nullptr;
    }

    void ScreenReaderInit()
    {
        if (_prismLibrary != nullptr)
            return;

        _prismLibrary = LoadPrismLibrary();
        if (_prismLibrary == nullptr)
            return;

        _prismInit = reinterpret_cast<PrismInit>(ResolvePrismSymbol(_prismLibrary, "prism_init"));
        _prismShutdown = reinterpret_cast<PrismShutdown>(ResolvePrismSymbol(_prismLibrary, "prism_shutdown"));
        _prismCreateBest = reinterpret_cast<PrismCreateBest>(ResolvePrismSymbol(_prismLibrary, "prism_registry_create_best"));
        _prismSpeak = reinterpret_cast<PrismSpeak>(ResolvePrismSymbol(_prismLibrary, "prism_backend_speak"));
        _prismStop = reinterpret_cast<PrismStop>(ResolvePrismSymbol(_prismLibrary, "prism_backend_stop"));
        _prismFree = reinterpret_cast<PrismFree>(ResolvePrismSymbol(_prismLibrary, "prism_backend_free"));

        if (_prismInit == nullptr || _prismShutdown == nullptr || _prismCreateBest == nullptr
            || _prismSpeak == nullptr || _prismFree == nullptr)
        {
            ScreenReaderShutdown();
            return;
        }

        _context = _prismInit(nullptr);
    }

    void ScreenReaderShutdown()
    {
        if (_backend != nullptr)
        {
            if (_prismStop != nullptr)
                _prismStop(_backend);
            if (_prismFree != nullptr)
                _prismFree(_backend);
            _backend = nullptr;
        }

        if (_context != nullptr)
        {
            if (_prismShutdown != nullptr)
                _prismShutdown(_context);
            _context = nullptr;
        }

        if (_prismLibrary == nullptr)
            return;

        FreePrismLibrary(_prismLibrary);
        _prismLibrary = nullptr;
        _prismInit = nullptr;
        _prismShutdown = nullptr;
        _prismCreateBest = nullptr;
        _prismSpeak = nullptr;
        _prismStop = nullptr;
        _prismFree = nullptr;
    }

    bool ScreenReaderIsAvailable()
    {
        return EnsureBackend();
    }

    void ScreenReaderSpeak(std::string_view utf8Text, bool interrupt)
    {
        if (_prismSpeak == nullptr || utf8Text.empty() || !EnsureBackend())
            return;

        const std::string clean = StripFormatCodes(utf8Text);
        if (clean.empty())
            return;

        const PrismError result = _prismSpeak(_backend, clean.c_str(), interrupt);

        // The backend's screen reader or TTS engine may have shut down. Drop it so the next call
        // re-selects the best backend instead of silently speaking through a dead one.
        if (result == kPrismErrorBackendNotAvailable)
        {
            if (_prismFree != nullptr)
                _prismFree(_backend);
            _backend = nullptr;
        }
    }
} // namespace OpenRCT2::Ui::Accessibility

#else

namespace OpenRCT2::Ui::Accessibility
{
    void ScreenReaderInit()
    {
    }

    void ScreenReaderShutdown()
    {
    }

    bool ScreenReaderIsAvailable()
    {
        return false;
    }

    void ScreenReaderSpeak(std::string_view, bool)
    {
    }
} // namespace OpenRCT2::Ui::Accessibility

#endif

// Position-suffixed speech and announcement history (platform-independent).
namespace OpenRCT2::Ui::Accessibility
{
    void ScreenReaderSpeakItem(std::string_view text, int32_t index, int32_t count)
    {
        std::string out(text);
        if (count > 0)
            out += ", " + std::to_string(index + 1) + " of " + std::to_string(count);
        ScreenReaderSpeak(out);
    }

    static std::vector<std::string> _history;
    static int _historyCursor = -1;
    static constexpr size_t kMaxHistory = 50;

    void LogAnnouncement(std::string_view text)
    {
        if (text.empty())
            return;

        _history.emplace_back(text);
        if (_history.size() > kMaxHistory)
            _history.erase(_history.begin());
        _historyCursor = static_cast<int>(_history.size()) - 1;
    }

    void CycleAnnouncementHistory(int direction)
    {
        if (_history.empty())
        {
            ScreenReaderSpeak("No announcements");
            return;
        }

        if (_historyCursor < 0)
            _historyCursor = static_cast<int>(_history.size()) - 1;

        int next = _historyCursor + direction;
        const int last = static_cast<int>(_history.size()) - 1;
        if (next < 0)
            next = 0;
        else if (next > last)
            next = last;

        _historyCursor = next;
        ScreenReaderSpeak(_history[_historyCursor]);
    }

    std::string JoinSpeech(std::initializer_list<std::string_view> fragments)
    {
        SpeechBuilder sb;
        for (const auto& fragment : fragments)
            sb.add(fragment);
        return sb.str();
    }

    void PlayCue(Audio::SoundId soundId, const CoordsXYZ& loc)
    {
        const int32_t pct = Config::Get().sound.accessibilityCueVolume;
        if (pct <= 0)
            return; // cues muted

        // Convert the 0-100% setting into a DirectSound-style volume offset (hundredths of a decibel).
        // 100% -> 0 (unchanged); quieter percentages give a negative offset. Clamped to silence.
        int32_t volumeAdjust = 0;
        if (pct < 100)
            volumeAdjust = std::max(-10000, static_cast<int32_t>(std::lround(2000.0 * std::log10(pct / 100.0))));

        // Route through the accessibility mixer group so this cue is scaled only by master volume and
        // the mod's own cue-volume setting (applied via volumeAdjust), never the game's sound slider.
        Audio::Play3D(soundId, loc, volumeAdjust, Audio::MixerGroup::Accessibility);
    }
} // namespace OpenRCT2::Ui::Accessibility
