/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "AccessUpdate.h"

#include "ScreenReader.h"

#include <SDL.h>
#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <openrct2/Context.h>
#include <openrct2/Version.h>
#include <openrct2/core/Http.h>
#include <openrct2/core/Json.hpp>
#include <openrct2/core/String.hpp>
#include <openrct2/core/Zip.h>
#include <openrct2/platform/Platform.h>
#include <openrct2/ui/UiContext.h>
#include <string>

namespace OpenRCT2::Ui::Accessibility
{
    // The version this build represents, taken from the single source of truth in Version.h
    // (kAccessVersion). Bump kAccessVersion before each release; the launch check compares this
    // to the fork's latest GitHub release tag and offers an update when they differ.
    static constexpr const char* kAccessModVersion = kAccessVersionTag;
    static constexpr const char* kReleasesApiUrl =
        "https://api.github.com/repos/" kAccessUpdateRepo "/releases/latest";

    static std::future<void> _checkFuture;
    static std::atomic<bool> _started{ false };
    static std::atomic<bool> _ready{ false }; // release/acquire gate for the strings written below
    static bool _announced = false;
    static bool _available = false;
    static std::string _tag;
    static std::string _name;
    static std::string _downloadUrl;

    // Install pipeline. _installState: 0 idle, 1 downloading/extracting, 2 staged (ready to swap),
    // 3 failed, 4 swapping (helper launched, quitting). Strings are written by the worker before
    // moving to state 2/3, then read on the main thread.
    static std::future<void> _installFuture;
    static std::atomic<int> _installState{ 0 };
    static std::filesystem::path _stagingDir;
    static std::string _installError;

    // After an install swaps the files and relaunches, the fresh build finds this marker (written by
    // the old build just before it quit) and offers to open the new release's changelog page. It is a
    // one-shot: read once, then deleted. Kept in the temp dir alongside the swap helper, so it is not
    // touched by the file swap and survives the relaunch.
    static bool _checkedUpdateMarker = false;
    static bool _changelogPromptPending = false;
    static std::string _changelogUrl;
    // Highlighted choice in the yes/no changelog menu: 0 = Yes (open the changelog), 1 = No.
    static int32_t _changelogChoice = 0;

    static std::filesystem::path ChangelogMarkerPath()
    {
        return std::filesystem::temp_directory_path() / "openrct2-access-changelog.txt";
    }

    // Written by the update helper when the installer refuses the update - most often because the
    // player's OpenRCT2 has been updated since this mod build was released, so the two no longer
    // match. Its presence on the next launch is how a failure that happened while the game was
    // closed gets reported to a player who cannot see the helper's console window.
    static std::filesystem::path InstallFailedMarkerPath()
    {
        return std::filesystem::temp_directory_path() / "openrct2-access-update-failed.txt";
    }

    // Where the helper sends the installer's output, so a refusal can be read after the fact.
    static std::filesystem::path InstallLogPath()
    {
        return std::filesystem::temp_directory_path() / "openrct2-access-update.log";
    }

    static void RunCheck()
    {
#if !defined(DISABLE_HTTP) && !defined(DISABLE_VERSION_CHECKER)
        try
        {
            Http::Request request;
            request.url = kReleasesApiUrl;
            request.method = Http::Method::GET;

            const auto res = Http::Do(request);
            if (res.status == Http::Status::Ok)
            {
                const auto root = Json::FromString(res.body);
                const std::string tag = Json::GetString(root["tag_name"]);
                const std::string name = Json::GetString(root["name"]);

                // Find the release's ZIP asset (fall back to the first asset).
                std::string url;
                const auto assets = root["assets"];
                if (assets.is_array())
                {
                    for (const auto& asset : assets)
                    {
                        const std::string assetName = Json::GetString(asset["name"]);
                        const std::string assetUrl = Json::GetString(asset["browser_download_url"]);
                        if (url.empty())
                            url = assetUrl;
                        if (String::endsWith(assetName, ".zip", true))
                        {
                            url = assetUrl;
                            break;
                        }
                    }
                }

                if (!tag.empty() && tag != kAccessModVersion)
                {
                    _tag = tag;
                    _name = name.empty() ? tag : name;
                    _downloadUrl = url;
                    _available = true;
                }
            }
        }
        catch (const std::exception&)
        {
            // Network/parse failure: silently leave _available false so we never nag on bad data.
        }
#endif
        _ready = true;
    }

    // Downloads the release ZIP and extracts it to a staging folder. Runs on a worker thread.
    static void RunInstall(std::string url)
    {
        try
        {
            Http::Request request;
            request.url = url;
            request.method = Http::Method::GET;
            const auto res = Http::Do(request);
            if (res.status != Http::Status::Ok || res.body.empty())
            {
                _installError = "the download did not complete";
                _installState = 3;
                return;
            }

            const auto tempDir = std::filesystem::temp_directory_path();
            const auto zipPath = tempDir / "openrct2-access-update.zip";
            {
                std::ofstream zipFile(zipPath, std::ios::binary | std::ios::trunc);
                zipFile.write(res.body.data(), static_cast<std::streamsize>(res.body.size()));
            }

            std::error_code ec;
            const auto staging = tempDir / "openrct2-access-update";
            std::filesystem::remove_all(staging, ec);
            std::filesystem::create_directories(staging, ec);

            auto zip = Zip::TryOpen(zipPath.string(), ZipAccess::read);
            if (zip == nullptr)
            {
                _installError = "the update archive could not be opened";
                _installState = 3;
                return;
            }

            const size_t count = zip->GetNumFiles();
            for (size_t i = 0; i < count; i++)
            {
                const std::string name = zip->GetFileName(i);
                if (name.empty() || name.back() == '/' || name.back() == '\\')
                    continue; // directory entry
                const auto outPath = staging / std::filesystem::u8path(name);
                std::filesystem::create_directories(outPath.parent_path(), ec);
                const auto data = zip->GetFileData(name);
                std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
                out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
            }

            _stagingDir = staging;
            _installState = 2;
        }
        catch (const std::exception& e)
        {
            _installError = e.what();
            _installState = 3;
        }
    }

    void StartAccessUpdateInstall()
    {
        if (!IsAccessUpdateAvailable())
            return;
        int expected = 0;
        if (!_installState.compare_exchange_strong(expected, 1))
            return; // already in progress
        if (_downloadUrl.empty())
        {
            _installError = "no download is available for this release";
            _installState = 3;
            return;
        }
        ScreenReaderSpeak("Downloading update, this may take a minute. The game will restart when it is ready.");
        _installFuture = std::async(std::launch::async, RunInstall, _downloadUrl);
    }

    // Launches the swap helper and quits. The helper waits for this process to exit, runs the
    // installer bundled in the release, relaunches, and cleans up.
    //
    // This used to be a plain "xcopy staging\* installDir\", which was fine when a release was a
    // whole portable copy of the game. Now that a release only carries the mod's own files, copying
    // them blindly would drop a build meant for one OpenRCT2 version onto whatever version the
    // player happens to be running - and because the executable is validated against data\g2.dat by
    // a sprite count compiled into it, the result is missing or wrong graphics rather than an error
    // anyone could act on. Deferring to the installer buys the version gate, the backup that makes
    // Uninstall-OpenRCT2Access.bat work, and a copy limited to the files the mod actually owns.
    static void FinishInstall()
    {
#ifdef _WIN32
        const std::string installDir = Platform::GetCurrentExecutableDirectory();
        const std::string staging = _stagingDir.string();
        const auto batPath = (std::filesystem::temp_directory_path() / "openrct2-access-update.bat").string();
        const std::string logPath = InstallLogPath().string();
        const std::string failMarker = InstallFailedMarkerPath().string();
        const std::string changelogMarker = ChangelogMarkerPath().string();

        std::ofstream bat(batPath, std::ios::binary | std::ios::trunc);
        bat << "@echo off\r\n"
            << ":wait\r\n"
            << "tasklist /fi \"imagename eq openrct2.exe\" 2>nul | find /i \"openrct2.exe\" >nul\r\n"
            << "if not errorlevel 1 (\r\n"
            << "  timeout /t 1 /nobreak >nul\r\n"
            << "  goto wait\r\n"
            << ")\r\n"
            // -Yes because the player already agreed to the update in-game; -NoPause because this
            // window is minimised and a prompt here would hang the update with nobody to answer it.
            << "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" << staging
            << "\\OpenRCT2Access-Installer.ps1\" -TargetPath \"" << installDir << "\" -Yes -NoPause > \"" << logPath
            << "\" 2>&1\r\n"
            // On refusal the game is untouched, so it is still safe to relaunch - but the changelog
            // prompt would be a lie, and the player needs to be told what happened.
            << "if errorlevel 1 (\r\n"
            << "  del \"" << changelogMarker << "\" >nul 2>&1\r\n"
            << "  echo " << logPath << "> \"" << failMarker << "\"\r\n"
            << ")\r\n"
            << "start \"\" \"" << installDir << "\\openrct2.exe\"\r\n"
            << "rmdir /s /q \"" << staging << "\"\r\n"
            << "del \"%~f0\"\r\n";
        bat.close();

        // Leave a marker so the relaunched build can offer to show this release's changelog.
        try
        {
            const std::string releaseUrl = "https://github.com/" kAccessUpdateRepo "/releases/tag/" + _tag;
            std::ofstream marker(ChangelogMarkerPath(), std::ios::binary | std::ios::trunc);
            marker << releaseUrl;
        }
        catch (const std::exception&)
        {
            // A missing changelog prompt is harmless; don't let it block the update.
        }

        ScreenReaderSpeak("Update downloaded. Installing now. The game will close and restart.");
        std::system(("start \"OpenRCT2 update\" /min \"" + batPath + "\"").c_str());
        ContextQuit();
#else
        ScreenReaderSpeak("Automatic install is only supported on Windows.");
        _installState = 0;
#endif
    }

    void TickAccessUpdate()
    {
        // On the first frame after a self-update relaunch, offer to open the new release's changelog.
        if (!_checkedUpdateMarker)
        {
            _checkedUpdateMarker = true;

            // A refused update leaves the game exactly as it was, so this is the only sign the
            // player gets that the update they asked for did not happen. Say so plainly and name
            // the log, rather than letting them assume they are running the new version.
            {
                std::error_code failEc;
                const auto failPath = InstallFailedMarkerPath();
                if (std::filesystem::exists(failPath, failEc))
                {
                    std::filesystem::remove(failPath, failEc); // one-shot
                    ScreenReaderSpeak(
                        "The update could not be installed, and the game has been left as it was. This usually means "
                        "the update was built for a different version of OpenRCT2 than the one you have. Details are "
                        "in openrct2-access-update.log in your temp folder.");
                    LogAnnouncement(
                        "Update refused. The mod build did not match this OpenRCT2 version. See "
                        + InstallLogPath().string());
                }
            }

            const auto markerPath = ChangelogMarkerPath();
            std::error_code ec;
            if (std::filesystem::exists(markerPath, ec))
            {
                std::ifstream marker(markerPath, std::ios::binary);
                std::getline(marker, _changelogUrl);
                marker.close();
                std::filesystem::remove(markerPath, ec); // one-shot: never prompt twice for the same update

                // Be tolerant of how the marker was written: strip a leading UTF-8 BOM and any
                // surrounding whitespace/CR so the URL handed to the browser is clean (a stray BOM
                // makes ShellExecute silently fail to open the page).
                if (_changelogUrl.rfind("\xEF\xBB\xBF", 0) == 0)
                    _changelogUrl.erase(0, 3);
                while (!_changelogUrl.empty()
                       && (_changelogUrl.back() == '\r' || _changelogUrl.back() == '\n'
                           || _changelogUrl.back() == ' ' || _changelogUrl.back() == '\t'))
                    _changelogUrl.pop_back();
                while (!_changelogUrl.empty() && (_changelogUrl.front() == ' ' || _changelogUrl.front() == '\t'))
                    _changelogUrl.erase(0, 1);

                // The relaunched game can come up behind other windows (the update helper spawns it
                // from a background console), leaving the player alt-tabbing to find it. Force our
                // window to the foreground so they land straight back in the game.
                if (auto* window = static_cast<SDL_Window*>(GetContext()->GetUiContext().GetWindow());
                    window != nullptr)
                {
                    SDL_RestoreWindow(window); // in case it came up minimised
                    SDL_ShowWindow(window);
                    SDL_RaiseWindow(window);
                }

                if (!_changelogUrl.empty())
                {
                    _changelogPromptPending = true;
                    _changelogChoice = 0; // default highlight: Yes
                    ScreenReaderSpeak(
                        "Update installed. Would you like to open the changelog in your browser? Use the up and "
                        "down arrow keys to choose, then press Enter. Yes.");
                }
            }
        }

        // Kick off the check on the first frame, then announce once the result is in.
        if (!_started.exchange(true))
        {
            _checkFuture = std::async(std::launch::async, RunCheck);
            return;
        }

        if (_ready.load() && !_announced)
        {
            _announced = true;
            if (_available)
                ScreenReaderSpeak(
                    "Accessibility mod update available, version " + _tag
                    + ". Press F5 to download and install it.");
        }

        // Drive the install pipeline once the worker has staged the files (or failed).
        const int installState = _installState.load();
        if (installState == 2)
        {
            _installState = 4; // swapping; prevent re-entry
            FinishInstall();
        }
        else if (installState == 3)
        {
            const std::string err = _installError;
            _installState = 0; // allow another attempt
            ScreenReaderSpeak("Update failed: " + (err.empty() ? std::string("unknown error") : err) + ".");
        }
    }

    bool IsAccessUpdateAvailable()
    {
        return _ready.load() && _available;
    }

    bool IsChangelogPromptPending()
    {
        return _changelogPromptPending;
    }

    void ChangelogPromptMove(int32_t delta)
    {
        if (!_changelogPromptPending)
            return;
        // Two options (Yes/No); wrap so Up and Down both simply flip between them.
        _changelogChoice = ((_changelogChoice + delta) % 2 + 2) % 2;
        ScreenReaderSpeak(_changelogChoice == 0 ? "Yes" : "No");
    }

    void ChangelogPromptConfirm()
    {
        if (!_changelogPromptPending)
            return;
        _changelogPromptPending = false;
        if (_changelogChoice == 0 && !_changelogUrl.empty())
            GetContext()->GetUiContext().OpenURL(_changelogUrl);
        else
            ScreenReaderSpeak("Continuing");
    }

    void ChangelogPromptCancel()
    {
        if (!_changelogPromptPending)
            return;
        _changelogPromptPending = false;
    }

    const char* GetAccessModVersion()
    {
        return kAccessModVersion;
    }

    std::string GetAccessUpdateTag()
    {
        return _tag;
    }

    std::string GetAccessUpdateName()
    {
        return _name;
    }

    std::string GetAccessUpdateDownloadUrl()
    {
        return _downloadUrl;
    }
} // namespace OpenRCT2::Ui::Accessibility
