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
} // namespace OpenRCT2::Ui::Accessibility
