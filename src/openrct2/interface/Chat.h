/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "../core/StringTypes.h"

#include <string_view>

namespace OpenRCT2
{
    struct ColourWithFlags;
}

constexpr int8_t kChatHistorySize = 10;
constexpr int16_t kChatInputSize = 1024;
constexpr uint8_t kChatMaxMessageLength = 200;
constexpr int16_t kChatMaxWindowWidth = 600;

struct ScreenCoordsXY;

enum class ChatInput : uint8_t
{
    none,
    send,
    close,
};

extern bool gChatOpen;

namespace OpenRCT2::Drawing
{
    struct RenderTarget;
}

bool ChatAvailable();
void ChatOpen();
void ChatClose();
void ChatToggle();

void ChatInit();
void ChatUpdate();
void ChatDraw(OpenRCT2::Drawing::RenderTarget& rt, OpenRCT2::ColourWithFlags chatBackgroundColour);

void ChatAddHistory(std::string_view s);
void ChatInput(ChatInput input);

// Accessibility: read-only access to the chat history so a screen-reader poll can announce new
// chat/system messages. ChatGetTotalMessages() is a monotonic count of every message ever added,
// so a watcher can tell how many are new even after the 10-entry ring buffer wraps. Index 0 is the
// newest entry.
uint32_t ChatGetTotalMessages();
size_t ChatGetHistoryCount();
const u8string& ChatGetHistoryEntry(size_t index);

// The message currently being composed (valid while gChatOpen). A screen-reader poll watches this
// to echo typed characters.
const u8string& ChatGetCurrentLine();

int32_t ChatStringWrappedGetHeight(u8string_view args, int32_t width);
