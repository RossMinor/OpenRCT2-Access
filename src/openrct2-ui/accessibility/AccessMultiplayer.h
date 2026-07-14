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
    // Polled once per frame. Speaks new multiplayer chat and system messages (player chat, join/leave
    // greetings, server broadcasts) and connection-status changes (connected, disconnected,
    // desynchronised) through the screen reader. No-op in single player.
    void TickMultiplayerAnnounce();
} // namespace OpenRCT2::Ui::Accessibility
