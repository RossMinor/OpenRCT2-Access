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
#include <openrct2/interface/Viewport.h>
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
        const auto& ted = OpenRCT2::TrackMetadata::GetTrackElementDescriptor(track.getTrackType());
        uint8_t connectionSides = ted.sequenceData.sequences[0].getEntranceConnectionSides();
        connectionSides = OpenRCT2::Numerics::rol4(connectionSides, track.getDirection());
        for (uint8_t count = 0; count < kNumOrthogonalDirections; count++)
        {
            if (connectionSides & (1 << count))
                return count;
        }
        return std::nullopt;
    }

    int32_t ScreenSideOf(Direction worldDir)
    {
        // The world direction at the top of the screen: North (3) at rotation 0, then East, South,
        // West as the camera turns - exactly what CameraFacingDirection and the F key report.
        const int32_t upDir = (3 + GetCurrentRotation()) & 3;
        // World directions run clockwise (East, South, West, North), so one step on is one screen side
        // clockwise: up, right, down, left.
        return (worldDir - upDir) & 3;
    }

    const char* GetScreenDirectionName(Direction worldDir)
    {
        static constexpr const char* kNames[] = { "up", "right", "down", "left" };
        return kNames[ScreenSideOf(worldDir)];
    }

    const char* GetScreenEdgeName(Direction worldDir)
    {
        static constexpr const char* kNames[] = { "Top edge", "Right edge", "Bottom edge", "Left edge" };
        return kNames[ScreenSideOf(worldDir)];
    }

    std::string GetScreenCornerName(Direction worldDirA, Direction worldDirB)
    {
        const int32_t a = ScreenSideOf(worldDirA);
        const int32_t b = ScreenSideOf(worldDirB);
        // One of the two lands on top/bottom, the other on left/right, whatever the rotation.
        const int32_t vertical = (a == 0 || a == 2) ? a : b;
        const int32_t horizontal = (a == 0 || a == 2) ? b : a;
        return std::string(vertical == 0 ? "Top " : "Bottom ") + (horizontal == 1 ? "right corner" : "left corner");
    }
} // namespace OpenRCT2::Ui::Accessibility
