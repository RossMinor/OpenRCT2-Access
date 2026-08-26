/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "GraphScreens.h"

#include "../MenuNavigation.h"

#include <openrct2-ui/windows/Windows.h>
#include <openrct2/interface/Widget.h>
#include <openrct2/interface/Window.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/ui/WindowManager.h>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace OpenRCT2::Ui::Accessibility::Graph
{
    static std::vector<GraphScreen> _screens;
    static std::unordered_map<WindowClass, GraphState> _states;

    void RegisterGraphScreen(GraphScreen screen)
    {
        _screens.push_back(std::move(screen));
    }

    // A generic recipe for a modal confirmation prompt (demolish / refurbish / fire / save): every
    // button widget becomes a navigable node in declaration order, Enter routes to the window's own
    // mouse-up handler (which runs that button's action), and Escape closes the window. The recipe is
    // widget-driven and never casts to a concrete window type, so one registration can serve several
    // prompt window classes - including the two different C++ prompt classes that share the demolish
    // window class. Each prompt still speaks its situational message on open; the buttons self-announce
    // as the user moves across them.
    static void BuildButtonPromptGraph(GraphBuilder& b, WindowBase& w)
    {
        auto* wp = &w;
        for (WidgetIndex i = 0; i < static_cast<WidgetIndex>(w.widgets.size()); i++)
        {
            if (w.widgets[i].type != WidgetType::button)
                continue;
            NodeVtable vt;
            vt.announcements.emplace_back([wp, i]() { return OpenRCT2::FormatStringID(wp->widgets[i].text); });
            vt.announcements.emplace_back(NodeAnnouncement::Static("button", AnnouncementKinds::kRole));
            vt.onActivate = [wp, i]() { wp->onMouseUp(i); };
            vt.focusRect = [wp, i]() -> std::optional<GraphRect> {
                const auto& wd = wp->widgets[i];
                return GraphRect{ wp->windowPos.x + wd.left, wp->windowPos.y + wd.top, wd.width() + 1, wd.height() + 1 };
            };
            b.AddItem(ControlId::Structural("btn:" + std::to_string(i)), std::move(vt));
        }
    }

    static void RegisterButtonPromptScreen(WindowClass wc)
    {
        GraphScreen screen;
        screen.windowClass = wc;
        screen.build = [](GraphBuilder& b, WindowBase& w) { BuildButtonPromptGraph(b, w); };
        RegisterGraphScreen(std::move(screen));
    }

    void EnsureGraphScreensRegistered()
    {
        static bool registered = false;
        if (registered)
            return;
        registered = true;

        // Each migrated window registers its recipe from its own source file (the recipe needs
        // the window's internals). Add new migrations here.
        Windows::RegisterRideListGraphScreen();
        Windows::RegisterOptionsGraphScreen();
        Windows::RegisterStaffListGraphScreen();
        Windows::RegisterScenarioSelectGraphScreen();
        Windows::RegisterGuestListGraphScreen();
        Windows::RegisterCheatsGraphScreen();
        Windows::RegisterParkGraphScreen();
        Windows::RegisterFinancesGraphScreen();

        // Modal confirmation prompts share one generic button-driven recipe. demolishRidePrompt is
        // used by both the demolish and refurbish prompts (different C++ classes, same window class).
        RegisterButtonPromptScreen(WindowClass::demolishRidePrompt);
        RegisterButtonPromptScreen(WindowClass::firePrompt);
        RegisterButtonPromptScreen(WindowClass::savePrompt);
    }

    bool GraphOwnsWindowClass(WindowClass wc)
    {
        for (const auto& s : _screens)
        {
            if (s.windowClass == wc)
                return true;
        }
        return false;
    }

    const GraphScreen* GraphScreenForClass(WindowClass wc)
    {
        for (const auto& s : _screens)
        {
            if (s.windowClass == wc)
                return &s;
        }
        return nullptr;
    }

    GraphState& GraphStateForClass(WindowClass wc)
    {
        return _states[wc];
    }

    void DropGraphStatesForClosedWindows()
    {
        auto* windowMgr = GetWindowManager();
        if (windowMgr == nullptr)
            return;
        for (auto it = _states.begin(); it != _states.end();)
        {
            if (windowMgr->FindByClass(it->first) == nullptr)
                it = _states.erase(it);
            else
                ++it;
        }
    }

    WindowBase* FrontNavigableWindow()
    {
        auto* windowMgr = GetWindowManager();
        if (windowMgr == nullptr)
            return nullptr;

        // While placing a pre-built ride, the keyboard map cursor positions it, so no window
        // should capture the keys (same guard as the legacy dispatcher).
        if (windowMgr->FindByClass(WindowClass::trackDesignPlace) != nullptr)
            return nullptr;

        // WindowVisitEach walks the window list back-to-front, so the last navigable window it
        // visits is the one currently in front - the real z-order, not a hand-authored ranking.
        WindowBase* result = nullptr;
        WindowVisitEach([&result](WindowBase* w) {
            if (GraphOwnsWindowClass(w->classification) || IsLegacyNavigableAccessibleClass(w->classification))
                result = w;
        });
        return result;
    }
} // namespace OpenRCT2::Ui::Accessibility::Graph
