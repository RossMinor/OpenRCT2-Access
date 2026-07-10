/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <openrct2/world/Location.hpp>
#include <optional>

namespace OpenRCT2::Ui::Accessibility
{
    // Result of checking whether a map tile has a walking route back to a park entrance, using the
    // same footpath flood-fill the guest rescue uses.
    enum class EntranceReachability
    {
        noEntrance,   // the park has no entrance to reach
        notOnPath,    // the tile has no footpath on it, so there is nothing to walk on
        reachable,    // a footpath on the tile connects to an entrance
        unreachable,  // a footpath on the tile exists but is cut off from every entrance
    };

    // Runs the footpath flood-fill from the park entrances and reports whether the given tile's path
    // connects to one. Bound to a keyboard command so a player can check any spot on the map.
    EntranceReachability CheckEntranceReachability(const TileCoordsXY& tile);

    // When a tile's path is cut off from every entrance (EntranceReachability::unreachable), this
    // returns the tile on that path's own network that comes closest to the entrance-connected network
    // (or, failing that, to the park entrance) - i.e. where the path stops short of connecting, so the
    // player can be told the coordinate of the break. Nullopt if the tile has no path.
    std::optional<TileCoordsXY> FindPathDisconnectPoint(const TileCoordsXY& tile);

    // Ctrl+H rescue: teleports every guest that is stranded (holds a "lost / go home / can't find"
    // thought and has no walking route to a park entrance, or is not on a footpath at all) to the
    // nearest park entrance, then announces how many were moved. Routed through the game's own
    // pick-up-and-place action so it stays deterministic and replicates in multiplayer; the teleports
    // run one guest at a time across action callbacks. Safe to call from map input.
    void RescueLostGuests();
} // namespace OpenRCT2::Ui::Accessibility
