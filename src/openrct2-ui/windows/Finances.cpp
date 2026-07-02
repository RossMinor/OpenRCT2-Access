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
#include <openrct2-ui/interface/Dropdown.h>
#include <openrct2-ui/interface/Graph.h>
#include <openrct2-ui/interface/Widget.h>
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
#include <openrct2/localisation/Localisation.Date.h>
#include <openrct2/management/Finance.h>
#include <openrct2/management/Research.h>
#include <openrct2/ride/RideData.h>
#include <openrct2/ride/ShopItem.h>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/world/Park.h>

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

        // Accessibility: loan-adjust sub-mode (entered with Enter on the Summary page) and the
        // currently focused marketing campaign on the Marketing page.
        bool _accessLoanMode = false;
        int32_t _accessMarketingIndex = -1;
        // Focused line in the Summary page's detailed read-out (the expenditure/income table). -1
        // means nothing focused yet, so the first Down arrow lands on the first line.
        int32_t _accessSummaryIndex = -1;
        // Focused row in the Research page's funding controls (funding level + 7 priority
        // checkboxes), navigated with up/down and toggled with Enter. -1 = nothing focused yet.
        int32_t _accessResearchIndex = -1;

        void SetDisabledTabs()
        {
            setWidgetDisabled(WIDX_TAB_5, (_parkData.flags & PARK_FLAGS_FORBID_MARKETING_CAMPAIGN) != 0);
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
                        + ", press Enter to adjust the loan";
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

            lines.push_back("Loan, " + cash(park.bankLoan) + ", press Enter to adjust");
            if (!(_parkData.flags & PARK_FLAGS_RCT1_INTEREST))
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

        void accessMoveSummary(int32_t delta)
        {
            const auto lines = buildSummaryLines();
            if (lines.empty())
                return;
            const int32_t n = static_cast<int32_t>(lines.size());
            _accessSummaryIndex = (_accessSummaryIndex + delta + n) % n;
            Accessibility::ScreenReaderSpeakItem(lines[_accessSummaryIndex], _accessSummaryIndex, n);
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

        void accessMoveResearch(int32_t delta)
        {
            const auto items = buildResearchItems();
            if (items.empty())
                return;
            const int32_t n = static_cast<int32_t>(items.size());
            _accessResearchIndex = (_accessResearchIndex + delta + n) % n;
            Accessibility::ScreenReaderSpeakItem(items[_accessResearchIndex].label, _accessResearchIndex, n);
        }

        void accessActivateResearch()
        {
            const auto items = buildResearchItems();
            if (_accessResearchIndex < 0 || _accessResearchIndex >= static_cast<int32_t>(items.size()))
                return;
            const auto& it = items[_accessResearchIndex];

            if (it.category < 0)
            {
                // Cycle the funding level (None -> Minimum -> Normal -> Maximum -> None) - the same
                // game action the funding dropdown runs.
                const auto& gameState = getGameState();
                const int32_t newLevel = (gameState.researchFundingLevel + 1) & 3;
                auto action = GameActions::ParkSetResearchFundingAction(gameState.researchPriorities, newLevel);
                GameActions::Execute(&action, getGameState());

                // The action applies immediately in single player; re-read the funding row so the
                // new level is heard.
                const auto updated = buildResearchItems();
                if (_accessResearchIndex >= 0 && _accessResearchIndex < static_cast<int32_t>(updated.size()))
                    Accessibility::ScreenReaderSpeak(updated[_accessResearchIndex].label);
            }
            else if (!it.enabled)
            {
                Accessibility::ScreenReaderSpeak("That category is fully researched and cannot be changed");
            }
            else
            {
                // Route through the window's own mouse-up handler, the exact path a checkbox click
                // takes (WindowResearchFundingMouseUp), so the toggle matches the game precisely.
                onMouseUp(static_cast<WidgetIndex>(WIDX_TRANSPORT_RIDES + it.category));

                // Announce just the resulting checkbox state, matching the other settings windows.
                const bool nowChecked = (getGameState().researchPriorities & (1 << it.category)) != 0;
                Accessibility::ScreenReaderSpeak(nowChecked ? "checked" : "unchecked");
            }
        }

        void changeAccessibilityTab(int32_t delta)
        {
            int32_t newPage = page;
            for (int32_t i = 0; i < WINDOW_FINANCES_PAGE_COUNT; i++)
            {
                newPage = (newPage + delta + WINDOW_FINANCES_PAGE_COUNT) % WINDOW_FINANCES_PAGE_COUNT;
                if (widgets[WIDX_TAB_1 + newPage].type != WidgetType::empty)
                    break;
            }
            setPage(newPage);
            _accessSummaryIndex = -1;  // restart the Summary table read-out from the top
            _accessResearchIndex = -1; // and the Research funding rows

            // Position among the visible tabs.
            int32_t total = 0, pos = 0;
            for (int32_t p = 0; p < WINDOW_FINANCES_PAGE_COUNT; p++)
            {
                if (widgets[WIDX_TAB_1 + p].type == WidgetType::empty)
                    continue;
                if (p == page)
                    pos = total;
                total++;
            }
            Accessibility::ScreenReaderSpeakItem(getAccessibilityPageSummary(), pos, total);
        }

        std::string accessLoanText()
        {
            return "Loan " + OpenRCT2::FormatStringID(STR_CURRENCY_FORMAT, getGameState().park.bankLoan);
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

        void accessMoveMarketing(int32_t delta)
        {
            std::vector<int32_t> avail;
            for (int32_t i = 0; i < 6; i++)
                if (widgets[WIDX_CAMPAIGN_1 + i].type != WidgetType::empty)
                    avail.push_back(i);
            if (avail.empty())
            {
                Accessibility::ScreenReaderSpeak("No campaigns available to start");
                return;
            }
            const int32_t n = static_cast<int32_t>(avail.size());
            int32_t cur = -1;
            for (int32_t k = 0; k < n; k++)
                if (avail[k] == _accessMarketingIndex)
                    cur = k;
            if (cur < 0)
                cur = (delta >= 0) ? 0 : n - 1;
            else
                cur = (cur + delta + n) % n;
            _accessMarketingIndex = avail[cur];
            Accessibility::ScreenReaderSpeakItem(accessCampaignName(_accessMarketingIndex) + ", press Enter to set up", cur, n);
        }

        bool onAccessibilityAction(AccessibilityAction action) override
        {
            // Loan-adjust sub-mode: Left repays, Right borrows, Enter/Escape exits.
            if (_accessLoanMode)
            {
                switch (action)
                {
                    case AccessibilityAction::moveLeft:
                        accessAdjustLoan(false);
                        return true;
                    case AccessibilityAction::moveRight:
                        accessAdjustLoan(true);
                        return true;
                    case AccessibilityAction::moveUp:
                    case AccessibilityAction::moveDown:
                    case AccessibilityAction::announce:
                        Accessibility::ScreenReaderSpeak(accessLoanText());
                        return true;
                    case AccessibilityAction::activate:
                    case AccessibilityAction::cancel:
                        _accessLoanMode = false;
                        Accessibility::ScreenReaderSpeak("Finished adjusting loan");
                        return true;
                    default:
                        return true; // swallow other keys while adjusting
                }
            }

            switch (action)
            {
                case AccessibilityAction::moveLeft:
                    _accessMarketingIndex = -1;
                    changeAccessibilityTab(-1);
                    return true;
                case AccessibilityAction::moveRight:
                    _accessMarketingIndex = -1;
                    changeAccessibilityTab(1);
                    return true;
                case AccessibilityAction::moveUp:
                    if (page == WINDOW_FINANCES_PAGE_MARKETING)
                        accessMoveMarketing(-1);
                    else if (page == WINDOW_FINANCES_PAGE_SUMMARY)
                        accessMoveSummary(-1);
                    else if (page == WINDOW_FINANCES_PAGE_RESEARCH)
                        accessMoveResearch(-1);
                    else
                        Accessibility::ScreenReaderSpeak(getAccessibilityPageSummary());
                    return true;
                case AccessibilityAction::moveDown:
                    if (page == WINDOW_FINANCES_PAGE_MARKETING)
                        accessMoveMarketing(1);
                    else if (page == WINDOW_FINANCES_PAGE_SUMMARY)
                        accessMoveSummary(1);
                    else if (page == WINDOW_FINANCES_PAGE_RESEARCH)
                        accessMoveResearch(1);
                    else
                        Accessibility::ScreenReaderSpeak(getAccessibilityPageSummary());
                    return true;
                case AccessibilityAction::activate:
                    if (page == WINDOW_FINANCES_PAGE_SUMMARY)
                    {
                        _accessLoanMode = true;
                        Accessibility::ScreenReaderSpeak(
                            "Adjusting loan. " + accessLoanText()
                            + ". Left arrow repays, right arrow borrows, Escape when done.");
                    }
                    else if (page == WINDOW_FINANCES_PAGE_MARKETING && _accessMarketingIndex >= 0)
                    {
                        onMouseUp(WIDX_CAMPAIGN_1 + _accessMarketingIndex); // opens the New Campaign window
                    }
                    else if (page == WINDOW_FINANCES_PAGE_RESEARCH)
                    {
                        accessActivateResearch();
                    }
                    return true;
                case AccessibilityAction::announce:
                    if (page == WINDOW_FINANCES_PAGE_MARKETING && _accessMarketingIndex >= 0)
                        Accessibility::ScreenReaderSpeak(accessCampaignName(_accessMarketingIndex) + ", press Enter to set up");
                    else if (page == WINDOW_FINANCES_PAGE_RESEARCH && _accessResearchIndex >= 0)
                    {
                        const auto items = buildResearchItems();
                        if (_accessResearchIndex < static_cast<int32_t>(items.size()))
                            Accessibility::ScreenReaderSpeak(items[_accessResearchIndex].label);
                    }
                    else
                        Accessibility::ScreenReaderSpeak(getAccessibilityPageSummary());
                    return true;
                case AccessibilityAction::cancel:
                    close();
                    Accessibility::ReannounceToolbarItemIfMenuMode();
                    return true;
                default:
                    return false;
            }
        }

        std::optional<ScreenRect> getAccessibilityFocusRect() override
        {
            WidgetIndex w = WIDX_TAB_1 + page;
            if (_accessLoanMode)
                w = WIDX_LOAN;
            else if (page == WINDOW_FINANCES_PAGE_MARKETING && _accessMarketingIndex >= 0)
                w = WIDX_CAMPAIGN_1 + _accessMarketingIndex;
            else if (page == WINDOW_FINANCES_PAGE_RESEARCH && _accessResearchIndex >= 0)
            {
                const auto items = buildResearchItems();
                if (_accessResearchIndex < static_cast<int32_t>(items.size()))
                {
                    const int32_t cat = items[_accessResearchIndex].category;
                    w = (cat < 0) ? WIDX_RESEARCH_FUNDING : static_cast<WidgetIndex>(WIDX_TRANSPORT_RIDES + cat);
                }
            }
            if (w >= widgets.size() || widgets[w].type == WidgetType::empty)
                return std::nullopt;
            const auto& wd = widgets[w];
            return ScreenRect{ windowPos + ScreenCoordsXY{ wd.left, wd.top },
                               windowPos + ScreenCoordsXY{ wd.right, wd.bottom } };
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
            if (!(_parkData.flags & PARK_FLAGS_RCT1_INTEREST))
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
                auto campaignButton = &widgets[WIDX_CAMPAIGN_1 + i];
                auto marketingCampaign = MarketingGetCampaign(i);
                if (marketingCampaign == nullptr && MarketingIsCampaignTypeApplicable(i))
                {
                    campaignButton->type = WidgetType::button;
                    campaignButton->top = y;
                    campaignButton->bottom = y + kButtonFaceHeight + 1;
                    y += kButtonFaceHeight + 2;
                }
                else
                {
                    campaignButton->type = WidgetType::empty;
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
                if (campaignButton->type != WidgetType::empty)
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
} // namespace OpenRCT2::Ui::Windows
