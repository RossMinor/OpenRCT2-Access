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
#include <string>

struct CoordsXY;

namespace OpenRCT2::Ui::Accessibility
{
    // === The single source of truth for elevation in the accessibility mod ===
    //
    // Every mod feature that deals with height must go through this file so the mod measures and
    // reports elevation the exact way the engine does, rather than its own land-only approximation.
    // Heights here are world Z (units of kCoordsZStep = 8), the engine's native scale.
    //
    // THE MOD'S UNIT IS THE HALF STEP - one base height unit, which is the granularity the engine
    // actually stores every tile element at. A land step, a path step and a track step are all two
    // half steps. Measuring in whole steps (as the mod used to) silently rounds away anything
    // sitting between two steps, and the game's own path height markers round it away too - so a
    // footpath built half a step off the grid, which can never connect to its neighbours, looked
    // and sounded identical to a correct one. Working in half steps is what lets the mod say so.

    // World Z of the level a player stands on at this tile: the ground's base height, or a footpath
    // or ride entrance/exit/park entrance resting above it (bridges, paths in the air, raised
    // station accesses). Use this for the elevation indicator so raised paths and entrances register
    // instead of reading the ground beneath them. It deliberately does not climb onto overhead track
    // or scenery - those are announced as their own tile features, not as "the level you are at".
    // Slope tops and water surfaces are also deliberately excluded: the tile's level is its base
    // (the number the coordinate readout speaks and building acts at, and what the game's own
    // height-marker labels show - they read the submerged land on water tiles). Water presence is
    // announced as a tile feature and by the water-level commands; only construction treats water
    // and slope tops as a floor, and that stays in the engine's MapGetHighestZ, which ride placement
    // keeps using. Returns -1 if the tile has no surface.
    int32_t AccessibleTopZ(const CoordsXY& tile);

    // World Z to half steps (Z / kCoordsZStep) - the mod's internal elevation unit. This is what
    // the elevation tone and every "has the height changed?" comparison run on, so a half-step
    // difference registers instead of being rounded into its neighbour.
    int32_t ElevationHalfSteps(int32_t worldZ);

    // The spoken elevation for a height in half steps. The whole-step figure matches the game's own
    // height markers exactly - the same land/path step scale, offset by kMapBaseZ - so the number
    // the player hears is the number a sighted player reads off the screen. A height sitting between
    // two steps speaks the difference rather than hiding it, as a decimal:
    //
    //   28 -> "0"        29 -> "0.5"        36 -> "4"       37 -> "4.5"
    //   13 -> "minus 0.5"                   12 -> "minus 1"
    //
    // A ".5" is therefore also a warning: nothing the player can legitimately build sits between
    // two steps, so hearing one means the height is off the grid.
    std::string ElevationText(int32_t halfSteps);
} // namespace OpenRCT2::Ui::Accessibility
