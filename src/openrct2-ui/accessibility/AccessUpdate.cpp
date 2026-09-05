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
#include <algorithm>
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
#include <vector>

namespace OpenRCT2::Ui::Accessibility
{
    // The version this build represents, taken from the single source of truth in Version.h
    // (kAccessVersion). Bump kAccessVersion before each release; the launch check compares this
    // to the fork's latest GitHub release tag and offers an update when they differ.
    static constexpr const char* kAccessModVersion = kAccessVersionTag;
    static constexpr const char* kReleasesApiUrl =
        "https://api.github.com/repos/" kAccessUpdateRepo "/releases/latest";

    // Upstream OpenRCT2, checked separately from the mod's own releases. Nothing in the game can act
    // on the result - the mod IS the executable, so moving to a newer OpenRCT2 means a new mod build,
    // not a download - but without this a player has no way to tell that the engine under them has
    // stopped keeping pace, and the mod pinning them to an old version looks like the game going
    // stale on its own.
    static constexpr const char* kEngineReleasesApiUrl =
        "https://api.github.com/repos/OpenRCT2/OpenRCT2/releases/latest";

    static std::future<void> _checkFuture;
    static std::atomic<bool> _started{ false };
    static std::atomic<bool> _ready{ false }; // release/acquire gate for the strings written below
    static bool _announced = false;
    static bool _available = false;
    static std::string _tag;
    static std::string _name;
    static std::string _downloadUrl;

    // Latest upstream OpenRCT2, and whether it is ahead of the engine this build was compiled
    // against. Written by the same worker as the fields above, under the same _ready gate.
    static std::string _engineLatest;
    static bool _engineBehind = false;

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

    // Spoken once per run, as soon as a screen reader is actually listening. Set when it has been
    // said (or deliberately skipped). _launchMomentClaimed is raised by the post-update messages
    // above, which are more urgent and would otherwise be talked over by the version line.
    static bool _announcedVersion = false;
    static bool _launchMomentClaimed = false;

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

    // Splits "0.5.4" into { 0, 5, 4 }, stopping at anything that is not a digit or a dot so a tag
    // like "v0.5.4-rc1" still yields the numbers in front of the suffix.
    static std::vector<int> SplitVersion(const std::string& version)
    {
        std::vector<int> parts;
        int current = 0;
        bool anyDigits = false;
        for (const char c : version)
        {
            if (c >= '0' && c <= '9')
            {
                current = current * 10 + (c - '0');
                anyDigits = true;
            }
            else if (c == '.')
            {
                parts.push_back(anyDigits ? current : 0);
                current = 0;
                anyDigits = false;
            }
            else
            {
                break;
            }
        }
        if (anyDigits)
            parts.push_back(current);
        return parts;
    }

    // Negative when a is older than b, 0 when equal, positive when newer. Compared component by
    // component rather than as strings, or "0.5.10" would sort below "0.5.4" and the check would
    // start claiming the engine had gone backwards.
    static int CompareVersions(const std::string& a, const std::string& b)
    {
        const auto pa = SplitVersion(a);
        const auto pb = SplitVersion(b);
        const size_t count = std::max(pa.size(), pb.size());
        for (size_t i = 0; i < count; i++)
        {
            const int va = i < pa.size() ? pa[i] : 0;
            const int vb = i < pb.size() ? pb[i] : 0;
            if (va != vb)
                return va < vb ? -1 : 1;
        }
        return 0;
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

        // Now the engine. Kept in its own try so a failure here cannot discard the mod-update result
        // gathered above - the actionable one of the two.
        try
        {
            Http::Request request;
            request.url = kEngineReleasesApiUrl;
            request.method = Http::Method::GET;

            const auto res = Http::Do(request);
            if (res.status == Http::Status::Ok)
            {
                const auto root = Json::FromString(res.body);
                std::string tag = Json::GetString(root["tag_name"]);
                if (!tag.empty() && (tag[0] == 'v' || tag[0] == 'V'))
                    tag.erase(0, 1);

                if (!tag.empty() && CompareVersions(kOpenRCT2Version, tag) < 0)
                {
                    _engineLatest = tag;
                    _engineBehind = true;
                }
            }
        }
        catch (const std::exception&)
        {
            // Same reasoning as above: on bad data, say nothing at all.
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
                    _launchMomentClaimed = true;
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
                    _launchMomentClaimed = true;
                    ScreenReaderSpeak(
                        "Update installed. Would you like to open the changelog in your browser? Use the up and "
                        "down arrow keys to choose, then press Enter. Yes.");
                }
            }
        }

        // Say what this build is, once, at the start of the session. A player who has just installed
        // or auto-updated has no other way to confirm what they are running without sight, and it is
        // the first thing worth knowing when something misbehaves. Both numbers matter and they are
        // not interchangeable: the mod version is what updates track, while the OpenRCT2 version is
        // what the installer gates on, so a build only ever fits the game it names here.
        //
        // Gated on the reader actually being available rather than on a frame count - this runs from
        // the input tick, which can start before ScreenReaderInit() has been reached, and speaking
        // into a reader that is not up yet would drop the line silently.
        if (!_announcedVersion && ScreenReaderIsAvailable())
        {
            _announcedVersion = true;
            if (!_launchMomentClaimed)
            {
                ScreenReaderSpeak(
                    "OpenRCT2 Access, mod version " + std::string(kAccessVersion) + ", for OpenRCT2 version "
                    + kOpenRCT2Version + ".");
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
            {
                ScreenReaderSpeak(
                    "Accessibility mod update available, version " + _tag
                    + ". Press F5 to download and install it.");
            }
            else if (_engineBehind)
            {
                // Only when there is no mod update to offer. A mod update is the actionable one, and
                // it may well be the very build that moves to this engine, so leading with the engine
                // notice would send the player after the wrong thing.
                //
                // The warning not to update OpenRCT2 by hand is the point of the message: the mod is
                // the executable, so installing stock OpenRCT2 over it removes the mod entirely, and
                // this is exactly the moment a player would be tempted to do that.
                ScreenReaderSpeak(
                    "OpenRCT2 " + _engineLatest + " has been released. This accessibility build is for OpenRCT2 "
                    + kOpenRCT2Version
                    + ". Updating OpenRCT2 yourself would remove the accessibility mod, so wait for a mod update "
                      "instead.");
                LogAnnouncement(
                    "OpenRCT2 " + _engineLatest + " is available; this accessibility build is for "
                    + kOpenRCT2Version + ".");
            }
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
