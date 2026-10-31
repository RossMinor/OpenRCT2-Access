/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <openrct2/Identifiers.h>
#include <openrct2/world/Location.hpp>
#include <string>

namespace OpenRCT2
{
    enum class StaffType : uint8_t;
}

namespace OpenRCT2::Ui::Accessibility
{
    // Keyboard replacements for the game's two patrol area tools, both on the map cursor.
    //
    // Setting (windows/PatrolArea.cpp): started by the staff window's "Set Patrol Area" choice. Space
    // adds the brush or marked area to the staff member's patrol area - or removes it when the
    // cursor's tile is already patrolled, as the game's first click decides. Escape goes back to the
    // staff window.
    //
    // Showing (the staff list's "show patrol area" tool): every patrol area of one staff type. Space on
    // a path picks the nearest staff member of that type who patrols it, as the game's click does, and
    // starts setting theirs. Escape goes back to the staff list.
    //
    // While either is on, the game shows the patrol areas on the map and the tile readings say who
    // patrols each tile.
    void BeginPatrolAreaMode(EntityId staffId);
    void BeginPatrolAreaView(StaffType staffType);
    bool IsPatrolAreaModeActive(); // either of the two
    bool IsPatrolAreaViewActive(); // showing only
    bool PatrolAreaModeWouldRemove(const TileCoordsXY& decidingTile);
    void PatrolAreaModeApply(const MapRange& area, const TileCoordsXY& decidingTile);
    void PatrolAreaViewLocate(const TileCoordsXY& tile);
    void PatrolAreaModeFinish();
    // Ends either without a word (a new park, or leaving the park scene).
    void PatrolAreaModeReset();

    // Who patrols this tile, or this brush of several tiles - empty when neither mode is on, since
    // the game only shows patrol areas while one of its patrol tools is open.
    std::string PatrolAreaTileText(const TileCoordsXY& tile);
    std::string PatrolAreaRangeText(const MapRange& area);
} // namespace OpenRCT2::Ui::Accessibility
