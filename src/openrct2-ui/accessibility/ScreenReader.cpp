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
#include <openrct2/Diagnostic.h>
#include <openrct2/Version.h>
#include <openrct2/audio/Audio.h>
#include <openrct2/audio/AudioMixer.h>
#include <openrct2/config/Config.h>
#include <openrct2/world/Location.hpp>
#include <string>
#include <vector>

namespace
{
    // Removes OpenRCT2 formatting tokens (e.g. "{BABYBLUE}") from text so the screen
    // reader doesn't read them aloud. Newline tokens become spaces.
    std::string StripFormatCodes(std::string_view text)
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
    #include <string>
    #include <windows.h>

// Every Prism entry point is resolved at runtime, so PRISM_STATIC strips the dllimport decoration
// from the declarations and the mod needs no import library. See prism/README.md.
    #define PRISM_STATIC
// C4309 fires inside the header itself: PrismBackendFeature's last enumerator is 1ULL << 63, which
// MSVC truncates while deducing the enum's underlying type. Nothing the mod can fix in a vendored
// file, and this build treats warnings as errors, so it is silenced across the include only.
    #pragma warning(push)
    #pragma warning(disable : 4309)
    #include "prism/prism.h"
    #pragma warning(pop)

namespace OpenRCT2::Ui::Accessibility
{
    // Speech goes through Prism (https://github.com/ethindp/prism), a screen-reader abstraction that
    // fans a single speak call out to whichever reader is actually running - NVDA, JAWS, Narrator,
    // System Access, ZDSR and others on Windows, VoiceOver on macOS, Speech Dispatcher or Orca on
    // Linux. The rest of the mod calls ScreenReaderSpeak and never learns which one answered.
    //
    // prism.dll is loaded with LoadLibrary rather than linked, and the mod's original direct NVDA
    // binding is kept behind it as a fallback. Both choices exist for the same reason: a link-time
    // dependency on a missing DLL stops openrct2.exe from starting at all, and a blind player is
    // far better served by a game that runs and falls back to NVDA - or in the worst case runs
    // silently - than by one that will not launch.
    namespace
    {
        using PrismConfigInitFn = PrismConfig(PRISM_CALL*)(void);
        using PrismInitFn = PrismContext*(PRISM_CALL*)(PrismConfig*);
        using PrismShutdownFn = void(PRISM_CALL*)(PrismContext*);
        using PrismRegistryCreateBestFn = PrismBackend*(PRISM_CALL*)(PrismContext*);
        using PrismBackendFreeFn = void(PRISM_CALL*)(PrismBackend*);
        using PrismBackendNameFn = const char*(PRISM_CALL*)(PrismBackend*);
        using PrismBackendSpeakFn = PrismError(PRISM_CALL*)(PrismBackend*, const char*, bool);
        using PrismBackendStopFn = PrismError(PRISM_CALL*)(PrismBackend*);

        HMODULE _prismLib = nullptr;
        PrismConfigInitFn _prismConfigInit = nullptr;
        PrismInitFn _prismInit = nullptr;
        PrismShutdownFn _prismShutdown = nullptr;
        PrismRegistryCreateBestFn _prismCreateBest = nullptr;
        PrismBackendFreeFn _prismBackendFree = nullptr;
        PrismBackendNameFn _prismBackendName = nullptr;
        PrismBackendSpeakFn _prismSpeak = nullptr;
        PrismBackendStopFn _prismStop = nullptr;

        PrismContext* _prismCtx = nullptr;
        PrismBackend* _prismBackend = nullptr;

        // Fallback only, used when prism.dll cannot be loaded. Matches the NVDA Controller Client
        // API; on x64 there is a single calling convention, so no decoration is needed.
        using NvdaError = unsigned long;
        using NvdaTestIfRunning = NvdaError (*)();
        using NvdaSpeakText = NvdaError (*)(const wchar_t*);
        using NvdaCancelSpeech = NvdaError (*)();

        HMODULE _nvdaClient = nullptr;
        NvdaTestIfRunning _testIfRunning = nullptr;
        NvdaSpeakText _speakText = nullptr;
        NvdaCancelSpeech _cancelSpeech = nullptr;

        template<typename T>
        bool ResolveExport(HMODULE lib, const char* name, T& out)
        {
            out = reinterpret_cast<T>(GetProcAddress(lib, name));
            return out != nullptr;
        }

        // Picks the highest-priority backend whose screen reader is actually running. Returns false
        // when nothing is running, which is not an error - the player may start a reader later, and
        // every speak calls this again.
        bool PrismAcquireBackend()
        {
            if (_prismBackend != nullptr)
                return true;
            if (_prismCtx == nullptr)
                return false;

            _prismBackend = _prismCreateBest(_prismCtx);
            if (_prismBackend == nullptr)
                return false;

            LOG_INFO("Accessibility: speaking through Prism backend '%s'", _prismBackendName(_prismBackend));
            return true;
        }

        // Releases the current backend so the next speak chooses again. This is how the mod copes
        // with the player starting, quitting or switching screen readers mid-game.
        void PrismDropBackend()
        {
            if (_prismBackend == nullptr)
                return;

            _prismBackendFree(_prismBackend);
            _prismBackend = nullptr;
        }

        void PrismUnload()
        {
            PrismDropBackend();
            if (_prismCtx != nullptr)
            {
                _prismShutdown(_prismCtx);
                _prismCtx = nullptr;
            }
            if (_prismLib != nullptr)
            {
                FreeLibrary(_prismLib);
                _prismLib = nullptr;
            }
            _prismConfigInit = nullptr;
            _prismInit = nullptr;
            _prismShutdown = nullptr;
            _prismCreateBest = nullptr;
            _prismBackendFree = nullptr;
            _prismBackendName = nullptr;
            _prismSpeak = nullptr;
            _prismStop = nullptr;
        }

        bool TryInitPrism()
        {
            _prismLib = LoadLibraryW(L"prism.dll");
            if (_prismLib == nullptr)
            {
                LOG_WARNING("Accessibility: prism.dll not found, falling back to NVDA");
                return false;
            }

            if (!ResolveExport(_prismLib, "prism_config_init", _prismConfigInit)
                || !ResolveExport(_prismLib, "prism_init", _prismInit)
                || !ResolveExport(_prismLib, "prism_shutdown", _prismShutdown)
                || !ResolveExport(_prismLib, "prism_registry_create_best", _prismCreateBest)
                || !ResolveExport(_prismLib, "prism_backend_free", _prismBackendFree)
                || !ResolveExport(_prismLib, "prism_backend_name", _prismBackendName)
                || !ResolveExport(_prismLib, "prism_backend_speak", _prismSpeak)
                || !ResolveExport(_prismLib, "prism_backend_stop", _prismStop))
            {
                LOG_WARNING("Accessibility: prism.dll is missing expected exports, falling back to NVDA");
                PrismUnload();
                return false;
            }

            // Ask Prism for the config rather than filling the struct here, so that a future
            // PRISM_CONFIG_VERSION bump stays source-compatible. Leaving availability_callback at
            // its default null is deliberate: it tells Prism not to spawn a background polling
            // thread, since the mod re-picks a backend on demand instead.
            PrismConfig cfg = _prismConfigInit();
            _prismCtx = _prismInit(&cfg);
            if (_prismCtx == nullptr)
            {
                LOG_WARNING("Accessibility: Prism failed to initialise, falling back to NVDA");
                PrismUnload();
                return false;
            }

            // A reader may not be running yet; that is fine, speak will try again.
            PrismAcquireBackend();
            return true;
        }

        void TryInitNvda()
        {
            _nvdaClient = LoadLibraryW(L"nvdaControllerClient64.dll");
            if (_nvdaClient == nullptr)
                return;

            _testIfRunning = reinterpret_cast<NvdaTestIfRunning>(GetProcAddress(_nvdaClient, "nvdaController_testIfRunning"));
            _speakText = reinterpret_cast<NvdaSpeakText>(GetProcAddress(_nvdaClient, "nvdaController_speakText"));
            _cancelSpeech = reinterpret_cast<NvdaCancelSpeech>(GetProcAddress(_nvdaClient, "nvdaController_cancelSpeech"));
        }

        void NvdaSpeak(const std::string& clean, bool interrupt)
        {
            if (_speakText == nullptr)
                return;

            const int required = MultiByteToWideChar(CP_UTF8, 0, clean.data(), static_cast<int>(clean.size()), nullptr, 0);
            if (required <= 0)
                return;

            std::wstring wide(static_cast<size_t>(required), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, clean.data(), static_cast<int>(clean.size()), wide.data(), required);

            if (interrupt && _cancelSpeech != nullptr)
                _cancelSpeech();

            _speakText(wide.c_str());
        }
    } // namespace

    void ScreenReaderInit()
    {
        if (_prismCtx != nullptr || _nvdaClient != nullptr)
            return;

        // Also the reason this literal exists in the binary at all - the installer greps for it to
        // recognise a modded executable. See kAccessVersionBanner.
        LOG_INFO("%s", kAccessVersionBanner);

        if (!TryInitPrism())
            TryInitNvda();
    }

    void ScreenReaderShutdown()
    {
        PrismUnload();

        if (_nvdaClient == nullptr)
            return;

        FreeLibrary(_nvdaClient);
        _nvdaClient = nullptr;
        _testIfRunning = nullptr;
        _speakText = nullptr;
        _cancelSpeech = nullptr;
    }

    bool ScreenReaderIsAvailable()
    {
        if (_prismCtx != nullptr)
            return PrismAcquireBackend();

        return _testIfRunning != nullptr && _testIfRunning() == 0;
    }

    const char* ScreenReaderBackendName()
    {
        if (_prismCtx != nullptr)
            return PrismAcquireBackend() ? _prismBackendName(_prismBackend) : "";

        return ScreenReaderIsAvailable() ? "NVDA" : "";
    }

    void ScreenReaderSpeak(std::string_view utf8Text, bool interrupt)
    {
        if (utf8Text.empty())
            return;

        const std::string clean = StripFormatCodes(utf8Text);
        if (clean.empty())
            return;

        if (_prismCtx == nullptr)
        {
            NvdaSpeak(clean, interrupt);
            return;
        }

        if (!PrismAcquireBackend())
            return; // no screen reader running at the moment

        // Prism takes UTF-8 directly, so unlike the NVDA path there is no conversion to wide
        // characters here and no chance of the two ends disagreeing about the encoding.
        PrismError err = _prismSpeak(_prismBackend, clean.c_str(), interrupt);
        if (err == PRISM_ERROR_BACKEND_NOT_AVAILABLE || err == PRISM_ERROR_NOT_INITIALIZED)
        {
            // The reader this backend spoke to has gone away - closed, or swapped for another one.
            // Choose again and repeat the line rather than dropping it silently.
            PrismDropBackend();
            if (!PrismAcquireBackend())
                return;

            err = _prismSpeak(_prismBackend, clean.c_str(), interrupt);
        }

        if (err != PRISM_OK)
            LOG_WARNING("Accessibility: Prism speak failed with error %d", static_cast<int>(err));
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

    const char* ScreenReaderBackendName()
    {
        return "";
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
        Audio::Play3D(soundId, loc, volumeAdjust, Audio::MixerGroup::accessibility);
    }
} // namespace OpenRCT2::Ui::Accessibility
