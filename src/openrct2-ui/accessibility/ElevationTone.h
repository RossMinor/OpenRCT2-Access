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

namespace OpenRCT2::Ui::Accessibility
{
    // Plays the elevation beep at a pitch encoding the given elevation (a surface baseHeight/2).
    // Higher elevation gives a higher pitch, clamped to a cap so tall terrain never gets piercing.
    // The sine for each elevation step is synthesised at its exact target frequency on first use and
    // cached for the session, then played at rate 1.0 so the mixer never resamples it. No-op when no
    // audio device is available.
    void PlayElevationTone(int32_t elevation);
} // namespace OpenRCT2::Ui::Accessibility
