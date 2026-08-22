/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Elevation.h"

#include <algorithm>
#include <openrct2/world/Location.hpp>
#include <openrct2/world/Map.h>
#include <openrct2/world/MapLimits.h>
#include <openrct2/world/tile_element/EntranceElement.h>
#include <openrct2/world/tile_element/TileElement.h>

namespace OpenRCT2::Ui::Accessibility
{
    int32_t AccessibleTopZ(const CoordsXY& tile)
    {
        int32_t top = MapGetHighestZ(tile);
        for (TileElement* el = MapGetFirstElementAt(tile); el != nullptr;)
        {
            if (!el->isGhost())
            {
                if (el->getType() == TileElementType::Path)
                {
                    // A footpath's base is the surface a guest walks on - the relevant level whether it
                    // sits on the ground or bridges over water/a gap.
                    top = std::max(top, el->getBaseZ());
                }
                else if (auto* entrance = el->asEntrance(); entrance != nullptr)
                {
                    const auto type = entrance->GetEntranceType();
                    if (type == ENTRANCE_TYPE_RIDE_ENTRANCE || type == ENTRANCE_TYPE_RIDE_EXIT
                        || type == ENTRANCE_TYPE_PARK_ENTRANCE)
                        top = std::max(top, el->getBaseZ());
                }
            }
            if (el->isLastForTile())
                break;
            el++;
        }
        return top;
    }

    int32_t ElevationNumber(int32_t worldZ)
    {
        return worldZ / (kCoordsZStep * 2);
    }
} // namespace OpenRCT2::Ui::Accessibility
