/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <openrct2/ride/RideTypes.h>
#include <openrct2/world/Location.hpp>

namespace OpenRCT2::Ui::Accessibility
{
    // True if this ride can be placed by the keyboard placement flow below. Currently shops and
    // stalls (facilities), which are a single footprint with no entrance or exit to position.
    bool AccessibleRidePlacementSupported(const RideSelection& item);

    // Creates the ride and enters keyboard placement mode. The map cursor then positions the
    // footprint; the player rotates with R, builds with Enter, and cancels with Escape. Replaces
    // the mouse-only construction window for the supported ride types.
    void BeginAccessibleRidePlacement(const RideSelection& item);

    // True while a placement started by BeginAccessibleRidePlacement is waiting for the player to
    // position and build the footprint. The map cursor uses this to route R / Enter / Escape.
    bool IsAccessibleRidePlacementActive();

    // Turns the footprint 90 degrees and announces the new facing.
    void AccessibleRidePlacementRotate();

    // Attempts to build the footprint at the given map tile (the keyboard cursor position),
    // searching upward for a valid height. On success the ride is opened; on failure the error is
    // announced and placement stays active so the player can try another tile.
    void AccessibleRidePlacementAtTile(const CoordsXY& mapCoords);

    // Aborts placement and demolishes the not-yet-built ride so no empty ride is left behind.
    void AccessibleRidePlacementCancel();
} // namespace OpenRCT2::Ui::Accessibility
