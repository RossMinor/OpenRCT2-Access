/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <string>

struct TrackDesign;

namespace OpenRCT2::Ui::Accessibility
{
    // Builds a specific, appearance-focused description of a pre-built track design from its own
    // data: the shape of its layout (hills, turns and how sharp they are, inversions, helixes,
    // patterns and notable features) and its default colour scheme. Size is deliberately left out
    // because the list already announces the design's dimensions. Falls back to the generic
    // per-ride-type description when a design has no analysable track (e.g. a maze).
    std::string DescribeTrackDesign(const TrackDesign& design);
} // namespace OpenRCT2::Ui::Accessibility
