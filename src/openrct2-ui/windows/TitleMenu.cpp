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
#include <openrct2-ui/accessibility/graph/GraphScreens.h>
#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/scripting/CustomMenu.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/Input.h>
#include <openrct2/ParkImporter.h>
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

    static constexpr ScreenSize MenuButtonDims = { 82, 82 };
    static constexpr ScreenSize UpdateButtonDims = { MenuButtonDims.width * 4, 28 };

    // clang-format off
    static constexpr auto _titleMenuWidgets = makeWidgets(
        makeWidget({0, UpdateButtonDims.height}, MenuButtonDims,   WidgetType::imgBtn, WindowColour::tertiary,  ImageId(SPR_MENU_NEW_GAME),       STR_START_NEW_GAME_TIP),
        makeWidget({0, UpdateButtonDims.height}, MenuButtonDims,   WidgetType::imgBtn, WindowColour::tertiary,  ImageId(SPR_MENU_LOAD_GAME),      STR_CONTINUE_SAVED_GAME_TIP),
        makeWidget({0, UpdateButtonDims.height}, MenuButtonDims,   WidgetType::imgBtn, WindowColour::tertiary,  ImageId(SPR_G2_MENU_MULTIPLAYER), STR_SHOW_MULTIPLAYER_TIP),
        makeWidget({0, UpdateButtonDims.height}, MenuButtonDims,   WidgetType::imgBtn, WindowColour::tertiary,  ImageId(SPR_MENU_TOOLBOX),        STR_GAME_TOOLS_TIP),
        makeWidget({0,                       0}, UpdateButtonDims, WidgetType::empty,  WindowColour::secondary, STR_UPDATE_AVAILABLE)
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
            if (item.Kind == Scripting::CustomToolbarMenuItemKind::Toolbox)
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

        // Keyboard-navigation cursor over the menu buttons, and whether the Game Tools
        // dropdown sub-menu currently owns navigation.
        int32_t _accessIndex = -1;
        bool _accessDropdownOpen = false;

    public:
        void onOpen() override
        {
            setWidgets(_titleMenuWidgets);

#ifdef DISABLE_NETWORK
            widgets[WIDX_MULTIPLAYER].type = WidgetType::empty;
#endif

            int32_t x = 0;
            for (Widget* widget = widgets.data(); widget != &widgets[WIDX_NEW_VERSION]; widget++)
            {
                if (widget->type != WidgetType::empty)
                {
                    widget->left = x;
                    widget->right = x + MenuButtonDims.width - 1;

                    x += MenuButtonDims.width;
                }
            }
            width = x;
            widgets[WIDX_NEW_VERSION].right = width;
            windowPos.x = (ContextGetWidth() - width) / 2;
            colours[1] = ColourWithFlags{ Drawing::Colour::lightOrange }.withFlag(ColourFlag::translucent, true);

            initScrollWidgets();

            Accessibility::ScreenReaderInit();
            Accessibility::CrashHandlerInit();
            // Default to the first menu item, matching the in-game menu standard.
            onAccessibilityAction(AccessibilityAction::moveDown);
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            WindowBase* windowToOpen = nullptr;

            auto* windowMgr = GetWindowManager();

            switch (widgetIndex)
            {
                case WIDX_START_NEW_GAME:
                    windowToOpen = windowMgr->FindByClass(WindowClass::scenarioSelect);
                    if (windowToOpen != nullptr)
                    {
                        windowMgr->BringToFront(*windowToOpen);
                    }
                    else
                    {
                        windowMgr->CloseByClass(WindowClass::loadsave);
                        windowMgr->CloseByClass(WindowClass::serverList);
                        ScenarioselectOpen(WindowTitleMenuScenarioselectCallback);
                    }
                    break;
                case WIDX_CONTINUE_SAVED_GAME:
                    windowToOpen = windowMgr->FindByClass(WindowClass::loadsave);
                    if (windowToOpen != nullptr)
                    {
                        windowMgr->BringToFront(*windowToOpen);
                    }
                    else
                    {
                        windowMgr->CloseByClass(WindowClass::scenarioSelect);
                        windowMgr->CloseByClass(WindowClass::serverList);
                        auto loadOrQuitAction = GameActions::LoadOrQuitAction(GameActions::LoadOrQuitModes::openSavePrompt);
                        GameActions::Execute(&loadOrQuitAction, getGameState());
                    }
                    break;
                case WIDX_MULTIPLAYER:
                    windowToOpen = windowMgr->FindByClass(WindowClass::serverList);
                    if (windowToOpen != nullptr)
                    {
                        windowMgr->BringToFront(*windowToOpen);
                    }
                    else
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
                        if (item.Kind == Scripting::CustomToolbarMenuItemKind::Toolbox)
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
                    colours[0].withFlag(ColourFlag::translucent, true), Dropdown::Flag::StayOpen, i);
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
            _filterRect = { windowPos + ScreenCoordsXY{ 0, UpdateButtonDims.height },
                            windowPos + ScreenCoordsXY{ width - 1, MenuButtonDims.height + UpdateButtonDims.height - 1 } };
            if (GetContext()->HasNewVersionInfo())
            {
                widgets[WIDX_NEW_VERSION].type = WidgetType::button;
                _filterRect.Point1.y = windowPos.y;
            }
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

        bool onAccessibilityTypeahead(uint32_t key) override
        {
            if (_accessDropdownOpen)
                return true; // not inside the open dropdown
            const auto items = getMenuItems();
            const int32_t count = static_cast<int32_t>(items.size());
            if (count == 0)
                return true;
            const char target = static_cast<char>(key);
            const int32_t start = (_accessIndex < 0) ? 0 : _accessIndex;
            for (int32_t i = 1; i <= count; i++)
            {
                const int32_t idx = (start + i) % count;
                const std::string name = getMenuItemName(items[idx]);
                char first = name.empty() ? '\0' : name[0];
                if (first >= 'A' && first <= 'Z')
                    first += 32;
                if (first == target)
                {
                    _accessIndex = idx;
                    Accessibility::ScreenReaderSpeak(getMenuItemName(items[idx]));
                    return true;
                }
            }
            return true;
        }

        std::optional<ScreenRect> getAccessibilityFocusRect() override
        {
            const auto items = getMenuItems();
            if (_accessIndex < 0 || _accessIndex >= static_cast<int32_t>(items.size()))
                return std::nullopt;
            const auto& wd = widgets[items[_accessIndex]];
            return ScreenRect{ windowPos + ScreenCoordsXY{ wd.left, wd.top },
                               windowPos + ScreenCoordsXY{ wd.right, wd.bottom } };
        }

        bool onAccessibilityAction(AccessibilityAction action) override
        {
            if (_accessDropdownOpen)
                return handleAccessibilityDropdown(action);

            const auto items = getMenuItems();
            if (items.empty())
                return false;
            const int32_t count = static_cast<int32_t>(items.size());

            switch (action)
            {
                case AccessibilityAction::cancel:
                    _accessIndex = -1;
                    return true;

                case AccessibilityAction::announce:
                    if (_accessIndex >= 0 && _accessIndex < count)
                        Accessibility::ScreenReaderSpeakItem(getMenuItemName(items[_accessIndex]), _accessIndex, count);
                    return true;

                case AccessibilityAction::moveUp:
                case AccessibilityAction::moveLeft:
                case AccessibilityAction::moveDown:
                case AccessibilityAction::moveRight:
                {
                    const bool forward = (action == AccessibilityAction::moveDown
                                          || action == AccessibilityAction::moveRight);
                    if (_accessIndex < 0 || _accessIndex >= count)
                        _accessIndex = forward ? 0 : count - 1;
                    else if (forward)
                        _accessIndex = (_accessIndex + 1) % count;
                    else
                        _accessIndex = (_accessIndex - 1 + count) % count;
                    Accessibility::ScreenReaderSpeak(getMenuItemName(items[_accessIndex]));
                    return true;
                }

                case AccessibilityAction::activate:
                {
                    if (_accessIndex < 0 || _accessIndex >= count)
                        return true;
                    const auto widgetIndex = items[_accessIndex];

                    // onMouseDown opens the dropdown button; onMouseUp handles the
                    // window-opening buttons. Exactly one acts for any given button.
                    onMouseDown(widgetIndex);
                    onMouseUp(widgetIndex);

                    auto* windowMgr = GetWindowManager();
                    if (windowMgr != nullptr && windowMgr->FindByClass(WindowClass::dropdown) != nullptr)
                    {
                        _accessDropdownOpen = true;
                        moveDropdownHighlight(1); // focus + announce first item
                    }
                    else
                    {
                        // If the button opened a window we can navigate, focus and announce its
                        // first item; otherwise just confirm the selection.
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
                        // A graph-owned window announces itself through the graph screen manager;
                        // poking or speaking here would double-announce (migration seam, spec 10.5).
                        if (opened != nullptr && Accessibility::Graph::GraphOwnsWindowClass(openedClass))
                        {
                            // The graph manager speaks the screen name and landing.
                        }
                        else if (opened == nullptr || !opened->onAccessibilityAction(AccessibilityAction::moveDown))
                            Accessibility::ScreenReaderSpeak(std::string("Selected ") + getMenuItemName(widgetIndex));
                    }
                    return true;
                }

                default:
                    return false;
            }
        }

        bool handleAccessibilityDropdown(AccessibilityAction action)
        {
            switch (action)
            {
                case AccessibilityAction::moveUp:
                case AccessibilityAction::moveLeft:
                    moveDropdownHighlight(-1);
                    return true;
                case AccessibilityAction::moveDown:
                case AccessibilityAction::moveRight:
                    moveDropdownHighlight(1);
                    return true;
                case AccessibilityAction::activate:
                    commitAccessibilityDropdown();
                    return true;
                case AccessibilityAction::cancel:
                    closeAccessibilityDropdown();
                    Accessibility::ScreenReaderSpeak("Game tools");
                    return true;
                default:
                    return false;
            }
        }

        void moveDropdownHighlight(int32_t delta)
        {
            const int32_t n = gDropdown.numItems;
            if (n <= 0)
                return;

            int32_t idx = gDropdown.highlightedIndex;
            for (int32_t steps = 0; steps < n; steps++)
            {
                idx += delta;
                if (idx < 0)
                    idx = n - 1;
                else if (idx >= n)
                    idx = 0;
                if (!gDropdown.items[idx].isSeparator())
                    break;
            }
            if (gDropdown.items[idx].isSeparator())
                return;

            gDropdown.highlightedIndex = idx;

            std::string text = gDropdown.items[idx].text;
            if (gDropdown.items[idx].isDisabled())
                text += ", unavailable";

            // Position among the selectable (non-separator) items.
            int32_t total = 0, pos = 0;
            for (int32_t j = 0; j < gDropdown.numItems; j++)
            {
                if (gDropdown.items[j].isSeparator())
                    continue;
                if (j == idx)
                    pos = total;
                total++;
            }
            Accessibility::ScreenReaderSpeakItem(text, pos, total);

            auto* windowMgr = GetWindowManager();
            if (windowMgr != nullptr)
                windowMgr->InvalidateByClass(WindowClass::dropdown);
        }

        void commitAccessibilityDropdown()
        {
            const int32_t idx = gDropdown.highlightedIndex;
            const bool valid = idx >= 0 && idx < gDropdown.numItems && !gDropdown.items[idx].isSeparator()
                && !gDropdown.items[idx].isDisabled();
            const std::string selectedText = valid ? std::string(gDropdown.items[idx].text) : std::string();

            closeAccessibilityDropdown();

            if (valid)
            {
                // Announce the chosen item before running it, so any speech the action itself
                // produces is heard last.
                if (!selectedText.empty())
                    Accessibility::ScreenReaderSpeak(selectedText);
                onDropdown(WIDX_GAME_TOOLS, idx);
            }
        }

        void closeAccessibilityDropdown()
        {
            WindowDropdownClose();
            InputSetState(InputState::normal);
            _accessDropdownOpen = false;
        }

#pragma endregion
    };

    /**
     * Creates the window containing the menu buttons on the title screen.
     */
    WindowBase* TitleMenuOpen()
    {
        const uint16_t windowHeight = MenuButtonDims.height + UpdateButtonDims.height;

        auto* windowMgr = GetWindowManager();
        return windowMgr->Create<TitleMenuWindow>(
            WindowClass::titleMenu, ScreenCoordsXY(0, ContextGetHeight() - 182), { 0, windowHeight },
            { WindowFlag::stickToBack, WindowFlag::transparent, WindowFlag::noBackground, WindowFlag::noTitleBar });
    }
} // namespace OpenRCT2::Ui::Windows
