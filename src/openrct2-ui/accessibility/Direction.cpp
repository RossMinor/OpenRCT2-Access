/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Direction.h"

#include <openrct2/core/Numerics.hpp>
#include <openrct2/ride/TrackData.h>
#include <openrct2/ride/ted/TrackElementDescriptor.h>
#include <openrct2/world/tile_element/EntranceElement.h>
#include <openrct2/world/tile_element/TrackElement.h>

namespace OpenRCT2::Ui::Accessibility
{
    const char* GetWorldDirectionName(Direction dir)
    {
        static constexpr const char* kNames[] = { "East", "South", "West", "North" };
        return kNames[dir & 3];
    }

    Direction GetEntranceFacing(Direction storedDirection)
    {
        return DirectionReverse(storedDirection);
    }

    Direction GetEntranceFacing(const EntranceElement& entrance)
    {
        return GetEntranceFacing(entrance.getDirection());
    }

    std::optional<Direction> GetShopFacing(const TrackElement& track)
    {
        const auto& ted = OpenRCT2::TrackMetadata::GetTrackElementDescriptor(track.GetTrackType());
        uint8_t connectionSides = ted.sequenceData.sequences[0].getEntranceConnectionSides();
        connectionSides = OpenRCT2::Numerics::rol4(connectionSides, track.getDirection());
        for (uint8_t count = 0; count < kNumOrthogonalDirections; count++)
        {
            if (connectionSides & (1 << count))
                return count;
        }
        return std::nullopt;
    }
} // namespace OpenRCT2::Ui::Accessibility
