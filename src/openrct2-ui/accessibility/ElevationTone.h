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
#include <vector>

namespace OpenRCT2::Ui::Accessibility
{
    // Plays the elevation beep at a pitch encoding the given elevation in HALF STEPS (see
    // ElevationHalfSteps in Elevation.h - the engine's own base-height unit, two to a land step).
    // Higher elevation gives a higher pitch, clamped to a cap so tall terrain never gets piercing.
    // The sine for each half step is synthesised at its exact target frequency on first use and
    // cached for the session, then played at rate 1.0 so the mixer never resamples it. No-op when no
    // audio device is available.
    void PlayElevationTone(int32_t halfSteps);

    // Plays one tone per height, in the order given, spaced far enough apart to be heard as separate
    // notes rather than a chord - so a tile holding a path at one level and track overhead at another
    // sounds like two distinct pitches. The first tone sounds at once; the rest are released by
    // TickElevationTones. Starting a new sequence drops any still pending, so sweeping the cursor
    // quickly never builds a backlog.
    void PlayElevationTones(const std::vector<int32_t>& halfSteps);

    // Releases the next queued tone once its gap has elapsed. Call once per frame.
    void TickElevationTones();
} // namespace OpenRCT2::Ui::Accessibility
