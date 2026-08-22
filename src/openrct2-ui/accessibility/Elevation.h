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

struct CoordsXY;

namespace OpenRCT2::Ui::Accessibility
{
    // === The single source of truth for elevation in the accessibility mod ===
    //
    // Every mod feature that deals with height must go through this file so the mod measures and
    // reports elevation the exact way the engine does, rather than its own land-only approximation.
    // Heights here are world Z (units of kCoordsZStep = 8), the engine's native scale; only the final
    // spoken figure is converted, in ElevationNumber, to the number the ride construction window
    // shows so the player hears the same value the game understands.

    // World Z of the highest level a player stands on at this tile: the ground (above), or a footpath
    // or ride entrance/exit/park entrance resting above it (bridges, paths in the air, raised station
    // accesses). Use this for the elevation indicator so raised paths and entrances register instead
    // of reading the ground beneath them. It deliberately does not climb onto overhead track or
    // scenery - those are announced as their own tile features, not as "the level you are at".
    // Returns -1 if the tile has no surface.
    int32_t AccessibleTopZ(const CoordsXY& tile);

    // Converts a world Z to the elevation number the engine uses for construction height
    // (Z / (kCoordsZStep * 2)), so every spoken elevation matches the figure shown in the ride
    // construction window.
    int32_t ElevationNumber(int32_t worldZ);
} // namespace OpenRCT2::Ui::Accessibility
