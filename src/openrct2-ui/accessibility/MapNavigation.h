/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <openrct2-ui/input/InputManager.h>
#include <openrct2/Identifiers.h>
#include <openrct2/world/Location.hpp>
#include <optional>
#include <string>

namespace OpenRCT2::Ui::Accessibility
{
    // In-game map cursor for screen-reader users. Arrow keys move a tile cursor around
    // the park, 'C' reads the current coordinates, and reaching the edge of the owned
    // land announces the border direction. Returns true if the event was consumed.
    bool HandleMapNavigationKey(const InputEvent& e);

    // Moves the map cursor to the bottom-left corner of the given ride, leaves menu mode,
    // centres the view, and announces the ride name and footprint size.
    void GoToRide(RideId rideId);

    // Returns "X n, Y n" for the ride's bottom-left tile relative to the park origin, or
    // an empty string if the ride has no tiles.
    std::string GetRideLocationText(RideId rideId);

    // If the in-game toolbar menu is active, re-speak its focused item. Used when a child
    // window closes and focus returns to the toolbar.
    void ReannounceToolbarItemIfMenuMode();

    // Returns true when the map cursor owns the arrow keys (gameplay scene, not in menu
    // mode). Use this to suppress competing view-scroll shortcuts on those keys.
    bool IsMapCursorActive();

    // Returns true when the in-game toolbar menu mode is active (Tab-opened toolbar navigation).
    // The focus overlay uses this to highlight the focused toolbar item.
    bool IsInMenuMode();

    // Returns true whenever keyboard navigation owns the arrow keys in-game - the map cursor, the
    // toolbar menu, or an accessible window (anything except mouse mode). Use this to suppress the
    // engine's arrow-key view scrolling so the camera doesn't drift while navigating menus.
    bool IsKeyboardNavActive();

    // The spoken description of what is on a given map tile - the same text the map cursor
    // announces as it moves (ride name, path, water, scenery, empty land, etc.).
    std::string DescribeTile(const TileCoordsXY& tile);

    // "X n, Y n" for a tile using the same absolute coordinate convention as the in-game map
    // cursor. Shared so features outside MapNavigation (e.g. ride construction) read identically.
    std::string SpokenTileCoordsText(const TileCoordsXY& tile);

    // A footpath surface object's name as the mod SPEAKS it - the game's own windows and tooltips
    // keep showing the object's real name. Some stock paths are named after something only a sighted
    // player can judge, so they gain a clarifying word while keeping the original name recognisable;
    // trailing shape suffixes like "(Rounded)" are dropped, since they describe the kerb artwork
    // rather than the surface. Shared so the tile readout and the footpath window never disagree
    // about what a path is called.
    std::string SpokenPathSurfaceName(std::string name);

    // Describes a placed ride entrance or exit for speech: which compass way its doorway faces and
    // whether a footpath is already connected there, e.g. ", facing East, path connected". Pass a
    // ride station's Entrance or Exit. Empty if the location is null.
    std::string DescribeEntranceExitConnection(const TileCoordsXYZD& loc);

    // True if the tile holds something that would block building a ride and is NOT auto-clearable -
    // a path, another ride's track, a ride entrance/exit, or a banner. Used to decide whether
    // clearing would actually let a ride be placed (it won't if a path is in the way). Small/large
    // scenery and walls/fences do not count, as the clear-scenery tool removes those automatically.
    bool TileHasNonSceneryBlocker(const TileCoordsXY& tile);

    // The screen position of the keyboard map cursor's tile (the viewport centre, since the cursor
    // keeps the view centred on itself). Ride construction feeds this to the build tool so pieces
    // are placed where the map cursor is, not where the mouse is. Empty if the cursor isn't ready.
    std::optional<ScreenCoordsXY> GetMapCursorScreenPos();

    // Moves the keyboard map cursor to the given tile and centres the view, silently (no
    // announcement). The follow feature calls this each time the watched NPC changes tile so the
    // cursor - and therefore 'C' coordinates and the camera - tracks them.
    void FollowCursorTo(const TileCoordsXY& tile);

    // Leaves the in-game toolbar menu mode (if active) silently, handing control back to the map
    // cursor. Used when something else takes over the keyboard, e.g. starting to follow an NPC.
    void LeaveMenuMode();

    // Polled each frame. Shows a tile-selection highlight on the keyboard map cursor's tile so
    // sighted users can see where focus is, and clears it when focus moves off the map.
    void TickFocusHighlight();

    // Polled each frame. Announces the cost/refund of player-initiated transactions (building,
    // demolishing, buying/selling land, marketing) once they occur. Guest spending and recurring
    // costs are not announced.
    void TickMoneyAnnounce();

    // Polled once per frame. When the map cursor is active and the player moves the mouse,
    // moves the focus to the tile under the pointer and reads it. Keyboard-driven warps are
    // ignored so the two input methods share a single focus without fighting.
    void UpdateMapCursorFromMouse();
} // namespace OpenRCT2::Ui::Accessibility
