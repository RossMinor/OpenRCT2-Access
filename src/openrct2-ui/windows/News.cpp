/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <algorithm>
#include <iterator>
#include <openrct2-ui/accessibility/MapNavigation.h>
#include <openrct2-ui/accessibility/ScreenReader.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/Context.h>
#include <openrct2/GameState.h>
#include <openrct2/SpriteIds.h>
#include <openrct2/audio/Audio.h>
#include <openrct2/config/Config.h>
#include <openrct2/drawing/ColourMap.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Rectangle.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/entity/EntityRegistry.h>
#include <openrct2/entity/Peep.h>
#include <openrct2/entity/Staff.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/localisation/Localisation.Date.h>
#include <openrct2/management/NewsItem.h>
#include <openrct2/object/ObjectManager.h>
#include <openrct2/object/PeepAnimationsObject.h>
#include <openrct2/ui/WindowManager.h>

using namespace OpenRCT2::Drawing;

namespace OpenRCT2::Ui::Windows
{
    static constexpr ScreenSize kWindowSize = { 400, 300 };
    static constexpr uint8_t kItemSeparatorHeight = 2;

    enum WindowNewsWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_TAB_BACKGROUND,
        WIDX_TAB_NEWS,
        WIDX_TAB_OPTIONS,
        WIDX_TAB_CONTENT,

        WIDX_SCROLL = WIDX_TAB_CONTENT,

        WIDX_CHECKBOX_0 = WIDX_TAB_CONTENT,
    };

    // clang-format off
    enum NewsWindowTab : uint8_t
    {
        newsTab,
        optionsTab,
    };

    static constexpr auto makeNewsWidgets = [](StringId title) {
        return makeWidgets(
            makeWindowShim(title, kWindowSize),
            makeWidget({  0, 43 }, { kWindowSize.width, 257 }, WidgetType::resize, WindowColour::secondary),
            makeTab   ({  3, 17 }, STR_RECENT_MESSAGES),
            makeTab   ({ 34, 17 }, STR_NOTIFICATION_SETTINGS)
        );
    };

    static constexpr auto kNewsTabWidgets = makeWidgets(
        makeNewsWidgets(STR_RECENT_MESSAGES),
        makeWidget({  4, 44 }, { 392, 252 }, WidgetType::scroll,  WindowColour::secondary, SCROLL_VERTICAL)
    );

    static constexpr auto kOptionsTabWidgets = makeWidgets(
        makeNewsWidgets(STR_NOTIFICATION_SETTINGS),
        makeWidget({ 10, 49 }, { 380,  14 }, WidgetType::checkbox, WindowColour::secondary)
    );
    // clang-format on

    struct NewsOption
    {
        StringId group;
        StringId caption;
        size_t configOffset;
    };

    // clang-format off
    static constexpr NewsOption kNewsItemOptionDefinitions[] = {
        { STR_NEWS_GROUP_PARK,  STR_NOTIFICATION_PARK_AWARD,                        offsetof(Config::Notification, parkAward)                     },
        { STR_NEWS_GROUP_PARK,  STR_NOTIFICATION_PARK_MARKETING_CAMPAIGN_FINISHED,  offsetof(Config::Notification, parkMarketingCampaignFinished) },
        { STR_NEWS_GROUP_PARK,  STR_NOTIFICATION_PARK_WARNINGS,                     offsetof(Config::Notification, parkWarnings)                  },
        { STR_NEWS_GROUP_PARK,  STR_NOTIFICATION_PARK_RATING_WARNINGS,              offsetof(Config::Notification, parkRatingWarnings)            },
        { STR_NEWS_GROUP_RIDE,  STR_NOTIFICATION_RIDE_BROKEN_DOWN,                  offsetof(Config::Notification, rideBrokenDown)                },
        { STR_NEWS_GROUP_RIDE,  STR_NOTIFICATION_RIDE_CRASHED,                      offsetof(Config::Notification, rideCrashed)                   },
        { STR_NEWS_GROUP_RIDE,  STR_NOTIFICATION_RIDE_CASUALTIES,                   offsetof(Config::Notification, rideCasualties)                },
        { STR_NEWS_GROUP_RIDE,  STR_NOTIFICATION_RIDE_WARNINGS,                     offsetof(Config::Notification, rideWarnings)                  },
        { STR_NEWS_GROUP_RIDE,  STR_NOTIFICATION_RIDE_RESEARCHED,                   offsetof(Config::Notification, rideResearched)                },
        { STR_NEWS_GROUP_RIDE,  STR_NOTIFICATION_RIDE_VEHICLE_STALLED,              offsetof(Config::Notification, rideStalledVehicles)           },
        { STR_NEWS_GROUP_GUEST, STR_NOTIFICATION_GUEST_WARNINGS,                    offsetof(Config::Notification, guestWarnings)                 },
        { STR_NEWS_GROUP_GUEST, STR_NOTIFICATION_GUEST_LEFT_PARK,                   offsetof(Config::Notification, guestLeftPark)                 },
        { STR_NEWS_GROUP_GUEST, STR_NOTIFICATION_GUEST_QUEUING_FOR_RIDE,            offsetof(Config::Notification, guestQueuingForRide)           },
        { STR_NEWS_GROUP_GUEST, STR_NOTIFICATION_GUEST_ON_RIDE,                     offsetof(Config::Notification, guestOnRide)                   },
        { STR_NEWS_GROUP_GUEST, STR_NOTIFICATION_GUEST_LEFT_RIDE,                   offsetof(Config::Notification, guestLeftRide)                 },
        { STR_NEWS_GROUP_GUEST, STR_NOTIFICATION_GUEST_BOUGHT_ITEM,                 offsetof(Config::Notification, guestBoughtItem)               },
        { STR_NEWS_GROUP_GUEST, STR_NOTIFICATION_GUEST_USED_FACILITY,               offsetof(Config::Notification, guestUsedFacility)             },
        { STR_NEWS_GROUP_GUEST, STR_NOTIFICATION_GUEST_DIED,                        offsetof(Config::Notification, guestDied)                     },
    };
    // clang-format on

    class NewsWindow final : public Window
    {
    private:
        int32_t _pressedNewsItemIndex{}, _pressedButtonIndex{}, _suspendUpdateTicks{};
        WidgetIndex _baseCheckboxIndex;

        // Accessible-navigation cursors: position within the archived message list, and
        // within the notification-options checkbox list.
        int32_t _accessNewsCursor = -1;
        int32_t _accessOptionCursor = -1;

        static int32_t CalculateNewsItemHeight()
        {
            return 4 * FontGetLineHeight(FontStyle::small) + 2;
        }

        void initNewsWidgets()
        {
            invalidate();
            page = newsTab;
            height = kWindowSize.height;
            setWidgets(kNewsTabWidgets);

            WindowInitScrollWidgets(*this);
            _pressedNewsItemIndex = -1;

            auto& widget = widgets[WIDX_SCROLL];
            ScreenSize scrollSize = onScrollGetSize(0);
            scrolls[0].contentOffsetY = std::max(0, scrollSize.height - (widget.height() - 2));
            widgetScrollUpdateThumbs(*this, WIDX_SCROLL);
        }

        void initOptionsWidgets()
        {
            invalidate();
            page = optionsTab;

            widgets.clear();
            widgets.insert(widgets.end(), kOptionsTabWidgets.begin(), kOptionsTabWidgets.end());

            // Collect widgets to insert at the end
            std::vector<Widget> groupWidgetsToInsert{};
            std::vector<Widget> checkboxWidgetsToInsert{};

            // Collect existing checkbox template, then remove it from the window
            const auto& baseCheckBox = kOptionsTabWidgets[WIDX_CHECKBOX_0];
            int16_t y = baseCheckBox.top;
            widgets.pop_back();

            StringId lastGroup = kStringIdNone;
            uint16_t numGroupElements = 0;

            for (const auto& def : kNewsItemOptionDefinitions)
            {
                // Create group elements as needed
                if (def.group != lastGroup)
                {
                    // Fit previous group to its contents
                    if (!groupWidgetsToInsert.empty())
                    {
                        auto& prevGroup = groupWidgetsToInsert.back();
                        prevGroup.bottom += numGroupElements * (kListRowHeight + 5) + 2;
                        numGroupElements = 0;
                        y += 7;
                    }

                    Widget groupWidget = {
                        .type = WidgetType::groupbox,
                        .colour = EnumValue(colours[1].colour),
                        .left = static_cast<int16_t>(baseCheckBox.left - 5),
                        .right = static_cast<int16_t>(baseCheckBox.right + 5),
                        .top = y,
                        .bottom = static_cast<int16_t>(y + kListRowHeight),
                        .text = def.group,
                    };

                    groupWidgetsToInsert.emplace_back(groupWidget);
                    lastGroup = def.group;
                    y += groupWidget.height() - 1;
                }

                // Create checkbox widgets
                Widget checkboxWidget = {
                    .type = WidgetType::checkbox,
                    .colour = EnumValue(colours[1].colour),
                    .left = baseCheckBox.left,
                    .right = baseCheckBox.right,
                    .top = y,
                    .bottom = static_cast<int16_t>(y + kListRowHeight + 3),
                    .text = def.caption,
                };

                checkboxWidgetsToInsert.emplace_back(checkboxWidget);
                numGroupElements++;
                y += kListRowHeight + 5;
            }

            // Fit final group to its contents
            if (!groupWidgetsToInsert.empty())
            {
                auto& prevGroup = groupWidgetsToInsert.back();
                prevGroup.bottom += numGroupElements * (kListRowHeight + 5) + 2;
                numGroupElements = 0;
            }

            for (auto& widget : groupWidgetsToInsert)
            {
                widgets.emplace_back(widget);
            }

            _baseCheckboxIndex = static_cast<WidgetIndex>(WIDX_CHECKBOX_0 + groupWidgetsToInsert.size());

            for (auto& widget : checkboxWidgetsToInsert)
            {
                widgets.emplace_back(widget);
            }

            // Fit panel to all widget elements, plus padding
            y += 7;

            if (height != y)
            {
                invalidate();
                height = y;
                widgets[WIDX_BACKGROUND].bottom = y - 1;
                widgets[WIDX_TAB_BACKGROUND].bottom = y - 1;
                invalidate();
            }

            // We're not using SetWidgets, so invoke ResizeFrame manually
            resizeFrame();
        }

        bool& GetNotificationValueRef(const NewsOption& def)
        {
            bool& configValue = *reinterpret_cast<bool*>(
                reinterpret_cast<size_t>(&Config::Get().notifications) + def.configOffset);
            return configValue;
        }

        void setPage(NewsWindowTab newPage)
        {
            if (page == newPage && !widgets.empty())
                return;

            switch (newPage)
            {
                case newsTab:
                    initNewsWidgets();
                    break;

                case optionsTab:
                    initOptionsWidgets();
                    break;
            }

            setWidgetPressed(WIDX_TAB_NEWS, page == newsTab);
            setWidgetPressed(WIDX_TAB_OPTIONS, page == optionsTab);
        }

        void DrawTabImages(RenderTarget& rt)
        {
            if (!isWidgetDisabled(WIDX_TAB_NEWS))
            {
                auto imageId = ImageId(SPR_G2_TAB_NEWS);
                auto& widget = widgets[WIDX_TAB_NEWS];
                GfxDrawSprite(rt, imageId, windowPos + ScreenCoordsXY{ widget.left + 3, widget.top });
            }

            if (!isWidgetDisabled(WIDX_TAB_OPTIONS))
            {
                auto imageId = ImageId(SPR_TAB_GEARS_0);
                if (page == optionsTab)
                    imageId = imageId.WithIndexOffset((currentFrame / 2) % 4);

                auto& widget = widgets[WIDX_TAB_OPTIONS];
                GfxDrawSprite(rt, imageId, windowPos + ScreenCoordsXY{ widget.left, widget.top });
            }
        }

    public:
        void onOpen() override
        {
            setPage(newsTab);
        }

#pragma region Accessibility

        // Spoken summary for the currently-selected message (News tab).
        void announceNewsItem()
        {
            const auto& archived = getGameState().newsItems.getArchived();
            if (_accessNewsCursor < 0 || _accessNewsCursor >= static_cast<int32_t>(archived.size()))
            {
                Accessibility::ScreenReaderSpeak("No messages");
                return;
            }

            const auto& item = archived[_accessNewsCursor];
            std::string text = OpenRCT2::FormatStringID(
                STR_NEWS_DATE_FORMAT, DateDayNames[item.day - 1], DateGameMonthNames[DateGetMonth(item.monthYear)]);
            text += ", " + item.text;
            if (item.typeHasLocation())
                text += ". Press Enter to view location";
            else if (item.typeHasSubject())
                text += ". Press Enter for details";
            Accessibility::ScreenReaderSpeakItem(text, _accessNewsCursor, static_cast<int32_t>(archived.size()));
        }

        // Spoken summary for the currently-selected notification option (Options tab).
        void announceOptionItem()
        {
            const auto count = static_cast<int32_t>(std::size(kNewsItemOptionDefinitions));
            if (_accessOptionCursor < 0 || _accessOptionCursor >= count)
                return;

            const auto& def = kNewsItemOptionDefinitions[_accessOptionCursor];
            std::string text = OpenRCT2::FormatStringID(def.caption);
            text += GetNotificationValueRef(def) ? ", checked" : ", unchecked";
            Accessibility::ScreenReaderSpeakItem(text, _accessOptionCursor, count);
        }

        std::optional<ScreenRect> getAccessibilityFocusRect() override
        {
            const WidgetIndex w = WIDX_TAB_NEWS + page; // newsTab/optionsTab map to the two tab widgets
            if (w >= widgets.size() || widgets[w].type == WidgetType::empty)
                return std::nullopt;
            const auto& wd = widgets[w];
            return ScreenRect{ windowPos + ScreenCoordsXY{ wd.left, wd.top },
                               windowPos + ScreenCoordsXY{ wd.right, wd.bottom } };
        }

        bool onAccessibilityAction(AccessibilityAction action) override
        {
            switch (action)
            {
                case AccessibilityAction::moveLeft:
                case AccessibilityAction::moveRight:
                {
                    const auto newPage = (page == newsTab) ? optionsTab : newsTab;
                    setPage(newPage);
                    if (newPage == newsTab)
                    {
                        const auto count = getGameState().newsItems.getArchived().size();
                        _accessNewsCursor = -1;
                        Accessibility::ScreenReaderSpeak(
                            "Recent messages, "
                            + (count == 0 ? std::string("no messages")
                                          : std::to_string(count) + (count == 1 ? " message" : " messages")));
                    }
                    else
                    {
                        // Land on the first option so its state is read immediately.
                        _accessOptionCursor = 0;
                        const auto& def = kNewsItemOptionDefinitions[0];
                        std::string text = "Notification settings, " + OpenRCT2::FormatStringID(def.caption);
                        text += GetNotificationValueRef(def) ? ", checked" : ", unchecked";
                        Accessibility::ScreenReaderSpeak(text);
                    }
                    return true;
                }

                case AccessibilityAction::moveUp:
                case AccessibilityAction::moveDown:
                {
                    if (page == newsTab)
                    {
                        const auto size = static_cast<int32_t>(getGameState().newsItems.getArchived().size());
                        if (size == 0)
                        {
                            Accessibility::ScreenReaderSpeak("No messages");
                            return true;
                        }
                        // List reads oldest-at-top to newest-at-bottom; start at the newest.
                        if (_accessNewsCursor < 0)
                            _accessNewsCursor = size - 1;
                        else if (action == AccessibilityAction::moveUp)
                            _accessNewsCursor = std::max(0, _accessNewsCursor - 1);
                        else
                            _accessNewsCursor = std::min(size - 1, _accessNewsCursor + 1);
                        announceNewsItem();
                    }
                    else
                    {
                        const auto count = static_cast<int32_t>(std::size(kNewsItemOptionDefinitions));
                        if (_accessOptionCursor < 0)
                            _accessOptionCursor = (action == AccessibilityAction::moveDown) ? 0 : count - 1;
                        else if (action == AccessibilityAction::moveDown)
                            _accessOptionCursor = (_accessOptionCursor + 1) % count;
                        else
                            _accessOptionCursor = (_accessOptionCursor - 1 + count) % count;
                        announceOptionItem();
                    }
                    return true;
                }

                case AccessibilityAction::activate:
                {
                    if (page == newsTab)
                    {
                        const auto& archived = getGameState().newsItems.getArchived();
                        if (_accessNewsCursor < 0 || _accessNewsCursor >= static_cast<int32_t>(archived.size()))
                            return true;
                        const auto& item = archived[_accessNewsCursor];
                        if (item.typeHasLocation())
                        {
                            auto loc = News::GetSubjectLocation(item.type, item.assoc);
                            auto* mainWindow = WindowGetMain();
                            if (loc.has_value() && mainWindow != nullptr)
                            {
                                WindowScrollToLocation(*mainWindow, loc.value());
                                Accessibility::ScreenReaderSpeak("Showing location");
                            }
                        }
                        else if (item.typeHasSubject())
                        {
                            News::OpenSubject(item.type, item.assoc);
                            Accessibility::ScreenReaderSpeak("Opening details");
                        }
                    }
                    else
                    {
                        const auto count = static_cast<int32_t>(std::size(kNewsItemOptionDefinitions));
                        if (_accessOptionCursor < 0 || _accessOptionCursor >= count)
                            return true;
                        const auto& def = kNewsItemOptionDefinitions[_accessOptionCursor];
                        bool& configValue = GetNotificationValueRef(def);
                        configValue = !configValue;
                        Config::Save();
                        invalidate();
                        announceOptionItem();
                    }
                    return true;
                }

                case AccessibilityAction::cancel:
                    close();
                    Accessibility::ReannounceToolbarItemIfMenuMode();
                    return true;

                default:
                    return false;
            }
        }

#pragma endregion

        void onPrepareDraw() override
        {
            if (page != optionsTab)
                return;

            uint16_t checkboxWidgetIndex = _baseCheckboxIndex;
            for (const auto& def : kNewsItemOptionDefinitions)
            {
                auto& widget = widgets[checkboxWidgetIndex];
                if (widget.type == WidgetType::groupbox)
                {
                    // Skip group box!
                    checkboxWidgetIndex++;
                    continue;
                }

                const bool& configValue = GetNotificationValueRef(def);
                setCheckboxValue(checkboxWidgetIndex, configValue);
                checkboxWidgetIndex++;
            }
        }

        void onDraw(RenderTarget& rt) override
        {
            drawWidgets(rt);
            DrawTabImages(rt);
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            switch (widgetIndex)
            {
                case WIDX_CLOSE:
                    close();
                    break;
                case WIDX_TAB_NEWS:
                    setPage(newsTab);
                    break;
                case WIDX_TAB_OPTIONS:
                    setPage(optionsTab);
                    break;
                default:
                {
                    if (page != optionsTab)
                        break;

                    int32_t checkBoxIndex = widgetIndex - _baseCheckboxIndex;
                    if (checkBoxIndex < 0)
                        break;

                    const auto& def = kNewsItemOptionDefinitions[checkBoxIndex];
                    bool& configValue = GetNotificationValueRef(def);
                    configValue = !configValue;
                    Config::Save();

                    invalidateWidget(widgetIndex);
                    break;
                }
            }
        }

        void onUpdateNews()
        {
            if (_pressedNewsItemIndex == -1 || --_suspendUpdateTicks != 0)
            {
                return;
            }

            invalidate();
            Audio::Play(Audio::SoundId::click2, 0, windowPos.x + (width / 2));

            size_t j = _pressedNewsItemIndex;
            _pressedNewsItemIndex = -1;
            auto& gameState = getGameState();

            if (j >= gameState.newsItems.getArchived().size())
            {
                return;
            }

            const auto& newsItem = gameState.newsItems.getArchived()[j];
            if (newsItem.hasButton())
            {
                return;
            }

            if (_pressedButtonIndex == 1)
            {
                News::OpenSubject(newsItem.type, newsItem.assoc);
            }
            else if (_pressedButtonIndex > 1)
            {
                static WindowBase* _mainWindow;
                auto subjectLoc = News::GetSubjectLocation(newsItem.type, newsItem.assoc);
                if (subjectLoc.has_value() && (_mainWindow = WindowGetMain()) != nullptr)
                {
                    WindowScrollToLocation(*_mainWindow, subjectLoc.value());
                }
            }
        }

        void onUpdateOptions()
        {
            currentFrame++;
            invalidateWidget(WIDX_TAB_OPTIONS);
        }

        void onUpdate() override
        {
            switch (page)
            {
                case newsTab:
                    onUpdateNews();
                    break;
                case optionsTab:
                    onUpdateOptions();
                    break;
            }
        }

        ScreenSize onScrollGetSize(int32_t scrollIndex) override
        {
            int32_t scrollHeight = static_cast<int32_t>(getGameState().newsItems.getArchived().size())
                    * CalculateNewsItemHeight()
                - kItemSeparatorHeight;
            return { kWindowSize.width, scrollHeight };
        }

        void onScrollMouseDown(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            int32_t itemHeight = CalculateNewsItemHeight();
            int32_t i = 0;
            int32_t buttonIndex = 0;
            auto mutableScreenCoords = screenCoords;
            for (const auto& newsItem : getGameState().newsItems.getArchived())
            {
                if (mutableScreenCoords.y < itemHeight)
                {
                    if (newsItem.hasButton() || mutableScreenCoords.y < 14 || mutableScreenCoords.y >= 38
                        || mutableScreenCoords.x < 328)
                    {
                        buttonIndex = 0;
                        break;
                    }
                    if (mutableScreenCoords.x < 351 && newsItem.typeHasSubject())
                    {
                        buttonIndex = 1;
                        break;
                    }
                    if (mutableScreenCoords.x < 376 && newsItem.typeHasLocation())
                    {
                        buttonIndex = 2;
                        break;
                    }
                }
                mutableScreenCoords.y -= itemHeight;
                i++;
            }

            if (buttonIndex != 0)
            {
                _pressedNewsItemIndex = i;
                _pressedButtonIndex = buttonIndex;
                _suspendUpdateTicks = 4;
                invalidate();
                Audio::Play(Audio::SoundId::click1, 0, windowPos.x + (width / 2));
            }
        }

        void onScrollDraw(int32_t scrollIndex, RenderTarget& rt) override
        {
            int32_t lineHeight = FontGetLineHeight(FontStyle::small);
            int32_t itemHeight = CalculateNewsItemHeight();
            int32_t y = 0;
            int32_t i = 0;

            const auto backgroundPaletteIndex = getColourMap(colours[3].colour).light;
            // Fill the scrollbar gap if no scrollbar is visible
            const bool scrollbarVisible = scrolls[0].contentHeight > widgets[WIDX_SCROLL].height() - 1;
            const auto scrollbarFill = scrollbarVisible ? 0 : kScrollBarWidth;

            for (const auto& newsItem : getGameState().newsItems.getArchived())
            {
                if (y >= rt.y + rt.height)
                    break;
                if (y + itemHeight < rt.y)
                {
                    y += itemHeight;
                    i++;
                    continue;
                }

                // Outer frame
                Rectangle::fillInset(
                    rt, { -1, y, 383 + scrollbarFill, y + itemHeight - 1 }, colours[1], Rectangle::BorderStyle::inset,
                    Rectangle::FillBrightness::light, Rectangle::FillMode::none);
                // Background
                Rectangle::fill(rt, { 0, y + 1, 381 + scrollbarFill, y + itemHeight - 2 }, backgroundPaletteIndex);

                // Date text
                {
                    auto ft = Formatter();
                    ft.Add<StringId>(DateDayNames[newsItem.day - 1]);
                    ft.Add<StringId>(DateGameMonthNames[DateGetMonth(newsItem.monthYear)]);
                    drawText(rt, { 2, y }, STR_NEWS_DATE_FORMAT, ft, { Drawing::Colour::white, FontStyle::small });
                }
                // Item text
                {
                    auto ft = Formatter();
                    ft.Add<const char*>(newsItem.text.c_str());
                    drawTextWrapped(
                        rt, { 2, y + lineHeight }, 325, STR_BOTTOM_TOOLBAR_NEWS_TEXT, ft,
                        { Drawing::Colour::brightGreen, FontStyle::small });
                }
                // Subject button
                if (newsItem.typeHasSubject() && !newsItem.hasButton())
                {
                    auto screenCoords = ScreenCoordsXY{ 328 + scrollbarFill, y + lineHeight + 4 };

                    auto borderStyle = Rectangle::BorderStyle::outset;
                    if (_pressedNewsItemIndex != -1)
                    {
                        News::IsValidIndex(_pressedNewsItemIndex + News::ItemHistoryStart);
                        if (i == _pressedNewsItemIndex && _pressedButtonIndex == 1)
                        {
                            borderStyle = Rectangle::BorderStyle::inset;
                        }
                    }
                    Rectangle::fillInset(
                        rt, { screenCoords, screenCoords + ScreenCoordsXY{ 23, 23 } }, colours[2], borderStyle);

                    switch (newsItem.type)
                    {
                        case News::ItemType::ride:
                            GfxDrawSprite(rt, ImageId(SPR_RIDE), screenCoords);
                            break;
                        case News::ItemType::peep:
                        case News::ItemType::peepOnRide:
                        {
                            RenderTarget clippedRT;
                            if (!ClipRenderTarget(clippedRT, rt, screenCoords + ScreenCoordsXY{ 1, 1 }, 22, 22))
                            {
                                break;
                            }

                            auto peep = getGameState().entities.TryGetEntity<Peep>(EntityId::FromUnderlying(newsItem.assoc));
                            if (peep == nullptr)
                            {
                                break;
                            }

                            auto clipCoords = ScreenCoordsXY{ 10, 19 };

                            // If normal peep set sprite to normal (no food)
                            // If staff set sprite to staff sprite
                            auto spriteType = PeepAnimationGroup::normal;
                            if (auto* staff = peep->as<Staff>(); staff != nullptr)
                            {
                                spriteType = staff->AnimationGroup;
                                if (staff->isEntertainer())
                                {
                                    clipCoords.y += 3;
                                }
                            }

                            auto& objManager = GetContext()->GetObjectManager();
                            auto* animObj = objManager.GetLoadedObject<PeepAnimationsObject>(peep->AnimationObjectIndex);

                            ImageIndex imageId = animObj->GetPeepAnimation(spriteType).baseImage + 1;
                            auto image = ImageId(imageId, peep->TshirtColour, peep->TrousersColour);
                            GfxDrawSprite(clippedRT, image, clipCoords);
                            break;
                        }
                        case News::ItemType::money:
                        case News::ItemType::campaign:
                            GfxDrawSprite(rt, ImageId(SPR_FINANCE), screenCoords);
                            break;
                        case News::ItemType::research:
                            GfxDrawSprite(rt, ImageId(newsItem.assoc < 0x10000 ? SPR_NEW_SCENERY : SPR_NEW_RIDE), screenCoords);
                            break;
                        case News::ItemType::peeps:
                            GfxDrawSprite(rt, ImageId(SPR_GUESTS), screenCoords);
                            break;
                        case News::ItemType::award:
                            GfxDrawSprite(rt, ImageId(SPR_AWARD), screenCoords);
                            break;
                        case News::ItemType::graph:
                            GfxDrawSprite(rt, ImageId(SPR_GRAPH), screenCoords);
                            break;
                        case News::ItemType::null:
                        case News::ItemType::blank:
                        case News::ItemType::count:
                            break;
                    }
                }

                // Location button
                if (newsItem.typeHasLocation() && !newsItem.hasButton())
                {
                    auto screenCoords = ScreenCoordsXY{ 352 + scrollbarFill, y + lineHeight + 4 };

                    auto borderStyle = Rectangle::BorderStyle::outset;
                    if (_pressedNewsItemIndex != -1)
                    {
                        News::IsValidIndex(_pressedNewsItemIndex + News::ItemHistoryStart);
                        if (i == _pressedNewsItemIndex && _pressedButtonIndex == 2)
                            borderStyle = Rectangle::BorderStyle::inset;
                    }
                    Rectangle::fillInset(
                        rt, { screenCoords, screenCoords + ScreenCoordsXY{ 23, 23 } }, colours[2], borderStyle);
                    GfxDrawSprite(rt, ImageId(SPR_LOCATE), screenCoords);
                }

                y += itemHeight;
                i++;
            }
        }
    };

    WindowBase* NewsOpen()
    {
        auto* windowMgr = GetWindowManager();
        return windowMgr->FocusOrCreate<NewsWindow>(WindowClass::recentNews, kWindowSize, {});
    }
} // namespace OpenRCT2::Ui::Windows
