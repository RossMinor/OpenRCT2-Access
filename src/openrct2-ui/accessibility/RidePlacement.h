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
#include <optional>
#include <string>

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

    // Handles Enter during placement. For the footprint this is two-stage: the first Enter freezes
    // the footprint at the cursor (a preview the player can arrow around and inspect); the second
    // builds it at that frozen spot, searching upward for a valid height. On a build failure the
    // error is announced and placement stays active. Entrance/exit stages place on the first Enter.
    void AccessibleRidePlacementAtTile(const CoordsXY& mapCoords);

    // Backspace during a footprint preview: picks the ride back up so it follows the cursor again for
    // repositioning. No-op unless a footprint preview is currently frozen.
    void AccessibleRidePlacementPickup();

    // If a footprint preview is frozen and covers the given tile, returns the ride's name so the tile
    // reader can announce the ride as though it were already placed there (letting the player trace
    // the footprint's shape by arrowing over it). Returns nullopt otherwise.
    std::optional<std::string> AccessibleRidePlacementPreviewLabel(const TileCoordsXY& tile);

    // Aborts placement and demolishes the not-yet-built ride so no empty ride is left behind.
    void AccessibleRidePlacementCancel();

    // While positioning a ride's footprint, fills outRange with the world-coordinate bounding box
    // the footprint would occupy with the cursor at its bottom-left corner, so the placement area
    // can be highlighted. Returns false when not positioning a footprint (use the single cursor
    // tile instead).
    bool AccessibleRidePlacementFootprintRange(const CoordsXY& cursor, MapRange& outRange);
} // namespace OpenRCT2::Ui::Accessibility
