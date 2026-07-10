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

namespace OpenRCT2
{
    struct EntranceElement;
    struct TrackElement;
} // namespace OpenRCT2

namespace OpenRCT2::Ui::Accessibility
{
    // === The single source of truth for spoken compass directions ===
    //
    // Every accessibility readout that names a direction MUST go through this file, so that "North"
    // always means the same world edge everywhere - in spoken tile coordinates, ride placement, path
    // ramps, ride entrance/exit facing and stall facing alike. When a direction reads wrong, fix it
    // here, once, instead of patching each call site (which is how the same bug kept reappearing).
    //
    // The engine's Direction is a FIXED world frame that does NOT rotate with the camera - the same
    // frame the spoken tile coordinates use:
    //   0 = -x = East, 1 = +y = South, 2 = +x = West, 3 = -y = North.

    // Compass name ("East"/"South"/"West"/"North") of an absolute world direction. Only the low two
    // bits are used, so callers may pass unmasked values.
    const char* GetWorldDirectionName(Direction dir);

    // The world direction a ride entrance or exit doorway faces - the way a guest walks to pass
    // through it - given the direction the entrance/exit is stored with. Entrance/exit data stores a
    // direction pointing INTO the ride (toward the platform), so the doorway faces the reverse. This
    // single rule backs both the tile-element and location-based callers.
    Direction GetEntranceFacing(Direction storedDirection);

    // Convenience overload for a ride entrance/exit tile element.
    Direction GetEntranceFacing(const EntranceElement& entrance);

    // The world direction a shop/stall faces - its front, where guests are served. Stalls have no
    // separate entrance tile, so the front is the piece's entrance-connection side rotated into
    // world space. The connection side IS the front (guests approach from there), so it is reported
    // directly with no reversal. Returns nullopt if the piece exposes no connection side.
    std::optional<Direction> GetShopFacing(const TrackElement& track);
} // namespace OpenRCT2::Ui::Accessibility
