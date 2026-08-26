/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "GraphTypes.h"

#include <openrct2-ui/input/InputManager.h>
#include <optional>

namespace OpenRCT2::Ui::Accessibility
{
    // The graph navigator: wires keyboard input to kernel operations and owns ALL speech for
    // graph-owned screens (spec §7). Announce-once is enforced by a frame differ - one code path
    // compares the identity last spoken against the identity now focused; hand-written announce
    // calls around focus mutations are forbidden. Speech provenance: keypress-driven landings
    // interrupt; differ/attach landings queue (A7).

    // Key chokepoint. Called from InputManager BEFORE the legacy accessibility dispatcher; when
    // the front-most navigable window is graph-owned, the graph handles (or deliberately
    // swallows) the key and the legacy layer never sees it - one keypress, one model.
    bool HandleGraphNavigationKey(const InputEvent& e);

    // Per-frame tick: the screen manager (poll-and-diff attach), the frame differ, and the live
    // watch. Call from InputManager::process().
    void TickGraphScreens();

    // The focused graph node's screen rectangle (for the sighted-user focus box), when the
    // attached screen provides one. Updated each tick/operation.
    std::optional<Graph::GraphRect> GraphFocusScreenRect();
} // namespace OpenRCT2::Ui::Accessibility
