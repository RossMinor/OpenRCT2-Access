/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <openrct2-ui/input/InputManager.h>

namespace OpenRCT2
{
    struct WindowBase;
}

namespace OpenRCT2::Drawing
{
    struct RenderTarget;
}

namespace OpenRCT2::Ui::Accessibility
{
    // Handles keyboard navigation for accessible menus (currently the title/main menu).
    // Moves a focus cursor with the up/down arrow keys, speaks the focused item, and
    // activates it with Enter. Returns true if the event was consumed and should not
    // be processed further (e.g. by the shortcut manager).
    bool HandleMenuNavigationKey(const InputEvent& e);

    // Focuses and announces the first item of the given menu window (generic widget
    // navigation), so a menu can default to its first item when it opens.
    void FocusFirstItem(WindowBase& w);

    // The window that currently owns accessible keyboard navigation (the front-most window we know
    // how to navigate), or nullptr if none. Exposed so other subsystems - e.g. the F1 context help -
    // can tell when a navigable menu is focused.
    WindowBase* GetActiveAccessibleWindow();

    // Draws a visible focus box around the element the keyboard accessibility focus is on (the
    // focused toolbar item in menu mode, or the active accessible window's focused widget). Called
    // as a top-level overlay after all windows are painted. The map cursor's tile highlight is
    // handled separately by the engine's tile selection.
    void DrawAccessibilityFocus(Drawing::RenderTarget& rt);
} // namespace OpenRCT2::Ui::Accessibility
