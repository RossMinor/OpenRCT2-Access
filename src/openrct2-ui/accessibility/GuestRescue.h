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
#include <string>

namespace OpenRCT2
{
    struct TileElement;
} // namespace OpenRCT2

namespace OpenRCT2::Ui::Accessibility
{
    // Result of checking whether a map tile has a walking route back to a park entrance, using the
    // same footpath flood-fill the guest rescue uses.
    enum class EntranceReachability
    {
        noEntrance,   // the park has no entrance to reach
        notOnPath,          // no footpath, ride entrance, ride exit or stall on the tile
        accessNotConnected, // a ride entrance, exit or stall with no footpath connected to it
        reachable,          // the tile's path (or the path connected to its entrance/exit/stall) reaches an entrance
        unreachable,        // that path exists but is cut off from every park entrance
    };

    // Runs the footpath flood-fill from the park entrances and reports whether the given tile connects
    // to one. A footpath on the tile is checked directly; without one, a ride entrance, exit or stall
    // is checked through the paths connected to it, using the game's own connection rule. Bound to a
    // keyboard command so a player can check any spot on the map.
    EntranceReachability CheckEntranceReachability(const TileCoordsXY& tile);

    // True if a footpath connects to this ride entrance, ride exit or stall (an element on `tile`),
    // under the game's own rule - the same one Ctrl+P and the game's "not connected" warnings use: a
    // path at its door or serving side, at a matching height, reaching back toward it. False for any
    // other element.
    bool IsAccessPointConnected(const TileCoordsXY& tile, const OpenRCT2::TileElement& el);

    // The same rule for a doorway given by position and the direction it faces - for announcing an
    // entrance or exit just placed, from the location the ride's station records for it.
    bool IsDoorwayConnected(const TileCoordsXYZ& loc, Direction doorway);

    // Names the ride entrance, exit or stall on this tile for speech: "ride entrance", "ride exit", or
    // the stall's own name (e.g. "Hot Dog Stall 1"). Empty if there is none.
    std::string DescribeAccessPoint(const TileCoordsXY& tile);

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
