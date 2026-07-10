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

    // After a self-update relaunch, the mod offers to open the new release's changelog as a modal
    // yes/no menu. While the prompt is pending it owns the keyboard: Up/Down move between Yes and No,
    // Enter confirms the highlighted choice (Yes opens the changelog, No continues), and Escape
    // cancels (same as No). One-shot per update.
    bool IsChangelogPromptPending();
    void ChangelogPromptMove(int32_t delta); // -1 = up/previous, +1 = down/next
    void ChangelogPromptConfirm();
    void ChangelogPromptCancel();

    // This build's accessibility mod version string (e.g. "v0.6").
    const char* GetAccessModVersion();

    // The newer release's tag/name and the download URL of its ZIP asset (empty until available).
    std::string GetAccessUpdateTag();
    std::string GetAccessUpdateName();
    std::string GetAccessUpdateDownloadUrl();
} // namespace OpenRCT2::Ui::Accessibility
