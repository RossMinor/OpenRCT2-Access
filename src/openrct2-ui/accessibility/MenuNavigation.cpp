/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "MenuNavigation.h"

#include "ScreenReader.h"

#include <SDL.h>
#include <algorithm>
#include <openrct2/interface/Widget.h>
#include <openrct2/interface/WindowBase.h>
#include <openrct2/interface/WindowClasses.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/localisation/StringIdType.h>
#include <openrct2/ui/WindowManager.h>
#include <optional>
#include <string>
#include <vector>

namespace OpenRCT2::Ui::Accessibility
{
    // Position of the focus cursor within the title menu's ordered widget list (generic
    // navigation fallback), or -1 when nothing is focused yet.
    static int _focusPos = -1;

    // The SDL key we consumed on key-down, so the matching key-up can be swallowed too
    // and not reach the shortcut manager.
    static uint32_t _lastHandledKey = 0;

    static std::optional<AccessibilityAction> MapKeyToAction(uint32_t key)
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
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                return AccessibilityAction::activate;
            case SDLK_ESCAPE:
                return AccessibilityAction::cancel;
            default:
                return std::nullopt;
        }
    }

    // The window that currently owns accessible keyboard navigation, chosen front-to-back
    // among the windows we know how to navigate.
    static WindowBase* GetActiveAccessibleWindow()
    {
        auto* windowMgr = GetWindowManager();
        if (windowMgr == nullptr)
            return nullptr;

        // While placing a pre-built ride, the keyboard map cursor positions it, so no window
        // should capture the arrow keys.
        if (windowMgr->FindByClass(WindowClass::trackDesignPlace) != nullptr)
            return nullptr;

        // Modal prompts sit in front and must take focus while open.
        if (auto* w = windowMgr->FindByClass(WindowClass::savePrompt))
            return w;
        if (auto* w = windowMgr->FindByClass(WindowClass::serverList))
            return w;
        if (auto* w = windowMgr->FindByClass(WindowClass::scenarioSelect))
            return w;
        // Pre-built ride (track design) selection list.
        if (auto* w = windowMgr->FindByClass(WindowClass::trackDesignList))
            return w;
        if (auto* w = windowMgr->FindByClass(WindowClass::titleMenu))
            return w;
        // In-game navigable windows take focus while open.
        if (auto* w = windowMgr->FindByClass(WindowClass::constructRide))
            return w;
        if (auto* w = windowMgr->FindByClass(WindowClass::rideList))
            return w;
        if (auto* w = windowMgr->FindByClass(WindowClass::staffList))
            return w;
        if (auto* w = windowMgr->FindByClass(WindowClass::guestList))
            return w;
        if (auto* w = windowMgr->FindByClass(WindowClass::finances))
            return w;
        if (auto* w = windowMgr->FindByClass(WindowClass::research))
            return w;
        if (auto* w = windowMgr->FindByClass(WindowClass::recentNews))
            return w;
        // The park window handles its own objective page; on other pages it declines so
        // map controls keep working.
        if (auto* w = windowMgr->FindByClass(WindowClass::parkInformation))
            return w;
        return nullptr;
    }

    static bool IsFocusableWidget(const Widget& widget)
    {
        if (!widget.isVisible())
            return false;

        switch (widget.type)
        {
            case WidgetType::imgBtn:
            case WidgetType::flatBtn:
            case WidgetType::colourBtn:
            case WidgetType::trnBtn:
            case WidgetType::tab:
            case WidgetType::button:
            case WidgetType::tableHeader:
            case WidgetType::dropdownMenu:
            case WidgetType::spinner:
            case WidgetType::checkbox:
            case WidgetType::closeBox:
                return true;
            default:
                return false;
        }
    }

    // Image-based widgets store an image in the content union, so their spoken label
    // must come from the tooltip. Text-based widgets carry a usable string id.
    static bool IsTextWidget(WidgetType type)
    {
        switch (type)
        {
            case WidgetType::button:
            case WidgetType::tableHeader:
            case WidgetType::label:
            case WidgetType::labelCentred:
            case WidgetType::caption:
            case WidgetType::checkbox:
            case WidgetType::groupbox:
                return true;
            default:
                return false;
        }
    }

    static std::string GetWidgetLabel(const Widget& widget)
    {
        if (widget.flags.has(WidgetFlag::textIsString) && widget.string != nullptr)
        {
            return std::string(widget.string);
        }

        StringId id = kStringIdNone;
        if (IsTextWidget(widget.type) && widget.text != kStringIdNone)
            id = widget.text;
        if (id == kStringIdNone)
            id = widget.tooltip;

        if (id == kStringIdNone)
            return {};

        return OpenRCT2::FormatStringID(id);
    }

    static void SpeakWidget(const Widget& widget)
    {
        std::string text = GetWidgetLabel(widget);
        const char* role = (widget.type == WidgetType::checkbox) ? "checkbox" : "button";
        if (!text.empty())
            text += ", ";
        text += role;
        ScreenReaderSpeak(text);
    }

    static std::vector<WidgetIndex> GetOrderedFocusableWidgets(const WindowBase& w)
    {
        std::vector<WidgetIndex> order;
        for (WidgetIndex i = 0; i < static_cast<WidgetIndex>(w.widgets.size()); i++)
        {
            if (IsFocusableWidget(w.widgets[i]))
                order.push_back(i);
        }

        // Present widgets top-to-bottom, then left-to-right, so a 2D layout reads as a
        // single vertical list to a keyboard/screen-reader user.
        std::sort(order.begin(), order.end(), [&w](WidgetIndex a, WidgetIndex b) {
            const auto& wa = w.widgets[a];
            const auto& wb = w.widgets[b];
            if (wa.top != wb.top)
                return wa.top < wb.top;
            return wa.left < wb.left;
        });
        return order;
    }

    // Generic navigation over a window's interactive widgets. Used for simple windows
    // (e.g. the title menu) that do not provide their own onAccessibilityAction.
    static bool HandleGenericWidgetNav(WindowBase& w, AccessibilityAction action)
    {
        const auto order = GetOrderedFocusableWidgets(w);
        if (order.empty())
            return false;

        const int count = static_cast<int>(order.size());

        switch (action)
        {
            case AccessibilityAction::activate:
                if (_focusPos >= 0 && _focusPos < count)
                {
                    const auto widgetIndex = order[_focusPos];
                    // Confirm the selection audibly before running the action, since the
                    // screen it opens is not voiced yet.
                    ScreenReaderSpeak("Selected " + GetWidgetLabel(w.widgets[widgetIndex]));
                    w.onMouseUp(widgetIndex);
                }
                return true;

            case AccessibilityAction::moveDown:
            case AccessibilityAction::moveUp:
                if (_focusPos < 0 || _focusPos >= count)
                    _focusPos = (action == AccessibilityAction::moveDown) ? 0 : count - 1;
                else if (action == AccessibilityAction::moveDown)
                    _focusPos = (_focusPos + 1) % count;
                else
                    _focusPos = (_focusPos - 1 + count) % count;

                SpeakWidget(w.widgets[order[_focusPos]]);
                return true;

            default:
                return false;
        }
    }

    void FocusFirstItem(WindowBase& w)
    {
        _focusPos = -1;
        HandleGenericWidgetNav(w, AccessibilityAction::moveDown);
    }

    bool HandleMenuNavigationKey(const InputEvent& e)
    {
        if (e.deviceKind != InputDeviceKind::keyboard)
            return false;

        const auto action = MapKeyToAction(e.button);
        if (!action.has_value())
            return false;

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
        {
            _focusPos = -1;
            return false;
        }

        // Let the window navigate its own contents first. The generic widget-navigation
        // fallback is only for simple menu windows (the title menu); other windows must
        // opt in via onAccessibilityAction so we never hijack in-game keys.
        bool handled = w->onAccessibilityAction(*action);
        if (!handled && w->classification == WindowClass::titleMenu)
            handled = HandleGenericWidgetNav(*w, *action);

        _lastHandledKey = handled ? e.button : 0;
        return handled;
    }
} // namespace OpenRCT2::Ui::Accessibility
