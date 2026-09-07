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

#include <SDL.h>
#include <openrct2-ui/UiContext.h>
#include <openrct2-ui/input/ShortcutManager.h>
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
    // The SDL key we consumed on key-down, so the matching key-up can be swallowed too
    // and not reach the shortcut manager.
    static uint32_t _lastHandledKey = 0;

    static std::optional<AccessibilityAction> MapKeyToAction(uint32_t key, uint32_t modifiers)
    {
        switch (key)
        {
            case SDLK_UP:
                return AccessibilityAction::moveUp;
            case SDLK_DOWN:
                return AccessibilityAction::moveDown;
            case SDLK_LEFT:
                return AccessibilityAction::moveLeft;
            case SDLK_RIGHT:
                return AccessibilityAction::moveRight;
            // Tab moves to the next sub-section/category, Shift+Tab the previous. Only windows
            // with tabs (e.g. Options) act on these; elsewhere they are ignored.
            case SDLK_TAB:
                return (modifiers & KMOD_SHIFT) ? AccessibilityAction::prevTab : AccessibilityAction::nextTab;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                // Shift+Enter is a secondary activate (e.g. follow an NPC instead of opening it).
                return (modifiers & KMOD_SHIFT) ? AccessibilityAction::activateAlt : AccessibilityAction::activate;
            case SDLK_ESCAPE:
                return AccessibilityAction::cancel;
            default:
                return std::nullopt;
        }
    }

    // The window classes the mod knows how to navigate by keyboard. This is pure membership: which
    // windows are navigable at all. Which one *wins* when several are open is decided by the real
    // z-order in GetActiveAccessibleWindow, not by the order of the cases here - so no manual
    // priority between window types has to be kept in sync (that was the source of keys landing in
    // the wrong window, e.g. the loan control instead of the park admission price). To make a new
    // window navigable, add its class here and implement onAccessibilityAction on the window.
    static bool IsNavigableAccessibleClass(WindowClass wc)
    {
        switch (wc)
        {
            case WindowClass::savePrompt:          // modal prompts
            case WindowClass::demolishRidePrompt:  // also refurbish-ride confirmation
            case WindowClass::firePrompt:
            case WindowClass::accessibilityOptions: // the mod's own settings (Ctrl+F1)
            case WindowClass::serverList:
            case WindowClass::networkStatus:  // connection-progress modal + password prompt
            case WindowClass::serverStart:    // host-a-server configuration form
            case WindowClass::multiplayer:    // in-game players / groups / options window
            case WindowClass::player:         // individual player info (group, kick, stats)
            case WindowClass::scenarioSelect:
            case WindowClass::trackDesignList:      // pre-built ride design list
            case WindowClass::titleMenu:
            case WindowClass::banner:               // banner/sign edit window
            case WindowClass::footpath:
            case WindowClass::constructRide:
            case WindowClass::ride:
            case WindowClass::peep:                 // guest or staff info window
            case WindowClass::scenery:
            case WindowClass::options:
            case WindowClass::cheats:
            case WindowClass::keyboardShortcutList:
            case WindowClass::rideList:
            case WindowClass::staffList:
            case WindowClass::guestList:
            case WindowClass::newCampaign:
            case WindowClass::finances:
            case WindowClass::research:
            case WindowClass::recentNews:
            case WindowClass::parkInformation: // declines on non-objective pages via onAccessibilityAction
                return true;
            default:
                return false;
        }
    }

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

    bool IsLegacyNavigableAccessibleClass(WindowClass wc)
    {
        return IsNavigableAccessibleClass(wc);
    }

    // The window that currently owns accessible keyboard navigation: the frontmost open window we
    // know how to navigate. We ask the window system for its real front-to-back order rather than
    // guessing from a hand-authored ranking, so keys always go to the window the player raised most
    // recently. A window that cannot navigate right now (e.g. the park window off its objective
    // page) simply returns false from onAccessibilityAction, and the key falls through to the map.
    WindowBase* GetActiveAccessibleWindow()
    {
        auto* windowMgr = GetWindowManager();
        if (windowMgr == nullptr)
            return nullptr;

        // While placing a pre-built ride, the keyboard map cursor positions it, so no window
        // should capture the arrow keys.
        if (windowMgr->FindByClass(WindowClass::trackDesignPlace) != nullptr)
            return nullptr;

        // WindowVisitEach walks the window list back-to-front, so the last navigable window it
        // visits is the one currently in front. Window classes owned by the graph navigator are
        // skipped wholesale (the migration ownership gate): the graph consumes their keys before
        // this dispatcher ever runs, and the legacy layer must never act on them.
        WindowBase* result = nullptr;
        WindowVisitEach([&result](WindowBase* w) {
            if (IsNavigableAccessibleClass(w->classification) && !Graph::GraphOwnsWindowClass(w->classification))
                result = w;
        });
        return result;
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
            // An open combo box draws its own highlighted item; don't double up.
            if (windowMgr->FindByClass(WindowClass::dropdown) == nullptr)
            {
                // A graph-owned screen provides its focused node's rect through the navigator;
                // in toolbar menu mode the toolbar owns focus (it isn't an "active accessible
                // window"); otherwise use whichever accessible window currently has focus.
                if (auto graphRect = GraphFocusScreenRect(); graphRect.has_value())
                {
                    rect = ScreenRect{ { graphRect->x, graphRect->y },
                                       { graphRect->x + graphRect->width, graphRect->y + graphRect->height } };
                }
                else
                {
                    WindowBase* w = IsInMenuMode() ? windowMgr->FindByClass(WindowClass::topToolbar)
                                                   : GetActiveAccessibleWindow();
                    if (w != nullptr)
                        rect = w->getAccessibilityFocusRect();
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

        // The union of legacy-navigable and graph-owned windows: a graph screen being open must
        // never read as "no window" (that would speak a false "Menu closed" while it is up).
        const bool hasWindow = Graph::FrontNavigableWindow() != nullptr;
        // When the last accessible window closes we are back in the game - unless we dropped into the
        // toolbar menu, which announces its own "Menu closed" via ExitMenuMode.
        if (hadWindow && !hasWindow && !IsInMenuMode())
            ScreenReaderSpeak("Menu closed");
        hadWindow = hasWindow;
    }

    bool HandleMenuNavigationKey(const InputEvent& e)
    {
        if (e.deviceKind != InputDeviceKind::keyboard)
            return false;

        // While a shortcut rebind is pending (the Change Shortcut window is open), the next key
        // press is captured by the shortcut manager as the new binding. Let every key fall through
        // to it, except Escape, which we use to cancel the rebind (Escape is otherwise a valid
        // binding, so the shortcut manager would bind it).
        if (auto& sm = GetShortcutManager(); sm.isPendingShortcutChange())
        {
            if (e.button == SDLK_ESCAPE)
            {
                if (e.state == InputEventState::down)
                {
                    if (auto* windowMgr = GetWindowManager(); windowMgr != nullptr)
                        windowMgr->CloseByClass(WindowClass::changeKeyboardShortcut);
                    ScreenReaderSpeak("Rebind cancelled");
                }
                _lastHandledKey = e.button;
                return true;
            }
            return false;
        }

        // The migration ownership gate (one keypress, one model): while the front-most navigable
        // window is graph-owned, the graph navigator has already had first refusal on this key -
        // anything it declined must fall through to the map/shortcuts, never be dispatched by
        // this legacy path to a covered window underneath.
        if (auto* graphFront = Graph::FrontNavigableWindow();
            graphFront != nullptr && Graph::GraphOwnsWindowClass(graphFront->classification))
            return false;

        const auto action = MapKeyToAction(e.button, e.modifiers);
        if (!action.has_value())
        {
            // First-letter navigation: route a printable letter to the focused window so it can
            // jump to the next item starting with that letter.
            if (e.button >= SDLK_a && e.button <= SDLK_z)
            {
                if (e.state != InputEventState::down)
                {
                    if (e.button == _lastHandledKey)
                    {
                        _lastHandledKey = 0;
                        return true;
                    }
                    return false;
                }
                auto* w = GetActiveAccessibleWindow();
                if (w == nullptr)
                    return false;
                const bool handled = w->onAccessibilityTypeahead(e.button);
                _lastHandledKey = handled ? e.button : 0;
                return handled;
            }
            return false;
        }

        // On key-up, swallow the key we consumed on key-down (even if the target window
        // has since closed, e.g. Escape closing a window).
        if (e.state != InputEventState::down)
        {
            if (e.button == _lastHandledKey)
            {
                _lastHandledKey = 0;
                return true;
            }
            return false;
        }

        auto* w = GetActiveAccessibleWindow();
        if (w == nullptr)
            return false;

        // A window not yet migrated to the graph navigates its own contents. There is no generic
        // fallback: a window must opt in via onAccessibilityAction, so we never hijack in-game keys.
        const bool handled = w->onAccessibilityAction(*action);

        // If the action moved focus to a different accessible window (a child opened, or a child
        // closed and we landed back on its parent), re-announce that window's current focus so the
        // player always hears where they are - at any nesting depth.
        if (handled)
        {
            auto* now = GetActiveAccessibleWindow();
            if (now != nullptr && now != w)
            {
                now->onAccessibilityAction(AccessibilityAction::announce);
            }
            else if (now == nullptr && IsInMenuMode())
            {
                // The window closed and no navigable window is front, but the toolbar menu (which
                // opened it) still owns focus underneath - e.g. Escaping the Options window opened
                // from the File dropdown lands back on the "Options" dropdown item. The toolbar isn't
                // a "navigable window", so re-announce via it directly. This works at any depth (a
                // reopened dropdown item or a toolbar button), matching the dropdown->toolbar case.
                if (auto* windowMgr = GetWindowManager(); windowMgr != nullptr)
                {
                    if (auto* toolbar = windowMgr->FindByClass(WindowClass::topToolbar); toolbar != nullptr)
                        toolbar->onAccessibilityAction(AccessibilityAction::announce);
                }
            }
        }

        _lastHandledKey = handled ? e.button : 0;
        return handled;
    }
} // namespace OpenRCT2::Ui::Accessibility
