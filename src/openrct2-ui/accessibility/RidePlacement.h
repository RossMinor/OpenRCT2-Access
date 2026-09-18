/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <cstdint>
#include <openrct2/ride/RideTypes.h>
#include <openrct2/world/Location.hpp>
#include <optional>
#include <string>

namespace OpenRCT2::Ui::Accessibility
{
    // What the build will do at a frozen placement preview, predicted with the same height search the
    // build itself runs. Shared by stalls/flat rides and pre-built rides so both explain it alike.
    enum class PreviewHeight
    {
        fits,          // builds exactly at the height asked for
        raised,        // something is in the way, so - like the game's own tool - it goes up on supports
        clearsScenery, // only scenery is in the way; the build clears it first
        blocked,       // no height works; Enter will refuse
    };

    // The spoken line for a frozen placement preview: size, where the ride will sit and, when the
    // game will not put it where it was aimed, what is in the way and what Enter will do about it.
    // askedZ/builtZ are world Z; builtZ is only read for `raised`. `reason` is the game's own error
    // text at askedZ (e.g. "Footpath in the way"); `detail` is appended to a blocked explanation.
    std::string DescribeRidePreview(
        const std::string& rideName, int32_t width, int32_t length, PreviewHeight kind, int32_t askedZ, int32_t builtZ,
        const std::string& reason, const std::string& detail = {});

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
    // repositioning, and returns the tile the preview was frozen at so the caller can put the map
    // cursor back there rather than leaving it wherever inspecting the footprint left it. Nullopt
    // (and no other effect) unless a footprint preview is currently frozen.
    std::optional<CoordsXY> AccessibleRidePlacementPickup();

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
