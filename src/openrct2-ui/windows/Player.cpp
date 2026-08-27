/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "../interface/Viewport.h"

#include <algorithm>
#include <openrct2-ui/accessibility/ScreenReader.h>
#include <openrct2-ui/accessibility/graph/GraphBuilder.h>
#include <openrct2-ui/accessibility/graph/GraphScreens.h>
#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/GameState.h>
#include <openrct2/Input.h>
#include <openrct2/SpriteIds.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/network/PlayerKickAction.h>
#include <openrct2/actions/network/PlayerSetGroupAction.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/network/Network.h>
#include <openrct2/network/NetworkAction.h>
#include <openrct2/ui/WindowManager.h>
#include <optional>
#include <string>
#include <utility>

namespace OpenRCT2::Ui::Windows
{
    enum WindowPlayerPage
    {
        WINDOW_PLAYER_PAGE_OVERVIEW,
        WINDOW_PLAYER_PAGE_STATISTICS,
    };

#pragma region Widgets

    enum WindowPlayerWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_PAGE_BACKGROUND,
        WIDX_TAB_1,
        WIDX_TAB_2,

        WIDX_GROUP = 6,
        WIDX_GROUP_DROPDOWN,
        WIDX_LOCATE,
        WIDX_KICK,
        WIDX_VIEWPORT,
    };

    static constexpr ScreenSize kWindowSize = { 192, 157 };
    // clang-format off

    static constexpr auto kCommonPlayerWidgets = makeWidgets(
        makeWindowShim(kStringIdNone, kWindowSize),
        makeWidget({ 0, 43}, {192, 114}, WidgetType::resize, WindowColour::secondary),
        makeTab   ({ 3, 17}                                                         ),
        makeTab   ({34, 17}                                                         )
    );

    static constexpr auto window_player_overview_widgets = makeWidgets(
        kCommonPlayerWidgets,
        makeWidget({  3, 46}, {175, 12}, WidgetType::dropdownMenu, WindowColour::secondary                                              ), // Permission group
        makeWidget({167, 47}, { 11, 10}, WidgetType::button,       WindowColour::secondary, STR_DROPDOWN_GLYPH                          ),
        makeWidget({179, 45}, { 12, 24}, WidgetType::flatBtn,      WindowColour::secondary, ImageId(SPR_LOCATE),   STR_LOCATE_PLAYER_TIP), // Locate button
        makeWidget({179, 69}, { 12, 24}, WidgetType::flatBtn,      WindowColour::secondary, ImageId(SPR_DEMOLISH), STR_KICK_PLAYER_TIP  ), // Kick button
        makeWidget({  3, 60}, {175, 61}, WidgetType::viewport,     WindowColour::secondary                                              )  // Viewport
    );

    static constexpr auto window_player_statistics_widgets = makeWidgets(
        kCommonPlayerWidgets
    );

    static constexpr std::span<const Widget> window_player_page_widgets[] = {
        window_player_overview_widgets,
        window_player_statistics_widgets,
    };
    // clang-format on

#pragma endregion

    class PlayerWindow final : public Window
    {
        int16_t _previousRotation = -1;
        bool _drawViewport = true;

        // Combo-box navigation state while the group dropdown is open (graph recipe).
        bool _accessDropdownOpen = false;
        WidgetIndex _accessDropdownChevron = 0;

    public:
        void init(const uint8_t id)
        {
            number = id;
            initScrollWidgets();
            setPage(WINDOW_PLAYER_PAGE_OVERVIEW);
        }

#pragma region Events

        void onOpen() override
        {
            page = 0;
            currentFrame = 0;
            listInformationType = 0;

            WindowSetResize(*this, { 210, 134 }, { 500, 450 });

            setPage(WINDOW_PLAYER_PAGE_OVERVIEW);
        }

        void onResize() override
        {
            switch (page)
            {
                case WINDOW_PLAYER_PAGE_OVERVIEW:
                    onResizeOverview();
                    break;

                case WINDOW_PLAYER_PAGE_STATISTICS:
                    onResizeStatistics();
                    break;
            }
        }

        void onUpdate() override
        {
            switch (page)
            {
                case WINDOW_PLAYER_PAGE_OVERVIEW:
                    onUpdateOverview();
                    break;

                case WINDOW_PLAYER_PAGE_STATISTICS:
                    onUpdateStatistics();
                    break;
            }
        }

        void onPrepareDraw() override
        {
            switch (page)
            {
                case WINDOW_PLAYER_PAGE_OVERVIEW:
                    onPrepareDrawOverview();
                    break;

                case WINDOW_PLAYER_PAGE_STATISTICS:
                    onPrepareDrawStatistics();
                    break;
            }
        }

        void onDraw(Drawing::RenderTarget& rt) override
        {
            switch (page)
            {
                case WINDOW_PLAYER_PAGE_OVERVIEW:
                    onDrawOverview(rt);
                    break;

                case WINDOW_PLAYER_PAGE_STATISTICS:
                    onDrawStatistics(rt);
                    break;
            }
        }

        void onMouseDown(WidgetIndex widgetIndex) override
        {
            switch (page)
            {
                case WINDOW_PLAYER_PAGE_OVERVIEW:
                    onMouseDownOverview(widgetIndex);
                    break;
            }
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            switch (widgetIndex)
            {
                case WIDX_CLOSE:
                    close();
                    return;
                case WIDX_TAB_1:
                case WIDX_TAB_2:
                    setPage(widgetIndex - WIDX_TAB_1);
                    return;
            }

            switch (page)
            {
                case WINDOW_PLAYER_PAGE_OVERVIEW:
                    onMouseUpOverview(widgetIndex);
                    break;
            }
        }

        void onDropdown(WidgetIndex widgetIndex, int32_t selectedIndex) override
        {
            switch (page)
            {
                case WINDOW_PLAYER_PAGE_OVERVIEW:
                    onDropdownOverview(widgetIndex, selectedIndex);
                    break;
            }
        }

        // ---- Accessible keyboard navigation ------------------------------------------------------
        // Same multi-tab pattern as the other accessible windows: item 0 is the tab selector
        // (Overview / Statistics), items 1..N are the page's controls and read-outs. The group
        // selector opens as a real combo box.

        // ---- graph accessibility recipe ----

        // Declare the current page's rows (or, while the group combo box's dropdown is open, that
        // list's items) fresh from live state. itemText() already composes each row's full spoken
        // line, so each row is a single announcement node.
        void BuildAccessGraph(Accessibility::Graph::GraphBuilder& b)
        {
            using namespace Accessibility::Graph;

            auto* windowMgr = GetWindowManager();
            if (_accessDropdownOpen && (windowMgr == nullptr || windowMgr->FindByClass(WindowClass::dropdown) == nullptr))
                _accessDropdownOpen = false;

            if (_accessDropdownOpen)
            {
                b.PushContext("Group", "menu");
                for (int32_t i = 0; i < gDropdown.numItems; i++)
                {
                    if (gDropdown.items[i].isSeparator())
                        continue;
                    NodeVtable vt;
                    vt.announcements.emplace_back(NodeAnnouncement::Static(gDropdown.items[i].text));
                    if (gDropdown.items[i].isChecked())
                        vt.announcements.emplace_back(NodeAnnouncement::Static("selected", AnnouncementKinds::kSelected));
                    vt.onActivate = [this, i]() { accessCommitDropdown(i); };
                    b.AddItem(ControlId::Structural("dd:" + std::to_string(i)), std::move(vt));
                }
                b.PopContext();
                return;
            }

            b.PushContext(Accessibility::JoinSpeech({ playerName(), accessPageName(page) }));
            const int32_t count = accessItemCount();
            for (int32_t i = 0; i < count; i++)
            {
                NodeVtable vt;
                vt.announcements.emplace_back([this, i]() { return itemText(i); });
                vt.focusRect = [this, i]() -> std::optional<Accessibility::Graph::GraphRect> {
                    const WidgetIndex wi = accessItemWidget(i);
                    if (wi == WIDX_BACKGROUND || wi >= widgets.size() || widgets[wi].type == WidgetType::empty)
                        return std::nullopt; // read-only text row, no widget to box
                    const auto& wd = widgets[wi];
                    return Accessibility::Graph::GraphRect{ windowPos.x + wd.left, windowPos.y + wd.top,
                                                            wd.width() + 1, wd.height() + 1 };
                };
                if (page == WINDOW_PLAYER_PAGE_OVERVIEW)
                {
                    switch (i)
                    {
                        case 0: // group combo box: Enter opens the list
                            vt.onActivate = [this]() { accessOpenDropdown(WIDX_GROUP_DROPDOWN); };
                            break;
                        case 1: // locate on map
                            vt.onActivate = [this]() {
                                Accessibility::ScreenReaderSpeak("Locating player on the map.");
                                onMouseUp(WIDX_LOCATE);
                            };
                            break;
                        case 2: // kick
                            vt.onActivate = [this]() {
                                if (isWidgetDisabled(WIDX_KICK))
                                {
                                    Accessibility::ScreenReaderSpeak("Kick is not available.");
                                    return;
                                }
                                Accessibility::ScreenReaderSpeak("Kicking " + playerName());
                                onMouseUp(WIDX_KICK);
                            };
                            break;
                        default:
                            break; // ping / last action are read-only
                    }
                }
                b.AddItem(
                    ControlId::Structural("player:" + std::to_string(page) + ":" + std::to_string(i)), std::move(vt));
            }
            b.PopContext();
        }

        // Tab/Shift+Tab: cycle the two pages (Overview / Statistics); the graph announces the landing.
        void AccessChangePage(int32_t delta)
        {
            setPage((page + delta + 2) % 2);
        }

        // Escape: close an open group dropdown first, returning focus to the combo box; otherwise let
        // the navigator close the window.
        bool AccessEscape()
        {
            if (!_accessDropdownOpen)
                return false;
            accessCloseDropdown();
            accessSuggestFocus(0); // back to the group combo box
            return true;
        }

    private:
        int32_t playerIdx() const
        {
            return Network::GetPlayerIndex(static_cast<uint8_t>(number));
        }

        std::string playerName() const
        {
            const int32_t p = playerIdx();
            return p != -1 ? std::string(Network::GetPlayerName(p)) : std::string{};
        }

        static const char* accessPageName(int32_t p)
        {
            return (p == WINDOW_PLAYER_PAGE_STATISTICS) ? "Statistics" : "Overview";
        }

        int32_t accessItemCount() const
        {
            // Overview: group, locate, kick, ping, last action. Statistics: commands ran, money spent.
            return (page == WINDOW_PLAYER_PAGE_OVERVIEW) ? 5 : 2;
        }

        std::string groupName() const
        {
            const int32_t p = playerIdx();
            if (p == -1)
                return "none";
            const int32_t g = Network::GetGroupIndex(Network::GetPlayerGroup(p));
            return g != -1 ? std::string(Network::GetGroupName(g)) : std::string("none");
        }

        std::string itemText(int32_t i)
        {
            const int32_t p = playerIdx();
            if (page == WINDOW_PLAYER_PAGE_OVERVIEW)
            {
                switch (i)
                {
                    case 0:
                        return Accessibility::JoinSpeech({ "Group", "combo box", groupName() });
                    case 1:
                        return "Locate on map, button";
                    case 2:
                        return isWidgetDisabled(WIDX_KICK) ? "Kick, button, not available" : "Kick, button";
                    case 3:
                        return p != -1 ? "Ping, " + std::to_string(Network::GetPlayerPing(p)) + " milliseconds"
                                       : std::string{};
                    case 4:
                    {
                        if (p == -1)
                            return {};
                        const int32_t la = Network::GetPlayerLastAction(p, 0);
                        const std::string action = (la != -999) ? FormatStringID(Network::GetActionNameStringID(la))
                                                                 : FormatStringID(STR_ACTION_NA);
                        return Accessibility::JoinSpeech({ "Last action", action });
                    }
                }
            }
            else if (p != -1)
            {
                switch (i)
                {
                    case 0:
                        return FormatStringID(STR_COMMANDS_RAN, static_cast<uint32_t>(Network::GetPlayerCommandsRan(p)));
                    case 1:
                        return FormatStringID(STR_MONEY_SPENT, static_cast<uint32_t>(Network::GetPlayerMoneySpent(p)));
                }
            }
            return {};
        }

        WidgetIndex accessItemWidget(int32_t i) const
        {
            if (page == WINDOW_PLAYER_PAGE_OVERVIEW)
            {
                switch (i)
                {
                    case 0: return WIDX_GROUP;
                    case 1: return WIDX_LOCATE;
                    case 2: return WIDX_KICK;
                    default: return WIDX_BACKGROUND; // ping / last action are read-only text
                }
            }
            return WIDX_BACKGROUND;
        }

        // Ask the graph to land on a page row at its next render (used to return focus to the group
        // combo box once its dropdown closes).
        void accessSuggestFocus(int32_t itemIndex)
        {
            Accessibility::Graph::GraphStateForClass(WindowClass::player).nextSuggestedMove
                = Accessibility::Graph::ControlId::Structural(
                    "player:" + std::to_string(page) + ":" + std::to_string(itemIndex));
        }

        // ---- Group combo-box navigation, mirroring the shared dropdown pattern -------------------
        void accessCloseDropdown()
        {
            if (auto* windowMgr = GetWindowManager(); windowMgr != nullptr)
                windowMgr->CloseByClass(WindowClass::dropdown);
            _accessDropdownOpen = false;
        }

        void accessOpenDropdown(WidgetIndex chevronWidx)
        {
            onMouseDown(chevronWidx); // populates and shows gDropdown
            auto* windowMgr = GetWindowManager();
            if (windowMgr == nullptr || windowMgr->FindByClass(WindowClass::dropdown) == nullptr)
                return;
            _accessDropdownOpen = true;
            _accessDropdownChevron = chevronWidx;
            // The graph rebuild now declares the dropdown's items; its differ announces the landing.
        }

        void accessCommitDropdown(int32_t idx)
        {
            const WidgetIndex chevron = _accessDropdownChevron;
            const bool valid = idx >= 0 && idx < gDropdown.numItems && !gDropdown.items[idx].isSeparator();
            accessCloseDropdown();
            if (valid)
                onDropdown(chevron, idx);
            accessSuggestFocus(0); // group combo box is item 0 on the overview page
        }

#pragma endregion

    private:
        void setPage(int32_t newPage)
        {
            // Skip setting page if we're already on this page, unless we're initialising the window
            if (page == newPage && !widgets.empty())
                return;

            int32_t originalPage = page;

            page = newPage;
            currentFrame = 0;

            setWidgets(window_player_page_widgets[newPage]);
            invalidate();
            onResize();
            onPrepareDraw();
            initScrollWidgets();
            invalidate();

            if (page == WINDOW_PLAYER_PAGE_OVERVIEW)
            {
                if (viewport == nullptr)
                {
                    const auto viewportFocus = Focus(TileCoordsXYZ(128, 128, 0).ToCoordsXYZ());
                    ViewportCreate(*this, windowPos, width, height, viewportFocus);
                    flags |= WindowFlag::noScrolling;
                    onPrepareDraw();
                    UpdateViewport(false);
                }
                else if (originalPage != page)
                {
                    onPrepareDraw();
                    UpdateViewport(false);
                }
            }
            else
            {
                removeViewport();
            }
        }

        void DrawTabImages(Drawing::RenderTarget& rt)
        {
            Widget* widget;

            // Tab 1
            if (!isWidgetDisabled(WIDX_TAB_1))
            {
                widget = &this->widgets[WIDX_TAB_1];
                auto screenCoords = windowPos + ScreenCoordsXY{ widget->left, widget->top };
                GfxDrawSprite(rt, ImageId(SPR_PEEP_LARGE_FACE_NORMAL), screenCoords);
            }

            // Tab 2
            if (!isWidgetDisabled(WIDX_TAB_2))
            {
                widget = &this->widgets[WIDX_TAB_2];
                auto screenCoords = windowPos + ScreenCoordsXY{ widget->left, widget->top };
                int32_t imageId = SPR_TAB_FINANCES_SUMMARY_0;

                if (page == WINDOW_PLAYER_PAGE_STATISTICS)
                {
                    imageId += (currentFrame / 2) & 7;
                }

                GfxDrawSprite(rt, ImageId(imageId), screenCoords);
            }
        }

        void UpdateViewport(bool scroll)
        {
            int32_t playerIndex = Network::GetPlayerIndex(static_cast<uint8_t>(number));
            if (playerIndex == -1)
            {
                return;
            }

            if (viewport != nullptr)
            {
                auto coord = Network::GetPlayerLastActionCoord(playerIndex);
                if (coord.x != 0 || coord.y != 0 || coord.z != 0)
                {
                    auto centreLoc = centre2dCoordinates(coord, viewport);
                    if (!centreLoc.has_value())
                    {
                        return;
                    }
                    // Don't scroll if the view was originally undefined
                    if (!_drawViewport)
                    {
                        scroll = false;
                    }

                    if (!scroll || savedViewPos != centreLoc.value())
                    {
                        flags |= WindowFlag::scrollingToLocation;
                        savedViewPos = centreLoc.value();
                        if (!scroll)
                        {
                            viewport->viewPos = centreLoc.value();
                        }
                        invalidateWidget(WIDX_VIEWPORT);
                    }

                    // Draw the viewport
                    _drawViewport = true;
                }
                else
                {
                    // Don't draw the viewport
                    _drawViewport = false;
                }
            }
        }

        void UpdateTitle()
        {
            int32_t player = Network::GetPlayerIndex(static_cast<uint8_t>(number));
            if (player != -1)
            {
                widgets[WIDX_TITLE].setString(Network::GetPlayerName(player));
            }
            else
            {
                widgets[WIDX_TITLE].setString("");
            }
        }

#pragma region Overview

        void onResizeOverview()
        {
            WindowSetResize(*this, { 240, 170 }, { 500, 300 });
        }

        void onUpdateOverview()
        {
            currentFrame++;
            invalidateWidget(WIDX_TAB_1 + page);

            if (Network::GetPlayerIndex(static_cast<uint8_t>(number)) == -1)
            {
                close();
                return;
            }

            // Update viewport
            bool scroll = true;

            // Use this spare window field for rotation check
            if (_previousRotation != GetCurrentRotation())
            {
                _previousRotation = GetCurrentRotation();
                scroll = false;
            }
            UpdateViewport(scroll);
        }

        void onPrepareDrawOverview()
        {
            int32_t playerIndex = Network::GetPlayerIndex(static_cast<uint8_t>(number));
            if (playerIndex == -1)
            {
                return;
            }

            widgetSetPressedExclusive(*this, { WIDX_TAB_1, WIDX_TAB_2 }, page + WIDX_TAB_1);

            UpdateTitle();

            widgets[WIDX_LOCATE].right = width - 2;
            widgets[WIDX_LOCATE].left = width - 25;
            widgets[WIDX_KICK].right = width - 2;
            widgets[WIDX_KICK].left = width - 25;
            widgets[WIDX_VIEWPORT].right = width - 26;
            widgets[WIDX_VIEWPORT].bottom = height - 14;

            int32_t groupDropdownWidth = widgets[WIDX_GROUP].width() - 1;
            widgets[WIDX_GROUP].left = (width - groupDropdownWidth) / 2;
            widgets[WIDX_GROUP].right = widgets[WIDX_GROUP].left + groupDropdownWidth;
            widgets[WIDX_GROUP_DROPDOWN].left = widgets[WIDX_GROUP].right - 10;
            widgets[WIDX_GROUP_DROPDOWN].right = widgets[WIDX_GROUP].right;

            WindowAlignTabs(this, WIDX_TAB_1, WIDX_TAB_2);

            if (viewport != nullptr)
            {
                Widget* viewportWidget = &widgets[WIDX_VIEWPORT];

                viewport->pos = windowPos + ScreenCoordsXY{ viewportWidget->left, viewportWidget->top };
                viewport->width = viewportWidget->width() - 1;
                viewport->height = viewportWidget->height() - 1;
            }

            // only enable kick button for other players
            const bool canKick = Network::CanPerformAction(
                Network::GetCurrentPlayerGroupIndex(), Network::Permission::kickPlayer);
            const bool isServer = Network::GetPlayerFlags(playerIndex) & Network::PlayerFlags::kIsServer;
            const bool isOwnWindow = (Network::GetCurrentPlayerId() == number);
            widgetSetEnabled(*this, WIDX_KICK, canKick && !isOwnWindow && !isServer);
        }

        void onDrawOverview(Drawing::RenderTarget& rt)
        {
            drawWidgets(rt);
            DrawTabImages(rt);

            int32_t player = Network::GetPlayerIndex(static_cast<uint8_t>(number));
            if (player == -1)
            {
                return;
            }

            // Draw current group
            int32_t groupindex = Network::GetGroupIndex(Network::GetPlayerGroup(player));
            if (groupindex != -1)
            {
                Widget* widget = &widgets[WIDX_GROUP];

                thread_local std::string _buffer;
                _buffer.assign("{WINDOW_COLOUR_2}");
                _buffer += Network::GetGroupName(groupindex);

                drawTextEllipsised(
                    rt, windowPos + ScreenCoordsXY{ widget->midX() - 5, widget->top }, widget->width() - 9, _buffer,
                    { TextAlignment::centre });
            }

            // Draw ping
            auto screenCoords = windowPos + ScreenCoordsXY{ 90, widgets[WIDX_TAB_1].midY() };

            auto ft = Formatter();
            ft.Add<StringId>(STR_PING);
            drawText(rt, screenCoords, STR_WINDOW_COLOUR_2_STRINGID, ft);
            char ping[64];
            snprintf(ping, 64, "%d ms", Network::GetPlayerPing(player));
            drawText(rt, screenCoords + ScreenCoordsXY(30, 0), ping, { colours[2] });

            // Draw last action
            screenCoords = windowPos + ScreenCoordsXY{ width / 2, height - 13 };
            int32_t updatedWidth = this->width - 8;
            int32_t lastaction = Network::GetPlayerLastAction(player, 0);
            ft = Formatter();
            if (lastaction != -999)
            {
                ft.Add<StringId>(Network::GetActionNameStringID(lastaction));
            }
            else
            {
                ft.Add<StringId>(STR_ACTION_NA);
            }
            drawTextEllipsised(rt, screenCoords, updatedWidth, STR_LAST_ACTION_RAN, ft, { TextAlignment::centre });

            if (viewport != nullptr && _drawViewport)
            {
                WindowDrawViewport(rt, *this);
            }
        }

        void onMouseDownOverview(WidgetIndex widgetIndex)
        {
            auto* widget = &widgets[widgetIndex];
            switch (widgetIndex)
            {
                case WIDX_GROUP_DROPDOWN:
                    ShowGroupDropdownOverview(widget);
                    break;
            }
        }

        void onMouseUpOverview(WidgetIndex widgetIndex)
        {
            switch (widgetIndex)
            {
                case WIDX_LOCATE:
                {
                    WindowBase* mainWindow = WindowGetMain();
                    if (mainWindow != nullptr)
                    {
                        int32_t player = Network::GetPlayerIndex(static_cast<uint8_t>(number));
                        if (player == -1)
                        {
                            return;
                        }
                        auto coord = Network::GetPlayerLastActionCoord(player);
                        if (coord.x || coord.y || coord.z)
                        {
                            WindowScrollToLocation(*mainWindow, coord);
                        }
                    }
                }
                break;
                case WIDX_KICK:
                {
                    auto kickPlayerAction = GameActions::PlayerKickAction(number);
                    GameActions::Execute(&kickPlayerAction, getGameState());
                }
                break;
            }
        }

        void onDropdownOverview(WidgetIndex widgetIndex, int32_t dropdownIndex)
        {
            const auto playerId = static_cast<uint8_t>(number);
            const auto playerIdx = Network::GetPlayerIndex(playerId);
            if (playerIdx == -1)
            {
                return;
            }
            if (dropdownIndex == -1)
            {
                return;
            }
            const auto groupId = Network::GetGroupID(dropdownIndex);
            const auto windowHandle = std::make_pair(classification, number);
            auto playerSetGroupAction = GameActions::PlayerSetGroupAction(playerId, groupId);
            playerSetGroupAction.SetCallback(
                [windowHandle](const GameActions::GameAction* ga, const GameActions::Result* result) {
                    if (result->error == GameActions::Status::ok)
                    {
                        auto* windowMgr = GetWindowManager();
                        windowMgr->InvalidateByNumber(windowHandle.first, windowHandle.second);
                    }
                });
            GameActions::Execute(&playerSetGroupAction, getGameState());
        }

        void ShowGroupDropdownOverview(Widget* widget)
        {
            Widget* dropdownWidget;
            int32_t numItems;
            int32_t player = Network::GetPlayerIndex(static_cast<uint8_t>(number));
            if (player == -1)
            {
                return;
            }

            dropdownWidget = widget - 1;

            numItems = Network::GetNumGroups();

            WindowDropdownShowTextCustomWidth(
                { windowPos.x + dropdownWidget->left, windowPos.y + dropdownWidget->top }, dropdownWidget->height(), colours[1],
                0, 0, numItems, widget->right - dropdownWidget->left);

            for (int32_t i = 0; i < Network::GetNumGroups(); i++)
            {
                gDropdown.items[i] = Dropdown::MenuLabel(Network::GetGroupName(i));
            }

            gDropdown.items[Network::GetGroupIndex(Network::GetPlayerGroup(player))].setChecked(true);
        }

#pragma endregion

#pragma region Statistics

        void onResizeStatistics()
        {
            WindowSetResize(*this, { 210, 80 }, { 210, 80 });
        }

        void onUpdateStatistics()
        {
            currentFrame++;
            invalidateWidget(WIDX_TAB_1 + page);

            if (Network::GetPlayerIndex(static_cast<uint8_t>(number)) == -1)
            {
                close();
            }
        }

        void onPrepareDrawStatistics()
        {
            widgetSetPressedExclusive(*this, { WIDX_TAB_1, WIDX_TAB_2 }, page + WIDX_TAB_1);

            UpdateTitle();

            WindowAlignTabs(this, WIDX_TAB_1, WIDX_TAB_2);
        }

        void onDrawStatistics(Drawing::RenderTarget& rt)
        {
            drawWidgets(rt);
            DrawTabImages(rt);

            int32_t player = Network::GetPlayerIndex(static_cast<uint8_t>(number));
            if (player == -1)
            {
                return;
            }

            auto screenCoords = windowPos
                + ScreenCoordsXY{ widgets[WIDX_PAGE_BACKGROUND].left + 4, widgets[WIDX_PAGE_BACKGROUND].top + 4 };

            auto ft = Formatter();
            ft.Add<uint32_t>(Network::GetPlayerCommandsRan(player));
            drawText(rt, screenCoords, STR_COMMANDS_RAN, ft);

            screenCoords.y += kListRowHeight;

            ft = Formatter();
            ft.Add<uint32_t>(Network::GetPlayerMoneySpent(player));
            drawText(rt, screenCoords, STR_MONEY_SPENT, ft);
        }

#pragma endregion
    };

    WindowBase* PlayerOpen(uint8_t id)
    {
        auto* windowMgr = GetWindowManager();
        auto* window = static_cast<PlayerWindow*>(windowMgr->BringToFrontByNumber(WindowClass::player, id));
        if (window == nullptr)
        {
            window = windowMgr->Create<PlayerWindow>(WindowClass::player, { 240, 170 }, WindowFlag::resizable);
        }

        window->init(id);

        return window;
    }

    // Register the player window with the graph accessibility navigator (called once at startup via
    // EnsureGraphScreensRegistered). From here on the graph owns this window class; the legacy
    // accessibility dispatcher stands down for it.
    void RegisterPlayerGraphScreen()
    {
        using namespace Accessibility::Graph;
        GraphScreen screen;
        screen.windowClass = WindowClass::player;
        screen.build = [](GraphBuilder& b, WindowBase& w) { static_cast<PlayerWindow&>(w).BuildAccessGraph(b); };
        screen.onTabKey = [](WindowBase& w, int32_t dir) {
            static_cast<PlayerWindow&>(w).AccessChangePage(dir);
            return true;
        };
        screen.onEscape = [](WindowBase& w) { return static_cast<PlayerWindow&>(w).AccessEscape(); };
        RegisterGraphScreen(std::move(screen));
    }
} // namespace OpenRCT2::Ui::Windows
