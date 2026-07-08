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
#include <functional>
#include <utility>

namespace OpenRCT2::Ui::Accessibility
{
    // Shared keyboard-list navigation. Every accessible list screen (rides, guests, staff, saves,
    // scenarios, ...) needs the same three things: step to the next/previous *visible* item with
    // wrap-around, and report "item X of Y" counting only visible items. Historically each window
    // re-implemented these, and the small differences between copies were heard by the player as the
    // game behaving inconsistently. These helpers are the single definition, so every list wraps,
    // skips hidden rows, and counts positions identically.
    //
    // `count` is the total number of items; `visible(i)` returns whether item i is currently shown
    // (filtered lists hide rows). Items are addressed by their index in [0, count).
    namespace ListNav
    {
        using VisibleFn = std::function<bool(int32_t)>;

        // Steps from `from` by `delta` (normally +1 or -1) to the next visible item, wrapping around
        // the ends. Returns the new index, or -1 if no item is visible (or the list is empty). Pass
        // from = -1 to start before the first item (a first Down lands on the first visible item).
        inline int32_t stepVisible(int32_t from, int32_t delta, int32_t count, const VisibleFn& visible)
        {
            if (count <= 0)
                return -1;
            int32_t idx = from;
            for (int32_t steps = 0; steps < count; steps++)
            {
                idx += delta;
                if (idx < 0)
                    idx = count - 1;
                else if (idx >= count)
                    idx = 0;
                if (visible(idx))
                    return idx;
            }
            return -1;
        }

        // The zero-based position of `index` among the visible items, and the visible total, for a
        // spoken "position of total" read-out. If `index` is not visible, the position is 0.
        inline std::pair<int32_t, int32_t> visiblePosition(int32_t index, int32_t count, const VisibleFn& visible)
        {
            int32_t total = 0;
            int32_t pos = 0;
            for (int32_t i = 0; i < count; i++)
            {
                if (!visible(i))
                    continue;
                if (i == index)
                    pos = total;
                total++;
            }
            return { pos, total };
        }
    } // namespace ListNav
} // namespace OpenRCT2::Ui::Accessibility
