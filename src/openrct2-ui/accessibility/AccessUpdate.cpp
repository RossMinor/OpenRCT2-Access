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

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <openrct2/Context.h>
#include <openrct2/core/Http.h>
#include <openrct2/core/Json.hpp>
#include <openrct2/core/String.hpp>
#include <openrct2/core/Zip.h>
#include <openrct2/platform/Platform.h>
#include <string>

namespace OpenRCT2::Ui::Accessibility
{
    // The version this build represents. Bump it to the version being shipped before each release;
    // the launch check compares it to the fork's latest GitHub release tag and offers an update
    // when they differ. (Set below the latest release to exercise the notification.)
    static constexpr const char* kAccessModVersion = "v0.5";
    static constexpr const char* kReleasesApiUrl =
        "https://api.github.com/repos/RossMinor/OpenRCT2-Access/releases/latest";

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

    // Launches the swap helper and quits. The helper waits for this process to exit, copies the
    // staged files over the install directory, relaunches, and cleans up.
    static void FinishInstall()
    {
#ifdef _WIN32
        const std::string installDir = Platform::GetCurrentExecutableDirectory();
        const std::string staging = _stagingDir.string();
        const auto batPath = (std::filesystem::temp_directory_path() / "openrct2-access-update.bat").string();

        std::ofstream bat(batPath, std::ios::binary | std::ios::trunc);
        bat << "@echo off\r\n"
            << ":wait\r\n"
            << "tasklist /fi \"imagename eq openrct2.exe\" 2>nul | find /i \"openrct2.exe\" >nul\r\n"
            << "if not errorlevel 1 (\r\n"
            << "  timeout /t 1 /nobreak >nul\r\n"
            << "  goto wait\r\n"
            << ")\r\n"
            << "xcopy /e /y /i \"" << staging << "\\*\" \"" << installDir << "\\\" >nul\r\n"
            << "start \"\" \"" << installDir << "\\openrct2.exe\"\r\n"
            << "rmdir /s /q \"" << staging << "\"\r\n"
            << "del \"%~f0\"\r\n";
        bat.close();

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
