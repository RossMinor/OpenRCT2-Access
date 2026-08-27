/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <algorithm>
#include <cassert>
#include <openrct2-ui/accessibility/ScreenReader.h>
#include <openrct2-ui/accessibility/graph/GraphBuilder.h>
#include <openrct2-ui/accessibility/graph/GraphScreens.h>
#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/SpriteIds.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/network/NetworkModifyGroupAction.h>
#include <openrct2/config/Config.h>
#include <openrct2/core/String.hpp>
#include <openrct2/drawing/ColourMap.h>
#include <openrct2/drawing/Drawing.String.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Rectangle.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/interface/ColourWithFlags.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/network/Network.h>
#include <string>
#include <vector>
#include <openrct2/ui/WindowManager.h>

using namespace OpenRCT2::Drawing;

namespace OpenRCT2::Ui::Windows
{
    enum
    {
        WINDOW_MULTIPLAYER_PAGE_INFORMATION,
        WINDOW_MULTIPLAYER_PAGE_PLAYERS,
        WINDOW_MULTIPLAYER_PAGE_GROUPS,
        WINDOW_MULTIPLAYER_PAGE_OPTIONS
    };

    enum WindowMultiplayerWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_CONTENT_PANEL,
        WIDX_TAB1,
        WIDX_TAB2,
        WIDX_TAB3,
        WIDX_TAB4,

        WIDX_HEADER_PLAYER = 8,
        WIDX_HEADER_GROUP,
        WIDX_HEADER_LAST_ACTION,
        WIDX_HEADER_PING,
        WIDX_LIST,

        WIDX_DEFAULT_GROUP = 8,
        WIDX_DEFAULT_GROUP_DROPDOWN,
        WIDX_ADD_GROUP,
        WIDX_REMOVE_GROUP,
        WIDX_RENAME_GROUP,
        WIDX_SELECTED_GROUP,
        WIDX_SELECTED_GROUP_DROPDOWN,
        WIDX_PERMISSIONS_LIST,

        WIDX_LOG_CHAT_CHECKBOX = 8,
        WIDX_LOG_SERVER_ACTIONS_CHECKBOX,
        WIDX_KNOWN_KEYS_ONLY_CHECKBOX,
    };

    static constexpr ScreenSize kWindowSize = { 340, 240 };
    // clang-format off

    static constexpr auto kMainMultiplayerWidgets = makeWidgets(
        makeWindowShim(kStringIdNone, kWindowSize),
        makeWidget({  0, 43}, {340, 197}, WidgetType::resize, WindowColour::secondary                          ),
        makeTab   ({  3, 17},                                                          STR_SHOW_SERVER_INFO_TIP),
        makeTab   ({ 34, 17},                                                          STR_PLAYERS_TIP         ),
        makeTab   ({ 65, 17},                                                          STR_GROUPS_TIP          ),
        makeTab   ({ 96, 17},                                                          STR_OPTIONS_TIP         )
    );

    static constexpr auto window_multiplayer_information_widgets = makeWidgets(
        kMainMultiplayerWidgets
    );

    static constexpr auto window_multiplayer_players_widgets = makeWidgets(
        kMainMultiplayerWidgets,
        makeWidget({  3, 46}, {173,  15}, WidgetType::tableHeader, WindowColour::primary  , STR_PLAYER     ), // Player name
        makeWidget({176, 46}, { 83,  15}, WidgetType::tableHeader, WindowColour::primary  , STR_GROUP      ), // Player name
        makeWidget({259, 46}, {100,  15}, WidgetType::tableHeader, WindowColour::primary  , STR_LAST_ACTION), // Player name
        makeWidget({359, 46}, { 42,  15}, WidgetType::tableHeader, WindowColour::primary  , STR_PING       ), // Player name
        makeWidget({  3, 60}, {334, 177}, WidgetType::scroll,      WindowColour::secondary, SCROLL_VERTICAL) // list
    );

    static constexpr auto window_multiplayer_groups_widgets = makeWidgets(
        kMainMultiplayerWidgets,
        makeWidget({141, 46}, {175,  12}, WidgetType::dropdownMenu, WindowColour::secondary                    ), // default group
        makeWidget({305, 47}, { 11,  10}, WidgetType::button,       WindowColour::secondary, STR_DROPDOWN_GLYPH),
        makeWidget({ 11, 65}, { 92,  12}, WidgetType::button,       WindowColour::secondary, STR_ADD_GROUP     ), // add group button
        makeWidget({113, 65}, { 92,  12}, WidgetType::button,       WindowColour::secondary, STR_REMOVE_GROUP  ), // remove group button
        makeWidget({215, 65}, { 92,  12}, WidgetType::button,       WindowColour::secondary, STR_RENAME_GROUP  ), // rename group button
        makeWidget({ 72, 80}, {175,  12}, WidgetType::dropdownMenu, WindowColour::secondary                    ), // selected group
        makeWidget({236, 81}, { 11,  10}, WidgetType::button,       WindowColour::secondary, STR_DROPDOWN_GLYPH),
        makeWidget({  3, 94}, {314, 207}, WidgetType::scroll,       WindowColour::secondary, SCROLL_VERTICAL   ) // permissions list
    );

    static constexpr auto window_multiplayer_options_widgets = makeWidgets(
        kMainMultiplayerWidgets,
        makeWidget({3, 50}, {295, 12}, WidgetType::checkbox, WindowColour::secondary, STR_LOG_CHAT,              STR_LOG_CHAT_TIP             ),
        makeWidget({3, 64}, {295, 12}, WidgetType::checkbox, WindowColour::secondary, STR_LOG_SERVER_ACTIONS,    STR_LOG_SERVER_ACTIONS_TIP   ),
        makeWidget({3, 78}, {295, 12}, WidgetType::checkbox, WindowColour::secondary, STR_ALLOW_KNOWN_KEYS_ONLY, STR_ALLOW_KNOWN_KEYS_ONLY_TIP)
    );

    static std::span<const Widget> window_multiplayer_page_widgets[] = {
        window_multiplayer_information_widgets,
        window_multiplayer_players_widgets,
        window_multiplayer_groups_widgets,
        window_multiplayer_options_widgets,
    };

    static constexpr StringId WindowMultiplayerPageTitles[] = {
        STR_MULTIPLAYER_INFORMATION_TITLE,
        STR_MULTIPLAYER_PLAYERS_TITLE,
        STR_MULTIPLAYER_GROUPS_TITLE,
        STR_MULTIPLAYER_OPTIONS_TITLE,
    };

    // clang-format on

    static constexpr int32_t window_multiplayer_animation_divisor[] = {
        4,
        4,
        2,
        2,
    };
    static constexpr int32_t window_multiplayer_animation_frames[] = {
        8,
        8,
        7,
        4,
    };

    static bool IsServerPlayerInvisible()
    {
        return Network::IsServerPlayerInvisible() && !Config::Get().general.debuggingTools;
    }

    class MultiplayerWindow final : public Window
    {
    private:
        std::optional<ScreenSize> _windowInformationSize;
        uint8_t _selectedGroup{ 0 };
        // Combo-box navigation state, while a group dropdown is open (graph recipe).
        bool _accessDropdownOpen = false;
        WidgetIndex _accessDropdownChevron = 0;
        int32_t _accessDropdownOwner = -1; // page-row index that owns the open dropdown
        // The Groups page presents its top controls (default-group, add, remove, rename, selected-group)
        // as items 0..4 within the page, then the permission rows follow.
        static constexpr int32_t kGroupControlCount = 5;

    private:
        void showGroupDropdown(WidgetIndex widgetIndex)
        {
            auto widget = &widgets[widgetIndex];
            Widget* dropdownWidget = widget - 1;
            auto numItems = Network::GetNumGroups();

            WindowDropdownShowTextCustomWidth(
                windowPos + ScreenCoordsXY{ dropdownWidget->left, dropdownWidget->top }, dropdownWidget->height(), colours[1],
                0, 0, numItems, widget->right - dropdownWidget->left);

            for (auto i = 0; i < Network::GetNumGroups(); i++)
            {
                gDropdown.items[i] = Dropdown::MenuLabel(Network::GetGroupName(i));
            }
            if (widget == &widgets[WIDX_DEFAULT_GROUP_DROPDOWN])
            {
                gDropdown.items[Network::GetGroupIndex(Network::GetDefaultGroup())].setChecked(true);
            }
            else if (widget == &widgets[WIDX_SELECTED_GROUP_DROPDOWN])
            {
                gDropdown.items[Network::GetGroupIndex(_selectedGroup)].setChecked(true);
            }
        }

        void informationPaint(RenderTarget& rt)
        {
            RenderTarget clippedRT;
            if (ClipRenderTarget(clippedRT, rt, windowPos, width, height))
            {
                auto screenCoords = ScreenCoordsXY{ 3, widgets[WIDX_CONTENT_PANEL].top + 7 };
                int32_t newWidth = width - 6;

                const auto& name = Network::GetServerName();
                {
                    screenCoords.y += drawTextWrapped(clippedRT, screenCoords, newWidth, name, { colours[1] });
                    screenCoords.y += kListRowHeight / 2;
                }

                const auto& description = Network::GetServerDescription();
                if (!description.empty())
                {
                    screenCoords.y += drawTextWrapped(clippedRT, screenCoords, newWidth, description, { colours[1] });
                    screenCoords.y += kListRowHeight / 2;
                }

                const auto& providerName = Network::GetServerProviderName();
                if (!providerName.empty())
                {
                    auto ft = Formatter();
                    ft.Add<const char*>(providerName.c_str());
                    drawText(clippedRT, screenCoords, STR_PROVIDER_NAME, ft);
                    screenCoords.y += kListRowHeight;
                }

                const auto& providerEmail = Network::GetServerProviderEmail();
                if (!providerEmail.empty())
                {
                    auto ft = Formatter();
                    ft.Add<const char*>(providerEmail.c_str());
                    drawText(clippedRT, screenCoords, STR_PROVIDER_EMAIL, ft);
                    screenCoords.y += kListRowHeight;
                }

                const auto& providerWebsite = Network::GetServerProviderWebsite();
                if (!providerWebsite.empty())
                {
                    auto ft = Formatter();
                    ft.Add<const char*>(providerWebsite.c_str());
                    drawText(clippedRT, screenCoords, STR_PROVIDER_WEBSITE, ft);
                }
            }
        }

        void playersPaint(RenderTarget& rt)
        {
            // Number of players
            StringId stringId = numListItems == 1 ? STR_MULTIPLAYER_PLAYER_COUNT : STR_MULTIPLAYER_PLAYER_COUNT_PLURAL;
            auto screenCoords = windowPos + ScreenCoordsXY{ 4, widgets[WIDX_LIST].bottom + 2 };
            auto ft = Formatter();
            ft.Add<uint16_t>(numListItems);
            drawText(rt, screenCoords, stringId, ft, { colours[2] });
        }

        void playersScrollPaint(int32_t scrollIndex, RenderTarget& rt) const
        {
            ScreenCoordsXY screenCoords;
            screenCoords.y = 0;

            const int32_t firstPlayerInList = (IsServerPlayerInvisible() ? 1 : 0);
            int32_t listPosition = 0;

            for (int32_t player = firstPlayerInList; player < Network::GetNumPlayers(); player++)
            {
                if (screenCoords.y > rt.y + rt.height)
                {
                    break;
                }

                if (screenCoords.y + kScrollableRowHeight + 1 >= rt.y)
                {
                    thread_local std::string _buffer;
                    _buffer.reserve(512);
                    _buffer.clear();

                    // Draw player name
                    auto colour = ColourWithFlags{ Drawing::Colour::black };
                    if (listPosition == selectedListItem)
                    {
                        Rectangle::filter(
                            rt, { 0, screenCoords.y, 800, screenCoords.y + kScrollableRowHeight - 1 },
                            FilterPaletteID::paletteDarken1);
                        _buffer += Network::GetPlayerName(player);
                        colour = colours[2];
                    }
                    else
                    {
                        if (Network::GetPlayerFlags(player) & Network::PlayerFlags::kIsServer)
                        {
                            _buffer += "{BABYBLUE}";
                        }
                        else
                        {
                            _buffer += "{BLACK}";
                        }
                        _buffer += Network::GetPlayerName(player);
                    }
                    screenCoords.x = 0;
                    drawTextEllipsised(rt, screenCoords, 230, _buffer, { colour });

                    // Draw group name
                    _buffer.resize(0);
                    int32_t group = Network::GetGroupIndex(Network::GetPlayerGroup(player));
                    if (group != -1)
                    {
                        _buffer += "{BLACK}";
                        screenCoords.x = 173;
                        _buffer += Network::GetGroupName(group);
                        drawTextEllipsised(rt, screenCoords, 80, _buffer, { colour });
                    }

                    // Draw last action
                    int32_t action = Network::GetPlayerLastAction(player, 2000);
                    auto ft = Formatter();
                    if (action != -999)
                    {
                        ft.Add<StringId>(Network::GetActionNameStringID(action));
                    }
                    else
                    {
                        ft.Add<StringId>(STR_ACTION_NA);
                    }
                    drawTextEllipsised(rt, { 256, screenCoords.y }, 100, STR_BLACK_STRING, ft);

                    // Draw ping
                    _buffer.resize(0);
                    int32_t ping = Network::GetPlayerPing(player);
                    if (ping <= 100)
                    {
                        _buffer += "{GREEN}";
                    }
                    else if (ping <= 250)
                    {
                        _buffer += "{YELLOW}";
                    }
                    else
                    {
                        _buffer += "{RED}";
                    }

                    char pingBuffer[64]{};
                    snprintf(pingBuffer, sizeof(pingBuffer), "%d ms", ping);
                    _buffer += pingBuffer;

                    screenCoords.x = 356;
                    drawText(rt, screenCoords, _buffer, { colour });
                }
                screenCoords.y += kScrollableRowHeight;
                listPosition++;
            }
        }

        void groupsPaint(RenderTarget& rt)
        {
            thread_local std::string _buffer;

            Widget* widget = &widgets[WIDX_DEFAULT_GROUP];
            int32_t group = Network::GetGroupIndex(Network::GetDefaultGroup());
            if (group != -1)
            {
                _buffer.assign("{WINDOW_COLOUR_2}");
                _buffer += Network::GetGroupName(group);

                auto ft = Formatter();
                ft.Add<const char*>(_buffer.c_str());
                drawTextEllipsised(
                    rt, windowPos + ScreenCoordsXY{ widget->midX() - 5, widget->top }, widget->width() - 9, STR_STRING, ft,
                    { TextAlignment::centre });
            }

            auto screenPos = windowPos
                + ScreenCoordsXY{ widgets[WIDX_CONTENT_PANEL].left + 4, widgets[WIDX_CONTENT_PANEL].top + 4 };

            drawText(rt, screenPos, STR_DEFAULT_GROUP, { colours[2] });

            screenPos.y += 20;

            Rectangle::fillInset(
                rt, { screenPos - ScreenCoordsXY{ 0, 6 }, screenPos + ScreenCoordsXY{ 310, -5 } }, colours[1],
                Rectangle::BorderStyle::inset);

            widget = &widgets[WIDX_SELECTED_GROUP];
            group = Network::GetGroupIndex(_selectedGroup);
            if (group != -1)
            {
                _buffer.assign("{WINDOW_COLOUR_2}");
                _buffer += Network::GetGroupName(group);
                drawTextEllipsised(
                    rt, windowPos + ScreenCoordsXY{ widget->midX() - 5, widget->top }, widget->width() - 9, _buffer,
                    { TextAlignment::centre });
            }
        }

        void groupsScrollPaint(int32_t scrollIndex, RenderTarget& rt) const
        {
            auto screenCoords = ScreenCoordsXY{ 0, 0 };

            auto rtCoords = ScreenCoordsXY{ rt.x, rt.y };
            Rectangle::fill(
                rt, { rtCoords, rtCoords + ScreenCoordsXY{ rt.width - 1, rt.height - 1 } },
                getColourMap(colours[1].colour).midLight);

            for (int32_t i = 0; i < Network::GetNumActions(); i++)
            {
                if (i == selectedListItem)
                {
                    Rectangle::filter(
                        rt, { 0, screenCoords.y, 800, screenCoords.y + kScrollableRowHeight - 1 },
                        FilterPaletteID::paletteDarken1);
                }
                if (screenCoords.y > rt.y + rt.height)
                {
                    break;
                }

                if (screenCoords.y + kScrollableRowHeight + 1 >= rt.y)
                {
                    int32_t groupindex = Network::GetGroupIndex(_selectedGroup);
                    if (groupindex != -1)
                    {
                        if (Network::CanPerformAction(groupindex, static_cast<Network::Permission>(i)))
                        {
                            screenCoords.x = 0;
                            drawText(rt, screenCoords, u8"{WINDOW_COLOUR_2}✓");
                        }
                    }

                    // Draw action name
                    auto ft = Formatter();
                    ft.Add<uint16_t>(Network::GetActionNameStringID(i));
                    drawText(rt, { 10, screenCoords.y }, STR_WINDOW_COLOUR_2_STRINGID, ft);
                }
                screenCoords.y += kScrollableRowHeight;
            }
        }

        void drawTabImage(RenderTarget& rt, int32_t page_number, int32_t spriteIndex)
        {
            WidgetIndex widgetIndex = WIDX_TAB1 + page_number;

            if (!isWidgetDisabled(widgetIndex))
            {
                if (page == page_number)
                {
                    int32_t numFrames = window_multiplayer_animation_frames[page];
                    if (numFrames > 1)
                    {
                        int32_t frame = currentFrame / window_multiplayer_animation_divisor[page];
                        spriteIndex += (frame % numFrames);
                    }
                }

                GfxDrawSprite(
                    rt, ImageId(spriteIndex),
                    windowPos + ScreenCoordsXY{ widgets[widgetIndex].left, widgets[widgetIndex].top });
            }
        }

        void drawTabImages(RenderTarget& rt)
        {
            drawTabImage(rt, WINDOW_MULTIPLAYER_PAGE_INFORMATION, SPR_TAB_KIOSKS_AND_FACILITIES_0);
            drawTabImage(rt, WINDOW_MULTIPLAYER_PAGE_PLAYERS, SPR_TAB_GUESTS_0);
            drawTabImage(rt, WINDOW_MULTIPLAYER_PAGE_GROUPS, SPR_TAB_STAFF_OPTIONS_0);
            drawTabImage(rt, WINDOW_MULTIPLAYER_PAGE_OPTIONS, SPR_TAB_GEARS_0);
        }

        ScreenSize informationGetSize()
        {
            assert(!_windowInformationSize.has_value());

            int32_t lineHeight = FontGetLineHeight(FontStyle::medium);

            // Base dimensions.
            const int32_t baseWidth = 450;
            int32_t baseHeight = 55;

            // Server name is displayed word-wrapped, so figure out how high it will be.
            {
                int32_t numLines;
                wrapString(Network::GetServerName(), baseWidth, FontStyle::medium, nullptr, &numLines);
                baseHeight += (numLines + 1) * lineHeight + (kListRowHeight / 2);
            }

            // Likewise, for the optional server description -- which can be a little longer.
            const auto& descString = Network::GetServerDescription();
            if (!descString.empty())
            {
                int32_t numLines;
                wrapString(descString, baseWidth, FontStyle::medium, nullptr, &numLines);
                baseHeight += (numLines + 1) * lineHeight + (kListRowHeight / 2);
            }

            // Finally, account for provider info, if present.
            {
                const auto& providerName = Network::GetServerProviderName();
                if (!providerName.empty())
                    baseHeight += kListRowHeight;

                const auto& providerEmail = Network::GetServerProviderEmail();
                if (!providerEmail.empty())
                    baseHeight += kListRowHeight;

                const auto& providerWebsite = Network::GetServerProviderWebsite();
                if (!providerWebsite.empty())
                    baseHeight += kListRowHeight;
            }

            // TODO: Are these casts still neccessary?
            _windowInformationSize = { static_cast<int16_t>(baseWidth), static_cast<int16_t>(baseHeight) };
            return _windowInformationSize.value();
        }

    public:
        void onOpen() override
        {
            setPage(WINDOW_MULTIPLAYER_PAGE_INFORMATION);
        }

        void setPage(int32_t page_number)
        {
            // Skip setting page if we're already on this page, unless we're initialising the window
            if (page == page_number && !widgets.empty())
                return;

            _windowInformationSize.reset();

            page = page_number;
            currentFrame = 0;
            numListItems = 0;
            selectedListItem = -1;

            setWidgets(window_multiplayer_page_widgets[page]);
            widgets[WIDX_TITLE].text = WindowMultiplayerPageTitles[page];
            setWidgetPressed(WIDX_TAB1 + page, true);

            onResize();
            onPrepareDraw();
            initScrollWidgets();
            invalidate();
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            switch (widgetIndex)
            {
                case WIDX_CLOSE:
                    close();
                    break;
                case WIDX_TAB1:
                case WIDX_TAB2:
                case WIDX_TAB3:
                case WIDX_TAB4:
                    if (page != widgetIndex - WIDX_TAB1)
                    {
                        setPage(widgetIndex - WIDX_TAB1);
                    }
                    break;
            }

            auto& gameState = getGameState();
            switch (page)
            {
                case WINDOW_MULTIPLAYER_PAGE_GROUPS:
                {
                    switch (widgetIndex)
                    {
                        case WIDX_ADD_GROUP:
                        {
                            auto networkModifyGroup = GameActions::NetworkModifyGroupAction(
                                GameActions::ModifyGroupType::addGroup);
                            GameActions::Execute(&networkModifyGroup, gameState);
                            break;
                        }
                        case WIDX_REMOVE_GROUP:
                        {
                            auto networkModifyGroup = GameActions::NetworkModifyGroupAction(
                                GameActions::ModifyGroupType::removeGroup, _selectedGroup);
                            GameActions::Execute(&networkModifyGroup, gameState);
                            break;
                        }
                        case WIDX_RENAME_GROUP:
                        {
                            int32_t groupIndex = Network::GetGroupIndex(_selectedGroup);
                            const utf8* groupName = Network::GetGroupName(groupIndex);
                            WindowTextInputRawOpen(
                                this, widgetIndex, STR_GROUP_NAME, STR_ENTER_NEW_NAME_FOR_THIS_GROUP, {}, groupName, 32);
                            break;
                        }
                    }
                    break;
                }
                case WINDOW_MULTIPLAYER_PAGE_OPTIONS:
                {
                    switch (widgetIndex)
                    {
                        case WIDX_LOG_CHAT_CHECKBOX:
                            Config::Get().network.logChat = !Config::Get().network.logChat;
                            Config::Save();
                            break;
                        case WIDX_LOG_SERVER_ACTIONS_CHECKBOX:
                            Config::Get().network.logServerActions = !Config::Get().network.logServerActions;
                            Config::Save();
                            break;
                        case WIDX_KNOWN_KEYS_ONLY_CHECKBOX:
                            Config::Get().network.knownKeysOnly = !Config::Get().network.knownKeysOnly;
                            Config::Save();
                            break;
                    }
                    break;
                }
            }
        }

        void onResize() override
        {
            switch (page)
            {
                case WINDOW_MULTIPLAYER_PAGE_INFORMATION:
                {
                    auto size = _windowInformationSize ? _windowInformationSize.value() : informationGetSize();
                    WindowSetResize(*this, size, size);
                    break;
                }
                case WINDOW_MULTIPLAYER_PAGE_PLAYERS:
                {
                    WindowSetResize(*this, { 420, 124 }, { 500, 450 });

                    numListItems = (IsServerPlayerInvisible() ? Network::GetNumVisiblePlayers() : Network::GetNumPlayers());

                    widgets[WIDX_HEADER_PING].right = width - 5;

                    selectedListItem = -1;
                    invalidate();
                    break;
                }
                case WINDOW_MULTIPLAYER_PAGE_GROUPS:
                {
                    WindowSetResize(*this, { 320, 200 }, { 320, 500 });

                    numListItems = Network::GetNumActions();

                    selectedListItem = -1;
                    invalidate();
                    break;
                }
                case WINDOW_MULTIPLAYER_PAGE_OPTIONS:
                {
                    WindowSetResize(*this, { 300, 100 }, { 300, 100 });
                    break;
                }
            }
        }

        void onUpdate() override
        {
            currentFrame++;
            invalidateWidget(WIDX_TAB1 + page);
        }

        void onPrepareDraw() override
        {
            switch (page)
            {
                case WINDOW_MULTIPLAYER_PAGE_INFORMATION:
                {
                    WindowAlignTabs(this, WIDX_TAB1, WIDX_TAB4);
                    break;
                }
                case WINDOW_MULTIPLAYER_PAGE_PLAYERS:
                {
                    widgets[WIDX_LIST].right = width - 4;
                    widgets[WIDX_LIST].bottom = height - 0x0F;
                    WindowAlignTabs(this, WIDX_TAB1, WIDX_TAB4);
                    break;
                }
                case WINDOW_MULTIPLAYER_PAGE_GROUPS:
                {
                    widgets[WIDX_PERMISSIONS_LIST].right = width - 4;
                    widgets[WIDX_PERMISSIONS_LIST].bottom = height - 0x0F;
                    WindowAlignTabs(this, WIDX_TAB1, WIDX_TAB4);

                    // select other group if one is removed
                    while (Network::GetGroupIndex(_selectedGroup) == -1 && _selectedGroup > 0)
                    {
                        _selectedGroup--;
                    }
                    break;
                }
                case WINDOW_MULTIPLAYER_PAGE_OPTIONS:
                {
                    WindowAlignTabs(this, WIDX_TAB1, WIDX_TAB4);

                    if (Network::GetMode() == Network::Mode::client)
                    {
                        widgets[WIDX_KNOWN_KEYS_ONLY_CHECKBOX].type = WidgetType::empty;
                    }

                    setCheckboxValue(WIDX_LOG_CHAT_CHECKBOX, Config::Get().network.logChat);
                    setCheckboxValue(WIDX_LOG_SERVER_ACTIONS_CHECKBOX, Config::Get().network.logServerActions);
                    setCheckboxValue(WIDX_KNOWN_KEYS_ONLY_CHECKBOX, Config::Get().network.knownKeysOnly);
                    break;
                }
            }
        }

        void onDraw(RenderTarget& rt) override
        {
            drawWidgets(rt);
            drawTabImages(rt);
            switch (page)
            {
                case WINDOW_MULTIPLAYER_PAGE_INFORMATION:
                {
                    informationPaint(rt);
                    break;
                }
                case WINDOW_MULTIPLAYER_PAGE_PLAYERS:
                {
                    playersPaint(rt);
                    break;
                }
                case WINDOW_MULTIPLAYER_PAGE_GROUPS:
                {
                    groupsPaint(rt);
                    break;
                }
            }
        }

        void onDropdown(WidgetIndex widgetIndex, int32_t selectedIndex) override
        {
            switch (page)
            {
                case WINDOW_MULTIPLAYER_PAGE_GROUPS:
                {
                    if (selectedIndex == -1)
                    {
                        return;
                    }

                    switch (widgetIndex)
                    {
                        case WIDX_DEFAULT_GROUP_DROPDOWN:
                        {
                            auto networkModifyGroup = GameActions::NetworkModifyGroupAction(
                                GameActions::ModifyGroupType::setDefault, Network::GetGroupID(selectedIndex));
                            GameActions::Execute(&networkModifyGroup, getGameState());
                            break;
                        }
                        case WIDX_SELECTED_GROUP_DROPDOWN:
                        {
                            _selectedGroup = Network::GetGroupID(selectedIndex);
                            break;
                        }
                    }
                    invalidate();
                    break;
                }
            }
        }

        void onTextInput(WidgetIndex widgetIndex, std::string_view text) override
        {
            switch (page)
            {
                case WINDOW_MULTIPLAYER_PAGE_GROUPS:
                {
                    if (widgetIndex != WIDX_RENAME_GROUP)
                        return;

                    if (text.empty())
                        return;

                    auto networkModifyGroup = GameActions::NetworkModifyGroupAction(
                        GameActions::ModifyGroupType::setName, _selectedGroup, std::string(text));
                    GameActions::Execute(&networkModifyGroup, getGameState());
                    break;
                }
            }
        }

        void onMouseDown(WidgetIndex widgetIndex) override
        {
            switch (page)
            {
                case WINDOW_MULTIPLAYER_PAGE_GROUPS:
                {
                    switch (widgetIndex)
                    {
                        case WIDX_DEFAULT_GROUP_DROPDOWN:
                        case WIDX_SELECTED_GROUP_DROPDOWN:
                            showGroupDropdown(widgetIndex);
                            break;
                    }
                    break;
                }
            }
        }

        ScreenSize onScrollGetSize(int32_t scrollIndex) override
        {
            ScreenSize screenSize{};
            switch (page)
            {
                case WINDOW_MULTIPLAYER_PAGE_PLAYERS:
                {
                    if (selectedListItem != -1)
                    {
                        selectedListItem = -1;
                        invalidate();
                    }

                    screenSize = { 0, Network::GetNumPlayers() * kScrollableRowHeight };
                    int32_t i = screenSize.height - widgets[WIDX_LIST].bottom + widgets[WIDX_LIST].top + 21;
                    if (i < 0)
                        i = 0;
                    if (i < scrolls[0].contentOffsetY)
                    {
                        scrolls[0].contentOffsetY = i;
                        invalidate();
                    }
                    break;
                }

                case WINDOW_MULTIPLAYER_PAGE_GROUPS:
                {
                    if (selectedListItem != -1)
                    {
                        selectedListItem = -1;
                        invalidate();
                    }

                    screenSize = { 0, Network::GetNumActions() * kScrollableRowHeight };
                    int32_t i = screenSize.height - widgets[WIDX_LIST].bottom + widgets[WIDX_LIST].top + 21;
                    if (i < 0)
                        i = 0;
                    if (i < scrolls[0].contentOffsetY)
                    {
                        scrolls[0].contentOffsetY = i;
                        invalidate();
                    }
                    break;
                }
            }
            return screenSize;
        }

        void onScrollMouseDown(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            switch (page)
            {
                case WINDOW_MULTIPLAYER_PAGE_PLAYERS:
                {
                    int32_t index = screenCoords.y / kScrollableRowHeight;
                    if (index >= numListItems)
                        return;

                    selectedListItem = index;
                    invalidate();

                    int32_t player = (IsServerPlayerInvisible() ? index + 1 : index);
                    PlayerOpen(Network::GetPlayerID(player));
                    break;
                }

                case WINDOW_MULTIPLAYER_PAGE_GROUPS:
                {
                    int32_t index = screenCoords.y / kScrollableRowHeight;
                    if (index >= numListItems)
                        return;

                    selectedListItem = index;
                    invalidate();

                    auto networkModifyGroup = GameActions::NetworkModifyGroupAction(
                        GameActions::ModifyGroupType::setPermissions, _selectedGroup, "", index,
                        GameActions::PermissionState::toggle);
                    GameActions::Execute(&networkModifyGroup, getGameState());
                    break;
                }
            }
        }

        void onScrollMouseOver(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            switch (page)
            {
                case WINDOW_MULTIPLAYER_PAGE_PLAYERS:
                case WINDOW_MULTIPLAYER_PAGE_GROUPS:
                {
                    int32_t index = screenCoords.y / kScrollableRowHeight;
                    if (index >= numListItems)
                        return;

                    selectedListItem = index;
                    invalidate();
                    break;
                }
            }
        }

        void onScrollDraw(int32_t scrollIndex, RenderTarget& rt) override
        {
            switch (page)
            {
                case WINDOW_MULTIPLAYER_PAGE_PLAYERS:
                    playersScrollPaint(scrollIndex, rt);
                    break;

                case WINDOW_MULTIPLAYER_PAGE_GROUPS:
                    groupsScrollPaint(scrollIndex, rt);
                    break;
            }
        }

        // ---- Accessible keyboard navigation ------------------------------------------------------
        // Modeled on the other multi-tab accessible windows (e.g. the Cheats window): item 0 is the
        // tab selector - Left/Right or Tab/Shift+Tab switch page - and items 1..N are the current
        // page's controls or list rows. Moving speaks "label, kind, value"; Left/Right adjusts a
        // control, Enter activates it. Group selectors open as real combo boxes, like every other
        // dropdown in the mod.

        // ---- graph accessibility recipe ----

        // Declare the current page's rows (or, while a group combo box's dropdown is open, that
        // list's items) fresh from live state. itemText() composes each row; interactive rows get a
        // behaviour, and rows carrying a value get a stateText that reads the result of Enter back.
        void BuildAccessGraph(Accessibility::Graph::GraphBuilder& b)
        {
            using namespace Accessibility::Graph;

            auto* windowMgr = GetWindowManager();
            if (_accessDropdownOpen && (windowMgr == nullptr || windowMgr->FindByClass(WindowClass::dropdown) == nullptr))
                _accessDropdownOpen = false;

            if (_accessDropdownOpen)
            {
                b.PushContext("Group", "menu");
                for (int32_t k = 0; k < gDropdown.numItems; k++)
                {
                    if (gDropdown.items[k].isSeparator())
                        continue;
                    NodeVtable vt;
                    vt.announcements.emplace_back(NodeAnnouncement::Static(gDropdown.items[k].text));
                    if (gDropdown.items[k].isChecked())
                        vt.announcements.emplace_back(NodeAnnouncement::Static("selected", AnnouncementKinds::kSelected));
                    vt.onActivate = [this, k]() { accessCommitDropdown(k); };
                    b.AddItem(ControlId::Structural("dd:" + std::to_string(k)), std::move(vt));
                }
                b.PopContext();
                return;
            }

            onPrepareDraw();
            b.PushContext(accessPageName(page));
            const int32_t count = accessItemCount();
            for (int32_t i = 0; i < count; i++)
            {
                NodeVtable vt;
                vt.announcements.emplace_back([this, i]() { return itemText(i); });
                vt.focusRect = [this, i]() -> std::optional<Accessibility::Graph::GraphRect> {
                    if (page == WINDOW_MULTIPLAYER_PAGE_INFORMATION)
                        return std::nullopt; // read-only text, no widget to box
                    const WidgetIndex wi = accessItemWidget(i);
                    if (wi == WIDX_BACKGROUND || wi >= widgets.size() || widgets[wi].type == WidgetType::empty)
                        return std::nullopt;
                    const auto& wd = widgets[wi];
                    return Accessibility::Graph::GraphRect{ windowPos.x + wd.left, windowPos.y + wd.top, wd.width() + 1,
                                                            wd.height() + 1 };
                };
                installItemBehavior(vt, i);
                b.AddItem(ControlId::Structural("mp:" + std::to_string(page) + ":" + std::to_string(i)), std::move(vt));
            }
            b.PopContext();
        }

        // Tab/Shift+Tab: cycle the four pages (Information / Players / Groups / Options).
        void AccessChangePage(int32_t delta)
        {
            setPage((page + delta + 4) % 4);
        }

        // Escape: close an open group dropdown first, returning focus to its combo box; otherwise let
        // the navigator close the window.
        bool AccessEscape()
        {
            if (!_accessDropdownOpen)
                return false;
            const int32_t owner = _accessDropdownOwner;
            accessCloseDropdown();
            accessSuggestFocus(owner);
            return true;
        }

    private:
        void installItemBehavior(Accessibility::Graph::NodeVtable& vt, int32_t i)
        {
            switch (page)
            {
                case WINDOW_MULTIPLAYER_PAGE_PLAYERS:
                    vt.onActivate = [this, i]() {
                        const int32_t player = IsServerPlayerInvisible() ? i + 1 : i;
                        if (player < 0 || player >= Network::GetNumPlayers())
                            return;
                        selectedListItem = i;
                        Accessibility::ScreenReaderSpeak("Opening " + std::string(Network::GetPlayerName(player)));
                        PlayerOpen(Network::GetPlayerID(player));
                    };
                    break;
                case WINDOW_MULTIPLAYER_PAGE_OPTIONS:
                    vt.onActivate = [this, i]() {
                        const auto opts = optionCheckboxes();
                        if (i >= 0 && i < static_cast<int32_t>(opts.size()))
                            onMouseUp(opts[i]);
                    };
                    vt.stateText = [this, i]() {
                        onPrepareDraw();
                        return valueText(i);
                    };
                    break;
                case WINDOW_MULTIPLAYER_PAGE_GROUPS:
                    if (i == 0 || i == 4)
                    {
                        const WidgetIndex chevron = (i == 0) ? WIDX_DEFAULT_GROUP_DROPDOWN : WIDX_SELECTED_GROUP_DROPDOWN;
                        vt.onActivate = [this, i, chevron]() { accessOpenDropdown(i, chevron); };
                    }
                    else if (i == 1)
                        vt.onActivate = [this]() {
                            onMouseUp(WIDX_ADD_GROUP);
                            Accessibility::ScreenReaderSpeak("Group added");
                        };
                    else if (i == 2)
                        vt.onActivate = [this]() {
                            onMouseUp(WIDX_REMOVE_GROUP);
                            Accessibility::ScreenReaderSpeak("Group removed");
                        };
                    else if (i == 3)
                        vt.onActivate = [this]() {
                            onMouseUp(WIDX_RENAME_GROUP);
                            Accessibility::ScreenReaderSpeak("Type a new group name, then press Enter.");
                        };
                    else
                    {
                        // Permission checkbox: toggle for the selected group; the host applies it now.
                        vt.onActivate = [this, i]() {
                            const int32_t actionIndex = i - kGroupControlCount;
                            auto action = GameActions::NetworkModifyGroupAction(
                                GameActions::ModifyGroupType::setPermissions, _selectedGroup, "", actionIndex,
                                GameActions::PermissionState::toggle);
                            GameActions::Execute(&action, getGameState());
                            invalidate();
                        };
                        vt.stateText = [this, i]() { return valueText(i); };
                    }
                    break;
                default:
                    break; // INFORMATION: read-only
            }
        }

        static const char* accessPageName(int32_t p)
        {
            static const char* kNames[] = { "Information", "Players", "Groups", "Options" };
            return (p >= 0 && p < 4) ? kNames[p] : "Multiplayer";
        }

        // Number of items on the current page, excluding the tab selector.
        int32_t accessItemCount() const
        {
            switch (page)
            {
                case WINDOW_MULTIPLAYER_PAGE_INFORMATION:
                    return static_cast<int32_t>(infoLines().size());
                case WINDOW_MULTIPLAYER_PAGE_PLAYERS:
                    return numListItems;
                case WINDOW_MULTIPLAYER_PAGE_GROUPS:
                    return kGroupControlCount + Network::GetNumActions();
                case WINDOW_MULTIPLAYER_PAGE_OPTIONS:
                    return static_cast<int32_t>(optionCheckboxes().size());
            }
            return 0;
        }

        // Ask the graph to land on a page row at its next render (used to return focus to the combo
        // box that owned a dropdown once the dropdown closes).
        void accessSuggestFocus(int32_t item)
        {
            Accessibility::Graph::GraphStateForClass(WindowClass::multiplayer).nextSuggestedMove
                = Accessibility::Graph::ControlId::Structural(
                    "mp:" + std::to_string(page) + ":" + std::to_string(item));
        }

        std::vector<std::string> infoLines() const
        {
            std::vector<std::string> lines;
            lines.push_back(Accessibility::JoinSpeech({ "Server name", Network::GetServerName() }));
            const auto& desc = Network::GetServerDescription();
            if (!desc.empty())
                lines.push_back(Accessibility::JoinSpeech({ "Description", desc }));
            const auto& pn = Network::GetServerProviderName();
            if (!pn.empty())
                lines.push_back(Accessibility::JoinSpeech({ "Provider", pn }));
            const auto& pe = Network::GetServerProviderEmail();
            if (!pe.empty())
                lines.push_back(Accessibility::JoinSpeech({ "Provider email", pe }));
            const auto& pw = Network::GetServerProviderWebsite();
            if (!pw.empty())
                lines.push_back(Accessibility::JoinSpeech({ "Provider website", pw }));
            return lines;
        }

        std::vector<WidgetIndex> optionCheckboxes() const
        {
            std::vector<WidgetIndex> opts{ WIDX_LOG_CHAT_CHECKBOX, WIDX_LOG_SERVER_ACTIONS_CHECKBOX };
            // The "known keys only" option is hidden for clients (see onPrepareDraw).
            if (Network::GetMode() != Network::Mode::client)
                opts.push_back(WIDX_KNOWN_KEYS_ONLY_CHECKBOX);
            return opts;
        }

        std::string groupName(uint8_t groupId) const
        {
            int32_t idx = Network::GetGroupIndex(groupId);
            return idx != -1 ? std::string(Network::GetGroupName(idx)) : std::string("none");
        }

        // "label, kind, value" for item i on the current page (spoken when moving onto it).
        std::string itemText(int32_t i)
        {
            switch (page)
            {
                case WINDOW_MULTIPLAYER_PAGE_INFORMATION:
                {
                    const auto lines = infoLines();
                    return (i >= 0 && i < static_cast<int32_t>(lines.size())) ? lines[i] : std::string{};
                }
                case WINDOW_MULTIPLAYER_PAGE_PLAYERS:
                    return playerText(i);
                case WINDOW_MULTIPLAYER_PAGE_GROUPS:
                    return groupsItemText(i);
                case WINDOW_MULTIPLAYER_PAGE_OPTIONS:
                {
                    const auto opts = optionCheckboxes();
                    if (i < 0 || i >= static_cast<int32_t>(opts.size()))
                        return {};
                    const auto wi = opts[i];
                    return Accessibility::JoinSpeech(
                        { FormatStringID(widgets[wi].text), "checkbox", isWidgetPressed(wi) ? "checked" : "unchecked" });
                }
            }
            return {};
        }

        // The value only, no label - spoken while adjusting or toggling item i.
        std::string valueText(int32_t i)
        {
            switch (page)
            {
                case WINDOW_MULTIPLAYER_PAGE_OPTIONS:
                {
                    const auto opts = optionCheckboxes();
                    if (i < 0 || i >= static_cast<int32_t>(opts.size()))
                        return {};
                    return isWidgetPressed(opts[i]) ? "checked" : "unchecked";
                }
                case WINDOW_MULTIPLAYER_PAGE_GROUPS:
                {
                    if (i == 0)
                        return groupName(Network::GetDefaultGroup());
                    if (i == 4)
                        return groupName(_selectedGroup);
                    const int32_t actionIndex = i - kGroupControlCount;
                    if (actionIndex < 0 || actionIndex >= Network::GetNumActions())
                        return {};
                    const int32_t groupIdx = Network::GetGroupIndex(_selectedGroup);
                    const bool allowed = groupIdx != -1
                        && Network::CanPerformAction(groupIdx, static_cast<Network::Permission>(actionIndex));
                    return allowed ? "allowed" : "not allowed";
                }
                default:
                    return {};
            }
        }

        std::string playerText(int32_t index)
        {
            const int32_t player = IsServerPlayerInvisible() ? index + 1 : index;
            if (player < 0 || player >= Network::GetNumPlayers())
                return {};
            Accessibility::SpeechBuilder sb;
            sb.add(Network::GetPlayerName(player));
            const int32_t group = Network::GetGroupIndex(Network::GetPlayerGroup(player));
            if (group != -1)
                sb.add(std::string("group ") + Network::GetGroupName(group));
            const int32_t lastAction = Network::GetPlayerLastAction(player, 2000);
            if (lastAction != -999)
                sb.add(std::string("last action ") + FormatStringID(Network::GetActionNameStringID(lastAction)));
            sb.add(std::to_string(Network::GetPlayerPing(player)) + " milliseconds ping");
            return sb.str();
        }

        std::string groupsItemText(int32_t i)
        {
            switch (i)
            {
                case 0:
                    return Accessibility::JoinSpeech(
                        { "Default group", "combo box", groupName(Network::GetDefaultGroup()) });
                case 1:
                    return "Add group, button";
                case 2:
                    return "Remove group, button";
                case 3:
                    return "Rename group, button";
                case 4:
                    return Accessibility::JoinSpeech({ "Selected group", "combo box", groupName(_selectedGroup) });
                default:
                    break;
            }
            const int32_t actionIndex = i - kGroupControlCount;
            if (actionIndex < 0 || actionIndex >= Network::GetNumActions())
                return {};
            const int32_t groupIdx = Network::GetGroupIndex(_selectedGroup);
            const bool allowed = groupIdx != -1
                && Network::CanPerformAction(groupIdx, static_cast<Network::Permission>(actionIndex));
            return Accessibility::JoinSpeech(
                { FormatStringID(Network::GetActionNameStringID(actionIndex)), "checkbox",
                  allowed ? "allowed" : "not allowed" });
        }

        // The widget backing item i on the current page, for the focus rectangle.
        WidgetIndex accessItemWidget(int32_t i) const
        {
            switch (page)
            {
                case WINDOW_MULTIPLAYER_PAGE_PLAYERS:
                    return WIDX_LIST;
                case WINDOW_MULTIPLAYER_PAGE_GROUPS:
                    switch (i)
                    {
                        case 0: return WIDX_DEFAULT_GROUP;
                        case 1: return WIDX_ADD_GROUP;
                        case 2: return WIDX_REMOVE_GROUP;
                        case 3: return WIDX_RENAME_GROUP;
                        case 4: return WIDX_SELECTED_GROUP;
                        default: return WIDX_PERMISSIONS_LIST;
                    }
                case WINDOW_MULTIPLAYER_PAGE_OPTIONS:
                {
                    const auto opts = optionCheckboxes();
                    return (i >= 0 && i < static_cast<int32_t>(opts.size())) ? opts[i] : WIDX_BACKGROUND;
                }
                default:
                    return WIDX_BACKGROUND;
            }
        }

        // ---- Group combo-box navigation, mirroring the shared dropdown pattern -------------------
        void accessCloseDropdown()
        {
            if (auto* windowMgr = GetWindowManager(); windowMgr != nullptr)
                windowMgr->CloseByClass(WindowClass::dropdown);
            _accessDropdownOpen = false;
        }

        void accessOpenDropdown(int32_t ownerItem, WidgetIndex chevronWidx)
        {
            onMouseDown(chevronWidx); // populates and shows gDropdown
            auto* windowMgr = GetWindowManager();
            if (windowMgr == nullptr || windowMgr->FindByClass(WindowClass::dropdown) == nullptr)
                return;
            _accessDropdownOpen = true;
            _accessDropdownChevron = chevronWidx;
            _accessDropdownOwner = ownerItem;
            // The graph rebuild now declares the dropdown's items; its differ announces the landing.
        }

        void accessCommitDropdown(int32_t idx)
        {
            const WidgetIndex chevron = _accessDropdownChevron;
            const int32_t owner = _accessDropdownOwner;
            const bool valid = idx >= 0 && idx < gDropdown.numItems && !gDropdown.items[idx].isSeparator();
            accessCloseDropdown();
            if (valid)
                onDropdown(chevron, idx);
            accessSuggestFocus(owner);
        }
    };

    WindowBase* MultiplayerOpen()
    {
        // Check if window is already open
        auto* windowMgr = GetWindowManager();
        auto* window = windowMgr->BringToFrontByClass(WindowClass::multiplayer);
        if (window == nullptr)
        {
            window = windowMgr->Create<MultiplayerWindow>(
                WindowClass::multiplayer, { 320, 144 },
                { WindowFlag::higherContrastOnPress, WindowFlag::resizable, WindowFlag::autoPosition });
        }

        return window;
    }

    // Register the multiplayer window with the graph accessibility navigator (called once at startup
    // via EnsureGraphScreensRegistered). From here on the graph owns this window class; the legacy
    // accessibility dispatcher stands down for it.
    void RegisterMultiplayerGraphScreen()
    {
        using namespace Accessibility::Graph;
        GraphScreen screen;
        screen.windowClass = WindowClass::multiplayer;
        screen.build = [](GraphBuilder& b, WindowBase& w) { static_cast<MultiplayerWindow&>(w).BuildAccessGraph(b); };
        screen.onTabKey = [](WindowBase& w, int32_t dir) {
            static_cast<MultiplayerWindow&>(w).AccessChangePage(dir);
            return true;
        };
        screen.onEscape = [](WindowBase& w) { return static_cast<MultiplayerWindow&>(w).AccessEscape(); };
        RegisterGraphScreen(std::move(screen));
    }
} // namespace OpenRCT2::Ui::Windows
