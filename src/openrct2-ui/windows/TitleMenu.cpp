/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <openrct2-ui/accessibility/AccessCrashHandler.h>
#include <openrct2-ui/accessibility/MenuNavigation.h>
#include <openrct2-ui/accessibility/ScreenReader.h>
#include <openrct2-ui/accessibility/graph/GraphBuilder.h>
#include <openrct2-ui/accessibility/graph/GraphScreens.h>
#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/interface/Window.h>
#include <openrct2-ui/scripting/CustomMenu.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/Input.h>
#include <openrct2/PlatformEnvironment.h>
#include <openrct2/SpriteIds.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/general/LoadOrQuitAction.h>
#include <openrct2/drawing/Rectangle.h>
#include <openrct2/interface/ColourWithFlags.h>
#include <openrct2/scenes/SceneManager.h>
#include <openrct2/scenes/editor/EditorScene.h>
#include <openrct2/ui/UiContext.h>
#include <openrct2/ui/WindowManager.h>
#include <string>
#include <vector>

namespace OpenRCT2::Ui::Windows
{
    using namespace OpenRCT2::Drawing;

    enum WindowTitleMenuWidgetIdx : WidgetIndex
    {
        WIDX_START_NEW_GAME,
        WIDX_CONTINUE_SAVED_GAME,
        WIDX_MULTIPLAYER,
        WIDX_GAME_TOOLS,
        WIDX_NEW_VERSION,
    };

    enum
    {
        DDIDX_SCENARIO_EDITOR,
        DDIDX_CONVERT_SAVED_GAME,
        DDIDX_TRACK_DESIGNER,
        DDIDX_TRACK_MANAGER,
        DDIDX_OPEN_CONTENT_FOLDER,
        DDIDX_CUSTOM_BEGIN = 6,
    };

    static constexpr ScreenSize kMenuButtonDims = { 82, 82 };
    static constexpr ScreenSize kUpdateButtonDims = { kMenuButtonDims.width * 4, 28 };

    // clang-format off
    static constexpr auto _titleMenuWidgets = makeWidgets(
        makeWidget({0, kUpdateButtonDims.height}, kMenuButtonDims,   WidgetType::imgBtn, WindowColour::tertiary,  ImageId(SPR_MENU_NEW_GAME),       STR_START_NEW_GAME_TIP),
        makeWidget({0, kUpdateButtonDims.height}, kMenuButtonDims,   WidgetType::imgBtn, WindowColour::tertiary,  ImageId(SPR_MENU_LOAD_GAME),      STR_CONTINUE_SAVED_GAME_TIP),
        makeWidget({0, kUpdateButtonDims.height}, kMenuButtonDims,   WidgetType::imgBtn, WindowColour::tertiary,  ImageId(SPR_G2_MENU_MULTIPLAYER), STR_SHOW_MULTIPLAYER_TIP),
        makeWidget({0, kUpdateButtonDims.height}, kMenuButtonDims,   WidgetType::imgBtn, WindowColour::tertiary,  ImageId(SPR_MENU_TOOLBOX),        STR_GAME_TOOLS_TIP),
        makeWidget({0, kUpdateButtonDims.height}, kUpdateButtonDims, WidgetType::button, WindowColour::secondary, STR_UPDATE_AVAILABLE)
    );
    // clang-format on

    static void WindowTitleMenuScenarioselectCallback(const utf8* path)
    {
        GameNotifyMapChange();
        GetContext()->LoadParkFromFile(path, false, true);
        GameLoadScripts();
        GameNotifyMapChanged();
    }

    static void InvokeCustomToolboxMenuItem(size_t index)
    {
#ifdef ENABLE_SCRIPTING
        const auto& customMenuItems = Scripting::CustomMenuItems;
        size_t i = 0;
        for (const auto& item : customMenuItems)
        {
            if (item.Kind == Scripting::CustomToolbarMenuItemKind::toolbox)
            {
                if (i == index)
                {
                    item.Invoke();
                    break;
                }
                i++;
            }
        }
#endif
    }

    class TitleMenuWindow final : public Window
    {
    private:
        ScreenRect _filterRect;

        // Whether the Game Tools dropdown sub-menu currently owns navigation. The focus cursor
        // itself lives in the graph screen manager, not here.
        bool _accessDropdownOpen = false;

    public:
        void onOpen() override
        {
            setWidgets(_titleMenuWidgets);

#ifdef DISABLE_NETWORK
            widgets[WIDX_MULTIPLAYER].setHidden();
#endif

            int32_t x = 0;
            for (Widget* widget = widgets.data(); widget != &widgets[WIDX_NEW_VERSION]; widget++)
            {
                if (widget->isVisible())
                {
                    widget->left = x;
                    widget->right = x + kMenuButtonDims.width - 1;

                    x += kMenuButtonDims.width;
                }
            }
            width = x;
            widgets[WIDX_NEW_VERSION].right = width;
            windowPos.x = (ContextGetWidth() - width) / 2;
            colours[1] = ColourWithFlags{ Drawing::Colour::lightOrange }.withFlag(ColourFlag::translucent, true);

            initScrollWidgets();

            Accessibility::ScreenReaderInit();
            Accessibility::CrashHandlerInit();
            // Focus lands on the first item and is announced by the graph screen manager's differ;
            // nothing to poke here.
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            auto* windowMgr = GetWindowManager();

            switch (widgetIndex)
            {
                case WIDX_START_NEW_GAME:
                    if (windowMgr->BringToFrontByClass(WindowClass::scenarioSelect) == nullptr)
                    {
                        windowMgr->CloseByClass(WindowClass::loadsave);
                        windowMgr->CloseByClass(WindowClass::serverList);
                        ScenarioselectOpen(WindowTitleMenuScenarioselectCallback);
                    }
                    break;
                case WIDX_CONTINUE_SAVED_GAME:
                    if (windowMgr->BringToFrontByClass(WindowClass::loadsave) == nullptr)
                    {
                        windowMgr->CloseByClass(WindowClass::scenarioSelect);
                        windowMgr->CloseByClass(WindowClass::serverList);
                        auto loadOrQuitAction = GameActions::LoadOrQuitAction(GameActions::LoadOrQuitModes::openSavePrompt);
                        GameActions::Execute(&loadOrQuitAction, getGameState());
                    }
                    break;
                case WIDX_MULTIPLAYER:
                    if (windowMgr->BringToFrontByClass(WindowClass::serverList) == nullptr)
                    {
                        windowMgr->CloseByClass(WindowClass::scenarioSelect);
                        windowMgr->CloseByClass(WindowClass::loadsave);
                        ContextOpenWindow(WindowClass::serverList);
                    }
                    break;
                case WIDX_NEW_VERSION:
                    ContextOpenWindowView(WindowView::newVersionInfo);
                    break;
            }
        }

        void onMouseDown(WidgetIndex widgetIndex) override
        {
            if (widgetIndex == WIDX_GAME_TOOLS)
            {
                int32_t i = 0;
                gDropdown.items[i++] = Dropdown::PlainMenuLabel(STR_SCENARIO_EDITOR);
                gDropdown.items[i++] = Dropdown::PlainMenuLabel(STR_CONVERT_SAVED_GAME_TO_SCENARIO);
                gDropdown.items[i++] = Dropdown::PlainMenuLabel(STR_ROLLER_COASTER_DESIGNER);
                gDropdown.items[i++] = Dropdown::PlainMenuLabel(STR_TRACK_DESIGNS_MANAGER);
                gDropdown.items[i++] = Dropdown::PlainMenuLabel(STR_OPEN_USER_CONTENT_FOLDER);

#ifdef ENABLE_SCRIPTING
                auto hasCustomItems = false;
                const auto& customMenuItems = Scripting::CustomMenuItems;
                if (!customMenuItems.empty())
                {
                    for (const auto& item : customMenuItems)
                    {
                        if (item.Kind == Scripting::CustomToolbarMenuItemKind::toolbox)
                        {
                            if (!hasCustomItems)
                            {
                                hasCustomItems = true;
                                gDropdown.items[i++] = Dropdown::Separator();
                            }

                            gDropdown.items[i] = Dropdown::PlainMenuLabel(item.Text.c_str());
                            i++;
                        }
                    }
                }
#endif

                Widget* widget = &widgets[widgetIndex];
                int32_t yOffset = 0;
                if (i > 5)
                {
                    yOffset = -(widget->height() - 1 + 5 + (i * 12));
                }

                WindowDropdownShowText(
                    windowPos + ScreenCoordsXY{ widget->left, widget->top + yOffset }, widget->height(),
                    colours[0].withFlag(ColourFlag::translucent, true), {}, i);
            }
        }

        void onDropdown(WidgetIndex widgetIndex, int32_t selectedIndex) override
        {
            if (selectedIndex == -1)
            {
                return;
            }
            if (widgetIndex == WIDX_GAME_TOOLS)
            {
                auto* sceneMgr = GetContext()->GetSceneManager();
                switch (selectedIndex)
                {
                    case DDIDX_SCENARIO_EDITOR:
                        sceneMgr->setActiveScene(sceneMgr->getScenarioEditorScene());
                        break;
                    case DDIDX_CONVERT_SAVED_GAME:
                    {
                        auto* editorScene = static_cast<EditorScene*>(sceneMgr->getScenarioEditorScene());
                        editorScene->ConvertSaveToScenario();
                        break;
                    }
                    case DDIDX_TRACK_DESIGNER:
                        sceneMgr->setActiveScene(sceneMgr->getTrackDesignerScene());
                        break;
                    case DDIDX_TRACK_MANAGER:
                        sceneMgr->setActiveScene(sceneMgr->getTrackManagerScene());
                        break;
                    case DDIDX_OPEN_CONTENT_FOLDER:
                    {
                        auto context = GetContext();
                        auto& env = context->GetPlatformEnvironment();
                        auto& uiContext = context->GetUiContext();
                        uiContext.OpenFolder(env.GetDirectoryPath(DirBase::user));
                        break;
                    }
                    default:
                        InvokeCustomToolboxMenuItem(selectedIndex - DDIDX_CUSTOM_BEGIN);
                        break;
                }
            }
        }

        CursorID onCursor(WidgetIndex, const ScreenCoordsXY&, CursorID cursorId) override
        {
            gTooltipCloseTimeout = gCurrentRealTimeTicks + 2000;
            return cursorId;
        }

        void onPrepareDraw() override
        {
            _filterRect = { windowPos + ScreenCoordsXY{ 0, kUpdateButtonDims.height },
                            windowPos + ScreenCoordsXY{ width - 1, kMenuButtonDims.height + kUpdateButtonDims.height - 1 } };

            const bool newVersionAvailable = GetContext()->HasNewVersionInfo();
            widgets[WIDX_NEW_VERSION].setVisible(newVersionAvailable);

            if (newVersionAvailable)
                _filterRect.Point1.y = windowPos.y;
        }

        void onDraw(RenderTarget& rt) override
        {
            Rectangle::filter(rt, _filterRect, FilterPaletteID::palette51);
            drawWidgets(rt);
        }

#pragma region Accessibility

        static const char* getMenuItemName(WidgetIndex i)
        {
            switch (i)
            {
                case WIDX_START_NEW_GAME:
                    return "New game";
                case WIDX_CONTINUE_SAVED_GAME:
                    return "Load game";
                case WIDX_MULTIPLAYER:
                    return "Multiplayer";
                case WIDX_GAME_TOOLS:
                    return "Game tools";
                case WIDX_NEW_VERSION:
                    return "Update available";
                default:
                    return "";
            }
        }

        std::vector<WidgetIndex> getMenuItems() const
        {
            std::vector<WidgetIndex> items;
            for (WidgetIndex i = WIDX_START_NEW_GAME; i <= WIDX_NEW_VERSION; i++)
            {
                if (widgets[i].type != WidgetType::empty)
                    items.push_back(i);
            }
            return items;
        }

    public:
        // ---- graph accessibility recipe ----

        // Declare the menu's buttons, or - while the Game Tools dropdown is open - that dropdown's
        // items instead. Immediate mode makes the dropdown a plain modal sub-list.
        void BuildAccessGraph(Accessibility::Graph::GraphBuilder& b)
        {
            using namespace Accessibility::Graph;

            // Re-derive the dropdown state from reality every build: the list can be dismissed
            // behind our back, and a stale flag would strand navigation inside a menu that is gone.
            auto* windowMgr = GetWindowManager();
            if (_accessDropdownOpen && (windowMgr == nullptr || windowMgr->FindByClass(WindowClass::dropdown) == nullptr))
                _accessDropdownOpen = false;

            if (_accessDropdownOpen)
            {
                b.PushContext(getMenuItemName(WIDX_GAME_TOOLS), "menu");
                for (int32_t i = 0; i < gDropdown.numItems; i++)
                {
                    if (gDropdown.items[i].isSeparator())
                        continue;
                    NodeVtable vt;
                    vt.announcements.emplace_back(NodeAnnouncement::Static(gDropdown.items[i].text));
                    if (gDropdown.items[i].isDisabled())
                        vt.announcements.emplace_back(NodeAnnouncement::Static("unavailable"));
                    vt.onActivate = [this, i]() { accessCommitDropdown(i); };
                                        // Keep the engine's drawn highlight on the row the keyboard is on; it clears
                    // that field from the mouse every tick, so it has to be re-asserted.
                    vt.onFocus = [i]() { Accessibility::SetKeyboardDropdownIndex(i); };
                    b.AddItem(ControlId::Structural("dd:" + std::to_string(i)), std::move(vt));
                }
                b.PopContext();
                return;
            }

            for (const WidgetIndex w : getMenuItems())
            {
                NodeVtable vt;
                vt.announcements.emplace_back(NodeAnnouncement::Static(getMenuItemName(w)));
                vt.onActivate = [this, w]() { accessActivateItem(w); };
                vt.focusRect = [this, w]() -> std::optional<GraphRect> {
                    if (w >= widgets.size() || widgets[w].type == WidgetType::empty)
                        return std::nullopt;
                    const auto& wd = widgets[w];
                    return GraphRect{ windowPos.x + wd.left, windowPos.y + wd.top, wd.width() + 1, wd.height() + 1 };
                };
                b.AddItem(ControlId::Structural("title:" + std::to_string(w)), std::move(vt));
            }
        }

        // Escape closes an open sub-menu and stays on Game Tools. With none open it is swallowed:
        // this is the root screen of the title sequence, and the navigator's default - closing the
        // focused window - would take the menu away with no way to bring it back.
        bool AccessEscape()
        {
            if (!_accessDropdownOpen)
                return true;
            accessCloseDropdown();
            accessSuggestFocus(WIDX_GAME_TOOLS);
            return true;
        }

    private:
        // Ask the graph to land on this button at its next render.
        void accessSuggestFocus(WidgetIndex w)
        {
            Accessibility::Graph::GraphStateForClass(WindowClass::titleMenu).nextSuggestedMove
                = Accessibility::Graph::ControlId::Structural("title:" + std::to_string(w));
        }

        void accessActivateItem(WidgetIndex widgetIndex)
        {
            // onMouseDown opens the dropdown button; onMouseUp handles the window-opening buttons.
            // Exactly one acts for any given button. The claim keeps a dropdown alive past the next
            // input tick - without it the engine finds no widget owning the open list and closes it
            // immediately.
            Accessibility::ClaimWidgetPressForKeyboard(*this, widgetIndex);
            onMouseDown(widgetIndex);
            onMouseUp(widgetIndex);

            auto* windowMgr = GetWindowManager();
            const bool dropdownOpened = windowMgr != nullptr
                && windowMgr->FindByClass(WindowClass::dropdown) != nullptr;
            if (!dropdownOpened)
                Accessibility::ReleaseWidgetPressForKeyboard(); // no list; drop the claim

            if (dropdownOpened)
            {
                // The rebuild now declares the dropdown's items and the differ announces the
                // landing, so there is nothing to speak here.
                _accessDropdownOpen = true;
                return;
            }

            // If the button opened a window we can navigate, let that window's own announcement
            // stand; otherwise confirm the selection so the keypress is never silent.
            WindowClass openedClass = WindowClass::null;
            switch (widgetIndex)
            {
                case WIDX_START_NEW_GAME:
                    openedClass = WindowClass::scenarioSelect;
                    break;
                case WIDX_CONTINUE_SAVED_GAME:
                    openedClass = WindowClass::loadsave;
                    break;
                case WIDX_MULTIPLAYER:
                    openedClass = WindowClass::serverList;
                    break;
                default:
                    break;
            }
            WindowBase* opened = (windowMgr != nullptr && openedClass != WindowClass::null)
                ? windowMgr->FindByClass(openedClass)
                : nullptr;
            // A graph-owned window announces itself through the graph screen manager; speaking here
            // would double-announce.
            if (opened != nullptr && Accessibility::Graph::GraphOwnsWindowClass(openedClass))
                return;
            if (opened == nullptr || !opened->onAccessibilityAction(AccessibilityAction::moveDown))
                Accessibility::ScreenReaderSpeak(std::string("Selected ") + getMenuItemName(widgetIndex));
        }

        void accessCommitDropdown(int32_t idx)
        {
            const bool valid = idx >= 0 && idx < gDropdown.numItems && !gDropdown.items[idx].isSeparator()
                && !gDropdown.items[idx].isDisabled();
            const std::string selectedText = valid ? std::string(gDropdown.items[idx].text) : std::string();

            accessCloseDropdown();

            if (valid)
            {
                // Announce the chosen item before running it, so any speech the action itself
                // produces is heard last.
                if (!selectedText.empty())
                    Accessibility::ScreenReaderSpeak(selectedText);
                onDropdown(WIDX_GAME_TOOLS, idx);
            }
            accessSuggestFocus(WIDX_GAME_TOOLS);
        }

        void accessCloseDropdown()
        {
            Accessibility::CloseWidgetDropdownFromKeyboard();
            _accessDropdownOpen = false;
        }

#pragma endregion
    };

    /**
     * Creates the window containing the menu buttons on the title screen.
     */
    WindowBase* TitleMenuOpen()
    {
        const uint16_t windowHeight = kMenuButtonDims.height + kUpdateButtonDims.height;

        auto* windowMgr = GetWindowManager();
        return windowMgr->Create<TitleMenuWindow>(
            WindowClass::titleMenu, ScreenCoordsXY(0, ContextGetHeight() - 182), { 0, windowHeight },
            { WindowFlag::stickToBack, WindowFlag::transparent, WindowFlag::noBackground, WindowFlag::noTitleBar });
    }

    void RegisterTitleMenuGraphScreen()
    {
        using namespace Accessibility::Graph;
        GraphScreen screen;
        screen.windowClass = WindowClass::titleMenu;
        screen.build = [](GraphBuilder& b, WindowBase& w) { static_cast<TitleMenuWindow&>(w).BuildAccessGraph(b); };
        screen.onEscape = [](WindowBase& w) { return static_cast<TitleMenuWindow&>(w).AccessEscape(); };
        RegisterGraphScreen(std::move(screen));
    }
} // namespace OpenRCT2::Ui::Windows
