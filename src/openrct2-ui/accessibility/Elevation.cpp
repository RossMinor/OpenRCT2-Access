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
#include <openrct2/world/tile_element/SurfaceElement.h>
#include <openrct2/world/tile_element/TileElement.h>

namespace OpenRCT2::Ui::Accessibility
{
    int32_t AccessibleTopZ(const CoordsXY& tile)
    {
        auto* surface = MapGetSurfaceElementAt(tile);
        if (surface == nullptr)
            return -1;

        // Land only, and the BASE of the tile - not MapGetHighestZ. That engine helper rounds sloped
        // land up to the slope's top and treats water as a floor; both are construction rules (where
        // a track piece may sit), not descriptions of the tile's level. The tile's level here is its
        // base height: the number the coordinate readout speaks and building acts at, and the number
        // the game's own height-marker labels show for the tile (they interpolate the tile centre,
        // which floors to the base for standard slopes; on water tiles they read the submerged land).
        // A slope's top belongs to the higher neighbouring tile; water is announced as a tile
        // feature and by the water-level commands. Ride placement keeps the engine's MapGetHighestZ.
        int32_t top = surface->getBaseZ();
        for (TileElement* el = MapGetFirstElementAt(tile); el != nullptr;)
        {
            if (!el->isGhost())
            {
                if (el->getType() == TileElementType::path)
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

    int32_t ElevationHalfSteps(int32_t worldZ)
    {
        return worldZ / kCoordsZStep;
    }

    std::string ElevationText(int32_t halfSteps)
    {
        // kMapBaseZ is the offset the game applies to every height marker it draws (land, path and
        // track alike), so subtracting it here is what makes the spoken number and the on-screen
        // label the same number. Two half steps to a step.
        const int32_t relative = halfSteps - kMapBaseZ * 2;
        const int32_t magnitude = std::abs(relative);

        // A half step is spoken as ".5" rather than "and a half": these numbers are often recited
        // several at a time, and a decimal keeps each one to a single short token.
        std::string text;
        if (relative < 0)
            text += "minus ";
        text += std::to_string(magnitude / 2);
        if ((magnitude % 2) != 0)
            text += ".5";
        return text;
    }
} // namespace OpenRCT2::Ui::Accessibility
