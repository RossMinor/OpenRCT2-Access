/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "AccessMultiplayer.h"

#include "ScreenReader.h"

#include <algorithm>
#include <cstdint>
#include <openrct2/interface/Chat.h>
#include <openrct2/network/Network.h>
#include <string>

namespace OpenRCT2::Ui::Accessibility
{
#ifndef DISABLE_NETWORK
    // Chat watcher: ChatGetTotalMessages() only ever grows, so comparing it across frames tells us
    // how many messages arrived (chat, "X has joined", server broadcasts - all funnel through
    // ChatAddHistory). The newest entry is at index 0.
    static uint32_t _lastChatCount = 0;
    static bool _watching = false;

    // Connection-state watchers.
    static Network::Mode _lastMode = Network::Mode::none;
    static Network::Status _lastStatus = Network::Status::none;
    static bool _lastDesync = false;

    // Chat compose watcher: echoes the message being typed (chat uses ContextStartTextInput but,
    // unlike the modal text-input window, has no screen-reader echo of its own).
    static bool _composeOpen = false;
    static std::string _lastComposeLine;

    // ChatAddHistory prepends a "[HH:MM] " clock to each entry; drop it so the reader doesn't
    // announce the time on every message. Colour/format codes are stripped by ScreenReaderSpeak.
    static std::string StripChatTimestamp(std::string_view e)
    {
        if (e.size() >= 8 && e[0] == '[' && e[3] == ':' && e[6] == ']' && e[7] == ' ')
            return std::string(e.substr(8));
        return std::string(e);
    }
#endif

    void TickMultiplayerAnnounce()
    {
#ifndef DISABLE_NETWORK
        // --- New chat / system messages ---
        const uint32_t total = ChatGetTotalMessages();
        if (!_watching)
        {
            // Skip whatever backlog already existed when we started watching.
            _lastChatCount = total;
            _watching = true;
        }
        else if (total > _lastChatCount)
        {
            const uint32_t added = total - _lastChatCount;
            const size_t have = ChatGetHistoryCount();
            const size_t n = std::min<size_t>(added, have);
            // Entries are push_front'd (index 0 = newest); announce oldest-first, queued so several
            // arriving together don't cut each other off.
            for (size_t i = n; i-- > 0;)
                ScreenReaderSpeak(StripChatTimestamp(ChatGetHistoryEntry(i)), false);
            _lastChatCount = total;
        }

        // --- Composing a chat message (echo typed characters) ---
        if (gChatOpen)
        {
            const std::string line = ChatGetCurrentLine();
            if (!_composeOpen)
            {
                ScreenReaderSpeak("Chat. Type your message, then press Enter to send or Escape to cancel.");
                _composeOpen = true;
            }
            else if (line != _lastComposeLine)
            {
                // Speak the newly typed text on growth, a short cue on deletion, else the whole line
                // (a paste or replacement). Interrupts, so fast typing stays responsive - matching the
                // modal text-input window's echo.
                if (line.size() > _lastComposeLine.size()
                    && line.compare(0, _lastComposeLine.size(), _lastComposeLine) == 0)
                    ScreenReaderSpeak(line.substr(_lastComposeLine.size()));
                else if (line.size() < _lastComposeLine.size())
                    ScreenReaderSpeak(line.empty() ? "blank" : "delete");
                else
                    ScreenReaderSpeak(line.empty() ? "blank" : line);
            }
            _lastComposeLine = line;
        }
        else if (_composeOpen)
        {
            _composeOpen = false;
            _lastComposeLine.clear();
        }

        // --- Connection lifecycle ---
        const auto mode = Network::GetMode();
        const auto status = Network::GetStatus();

        const bool wasConnectedClient = _lastMode == Network::Mode::client
            && _lastStatus == Network::Status::connected;
        if (mode == Network::Mode::client && status == Network::Status::connected && !wasConnectedClient)
            ScreenReaderSpeak("Connected to the server.");
        else if (mode == Network::Mode::server && _lastMode != Network::Mode::server)
            ScreenReaderSpeak("Server started.");
        else if (mode == Network::Mode::none && _lastMode != Network::Mode::none)
            ScreenReaderSpeak(
                _lastMode == Network::Mode::server ? "Server stopped." : "Disconnected from the server.");

        // --- Desync ---
        const bool desync = Network::IsDesynchronised();
        if (desync && !_lastDesync)
            ScreenReaderSpeak("Warning: the game has desynchronised from the server.");
        _lastDesync = desync;

        _lastMode = mode;
        _lastStatus = status;
#endif
    }
} // namespace OpenRCT2::Ui::Accessibility
