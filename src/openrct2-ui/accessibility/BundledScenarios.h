/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

namespace OpenRCT2
{
    struct IPlatformEnvironment;
}

namespace OpenRCT2::Ui::Accessibility
{
    // Copies the flat starter maps the mod ships into the player's own scenario folder, so they can
    // be picked from the scenario list without anyone having to find a folder and move files into
    // it. They are flat and empty on purpose: laying out a park by keyboard is far easier on ground
    // that has no hills to fight, and the stock scenarios all have terrain.
    //
    // A copy is unavoidable. The scenario index scans exactly three places - the RCT1 install, the
    // RCT2 install, and the player's own scenario folder - and the OpenRCT2 program folder is not
    // one of them, so a map shipped beside the executable would simply never be seen.
    //
    // Only copies a map that is not already there, and never overwrites one: a player who has
    // edited a starter map keeps their version. The flip side is that deleting one brings it back
    // on the next launch, which is the price of not keeping a record of what has been installed
    // before.
    //
    // Must run BEFORE the scenario index is built, or the maps will not appear until the following
    // launch. Failures are logged and swallowed - not being able to copy a bonus map is never a
    // reason to stop the game from starting.
    void InstallBundledScenarios(IPlatformEnvironment& env);
} // namespace OpenRCT2::Ui::Accessibility
