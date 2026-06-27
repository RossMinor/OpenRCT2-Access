/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <openrct2/ride/RideTypes.h>
#include <string>

namespace OpenRCT2::Ui::Accessibility
{
    // A short, human-authored description of what a ride of the given type physically looks like
    // and how it moves, for players who cannot see it. Returns an empty string for ride types that
    // do not yet have a description written. Keyed on the base ride type, not the specific vehicle.
    std::string GetRideVisualDescription(ride_type_t rideType);
} // namespace OpenRCT2::Ui::Accessibility
