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
#include <openrct2/world/ScenerySelection.h>
#include <string>

namespace OpenRCT2::Ui::Accessibility
{
    // Starts keyboard placement of the given scenery object (chosen in the scenery window). The
    // map cursor positions it; R rotates (small/large/banner), Shift+arrow picks the edge
    // (walls/banners), Enter places, Escape finishes. Placement stays active so several copies can
    // be placed in a row.
    void BeginAccessibleSceneryPlacement(const ScenerySelection& selection, const std::string& name);

    bool IsAccessibleSceneryPlacementActive();
    void AccessibleSceneryPlacementRotate();
    // screenEdge: 0 = top, 1 = right, 2 = bottom, 3 = left (as the player sees the tile). Converted
    // to a world edge using the current view rotation. For walls and banners.
    void AccessibleSceneryPlacementSetEdge(int32_t screenEdge, const char* label);
    // screenCorner: 0 = top-left, 1 = top-right, 2 = bottom-right, 3 = bottom-left. Converted to a
    // world quadrant using the current view rotation. For small scenery.
    void AccessibleSceneryPlacementSetCorner(int32_t screenCorner, const char* label);
    void AccessibleSceneryPlacementAtTile(const CoordsXY& mapCoords);
    void AccessibleSceneryPlacementCancel();
} // namespace OpenRCT2::Ui::Accessibility
