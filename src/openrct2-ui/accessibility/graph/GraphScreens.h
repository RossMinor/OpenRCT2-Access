/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "GraphBuilder.h"

#include <functional>
#include <openrct2/interface/WindowClasses.h>
#include <string>

namespace OpenRCT2
{
    struct WindowBase;
}

namespace OpenRCT2::Ui::Accessibility::Graph
{
    // A SCREEN RECIPE: how one window class presents itself to the graph navigator. Screens are
    // registered once (see EnsureScreensRegistered) and polled every tick - activation is
    // re-derived from live window state each poll (poll-and-diff, never event subscription), which
    // is what makes the stack robust to the game recreating its windows at will.
    //
    // Migration note (spec 10.5): a window class with a registered screen is OWNED by the graph -
    // the legacy accessibility layer stands down for it wholesale via GraphOwnsWindowClass, never
    // per-feature.
    struct GraphScreen
    {
        WindowClass windowClass{};

        // Spoken (queued) when the screen gains navigator focus; empty = silent.
        std::function<std::string()> screenName;

        // Declare the screen's nodes fresh from live window state. Runs on EVERY operation and
        // once per idle tick - keep it cheap. Throws are caught and logged by the navigator.
        std::function<void(GraphBuilder&, WindowBase&)> build;

        // The mod's established Tab/Shift+Tab page-switch verb (dir = +1/-1). Drives the
        // window's OWN page change (never reimplements it); return false when the screen has no
        // pages so the key falls through. The landing on the new page is announced by the
        // navigator's differ - the hook must not speak.
        std::function<bool(WindowBase&, int32_t dir)> onTabKey;

        // The screen's Back action for Escape. Return true when handled (e.g. an open dropdown
        // sub-list was closed, keeping the window); false = the navigator closes the window.
        std::function<bool(WindowBase&)> onEscape;

        // Arrow wrap-around at list edges (legacy parity: the mod's lists wrap).
        bool wrapArrows = true;
    };

    // Register a screen recipe. Call once per window class (typically from that window's source
    // file via its Register*GraphScreen function).
    void RegisterGraphScreen(GraphScreen screen);

    // One-time registration of all migrated screens (lazy; safe to call every tick).
    void EnsureGraphScreensRegistered();

    // THE ownership gate (spec 10.5): does the graph own this window class? Consulted by every
    // legacy accessibility path so exactly one model acts on each window.
    bool GraphOwnsWindowClass(WindowClass wc);

    // The registered screen for a class, or null.
    const GraphScreen* GraphScreenForClass(WindowClass wc);

    // The persistent cursor for a window class (created on demand). A covered window keeps its
    // state and restores exactly where the user was; a closed window's state is dropped by
    // DropGraphStatesForClosedWindows.
    GraphState& GraphStateForClass(WindowClass wc);

    // Housekeeping: drop cursor state for graph windows that no longer exist (reopening starts
    // fresh).
    void DropGraphStatesForClosedWindows();

    // The front-most keyboard-navigable window (graph-owned or legacy-navigable), respecting the
    // real z-order - or null (also null while a placement tool owns the keyboard). The graph owns
    // the frame iff this window's class is graph-owned.
    WindowBase* FrontNavigableWindow();
} // namespace OpenRCT2::Ui::Accessibility::Graph
