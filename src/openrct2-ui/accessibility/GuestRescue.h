/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

namespace OpenRCT2::Ui::Accessibility
{
    // Ctrl+H rescue: teleports every guest that is stranded (holds a "lost / go home / can't find"
    // thought and has no walking route to a park entrance, or is not on a footpath at all) to the
    // nearest park entrance, then announces how many were moved. Routed through the game's own
    // pick-up-and-place action so it stays deterministic and replicates in multiplayer; the teleports
    // run one guest at a time across action callbacks. Safe to call from map input.
    void RescueLostGuests();
} // namespace OpenRCT2::Ui::Accessibility
