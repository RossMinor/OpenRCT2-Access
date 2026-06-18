/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "ScreenReader.h"

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

namespace OpenRCT2::Ui::Accessibility
{
    // Matches the NVDA Controller Client API. On x64 there is a single calling
    // convention, so the function pointers can be declared without decoration.
    using NvdaError = unsigned long;
    using NvdaTestIfRunning = NvdaError (*)();
    using NvdaSpeakText = NvdaError (*)(const wchar_t*);
    using NvdaCancelSpeech = NvdaError (*)();

    static HMODULE _nvdaClient = nullptr;
    static NvdaTestIfRunning _testIfRunning = nullptr;
    static NvdaSpeakText _speakText = nullptr;
    static NvdaCancelSpeech _cancelSpeech = nullptr;

    void ScreenReaderInit()
    {
        if (_nvdaClient != nullptr)
            return;

        _nvdaClient = LoadLibraryW(L"nvdaControllerClient64.dll");
        if (_nvdaClient == nullptr)
            return;

        _testIfRunning = reinterpret_cast<NvdaTestIfRunning>(GetProcAddress(_nvdaClient, "nvdaController_testIfRunning"));
        _speakText = reinterpret_cast<NvdaSpeakText>(GetProcAddress(_nvdaClient, "nvdaController_speakText"));
        _cancelSpeech = reinterpret_cast<NvdaCancelSpeech>(GetProcAddress(_nvdaClient, "nvdaController_cancelSpeech"));
    }

    void ScreenReaderShutdown()
    {
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
        return _testIfRunning != nullptr && _testIfRunning() == 0;
    }

    void ScreenReaderSpeak(std::string_view utf8Text, bool interrupt)
    {
        if (_speakText == nullptr || utf8Text.empty())
            return;

        const std::string clean = StripFormatCodes(utf8Text);
        if (clean.empty())
            return;

        const int required = MultiByteToWideChar(
            CP_UTF8, 0, clean.data(), static_cast<int>(clean.size()), nullptr, 0);
        if (required <= 0)
            return;

        std::wstring wide(static_cast<size_t>(required), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, clean.data(), static_cast<int>(clean.size()), wide.data(), required);

        if (interrupt && _cancelSpeech != nullptr)
            _cancelSpeech();

        _speakText(wide.c_str());
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
} // namespace OpenRCT2::Ui::Accessibility
