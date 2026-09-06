/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include <openrct2-ui/accessibility/MapNavigation.h>
#include <openrct2-ui/accessibility/ScreenReader.h>
#include <openrct2-ui/accessibility/graph/GraphBuilder.h>
#include <openrct2-ui/accessibility/graph/GraphScreens.h>
#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/Graph.h>
#include <openrct2-ui/interface/Widget.h>
#include <openrct2-ui/interface/Window.h>
#include <openrct2-ui/windows/Windows.h>
#include <openrct2/GameState.h>
#include <openrct2/SpriteIds.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/park/ParkSetLoanAction.h>
#include <openrct2/actions/park/ParkSetResearchFundingAction.h>
#include <openrct2/drawing/ColourMap.h>
#include <openrct2/drawing/Drawing.String.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Rectangle.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/management/Finance.h>
#include <openrct2/management/Research.h>
#include <openrct2/ride/RideData.h>
#include <openrct2/ride/ShopItem.h>
#include <openrct2/ui/WindowManager.h>

namespace OpenRCT2::Ui::Windows
{
    using namespace OpenRCT2::Drawing;

    using Park::ParkData;

    enum
    {
        WINDOW_FINANCES_PAGE_SUMMARY,
        WINDOW_FINANCES_PAGE_FINANCIAL_GRAPH,
        WINDOW_FINANCES_PAGE_VALUE_GRAPH,
        WINDOW_FINANCES_PAGE_PROFIT_GRAPH,
        WINDOW_FINANCES_PAGE_MARKETING,
        WINDOW_FINANCES_PAGE_RESEARCH,
        WINDOW_FINANCES_PAGE_COUNT
    };

    enum WindowFinancesWidgetIdx : WidgetIndex
    {
        WIDX_BACKGROUND,
        WIDX_TITLE,
        WIDX_CLOSE,
        WIDX_PAGE_BACKGROUND,
        WIDX_TAB_1,
        WIDX_TAB_2,
        WIDX_TAB_3,
        WIDX_TAB_4,
        WIDX_TAB_5,
        WIDX_TAB_6,

        WIDX_TAB_CONTENT,

        WIDX_SUMMARY_SCROLL = WIDX_TAB_CONTENT,
        WIDX_LOAN,
        WIDX_LOAN_INCREASE,
        WIDX_LOAN_DECREASE,

        WIDX_ACTIVE_CAMPAIGNS_GROUP = WIDX_TAB_CONTENT,
        WIDX_CAMPAIGNS_AVAILABLE_GROUP,
        WIDX_CAMPAIGN_1,
        WIDX_CAMPAIGN_2,
        WIDX_CAMPAIGN_3,
        WIDX_CAMPAIGN_4,
        WIDX_CAMPAIGN_5,
        WIDX_CAMPAIGN_6,

        WIDX_RESEARCH_FUNDING_GROUP = WIDX_TAB_CONTENT,
        WIDX_RESEARCH_FUNDING,
        WIDX_RESEARCH_FUNDING_DROPDOWN_BUTTON,
        WIDX_RESEARCH_PRIORITIES_GROUP,
        WIDX_TRANSPORT_RIDES,
        WIDX_GENTLE_RIDES,
        WIDX_ROLLER_COASTERS,
        WIDX_THRILL_RIDES,
        WIDX_WATER_RIDES,
        WIDX_SHOPS_AND_STALLS,
        WIDX_SCENERY_AND_THEMING,
    };

#pragma region Measurements

    static constexpr ScreenSize kWindowSizeResearch = { 320, 207 };
    static constexpr ScreenSize kTabContentSizeResearch = kWindowSizeResearch - ScreenSize(0, kTabBarHeight);

    static constexpr ScreenSize kWindowSizeSummary = { 530, 309 };
    static constexpr ScreenSize kTabContentSizeSummary = kWindowSizeSummary - ScreenSize(0, kTabBarHeight);

    static constexpr ScreenSize kWindowSizeGraphsMarketing = { 530, 257 };
    static constexpr ScreenSize kTabContentSizeGraphsMarketing = kWindowSizeGraphsMarketing - ScreenSize(0, kTabBarHeight);

    static constexpr int32_t kCostPerWeekOffset = 321;

#pragma endregion

    // clang-format off
#pragma region Widgets

    static constexpr auto makeFinancesWidgets = [](StringId title, ScreenSize resizeSize, ScreenSize frameSize) {
        return makeWidgets(
            makeWindowShim(title, frameSize),
            makeWidget({   0, 43 }, resizeSize, WidgetType::resize, WindowColour::secondary),
            makeTab   ({   3, 17 }, STR_FINANCES_SHOW_SUMMARY_TAB_TIP),
            makeTab   ({  34, 17 }, STR_FINANCES_SHOW_CASH_TAB_TIP),
            makeTab   ({  65, 17 }, STR_FINANCES_SHOW_PARK_VALUE_TAB_TIP),
            makeTab   ({  96, 17 }, STR_FINANCES_SHOW_WEEKLY_PROFIT_TAB_TIP),
            makeTab   ({ 127, 17 }, STR_FINANCES_SHOW_MARKETING_TAB_TIP),
            makeTab   ({ 158, 17 }, STR_FINANCES_RESEARCH_TIP)
        );
    };

    static constexpr auto _windowFinancesSummaryWidgets = makeWidgets(
        makeFinancesWidgets(STR_FINANCIAL_SUMMARY, kTabContentSizeSummary, kWindowSizeSummary),
        makeWidget                ({130,  50}, {391, 211}, WidgetType::scroll,  WindowColour::secondary, SCROLL_HORIZONTAL              ),
        makeHoldableSpinnerWidgets({ 64, 277}, { 97,  14}, WidgetType::spinner, WindowColour::secondary                                 ) // NB: 3 widgets
    );

    static constexpr auto _windowFinancesCashWidgets = makeWidgets(
        makeFinancesWidgets(STR_FINANCIAL_GRAPH, kTabContentSizeGraphsMarketing, kWindowSizeGraphsMarketing)
    );

    static constexpr auto _windowFinancesParkValueWidgets = makeWidgets(
        makeFinancesWidgets(STR_PARK_VALUE_GRAPH, kTabContentSizeGraphsMarketing, kWindowSizeGraphsMarketing)
    );

    static constexpr auto _windowFinancesProfitWidgets = makeWidgets(
        makeFinancesWidgets(STR_PROFIT_GRAPH, kTabContentSizeGraphsMarketing, kWindowSizeGraphsMarketing)
    );

    static constexpr auto _windowFinancesMarketingWidgets = makeWidgets(
        makeFinancesWidgets(STR_MARKETING, kTabContentSizeGraphsMarketing, kWindowSizeGraphsMarketing),
        makeWidget({3, 47}, { kWindowSizeGraphsMarketing.width - 6,  45}, WidgetType::groupbox, WindowColour::tertiary , STR_MARKETING_CAMPAIGNS_IN_OPERATION                                   ),
        makeWidget({3, 47}, { kWindowSizeGraphsMarketing.width - 6, 206}, WidgetType::groupbox, WindowColour::tertiary , STR_MARKETING_CAMPAIGNS_AVAILABLE                                      ),
        makeWidget({8,  0}, {kWindowSizeGraphsMarketing.width - 16,  14}, WidgetType::imgBtn,   WindowColour::secondary, 0xFFFFFFFF,                           STR_START_THIS_MARKETING_CAMPAIGN),
        makeWidget({8,  0}, {kWindowSizeGraphsMarketing.width - 16,  14}, WidgetType::imgBtn,   WindowColour::secondary, 0xFFFFFFFF,                           STR_START_THIS_MARKETING_CAMPAIGN),
        makeWidget({8,  0}, {kWindowSizeGraphsMarketing.width - 16,  14}, WidgetType::imgBtn,   WindowColour::secondary, 0xFFFFFFFF,                           STR_START_THIS_MARKETING_CAMPAIGN),
        makeWidget({8,  0}, {kWindowSizeGraphsMarketing.width - 16,  14}, WidgetType::imgBtn,   WindowColour::secondary, 0xFFFFFFFF,                           STR_START_THIS_MARKETING_CAMPAIGN),
        makeWidget({8,  0}, {kWindowSizeGraphsMarketing.width - 16,  14}, WidgetType::imgBtn,   WindowColour::secondary, 0xFFFFFFFF,                           STR_START_THIS_MARKETING_CAMPAIGN),
        makeWidget({8,  0}, {kWindowSizeGraphsMarketing.width - 16,  14}, WidgetType::imgBtn,   WindowColour::secondary, 0xFFFFFFFF,                           STR_START_THIS_MARKETING_CAMPAIGN)
    );

    static constexpr auto _windowFinancesResearchWidgets = makeWidgets(
        makeFinancesWidgets(STR_RESEARCH_FUNDING, kTabContentSizeResearch, kWindowSizeResearch),
        makeWidget({  3,  47}, { kWindowSizeResearch.width - 6,  45}, WidgetType::groupbox,     WindowColour::tertiary, STR_RESEARCH_FUNDING_                                                             ),
        makeWidget({  8,  59}, {                           160,  14}, WidgetType::dropdownMenu, WindowColour::tertiary, 0xFFFFFFFF,                           STR_SELECT_LEVEL_OF_RESEARCH_AND_DEVELOPMENT),
        makeWidget({156,  60}, {                            11,  12}, WidgetType::button,       WindowColour::tertiary, STR_DROPDOWN_GLYPH,                   STR_SELECT_LEVEL_OF_RESEARCH_AND_DEVELOPMENT),
        makeWidget({  3,  96}, {kWindowSizeResearch.width -  6, 107}, WidgetType::groupbox,     WindowColour::tertiary, STR_RESEARCH_PRIORITIES                                                           ),
        makeWidget({  8, 108}, {kWindowSizeResearch.width - 14,  12}, WidgetType::checkbox,     WindowColour::tertiary, STR_RESEARCH_NEW_TRANSPORT_RIDES,     STR_RESEARCH_NEW_TRANSPORT_RIDES_TIP        ),
        makeWidget({  8, 121}, {kWindowSizeResearch.width - 14,  12}, WidgetType::checkbox,     WindowColour::tertiary, STR_RESEARCH_NEW_GENTLE_RIDES,        STR_RESEARCH_NEW_GENTLE_RIDES_TIP           ),
        makeWidget({  8, 134}, {kWindowSizeResearch.width - 14,  12}, WidgetType::checkbox,     WindowColour::tertiary, STR_RESEARCH_NEW_ROLLER_COASTERS,     STR_RESEARCH_NEW_ROLLER_COASTERS_TIP        ),
        makeWidget({  8, 147}, {kWindowSizeResearch.width - 14,  12}, WidgetType::checkbox,     WindowColour::tertiary, STR_RESEARCH_NEW_THRILL_RIDES,        STR_RESEARCH_NEW_THRILL_RIDES_TIP           ),
        makeWidget({  8, 160}, {kWindowSizeResearch.width - 14,  12}, WidgetType::checkbox,     WindowColour::tertiary, STR_RESEARCH_NEW_WATER_RIDES,         STR_RESEARCH_NEW_WATER_RIDES_TIP            ),
        makeWidget({  8, 173}, {kWindowSizeResearch.width - 14,  12}, WidgetType::checkbox,     WindowColour::tertiary, STR_RESEARCH_NEW_SHOPS_AND_STALLS,    STR_RESEARCH_NEW_SHOPS_AND_STALLS_TIP       ),
        makeWidget({  8, 186}, {kWindowSizeResearch.width - 14,  12}, WidgetType::checkbox,     WindowColour::tertiary, STR_RESEARCH_NEW_SCENERY_AND_THEMING, STR_RESEARCH_NEW_SCENERY_AND_THEMING_TIP    )
    );
    // clang-format on

    static constexpr std::span<const Widget> _windowFinancesPageWidgets[] = {
        _windowFinancesSummaryWidgets,   // WINDOW_FINANCES_PAGE_SUMMARY
        _windowFinancesCashWidgets,      // WINDOW_FINANCES_PAGE_FINANCIAL_GRAPH
        _windowFinancesParkValueWidgets, // WINDOW_FINANCES_PAGE_VALUE_GRAPH
        _windowFinancesProfitWidgets,    // WINDOW_FINANCES_PAGE_PROFIT_GRAPH
        _windowFinancesMarketingWidgets, // WINDOW_FINANCES_PAGE_MARKETING
        _windowFinancesResearchWidgets,  // WINDOW_FINANCES_PAGE_RESEARCH
    };
    static_assert(std::size(_windowFinancesPageWidgets) == WINDOW_FINANCES_PAGE_COUNT);

#pragma endregion

#pragma region Constants

    static constexpr StringId _windowFinancesSummaryRowLabels[EnumValue(ExpenditureType::count)] = {
        STR_FINANCES_SUMMARY_RIDE_CONSTRUCTION,
        STR_FINANCES_SUMMARY_RIDE_RUNNING_COSTS,
        STR_FINANCES_SUMMARY_LAND_PURCHASE,
        STR_FINANCES_SUMMARY_LANDSCAPING,
        STR_FINANCES_SUMMARY_PARK_ENTRANCE_TICKETS,
        STR_FINANCES_SUMMARY_RIDE_TICKETS,
        STR_FINANCES_SUMMARY_SHOP_SALES,
        STR_FINANCES_SUMMARY_SHOP_STOCK,
        STR_FINANCES_SUMMARY_FOOD_DRINK_SALES,
        STR_FINANCES_SUMMARY_FOOD_DRINK_STOCK,
        STR_FINANCES_SUMMARY_STAFF_WAGES,
        STR_FINANCES_SUMMARY_MARKETING,
        STR_FINANCES_SUMMARY_RESEARCH,
        STR_FINANCES_SUMMARY_LOAN_INTEREST,
    };

    static constexpr int32_t _windowFinancesTabAnimationFrames[] = {
        8,  // WINDOW_FINANCES_PAGE_SUMMARY
        16, // WINDOW_FINANCES_PAGE_FINANCIAL_GRAPH
        16, // WINDOW_FINANCES_PAGE_VALUE_GRAPH
        16, // WINDOW_FINANCES_PAGE_PROFIT_GRAPH
        19, // WINDOW_FINANCES_PAGE_MARKETING
        8,  // WINDOW_FINANCES_PAGE_RESEARCH
    };
    static_assert(std::size(_windowFinancesTabAnimationFrames) == WINDOW_FINANCES_PAGE_COUNT);

    static constexpr int32_t kExpenditureColumnWidth = 80;

    static constexpr ScreenCoordsXY kGraphTopLeftPadding{ 88, 20 };
    static constexpr ScreenCoordsXY kGraphBottomRightPadding{ 15, 18 };
    static constexpr uint8_t kGraphNumYLabels = 5;
    static constexpr int32_t kGraphNumPoints = 64;

#pragma endregion

    class FinancesWindow final : public Window
    {
    private:
        uint32_t _lastPaintedMonth = std::numeric_limits<uint32_t>::max();
        ScreenRect _graphBounds;
        Graph::GraphProperties<money64> _graphProps{};
        u8string _loanSpinnerText{};
        ParkData& _parkData;

        void SetDisabledTabs()
        {
            setWidgetDisabled(WIDX_TAB_5, _parkData.flags.has(ParkFlag::forbidMarketingCampaigns));
        }

    public:
        FinancesWindow(ParkData& parkData)
            : _parkData(parkData) {};

        void onOpen() override
        {
            setPage(WINDOW_FINANCES_PAGE_SUMMARY);
            _lastPaintedMonth = std::numeric_limits<uint32_t>::max();
            ResearchUpdateUncompletedTypes();
            _graphProps.hoverIdx = -1;
        }

        void onUpdate() override
        {
            currentFrame++;
            invalidateWidget(WIDX_TAB_1 + page);

            if (page == WINDOW_FINANCES_PAGE_VALUE_GRAPH || page == WINDOW_FINANCES_PAGE_PROFIT_GRAPH
                || page == WINDOW_FINANCES_PAGE_FINANCIAL_GRAPH)
            {
                if (_graphProps.UpdateHoverIndex())
                {
                    invalidateWidget(WIDX_BACKGROUND);
                }
            }
        }

        void onMouseDown(WidgetIndex widgetIndex) override
        {
            switch (page)
            {
                case WINDOW_FINANCES_PAGE_SUMMARY:
                    onMouseDownSummary(widgetIndex);
                    break;
                case WINDOW_FINANCES_PAGE_RESEARCH:
                    WindowResearchFundingMouseDown(this, widgetIndex, WIDX_RESEARCH_FUNDING);
                    break;
            }
        }

        void onMouseUp(WidgetIndex widgetIndex) override
        {
            switch (widgetIndex)
            {
                case WIDX_CLOSE:
                    close();
                    break;
                case WIDX_TAB_1:
                case WIDX_TAB_2:
                case WIDX_TAB_3:
                case WIDX_TAB_4:
                case WIDX_TAB_5:
                case WIDX_TAB_6:
                    setPage(widgetIndex - WIDX_TAB_1);
                    break;
                default:
                    switch (page)
                    {
                        case WINDOW_FINANCES_PAGE_MARKETING:
                            onMouseUpMarketing(widgetIndex);
                            break;
                        case WINDOW_FINANCES_PAGE_RESEARCH:
                            WindowResearchFundingMouseUp(widgetIndex, WIDX_RESEARCH_FUNDING);
                    }
                    break;
            }
        }

        std::string getAccessibilityPageSummary()
        {
            auto& gameState = getGameState();
            auto& park = gameState.park;
            const auto cash = [](money64 m) { return OpenRCT2::FormatStringID(STR_CURRENCY_FORMAT, m); };

            switch (page)
            {
                case WINDOW_FINANCES_PAGE_SUMMARY:
                    return "Overview: Cash " + cash(park.cash) + ", Loan " + cash(park.bankLoan) + ", Company value "
                        + cash(park.companyValue) + ", Weekly profit " + cash(park.currentProfit)
                        + ". Use up and down to review; on the Loan line, left and right adjust it";
                case WINDOW_FINANCES_PAGE_FINANCIAL_GRAPH:
                    return "Cash " + cash(park.cash);
                case WINDOW_FINANCES_PAGE_VALUE_GRAPH:
                    return "Park value " + cash(park.value);
                case WINDOW_FINANCES_PAGE_PROFIT_GRAPH:
                    return "Weekly profit " + cash(park.currentProfit);
                case WINDOW_FINANCES_PAGE_MARKETING:
                {
                    const auto count = park.marketingCampaigns.size();
                    const std::string hint = ". Use up and down to choose a campaign to start";
                    if (count == 0)
                        return "No active marketing campaigns" + hint;
                    return std::to_string(count)
                        + (count == 1 ? " active marketing campaign" : " active marketing campaigns") + hint;
                }
                case WINDOW_FINANCES_PAGE_RESEARCH:
                    // The funding level and each priority checkbox are navigable one at a time with
                    // up/down and toggled with Enter (see buildResearchItems), so the entry
                    // announcement is just a short header.
                    return "Research funding, "
                        + OpenRCT2::FormatStringID(kResearchFundingLevelNames[gameState.researchFundingLevel & 3])
                        + ". Use up and down to review and change funding and priorities";
            }
            return {};
        }

        // The Summary page's full expenditure / income table, one line per category for the current
        // month (every category, even zero ones, so nothing on screen is skipped), then the month
        // profit and the loan / interest / cash / value totals drawn beneath the table. Navigated
        // line by line with the up and down arrows.
        std::vector<std::string> buildSummaryLines()
        {
            auto& park = getGameState().park;
            const auto cash = [](money64 m) { return OpenRCT2::FormatStringID(STR_CURRENCY_FORMAT, m); };
            std::vector<std::string> lines;

            money64 profit = 0;
            for (int32_t j = 0; j < static_cast<int32_t>(ExpenditureType::count); j++)
            {
                const money64 v = _parkData.expenditureTable[0][j]; // index 0 = current month
                profit += v;
                lines.push_back(OpenRCT2::FormatStringID(_windowFinancesSummaryRowLabels[j]) + ", " + cash(v));
            }
            lines.push_back("Month profit, " + cash(profit));

            lines.push_back("Loan, " + cash(park.bankLoan) + ", use left and right to adjust");
            if (!_parkData.flags.has(ParkFlag::rct1Interest))
                lines.push_back(
                    "Interest rate, "
                    + OpenRCT2::FormatStringID(
                        STR_FINANCES_SUMMARY_AT_X_PER_YEAR, static_cast<uint16_t>(_parkData.bankLoanInterestRate)));
            lines.push_back("Cash, " + cash(park.cash));
            lines.push_back("Park value, " + cash(park.value));
            lines.push_back("Company value, " + cash(park.companyValue));
            lines.push_back("Weekly profit, " + cash(park.currentProfit));
            return lines;
        }

        // Index of the loan line inside buildSummaryLines(): the expenditure/income categories come
        // first, then the month-profit line, then the loan line.
        int32_t accessLoanLineIndex() const
        {
            return static_cast<int32_t>(EnumValue(ExpenditureType::count)) + 1;
        }

        // One navigable row on the Research page's funding controls. category -1 is the funding-level
        // row (Enter cycles None/Minimum/Normal/Maximum, matching the game's funding dropdown); 0..6
        // are the priority checkboxes (Enter toggles on/off, exactly as clicking the checkbox does).
        struct AxResearchItem
        {
            std::string label;
            int32_t category;
            bool enabled;
        };

        std::vector<AxResearchItem> buildResearchItems()
        {
            const auto& gameState = getGameState();
            std::vector<AxResearchItem> items;

            // Funding level (hidden in no-money scenarios and once all research is done).
            if (widgets[WIDX_RESEARCH_FUNDING].type != WidgetType::empty)
            {
                const int32_t level = gameState.researchFundingLevel & 3;
                items.push_back({ "Funding level, " + OpenRCT2::FormatStringID(kResearchFundingLevelNames[level]) + ", "
                                      + OpenRCT2::FormatStringID(STR_RESEARCH_COST_PER_MONTH, kResearchCosts[level]),
                                  -1, true });
            }

            static constexpr StringId kLabels[7] = {
                STR_RESEARCH_NEW_TRANSPORT_RIDES, STR_RESEARCH_NEW_GENTLE_RIDES,
                STR_RESEARCH_NEW_ROLLER_COASTERS, STR_RESEARCH_NEW_THRILL_RIDES,
                STR_RESEARCH_NEW_WATER_RIDES,     STR_RESEARCH_NEW_SHOPS_AND_STALLS,
                STR_RESEARCH_NEW_SCENERY_AND_THEMING,
            };
            for (int32_t i = 0; i < 7; i++)
            {
                const int32_t mask = 1 << i;
                const bool enabled = (gameState.researchUncompletedCategories & mask) != 0;
                const char* state = !enabled ? "fully researched"
                                             : ((gameState.researchPriorities & mask) ? "checked" : "unchecked");
                items.push_back(
                    { OpenRCT2::FormatStringID(kLabels[i]) + ", " + state + ", checkbox", i, enabled });
            }
            return items;
        }

        // Enter on a Research row. category -1 cycles the funding level (None -> Minimum -> Normal ->
        // Maximum -> None, the same game action the funding dropdown runs); 0..6 toggle the priority
        // checkbox. Both apply immediately in single player; the node's stateText re-reads the result.
        void accessToggleResearch(int32_t category)
        {
            if (category < 0)
            {
                const auto& gameState = getGameState();
                const int32_t newLevel = (gameState.researchFundingLevel + 1) & 3;
                auto action = GameActions::ParkSetResearchFundingAction(gameState.researchPriorities, newLevel);
                GameActions::Execute(&action, getGameState());
                return;
            }
            const int32_t mask = 1 << category;
            if ((getGameState().researchUncompletedCategories & mask) == 0)
                return; // fully researched, cannot be changed
            // Route through the window's own mouse-up handler, the exact path a checkbox click takes.
            onMouseUp(static_cast<WidgetIndex>(WIDX_TRANSPORT_RIDES + category));
        }

        // Short spoken feedback for a Research row after Enter: the funding level, or the checkbox
        // state, matching the other settings windows.
        std::string accessResearchState(int32_t category)
        {
            const auto& gameState = getGameState();
            if (category < 0)
                return OpenRCT2::FormatStringID(kResearchFundingLevelNames[gameState.researchFundingLevel & 3]);
            const int32_t mask = 1 << category;
            if ((gameState.researchUncompletedCategories & mask) == 0)
                return "fully researched";
            return (gameState.researchPriorities & mask) ? "checked" : "unchecked";
        }

        // Tab/Shift+Tab: cycle the visible finance pages via the window's own page switch (skipping
        // tabs hidden for the current scenario); the graph announces the new page's header + landing.
        void AccessChangePage(int32_t delta)
        {
            int32_t newPage = page;
            for (int32_t i = 0; i < WINDOW_FINANCES_PAGE_COUNT; i++)
            {
                newPage = (newPage + delta + WINDOW_FINANCES_PAGE_COUNT) % WINDOW_FINANCES_PAGE_COUNT;
                if (widgets[WIDX_TAB_1 + newPage].type != WidgetType::empty)
                    break;
            }
            setPage(newPage);
        }

        void accessAdjustLoan(bool borrow)
        {
            auto& park = getGameState().park;
            money64 newLoan;
            if (borrow)
            {
                if (park.bankLoan >= park.maxBankLoan)
                {
                    Accessibility::ScreenReaderSpeak("Loan is at the maximum");
                    return;
                }
                newLoan = std::min(park.maxBankLoan, park.bankLoan + 1000.00_GBP);
            }
            else
            {
                if (park.bankLoan == 0)
                {
                    Accessibility::ScreenReaderSpeak("Loan is already zero");
                    return;
                }
                newLoan = std::max(0.00_GBP, park.bankLoan - 1000.00_GBP);
            }
            // Applies a tick later, so read the new loan from the callback.
            auto action = GameActions::ParkSetLoanAction(newLoan);
            action.SetCallback([](const GameActions::GameAction*, const GameActions::Result* result) {
                if (result->error == GameActions::Status::ok)
                    Accessibility::ScreenReaderSpeak(
                        "Loan " + OpenRCT2::FormatStringID(STR_CURRENCY_FORMAT, getGameState().park.bankLoan));
            });
            GameActions::Execute(&action, getGameState());
        }

        std::string accessCampaignName(int32_t i)
        {
            // The campaign buttons draw their label directly (kMarketingCampaignNames), not via the
            // widget's text field, so read it from there and include the weekly cost - the same two
            // pieces of text a sighted player sees on the button.
            std::string name = OpenRCT2::FormatStringID(kMarketingCampaignNames[i][0]);
            if (name.empty())
                name = "Campaign";
            return name + ", "
                + OpenRCT2::FormatStringID(STR_MARKETING_PER_WEEK, AdvertisingCampaignPricePerWeek[i]);
        }

        // Screen-shaped focus rectangle for a widget (host tag for the visual focus box).
        std::optional<Accessibility::Graph::GraphRect> accessWidgetRect(WidgetIndex w)
        {
            if (w >= widgets.size() || widgets[w].type == WidgetType::empty)
                return std::nullopt;
            const auto& wd = widgets[w];
            return Accessibility::Graph::GraphRect{ windowPos.x + wd.left, windowPos.y + wd.top, wd.width() + 1,
                                                    wd.height() + 1 };
        }

        // ---- graph accessibility recipe ----

        // Declare the current page's navigable content fresh from live state. Each list page groups
        // its rows under the page header (the former per-tab summary) so a Tab switch reads the
        // header before the first row; the chart pages are a single info node reading their value.
        void BuildAccessGraph(Accessibility::Graph::GraphBuilder& b)
        {
            using namespace Accessibility::Graph;

            const WidgetIndex tab = static_cast<WidgetIndex>(WIDX_TAB_1 + page);

            switch (page)
            {
                case WINDOW_FINANCES_PAGE_SUMMARY:
                {
                    b.PushContext(getAccessibilityPageSummary());
                    const auto lines = buildSummaryLines();
                    const int32_t loanLine = accessLoanLineIndex();
                    for (int32_t i = 0; i < static_cast<int32_t>(lines.size()); i++)
                    {
                        NodeVtable vt;
                        vt.announcements.emplace_back(NodeAnnouncement::Static(lines[i]));
                        if (i == loanLine)
                        {
                            // Left/Right repay/borrow; the game-action callback speaks the new loan.
                            vt.onAdjust = [this](int32_t sign, bool) { accessAdjustLoan(sign > 0); };
                            vt.focusRect = [this]() { return accessWidgetRect(WIDX_LOAN); };
                        }
                        else
                        {
                            vt.focusRect = [this, tab]() { return accessWidgetRect(tab); };
                        }
                        b.AddItem(ControlId::Structural("fin:sum:" + std::to_string(i)), std::move(vt));
                    }
                    b.PopContext();
                    break;
                }
                case WINDOW_FINANCES_PAGE_MARKETING:
                {
                    b.PushContext(getAccessibilityPageSummary());
                    int32_t declared = 0;
                    for (int32_t i = 0; i < 6; i++)
                    {
                        if (widgets[WIDX_CAMPAIGN_1 + i].type == WidgetType::empty)
                            continue;
                        NodeVtable vt;
                        vt.announcements.emplace_back([this, i]() { return accessCampaignName(i); });
                        vt.announcements.emplace_back(NodeAnnouncement::Static("press Enter to set up"));
                        vt.onActivate = [this, i]() { onMouseUp(static_cast<WidgetIndex>(WIDX_CAMPAIGN_1 + i)); };
                        vt.focusRect = [this, i]() {
                            return accessWidgetRect(static_cast<WidgetIndex>(WIDX_CAMPAIGN_1 + i));
                        };
                        b.AddItem(ControlId::Structural("fin:mkt:" + std::to_string(i)), std::move(vt));
                        declared++;
                    }
                    if (declared == 0)
                    {
                        NodeVtable vt;
                        vt.announcements.emplace_back(NodeAnnouncement::Static("No campaigns available to start"));
                        vt.focusRect = [this, tab]() { return accessWidgetRect(tab); };
                        b.AddItem(ControlId::Structural("fin:mkt:none"), std::move(vt));
                    }
                    b.PopContext();
                    break;
                }
                case WINDOW_FINANCES_PAGE_RESEARCH:
                {
                    b.PushContext(getAccessibilityPageSummary());
                    for (const auto& it : buildResearchItems())
                    {
                        const int32_t cat = it.category;
                        NodeVtable vt;
                        vt.announcements.emplace_back(NodeAnnouncement::Static(it.label));
                        // Enter cycles the funding level / toggles the priority checkbox; the state
                        // text reads the result back.
                        vt.onActivate = [this, cat]() { accessToggleResearch(cat); };
                        vt.stateText = [this, cat]() { return accessResearchState(cat); };
                        vt.focusRect = [this, cat]() {
                            return accessWidgetRect(
                                cat < 0 ? WIDX_RESEARCH_FUNDING : static_cast<WidgetIndex>(WIDX_TRANSPORT_RIDES + cat));
                        };
                        b.AddItem(ControlId::Structural("fin:res:" + std::to_string(cat)), std::move(vt));
                    }
                    b.PopContext();
                    break;
                }
                default:
                {
                    // Chart pages (cash / value / profit graphs): one info node reading the value.
                    NodeVtable vt;
                    vt.announcements.emplace_back([this]() { return getAccessibilityPageSummary(); });
                    vt.focusRect = [this, tab]() { return accessWidgetRect(tab); };
                    b.AddItem(ControlId::Structural("fin:graph:" + std::to_string(page)), std::move(vt));
                    break;
                }
            }
        }

        void onDropdown(WidgetIndex widgetIndex, int32_t selectedIndex) override
        {
            if (page == WINDOW_FINANCES_PAGE_RESEARCH)
            {
                WindowResearchFundingDropdown(widgetIndex, selectedIndex, WIDX_RESEARCH_FUNDING);
            }
        }

        void onPrepareDraw() override
        {
            WindowAlignTabs(this, WIDX_TAB_1, WIDX_TAB_6);

            for (auto i = 0; i < WINDOW_FINANCES_PAGE_COUNT; i++)
                setWidgetPressed(WIDX_TAB_1 + i, false);
            setWidgetPressed(WIDX_TAB_1 + page, true);

            Widget* graphPageWidget;
            bool centredGraph;
            switch (page)
            {
                case WINDOW_FINANCES_PAGE_SUMMARY:
                    onPrepareDrawSummary();
                    return;
                case WINDOW_FINANCES_PAGE_MARKETING:
                    onPrepareDrawMarketing();
                    return;
                case WINDOW_FINANCES_PAGE_RESEARCH:
                    WindowResearchFundingPrepareDraw(this, WIDX_RESEARCH_FUNDING);
                    return;
                case WINDOW_FINANCES_PAGE_VALUE_GRAPH:
                    graphPageWidget = &widgets[WIDX_PAGE_BACKGROUND];
                    centredGraph = false;
                    _graphProps.series = _parkData.valueHistory;
                    break;
                case WINDOW_FINANCES_PAGE_PROFIT_GRAPH:
                    graphPageWidget = &widgets[WIDX_PAGE_BACKGROUND];
                    centredGraph = true;
                    _graphProps.series = _parkData.weeklyProfitHistory;
                    break;
                case WINDOW_FINANCES_PAGE_FINANCIAL_GRAPH:
                    graphPageWidget = &widgets[WIDX_PAGE_BACKGROUND];
                    centredGraph = true;
                    _graphProps.series = _parkData.cashHistory;
                    break;
                default:
                    return;
            }
            onPrepareDrawGraph(graphPageWidget, centredGraph);
        }

        void onDraw(RenderTarget& rt) override
        {
            drawWidgets(rt);
            DrawTabImages(rt);

            switch (page)
            {
                case WINDOW_FINANCES_PAGE_SUMMARY:
                    onDrawSummary(rt);
                    break;
                case WINDOW_FINANCES_PAGE_FINANCIAL_GRAPH:
                {
                    const auto cashLessLoan = _parkData.cash - _parkData.bankLoan;
                    const auto fmt = cashLessLoan >= 0 ? STR_FINANCES_FINANCIAL_GRAPH_CASH_LESS_LOAN_POSITIVE
                                                       : STR_FINANCES_FINANCIAL_GRAPH_CASH_LESS_LOAN_NEGATIVE;
                    onDrawGraph(rt, cashLessLoan, fmt);
                    break;
                }
                case WINDOW_FINANCES_PAGE_VALUE_GRAPH:
                    onDrawGraph(rt, getGameState().park.value, STR_FINANCES_PARK_VALUE);
                    break;
                case WINDOW_FINANCES_PAGE_PROFIT_GRAPH:
                {
                    const auto fmt = _parkData.currentProfit >= 0 ? STR_FINANCES_WEEKLY_PROFIT_POSITIVE
                                                                  : STR_FINANCES_WEEKLY_PROFIT_LOSS;
                    onDrawGraph(rt, _parkData.currentProfit, fmt);
                    break;
                }
                case WINDOW_FINANCES_PAGE_MARKETING:
                    onDrawMarketing(rt);
                    break;
                case WINDOW_FINANCES_PAGE_RESEARCH:
                    WindowResearchFundingDraw(this, rt);
                    break;
            }
        }

        ScreenSize onScrollGetSize(int32_t scrollIndex) override
        {
            if (page == WINDOW_FINANCES_PAGE_SUMMARY)
            {
                return { kExpenditureColumnWidth * (SummaryMaxAvailableMonth() + 1), 0 };
            }

            return {};
        }

        void onScrollDraw(int32_t scrollIndex, RenderTarget& rt) override
        {
            if (page != WINDOW_FINANCES_PAGE_SUMMARY)
                return;

            auto screenCoords = ScreenCoordsXY{ 0, kTableCellHeight + 2 };

            auto& self = widgets[WIDX_SUMMARY_SCROLL];
            int32_t row_width = std::max<uint16_t>(scrolls[0].contentWidth, self.width() - 1);

            // Expenditure / Income row labels
            for (int32_t i = 0; i < static_cast<int32_t>(ExpenditureType::count); i++)
            {
                // Darken every even row
                if (i % 2 == 0)
                    Rectangle::fill(
                        rt,
                        { screenCoords - ScreenCoordsXY{ 0, 1 },
                          screenCoords + ScreenCoordsXY{ row_width, (kTableCellHeight - 2) } },
                        getColourMap(colours[1].colour).lighter, true);

                screenCoords.y += kTableCellHeight;
            }

            // Expenditure / Income values for each month
            auto currentMonthYear = GetDate().GetMonthsElapsed();
            for (int32_t i = SummaryMaxAvailableMonth(); i >= 0; i--)
            {
                screenCoords.y = 0;

                uint16_t monthyear = currentMonthYear - i;

                // Month heading
                auto ft = Formatter();
                ft.Add<StringId>(STR_FINANCES_SUMMARY_MONTH_HEADING);
                ft.Add<uint16_t>(monthyear);
                drawText(
                    rt, screenCoords + ScreenCoordsXY{ kExpenditureColumnWidth, 0 },
                    monthyear == currentMonthYear ? STR_WINDOW_COLOUR_2_STRINGID : STR_BLACK_STRING, ft,
                    { { TextPaintFlag::underline }, TextAlignment::right });
                screenCoords.y += 14;

                // Month expenditures
                money64 profit = 0;
                for (int32_t j = 0; j < static_cast<int32_t>(ExpenditureType::count); j++)
                {
                    auto expenditure = _parkData.expenditureTable[i][j];
                    if (expenditure != 0)
                    {
                        profit += expenditure;
                        const StringId format = expenditure >= 0 ? STR_FINANCES_SUMMARY_INCOME_VALUE
                                                                 : STR_FINANCES_SUMMARY_EXPENDITURE_VALUE;
                        ft = Formatter();
                        ft.Add<money64>(expenditure);
                        drawText(
                            rt, screenCoords + ScreenCoordsXY{ kExpenditureColumnWidth, 0 }, format, ft,
                            { TextAlignment::right });
                    }
                    screenCoords.y += kTableCellHeight;
                }
                screenCoords.y += 4;

                // Month profit
                const StringId format = profit >= 0 ? STR_FINANCES_SUMMARY_INCOME_VALUE : STR_FINANCES_SUMMARY_LOSS_VALUE;
                ft = Formatter();
                ft.Add<money64>(profit);
                drawText(rt, screenCoords + ScreenCoordsXY{ kExpenditureColumnWidth, 0 }, format, ft, { TextAlignment::right });

                Rectangle::fill(
                    rt,
                    { screenCoords + ScreenCoordsXY{ 10, -2 }, screenCoords + ScreenCoordsXY{ kExpenditureColumnWidth, -2 } },
                    PaletteIndex::pi10);

                screenCoords.x += kExpenditureColumnWidth;
            }

            _lastPaintedMonth = currentMonthYear;
        }

        void setPage(int32_t p)
        {
            // Skip setting page if we're already on this page, unless we're initialising the window
            if (page == p && !widgets.empty())
                return;

            page = p;
            currentFrame = 0;

            invalidate();
            if (p == WINDOW_FINANCES_PAGE_RESEARCH)
            {
                width = kWindowSizeResearch.width;
                height = kWindowSizeResearch.height;
                flags.unset(WindowFlag::resizable);
            }
            else if (p == WINDOW_FINANCES_PAGE_SUMMARY)
            {
                width = kWindowSizeSummary.width;
                height = kWindowSizeSummary.height;
                flags.unset(WindowFlag::resizable);
            }
            else if (
                p == WINDOW_FINANCES_PAGE_VALUE_GRAPH || p == WINDOW_FINANCES_PAGE_PROFIT_GRAPH
                || p == WINDOW_FINANCES_PAGE_FINANCIAL_GRAPH)
            {
                flags |= WindowFlag::resizable;

                // We need to compensate for the enlarged title bar for windows that do not
                // constrain the window height between tabs (e.g. chart tabs)
                height -= getTitleBarDiffNormal();

                WindowSetResize(*this, kWindowSizeGraphsMarketing, kMaxWindowSize);
            }
            else
            {
                width = kWindowSizeGraphsMarketing.width;
                height = kWindowSizeGraphsMarketing.height;
                flags.unset(WindowFlag::resizable);
            }

            setWidgets(_windowFinancesPageWidgets[p]);
            SetDisabledTabs();

            widgetSetPressedExclusive(
                *this, { WIDX_TAB_1, WIDX_TAB_2, WIDX_TAB_3, WIDX_TAB_4, WIDX_TAB_5, WIDX_TAB_6 }, WIDX_TAB_1 + p);

            resizeFrame();
            onPrepareDraw();
            initScrollWidgets();

            // Scroll summary all the way to the right, initially.
            if (p == WINDOW_FINANCES_PAGE_SUMMARY)
                initialiseScrollPosition(WIDX_SUMMARY_SCROLL, 0);

            invalidate();
        }

#pragma region Summary Events

        void onMouseDownSummary(WidgetIndex widgetIndex)
        {
            switch (widgetIndex)
            {
                case WIDX_LOAN_INCREASE:
                {
                    // If loan can be increased, do so.
                    // If not, action shows error message.
                    auto newLoan = _parkData.bankLoan + 1000.00_GBP;
                    if (_parkData.bankLoan < _parkData.maxBankLoan)
                    {
                        newLoan = std::min(_parkData.maxBankLoan, newLoan);
                    }
                    auto gameAction = GameActions::ParkSetLoanAction(newLoan);
                    GameActions::Execute(&gameAction, getGameState());
                    break;
                }
                case WIDX_LOAN_DECREASE:
                {
                    // If loan is positive, decrease it.
                    // If loan is negative, action shows error message.
                    // If loan is exactly 0, prevent error message.
                    if (_parkData.bankLoan != 0)
                    {
                        auto newLoan = _parkData.bankLoan - 1000.00_GBP;
                        if (_parkData.bankLoan > 0)
                        {
                            newLoan = std::max(0.00_GBP, newLoan);
                        }
                        auto gameAction = GameActions::ParkSetLoanAction(newLoan);
                        GameActions::Execute(&gameAction, getGameState());
                    }
                    break;
                }
            }
        }

        void onPrepareDrawSummary()
        {
            _loanSpinnerText = FormatStringID(STR_CURRENCY_FORMAT, getGameState().park.bankLoan);
            widgets[WIDX_LOAN].setString(_loanSpinnerText.c_str());

            // Keep up with new months being added in the first two years.
            if (GetDate().GetMonthsElapsed() != _lastPaintedMonth)
                initialiseScrollPosition(WIDX_SUMMARY_SCROLL, 0);
        }

        void onDrawSummary(RenderTarget& rt)
        {
            auto titleBarBottom = widgets[WIDX_TITLE].bottom;
            auto screenCoords = windowPos + ScreenCoordsXY{ 8, titleBarBottom + 37 };
            auto& gameState = getGameState();

            // Expenditure / Income heading
            drawText(
                rt, screenCoords, STR_FINANCES_SUMMARY_EXPENDITURE_INCOME,
                { Drawing::Colour::black, { TextPaintFlag::underline }, TextAlignment::left });
            screenCoords.y += 14;

            // Expenditure / Income row labels
            for (int32_t i = 0; i < static_cast<int32_t>(ExpenditureType::count); i++)
            {
                // Darken every even row
                if (i % 2 == 0)
                    Rectangle::fill(
                        rt,
                        { screenCoords - ScreenCoordsXY{ 0, 1 }, screenCoords + ScreenCoordsXY{ 121, (kTableCellHeight - 2) } },
                        getColourMap(colours[1].colour).lighter, true);

                drawText(rt, screenCoords - ScreenCoordsXY{ 0, 1 }, _windowFinancesSummaryRowLabels[i]);
                screenCoords.y += kTableCellHeight;
            }

            // Horizontal rule below expenditure / income table
            Rectangle::fillInset(
                rt,
                { windowPos + ScreenCoordsXY{ 8, titleBarBottom + 258 },
                  windowPos + ScreenCoordsXY{ 8 + 513, titleBarBottom + 258 + 1 } },
                colours[1], Rectangle::BorderStyle::inset);

            // Loan and interest rate
            drawText(rt, windowPos + ScreenCoordsXY{ 8, titleBarBottom + 265 }, STR_FINANCES_SUMMARY_LOAN);
            if (!_parkData.flags.has(ParkFlag::rct1Interest))
            {
                auto ft = Formatter();
                ft.Add<uint16_t>(_parkData.bankLoanInterestRate);
                drawText(rt, windowPos + ScreenCoordsXY{ 167, titleBarBottom + 265 }, STR_FINANCES_SUMMARY_AT_X_PER_YEAR, ft);
            }

            // Current cash
            auto ft = Formatter();
            ft.Add<money64>(_parkData.cash);
            StringId stringId = _parkData.cash >= 0 ? STR_CASH_LABEL : STR_CASH_NEGATIVE_LABEL;
            drawText(rt, windowPos + ScreenCoordsXY{ 8, titleBarBottom + 280 }, stringId, ft);

            // Objective related financial information
            if (gameState.scenarioOptions.objective.Type == Scenario::ObjectiveType::monthlyFoodIncome)
            {
                auto lastMonthProfit = FinanceGetLastMonthShopProfit();
                ft = Formatter();
                ft.Add<money64>(lastMonthProfit);
                drawText(
                    rt, windowPos + ScreenCoordsXY{ 280, titleBarBottom + 265 },
                    STR_LAST_MONTH_PROFIT_FROM_FOOD_DRINK_MERCHANDISE_SALES_LABEL, ft);
            }
            else
            {
                // Park value and company value
                ft = Formatter();
                ft.Add<money64>(_parkData.value);
                drawText(rt, windowPos + ScreenCoordsXY{ 280, titleBarBottom + 265 }, STR_PARK_VALUE_LABEL, ft);
                ft = Formatter();
                ft.Add<money64>(_parkData.companyValue);
                drawText(rt, windowPos + ScreenCoordsXY{ 280, titleBarBottom + 280 }, STR_COMPANY_VALUE_LABEL, ft);
            }
        }

        uint16_t SummaryMaxAvailableMonth()
        {
            return std::min<uint16_t>(GetDate().GetMonthsElapsed(), kExpenditureTableMonthCount - 1);
        }

#pragma endregion

#pragma region Marketing Events

        void onMouseUpMarketing(WidgetIndex widgetIndex)
        {
            if (widgetIndex >= WIDX_CAMPAIGN_1 && widgetIndex <= WIDX_CAMPAIGN_6)
            {
                ContextOpenDetailWindow(WindowDetail::newCampaign, widgetIndex - WIDX_CAMPAIGN_1);
            }
        }

        void onPrepareDrawMarketing()
        {
            // Count number of active campaigns
            int32_t numActiveCampaigns = static_cast<int32_t>(getGameState().park.marketingCampaigns.size());
            int32_t y = widgets[WIDX_TAB_1].top + std::max(1, numActiveCampaigns) * kListRowHeight + 75;

            // Update group box positions
            widgets[WIDX_ACTIVE_CAMPAIGNS_GROUP].bottom = y - 22;
            widgets[WIDX_CAMPAIGNS_AVAILABLE_GROUP].top = y - 13;

            // Update new campaign button visibility
            y += 3;
            for (int32_t i = 0; i < ADVERTISING_CAMPAIGN_COUNT; i++)
            {
                auto& campaignButton = widgets[WIDX_CAMPAIGN_1 + i];
                auto* marketingCampaign = MarketingGetCampaign(i);
                if (marketingCampaign == nullptr && MarketingIsCampaignTypeApplicable(i))
                {
                    campaignButton.setVisible();
                    campaignButton.top = y;
                    campaignButton.bottom = y + kButtonFaceHeight;
                    y += kButtonFaceHeight + 1;
                }
                else
                {
                    campaignButton.setHidden();
                }
            }
        }

        void onDrawMarketing(RenderTarget& rt)
        {
            auto screenCoords = windowPos + ScreenCoordsXY{ 8, widgets[WIDX_TAB_1].top + 45 };
            int32_t noCampaignsActive = 1;
            for (int32_t i = 0; i < ADVERTISING_CAMPAIGN_COUNT; i++)
            {
                auto marketingCampaign = MarketingGetCampaign(i);
                if (marketingCampaign == nullptr)
                    continue;

                noCampaignsActive = 0;
                auto ft = Formatter();

                // Set special parameters
                switch (i)
                {
                    case ADVERTISING_CAMPAIGN_RIDE_FREE:
                    case ADVERTISING_CAMPAIGN_RIDE:
                    {
                        auto campaignRide = GetRide(marketingCampaign->rideId);
                        if (campaignRide != nullptr)
                        {
                            campaignRide->formatNameTo(ft);
                        }
                        else
                        {
                            ft.Add<StringId>(kStringIdNone);
                        }
                        break;
                    }
                    case ADVERTISING_CAMPAIGN_FOOD_OR_DRINK_FREE:
                        ft.Add<StringId>(GetShopItemDescriptor(marketingCampaign->shopItemType).Naming.Plural);
                        break;
                    default:
                    {
                        auto parkName = getGameState().park.name.c_str();
                        ft.Add<StringId>(STR_STRING);
                        ft.Add<const char*>(parkName);
                    }
                }
                // Advertisement
                drawTextEllipsised(rt, screenCoords + ScreenCoordsXY{ 4, 0 }, 296, kMarketingCampaignNames[i][1], ft);

                // Duration
                uint16_t weeksRemaining = marketingCampaign->weeksLeft;
                ft = Formatter();
                ft.Add<uint16_t>(weeksRemaining);
                drawText(
                    rt, screenCoords + ScreenCoordsXY{ 304, 0 },
                    weeksRemaining == 1 ? STR_1_WEEK_REMAINING : STR_X_WEEKS_REMAINING, ft);

                screenCoords.y += kListRowHeight;
            }

            if (noCampaignsActive)
            {
                drawText(rt, screenCoords + ScreenCoordsXY{ 4, 0 }, STR_MARKETING_CAMPAIGNS_NONE);
            }

            // Draw campaign button text
            for (int32_t i = 0; i < ADVERTISING_CAMPAIGN_COUNT; i++)
            {
                auto campaignButton = &widgets[WIDX_CAMPAIGN_1 + i];
                if (campaignButton->isVisible())
                {
                    // Draw button text
                    screenCoords = windowPos + ScreenCoordsXY{ campaignButton->left, campaignButton->textTop() };
                    drawText(rt, screenCoords + ScreenCoordsXY{ 4, 0 }, kMarketingCampaignNames[i][0]);
                    auto ft = Formatter();
                    ft.Add<money64>(AdvertisingCampaignPricePerWeek[i]);
                    drawText(rt, screenCoords + ScreenCoordsXY{ kCostPerWeekOffset, 0 }, STR_MARKETING_PER_WEEK, ft);
                }
            }
        }

#pragma endregion

#pragma region Graph Events

        void onDrawGraph(RenderTarget& rt, const money64 currentValue, const StringId fmt) const
        {
            Formatter ft;
            ft.Add<money64>(currentValue);
            drawText(rt, _graphBounds.Point1 - ScreenCoordsXY{ 0, 11 }, fmt, ft);

            // Graph
            Rectangle::fillInset(
                rt, _graphBounds, colours[1], Rectangle::BorderStyle::inset, Rectangle::FillBrightness::light,
                Rectangle::FillMode::none);
            // hide resize widget on graph area
            constexpr ScreenCoordsXY offset{ 1, 1 };
            constexpr ScreenCoordsXY bigOffset{ 5, 5 };
            Rectangle::fillInset(
                rt, { _graphBounds.Point2 - bigOffset, _graphBounds.Point2 - offset }, colours[1], Rectangle::BorderStyle::none,
                Rectangle::FillBrightness::light, Rectangle::FillMode::dontLightenWhenInset);

            Graph::DrawFinanceGraph(rt, _graphProps);
        }

        void onPrepareDrawGraph(const Widget* graphPageWidget, const bool centredGraph)
        {
            // Calculate Y axis max and min.
            money64 maxVal = 0;
            const auto series = _graphProps.series;
            for (int32_t i = 0; i < kGraphNumPoints; i++)
            {
                if (series[i] == kMoney64Undefined)
                    continue;
                auto val = std::abs(series[i]);
                if (val > maxVal)
                    maxVal = val;
            }
            // This algorithm increments the leading digit of the max and sets all other digits to zero.
            // e.g. 681 => 700.
            money64 oom = 10;
            while (maxVal / oom >= 10)
                oom *= 10;
            const money64 max = std::max(10.00_GBP, ((maxVal + oom - 1) / oom) * oom);

            _graphProps.min = centredGraph ? -max : 0.00_GBP;
            _graphProps.max = max;

            // dynamic padding for long axis labels:
            char buffer[64]{};
            FormatStringToBuffer(buffer, sizeof(buffer), "{BLACK}{CURRENCY2DP}", centredGraph ? -max : max);
            int32_t maxGraphWidth = getStringWidth(buffer, FontStyle::small) + Graph::kYTickMarkPadding + 1;
            const ScreenCoordsXY dynamicPadding{ std::max(maxGraphWidth, kGraphTopLeftPadding.x), kGraphTopLeftPadding.y };

            _graphBounds = { windowPos + ScreenCoordsXY{ graphPageWidget->left + 4, graphPageWidget->top + 15 },
                             windowPos + ScreenCoordsXY{ graphPageWidget->right - 4, graphPageWidget->bottom - 4 } };
            _graphProps.RecalculateLayout(
                { _graphBounds.Point1 + dynamicPadding, _graphBounds.Point2 - kGraphBottomRightPadding }, kGraphNumYLabels,
                kGraphNumPoints);
            _graphProps.lineCol = colours[2];
        }

#pragma endregion

        void initialiseScrollPosition(WidgetIndex widgetIndex, int32_t scrollId)
        {
            const auto& widget = this->widgets[widgetIndex];
            scrolls[scrollId].contentOffsetX = std::max(0, scrolls[scrollId].contentWidth - (widget.width() - 3));

            widgetScrollUpdateThumbs(*this, widgetIndex);
        }

        void DrawTabImage(RenderTarget& rt, int32_t tabPage, int32_t spriteIndex)
        {
            WidgetIndex widgetIndex = WIDX_TAB_1 + tabPage;

            if (!isWidgetDisabled(widgetIndex))
            {
                if (this->page == tabPage)
                {
                    int32_t frame = currentFrame / 2;
                    spriteIndex += (frame % _windowFinancesTabAnimationFrames[this->page]);
                }

                GfxDrawSprite(
                    rt, ImageId(spriteIndex),
                    windowPos + ScreenCoordsXY{ widgets[widgetIndex].left, widgets[widgetIndex].top });
            }
        }

        void DrawTabImages(RenderTarget& rt)
        {
            DrawTabImage(rt, WINDOW_FINANCES_PAGE_SUMMARY, SPR_TAB_FINANCES_SUMMARY_0);
            DrawTabImage(rt, WINDOW_FINANCES_PAGE_FINANCIAL_GRAPH, SPR_TAB_FINANCES_FINANCIAL_GRAPH_0);
            DrawTabImage(rt, WINDOW_FINANCES_PAGE_VALUE_GRAPH, SPR_TAB_FINANCES_VALUE_GRAPH_0);
            DrawTabImage(rt, WINDOW_FINANCES_PAGE_PROFIT_GRAPH, SPR_TAB_FINANCES_PROFIT_GRAPH_0);
            DrawTabImage(rt, WINDOW_FINANCES_PAGE_MARKETING, SPR_TAB_FINANCES_MARKETING_0);
            DrawTabImage(rt, WINDOW_FINANCES_PAGE_RESEARCH, SPR_TAB_FINANCES_RESEARCH_0);
        }
    };

    static FinancesWindow* FinancesWindowOpen(uint8_t page)
    {
        // TODO: find by class and number (park id)
        auto* windowMgr = GetWindowManager();
        auto* window = reinterpret_cast<FinancesWindow*>(windowMgr->BringToFrontByClass(WindowClass::finances));
        if (window == nullptr)
        {
            // TODO: get parkData from parameter (park id)
            auto& parkData = getGameState().park;
            window = windowMgr->Create<FinancesWindow>(
                WindowClass::finances, kWindowSizeSummary, WindowFlag::higherContrastOnPress, parkData);
        }

        if (window != nullptr && page != WINDOW_FINANCES_PAGE_SUMMARY)
            window->setPage(page);

        return window;
    }

    WindowBase* FinancesOpen()
    {
        return FinancesWindowOpen(WINDOW_FINANCES_PAGE_SUMMARY);
    }

    WindowBase* FinancesResearchOpen()
    {
        return FinancesWindowOpen(WINDOW_FINANCES_PAGE_RESEARCH);
    }

    WindowBase* FinancesMarketingOpen()
    {
        return FinancesWindowOpen(WINDOW_FINANCES_PAGE_MARKETING);
    }

    // Register the finances window with the graph accessibility navigator (called once at startup via
    // EnsureGraphScreensRegistered). From here on the graph owns this window class; the legacy
    // accessibility dispatcher stands down for it.
    void RegisterFinancesGraphScreen()
    {
        using namespace Accessibility::Graph;
        GraphScreen screen;
        screen.windowClass = WindowClass::finances;
        screen.build = [](GraphBuilder& b, WindowBase& w) { static_cast<FinancesWindow&>(w).BuildAccessGraph(b); };
        screen.onTabKey = [](WindowBase& w, int32_t dir) {
            static_cast<FinancesWindow&>(w).AccessChangePage(dir);
            return true;
        };
        RegisterGraphScreen(std::move(screen));
    }
} // namespace OpenRCT2::Ui::Windows
