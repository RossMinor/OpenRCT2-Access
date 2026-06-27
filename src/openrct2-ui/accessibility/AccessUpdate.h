/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <string>

namespace OpenRCT2::Ui::Accessibility
{
    // Polled once per frame. On the first call it kicks off a background check of the accessibility
    // mod's latest GitHub release; when the result arrives it announces (once) whether an update is
    // available. The actual download/install is driven separately.
    void TickAccessUpdate();

    // True once the background check has finished and found a newer release than this build.
    bool IsAccessUpdateAvailable();

    // Begins downloading and installing the available update: fetches the release ZIP, extracts it,
    // then (on the next tick) launches a helper that swaps the files in once the game exits and
    // relaunches. Safe to call repeatedly; ignored if no update is available or one is in progress.
    void StartAccessUpdateInstall();

    // This build's accessibility mod version string (e.g. "v0.6").
    const char* GetAccessModVersion();

    // The newer release's tag/name and the download URL of its ZIP asset (empty until available).
    std::string GetAccessUpdateTag();
    std::string GetAccessUpdateName();
    std::string GetAccessUpdateDownloadUrl();
} // namespace OpenRCT2::Ui::Accessibility
