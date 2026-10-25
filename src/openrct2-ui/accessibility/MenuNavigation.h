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
#include <openrct2/interface/WindowClasses.h>
#include <openrct2/interface/WindowTypes.h>

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
    // What is left of the mod's first keyboard layer, whose key dispatcher every window has since
    // moved off (windows are navigated by the graph navigator, accessibility/graph/): the shared
    // keyboard dropdown helpers, the "Menu closed" cue and the focus box.

    // Polled once per frame. Announces "Menu closed" when the front-most accessible window closes and
    // focus returns to the game, so every top-level window gives the same cue the toolbar menu does.
    void TickMenuClosedAnnounce();

    // Opens a window's combo-box dropdown from the keyboard, and keeps it open.
    //
    // Calling the window's onMouseDown directly is NOT enough. Showing a dropdown puts the engine
    // into InputState::dropdownActive, which is a MOUSE state machine: on every input tick
    // InputStateWidgetPressed looks up gPressedWidget - the widget a mouse press left behind - and
    // if that window cannot be found it concludes the interaction is over, closes the dropdown and
    // resets the input state. A keyboard player has never pressed a widget, so gPressedWidget is
    // WindowClass::null, and the list is torn down on the very next tick - before a single arrow
    // key can reach it. From the player's side the menu opens, announces, and is instantly gone.
    //
    // So claim the press for the widget being opened, exactly as a real click would. The state
    // machine then finds a live owner and leaves the list alone, and a mouse click on an item still
    // commits through the engine's own path.
    //
    // Returns false if no dropdown opened (leaving nothing behind); pair every true with
    // CloseWidgetDropdownFromKeyboard.
    bool OpenWidgetDropdownFromKeyboard(WindowBase& w, WidgetIndex chevronWidx);

    // The two halves of the above, for callers that cannot use it directly - the toolbar and title
    // menu activate a button with onMouseDown AND onMouseUp, without knowing in advance whether a
    // dropdown will appear. Claim before activating; if a dropdown opened, leave the claim in place
    // until it closes, otherwise Release immediately.
    void ClaimWidgetPressForKeyboard(WindowBase& w, WidgetIndex widgetIndex);
    void ReleaseWidgetPressForKeyboard();

    // The keyboard's highlighted item within an open dropdown, owned by the mod.
    //
    // gDropdown.highlightedIndex CANNOT hold this. While a dropdown is open the engine rewrites
    // that field from the MOUSE on every input tick - clearing it to -1 first and only restoring it
    // if the pointer is actually over an item (InputStateWidgetPressed, MouseInput.cpp). A keyboard
    // highlight written there is therefore gone before the next keypress arrives: every move reads
    // back -1, so Down from -1 lands on the first item forever, Up lands on the last, and Enter
    // commits nothing.
    //
    // So the mod keeps its own index and treats gDropdown.highlightedIndex as an output only -
    // mirrored each frame by TickKeyboardDropdownHighlight so the drawn highlight follows the
    // keyboard. -1 means "no keyboard selection".
    void SetKeyboardDropdownIndex(int32_t index);
    int32_t GetKeyboardDropdownIndex();

    // Polled once per frame: re-asserts the keyboard's highlight into gDropdown, undoing the
    // engine's per-tick mouse-hover reset while a keyboard-driven dropdown is open.
    void TickKeyboardDropdownHighlight();

    // Closes a dropdown opened by OpenWidgetDropdownFromKeyboard and hands the engine back the
    // input state it would have after a mouse-driven dropdown closes. Leaving it in dropdownActive
    // with a pressed widget that owns nothing would misroute the player's next real click.
    void CloseWidgetDropdownFromKeyboard();

    // Draws a visible focus box around the control the graph navigator has focused. Called
    // as a top-level overlay after all windows are painted. The map cursor's tile highlight is
    // handled separately by the engine's tile selection.
    void DrawAccessibilityFocus(Drawing::RenderTarget& rt);
} // namespace OpenRCT2::Ui::Accessibility
