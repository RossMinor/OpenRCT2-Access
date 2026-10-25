/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "MenuNavigation.h"

#include "MapNavigation.h"
#include "RidePlacement.h"
#include "SceneryPlacement.h"
#include "ScreenReader.h"
#include "graph/GraphNavigator.h"
#include "graph/GraphScreens.h"
#include "graph/GraphVocab.h"

#include <openrct2-ui/UiContext.h>
#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Input.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/config/Config.h>
#include <openrct2/drawing/Colour.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Rectangle.h>
#include <openrct2/drawing/RenderTarget.h>
#include <openrct2/interface/ColourWithFlags.h>
#include <openrct2/interface/Window.h>
#include <openrct2/interface/WindowBase.h>
#include <openrct2/interface/WindowClasses.h>
#include <openrct2/ui/WindowManager.h>
#include <optional>
#include <string>
#include <vector>

namespace OpenRCT2::Ui::Accessibility
{
    // The keyboard's position inside an open dropdown. See the header for why this cannot live in
    // Windows::gDropdown.highlightedIndex.
    static int32_t _keyboardDropdownIndex = -1;

    void SetKeyboardDropdownIndex(int32_t index)
    {
        _keyboardDropdownIndex = index;
        // Mirror it immediately so the highlight is right on this frame too, not just after the
        // next tick.
        if (index >= 0 && index < Windows::gDropdown.numItems)
        {
            Windows::gDropdown.highlightedIndex = index;
            if (auto* windowMgr = GetWindowManager(); windowMgr != nullptr)
                windowMgr->InvalidateByClass(WindowClass::dropdown);
        }
    }

    int32_t GetKeyboardDropdownIndex()
    {
        return _keyboardDropdownIndex;
    }

    void TickKeyboardDropdownHighlight()
    {
        if (_keyboardDropdownIndex < 0)
            return;

        auto* windowMgr = GetWindowManager();
        if (windowMgr == nullptr || windowMgr->FindByClass(WindowClass::dropdown) == nullptr)
        {
            _keyboardDropdownIndex = -1; // the dropdown went away; forget the position
            return;
        }

        if (_keyboardDropdownIndex >= Windows::gDropdown.numItems)
        {
            _keyboardDropdownIndex = -1;
            return;
        }

        // The engine clears this from the mouse every input tick, so put it back.
        if (Windows::gDropdown.highlightedIndex != _keyboardDropdownIndex)
        {
            Windows::gDropdown.highlightedIndex = _keyboardDropdownIndex;
            windowMgr->InvalidateByClass(WindowClass::dropdown);
        }
    }

    void ClaimWidgetPressForKeyboard(WindowBase& w, WidgetIndex widgetIndex)
    {
        gPressedWidget.windowClassification = w.classification;
        gPressedWidget.windowNumber = w.number;
        gPressedWidget.widgetIndex = widgetIndex;
    }

    void ReleaseWidgetPressForKeyboard()
    {
        _keyboardDropdownIndex = -1;

        // Hand the engine back the state a mouse-driven close leaves it in. Staying in
        // dropdownActive would send the player's next real click through the dropdown commit path
        // with nothing open, and a lingering pressed widget draws a stuck-looking chevron.
        if (InputGetState() == InputState::dropdownActive)
            InputSetState(InputState::normal);
        gInputFlags.unset(InputFlag::widgetPressed);
        gPressedWidget.windowClassification = WindowClass::null;
    }

    bool OpenWidgetDropdownFromKeyboard(WindowBase& w, WidgetIndex chevronWidx)
    {
        // Claim the press BEFORE opening: onMouseDown shows the dropdown, and the engine can act on
        // gPressedWidget from the very next tick. See the header for why this is load-bearing.
        ClaimWidgetPressForKeyboard(w, chevronWidx);

        w.onMouseDown(chevronWidx); // populates and shows gDropdown

        auto* windowMgr = GetWindowManager();
        if (windowMgr == nullptr || windowMgr->FindByClass(WindowClass::dropdown) == nullptr)
        {
            // Nothing opened - don't leave a press behind claiming otherwise.
            ReleaseWidgetPressForKeyboard();
            return false;
        }
        return true;
    }

    void CloseWidgetDropdownFromKeyboard()
    {
        if (auto* windowMgr = GetWindowManager(); windowMgr != nullptr)
            windowMgr->CloseByClass(WindowClass::dropdown);
        ReleaseWidgetPressForKeyboard();
    }

    // The focus box is drawn on top of everything each frame. Because the engine only repaints
    // invalidated screen regions, a box drawn over a window that does not repaint on navigation (the
    // top toolbar menu, the Options window, etc.) would linger: each move would leave the previous box
    // behind and they would pile up. So we remember the last box and, whenever it moves or disappears,
    // mark that region dirty so the window underneath repaints and erases the old box.
    static std::optional<ScreenRect> _lastFocusRect;

    static void InvalidateFocusRect(const ScreenRect& rect)
    {
        // Inflate a little so the border (drawn outset, slightly outside the rect) is fully cleared.
        GfxSetDirtyBlocks({ { rect.GetLeft() - 2, rect.GetTop() - 2 }, { rect.GetRight() + 2, rect.GetBottom() + 2 } });
    }

    void DrawAccessibilityFocus(Drawing::RenderTarget& rt)
    {
        // Work out where the focus box should be this frame (nullopt = no box).
        std::optional<ScreenRect> rect;
        if (auto* windowMgr = GetWindowManager(); windowMgr != nullptr)
        {
            // An open combo box draws its own highlighted item; don't double up. Otherwise the box
            // goes round the focused control of the graph screen holding the keyboard.
            if (windowMgr->FindByClass(WindowClass::dropdown) == nullptr)
            {
                if (auto graphRect = GraphFocusScreenRect(); graphRect.has_value())
                {
                    rect = ScreenRect{ { graphRect->x, graphRect->y },
                                       { graphRect->x + graphRect->width, graphRect->y + graphRect->height } };
                }
            }
        }

        // If the box moved or vanished, repaint the area it used to cover so it doesn't linger.
        // (ScreenRect has no operator==, so compare corners.)
        const auto sameRect = [](const ScreenRect& a, const ScreenRect& b) {
            return a.GetLeft() == b.GetLeft() && a.GetTop() == b.GetTop() && a.GetRight() == b.GetRight()
                && a.GetBottom() == b.GetBottom();
        };
        const bool changed = _lastFocusRect.has_value()
            && (!rect.has_value() || !sameRect(*_lastFocusRect, *rect));
        if (changed)
            InvalidateFocusRect(*_lastFocusRect);
        _lastFocusRect = rect;

        if (!rect.has_value())
            return;

        // A border-only box (FillMode::none) in the user's chosen high-contrast colour, over everything.
        const auto colour = static_cast<Drawing::Colour>(Config::Get().sound.accessibilityFocusColour);
        Drawing::Rectangle::fillInset(
            rt, *rect, ColourWithFlags{ colour }, Drawing::Rectangle::BorderStyle::outset,
            Drawing::Rectangle::FillBrightness::light, Drawing::Rectangle::FillMode::none);
    }

    // Announces "Menu closed" when the front-most accessible window closes and focus returns to the
    // game, so closing ANY top-level window (e.g. the guest list opened with Shift+G, not just the
    // toolbar menu) gives the same audible cue. It watches the focus-owning window across frames
    // rather than hooking each window's close, so every navigable window - now and in future - gets
    // this automatically. Whatever accessible window is front-most counts as the current top level,
    // however it was opened.
    void TickMenuClosedAnnounce()
    {
        static bool hadWindow = false;

        // Only while actually playing (so leaving the title menu to start a scenario is not mistaken
        // for closing a menu), and never mid-placement: choosing a ride/design closes its list to
        // start the placement tool, which is not "returning to the game".
        const bool placing = Windows::WindowTrackPlaceIsActive() || IsAccessibleRidePlacementActive()
            || IsAccessibleSceneryPlacementActive();
        if (gLegacyScene != LegacyScene::playing || placing)
        {
            hadWindow = false;
            return;
        }

        // Any window holding the keyboard counts: a graph screen being open must never read as
        // "no window" (that would speak a false "Menu closed" while it is up).
        const bool hasWindow = Graph::FrontNavigableWindow() != nullptr;
        // When the last accessible window closes we are back in the game - unless we dropped into the
        // toolbar menu, which announces its own "Menu closed" via ExitMenuMode.
        if (hadWindow && !hasWindow && !IsInMenuMode())
            ScreenReaderSpeak("Menu closed");
        hadWindow = hasWindow;
    }
} // namespace OpenRCT2::Ui::Accessibility
