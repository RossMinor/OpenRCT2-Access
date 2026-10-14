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
#include <openrct2/world/Location.hpp>
#include <string>

namespace OpenRCT2::Ui::Accessibility
{
    // Q: the route of the queue under the cursor, spoken as directions a player can follow with the
    // arrow keys from where they stand - "Mini Coaster 1 queue, 23 tiles. Starts at X 40, Y 52,
    // connected to path. 16 tiles to the entrance: right 3, up 1, left 2 on a ramp up to elevation 3,
    // left 4, up 1, right 5." From a queue tile it leads to the entrance; from a ride entrance, out to
    // the queue's start. Where a tile holds several (a queue bridging another), the one nearest
    // focusHeight (half steps) is used. Returns the full speech.
    //
    // Shift+Q passes directionsOnly: just the way to the entrance ("16 tiles to the entrance: right
    // 3, ..."), with no ride name, length or start.
    std::string DescribeQueueRoute(const TileCoordsXY& tile, int32_t focusHeight, bool directionsOnly);
} // namespace OpenRCT2::Ui::Accessibility
