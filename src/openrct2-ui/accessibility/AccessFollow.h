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
#include <string>

namespace OpenRCT2::Ui::Accessibility
{
    // Locks the main camera onto the given entity (guest/staff) so it follows them around. The
    // engine updates the viewport each frame and clears it if the entity despawns. Escape stops it.
    void StartFollowingEntity(EntityId id, const std::string& name);
    bool IsFollowingEntity();
    void StopFollowingEntity();

    // Polled each frame: detects when the followed entity has gone (engine cleared the follow) and
    // announces it, so the follow state never gets stuck.
    void TickFollowEntity();
} // namespace OpenRCT2::Ui::Accessibility
