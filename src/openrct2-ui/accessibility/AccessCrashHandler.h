/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

namespace OpenRCT2::Ui::Accessibility
{
    // Installs a top-level unhandled-exception handler that, on a crash, builds a concise
    // crash report (version, exception details, faulting module, a symbolised call stack and
    // the last assertion message) and copies it to the Windows clipboard, then shows a message
    // box telling the player it has been copied. This lets a blind player paste the report
    // straight into a GitHub issue instead of having to locate a dump file on disk.
    //
    // Safe to call multiple times; only the first call installs the handler. No-op off Windows.
    void CrashHandlerInit();
} // namespace OpenRCT2::Ui::Accessibility
