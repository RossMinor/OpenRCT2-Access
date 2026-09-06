/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifdef _WIN32

// clang-format off
    // windows.h needs to be included first
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <shellapi.h>
    #undef CreateWindow
// clang-format on

    // Then the rest
    #include "UiContext.h"

    #include <SDL_events.h>
    #include <SDL_syswm.h>
    #include <openrct2/Diagnostic.h>
    #include <openrct2/core/Path.hpp>
    #include <openrct2/core/String.hpp>
    #include <openrct2/ui/UiContext.h>
    #include <shobjidl.h>
    #include <wrl/client.h>

    // Native resource IDs
    #include "../../resources/resource.h"

using namespace Microsoft::WRL;

class CCoInitialize
{
public:
    CCoInitialize(DWORD dwCoInit)
        : m_hr(CoInitializeEx(nullptr, dwCoInit))
    {
    }

    ~CCoInitialize()
    {
        if (SUCCEEDED(m_hr))
        {
            CoUninitialize();
        }
    }

    operator bool() const
    {
        return SUCCEEDED(m_hr);
    }

private:
    HRESULT m_hr;
};

// Drops the game out of fullscreen for as long as a native modal dialog is on screen, then puts it
// back exactly as it was.
//
// A modal dialog owned by a fullscreen SDL window is a keyboard trap. Windows disables the owner
// while the dialog is up, but the dialog itself is drawn behind the fullscreen window, so the game
// stops responding and there is nothing on screen to interact with. Borderless fullscreen makes it
// worse: SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS minimises the game the moment the dialog takes focus,
// and Windows minimises owned windows along with their owner - so the dialog vanishes too, leaving a
// disabled game and an invisible dialog with no way back but killing the process.
//
// Going windowed first sidesteps both: the minimise hint only applies to fullscreen windows, and a
// windowed owner lets the dialog sit above the game where it belongs.
class CFullscreenSuspender
{
public:
    explicit CFullscreenSuspender(SDL_Window* window)
        : m_window(window)
    {
        if (m_window == nullptr)
            return;

        m_savedFlags = SDL_GetWindowFlags(m_window) & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_FULLSCREEN_DESKTOP);
        if (m_savedFlags != 0)
        {
            SDL_SetWindowFullscreen(m_window, 0);
            // Let the window manager actually apply the change before a modal dialog is parented to
            // the window, or the dialog can still be owned by a window Windows thinks is fullscreen.
            SDL_PumpEvents();
        }
    }

    ~CFullscreenSuspender()
    {
        if (m_window == nullptr)
            return;

        if (m_savedFlags != 0)
        {
            SDL_SetWindowFullscreen(m_window, m_savedFlags);
        }
        // Bring the game back to the front whether or not it was fullscreen: the dialog took focus,
        // and without this the player can be left tabbed away from a game they cannot see.
        SDL_RaiseWindow(m_window);
    }

    CFullscreenSuspender(const CFullscreenSuspender&) = delete;
    CFullscreenSuspender& operator=(const CFullscreenSuspender&) = delete;

private:
    SDL_Window* m_window;
    Uint32 m_savedFlags = 0;
};

namespace OpenRCT2::Ui
{
    class Win32Context : public IPlatformUiContext
    {
    private:
        HMODULE _win32module;

    public:
        Win32Context()
        {
            _win32module = GetModuleHandle(nullptr);
        }

        void SetWindowIcon(SDL_Window* window) override
        {
            if (_win32module != nullptr)
            {
                HICON icon = LoadIcon(_win32module, MAKEINTRESOURCE(IDI_ICON));
                if (icon != nullptr)
                {
                    HWND hwnd = GetHWND(window);
                    if (hwnd != nullptr)
                    {
                        SendMessage(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon));
                    }
                }
            }
        }

        bool IsSteamOverlayAttached() override
        {
            return (GetModuleHandleW(L"GameOverlayRenderer.dll") != nullptr);
        }

        void ShowMessageBox(SDL_Window* window, const std::string& message) override
        {
            // Same fullscreen trap as the file dialog: a message box owned by a fullscreen window
            // disables the game and then hides behind it. An error the player cannot dismiss is the
            // worst place for this to happen, since it is usually telling them something went wrong.
            CFullscreenSuspender fullscreenSuspender(window);

            HWND hwnd = GetHWND(window);
            std::wstring messageW = String::toWideChar(message);
            MessageBoxW(hwnd, messageW.c_str(), L"OpenRCT2", MB_OK);
        }

        bool HasMenuSupport() override
        {
            return false;
        }

        int32_t ShowMenuDialog(
            const std::vector<std::string>& options, const std::string& title, const std::string& text) override
        {
            return -1;
        }

        void OpenFolder(const std::string& path) override
        {
            std::wstring pathW = String::toWideChar(path);
            ShellExecuteW(NULL, L"open", pathW.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }

        void OpenURL(const std::string& url) override
        {
            std::wstring urlW = String::toWideChar(url);
            ShellExecuteW(NULL, L"open", urlW.c_str(), NULL, NULL, SW_SHOWNORMAL);
        }

        std::string ShowFileDialogInternal(SDL_Window* window, const FileDialogDesc& desc, bool isFolder)
        {
            std::string resultFilename;

            // Must outlive the dialog, and restore on every exit path - including the early returns
            // when COM or the dialog itself fails to start.
            CFullscreenSuspender fullscreenSuspender(window);

            CCoInitialize coInitialize(COINIT_APARTMENTTHREADED);
            if (coInitialize)
            {
                CLSID dialogId = CLSID_FileOpenDialog;
                DWORD flagsToSet = FOS_FORCEFILESYSTEM;
                if (desc.Type == FileDialogType::save)
                {
                    dialogId = CLSID_FileSaveDialog;
                    flagsToSet |= FOS_OVERWRITEPROMPT | FOS_CREATEPROMPT | FOS_STRICTFILETYPES;
                }
                if (isFolder)
                {
                    flagsToSet |= FOS_PICKFOLDERS;
                }

                ComPtr<IFileDialog> fileDialog;
                if (SUCCEEDED(
                        CoCreateInstance(dialogId, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(fileDialog.GetAddressOf()))))
                {
                    DWORD flags;
                    if (SUCCEEDED(fileDialog->GetOptions(&flags)) && SUCCEEDED(fileDialog->SetOptions(flags | flagsToSet)))
                    {
                        fileDialog->SetTitle(String::toWideChar(desc.Title).c_str());
                        fileDialog->SetFileName(String::toWideChar(Path::GetFileName(desc.DefaultFilename)).c_str());

                        // Set default directory (optional, don't fail the operation if it fails to set)
                        ComPtr<IShellItem> defaultDirectory;
                        if (SUCCEEDED(SHCreateItemFromParsingName(
                                String::toWideChar(desc.InitialDirectory).c_str(), nullptr,
                                IID_PPV_ARGS(defaultDirectory.GetAddressOf()))))
                        {
                            fileDialog->SetFolder(defaultDirectory.Get());
                        }

                        // Opt-in to automatic extensions, this will ensure extension of the selected file matches the filter
                        // Setting it to an empty string so "All Files" does not get anything appended
                        fileDialog->SetDefaultExtension(L"");

                        // Filters need an "auxillary" storage for wide strings
                        std::vector<std::wstring> filtersStorage;
                        auto filters = GetFilters(desc.Filters, filtersStorage);

                        bool filtersSet = true;
                        if (!filters.empty())
                        {
                            filtersSet = SUCCEEDED(fileDialog->SetFileTypes(static_cast<UINT>(filters.size()), filters.data()));
                        }

                        // Owned by the game window rather than opened parentless. An unowned dialog is
                        // not modal to the game, so it can end up behind it - and a screen reader user
                        // then has a focused dialog they cannot see and the game will not respond to,
                        // with no visual cue to alt-tab back to. Ownership also returns focus to the
                        // game window when the dialog closes.
                        if (filtersSet && SUCCEEDED(fileDialog->Show(GetHWND(window))))
                        {
                            ComPtr<IShellItem> resultItem;
                            if (SUCCEEDED(fileDialog->GetResult(resultItem.GetAddressOf())))
                            {
                                PWSTR filePath = nullptr;
                                if (SUCCEEDED(resultItem->GetDisplayName(SIGDN_FILESYSPATH, &filePath)))
                                {
                                    resultFilename = String::toUtf8(filePath);
                                    CoTaskMemFree(filePath);
                                }
                            }
                        }
                    }
                }
            }
            return resultFilename;
        }

        std::string ShowFileDialog(SDL_Window* window, const FileDialogDesc& desc) override
        {
            return ShowFileDialogInternal(window, desc, false);
        }

        std::string ShowDirectoryDialog(SDL_Window* window, const std::string& title) override
        {
            FileDialogDesc desc;
            desc.Title = title;
            return ShowFileDialogInternal(window, desc, true);
        }

        bool HasFilePicker() const override
        {
            return true;
        }

    private:
        HWND GetHWND(SDL_Window* window)
        {
            HWND result = nullptr;
            if (window != nullptr)
            {
                SDL_SysWMinfo wmInfo;
                SDL_VERSION(&wmInfo.version);
                if (SDL_GetWindowWMInfo(window, &wmInfo) != SDL_TRUE)
                {
                    LOG_ERROR("SDL_GetWindowWMInfo failed %s", SDL_GetError());
                    exit(-1);
                }

                result = wmInfo.info.win.window;
            }
            return result;
        }

        static std::vector<COMDLG_FILTERSPEC> GetFilters(
            const std::vector<FileDialogDesc::Filter>& filters, std::vector<std::wstring>& outFiltersStorage)
        {
            std::vector<COMDLG_FILTERSPEC> result;
            for (const auto& filter : filters)
            {
                outFiltersStorage.emplace_back(String::toWideChar(filter.Name));
                outFiltersStorage.emplace_back(String::toWideChar(filter.Pattern));
            }

            for (auto it = outFiltersStorage.begin(); it != outFiltersStorage.end();)
            {
                const wchar_t* Name = (it++)->c_str();
                const wchar_t* Pattern = (it++)->c_str();
                result.push_back({ Name, Pattern });
            }

            return result;
        }
    };

    std::unique_ptr<IPlatformUiContext> CreatePlatformUiContext()
    {
        return std::make_unique<Win32Context>();
    }
} // namespace OpenRCT2::Ui

#endif // _WIN32
