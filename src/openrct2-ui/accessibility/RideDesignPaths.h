/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <openrct2/Identifiers.h>
#include <openrct2/world/Location.hpp>

namespace OpenRCT2::Ui::Accessibility
{
    // Pre-built ride designs can carry their own footpaths and queues. The engine lays those down as
    // ordinary path elements with no link back to the ride - FootpathLayoutPlaceAction even clears
    // the ride index - so demolishing the ride leaves them stranded, and nothing in the save records
    // where they came from.
    //
    // So the mod records it. Placing a design lists the path tiles in the area just before and just
    // after the build; the difference is exactly what the design added. When the ride later goes
    // away, those tiles go with it. Only tiles the design itself created are stored, so a path the
    // player runs across the ride's footprint afterwards is never touched.

    // Lists the path tiles already present, before a design is placed. min/max are inclusive tile
    // coordinates and should be generous: anything already standing there is excluded by the
    // comparison, so an oversized area costs a slightly longer scan and nothing else.
    void BeginDesignPathCapture(const TileCoordsXY& min, const TileCoordsXY& max);

    // Records the path tiles that appeared since BeginDesignPathCapture as this ride's, and saves.
    // Call once the design has actually been built.
    void EndDesignPathCapture(RideId rideId);

    // Drops an in-flight capture, for a placement that failed or was abandoned.
    void AbortDesignPathCapture();

    // Polled once per frame. Keeps the records in step with the loaded park, and removes a ride's
    // recorded paths once that ride is gone from the map - which covers every route a ride can be
    // demolished by, rather than only the ones the mod knows to hook.
    void TickRideDesignPaths();
} // namespace OpenRCT2::Ui::Accessibility
