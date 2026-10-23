/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "../UiStringIds.h"
#include "../interface/Dropdown.h"
#include "../interface/Widget.h"
#include "../interface/Window.h"
#include "Windows.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <optional>
#include <openrct2-ui/accessibility/MenuNavigation.h>
#include <openrct2-ui/accessibility/ScreenReader.h>
#include <openrct2-ui/accessibility/graph/GraphBuilder.h>
#include <openrct2-ui/accessibility/graph/GraphScreens.h>
#include <openrct2-ui/accessibility/graph/GraphVocab.h>
#include <openrct2/Context.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/SpriteIds.h>
#include <openrct2/actions/GameActionRunner.h>
#include <openrct2/actions/general/ScenarioSetSettingAction.h>
#include <openrct2/actions/park/ParkSetNameAction.h>
#include <openrct2/config/Config.h>
#include <openrct2/core/String.hpp>
#include <openrct2/core/UnitConversion.h>
#include <openrct2/drawing/ColourMap.h>
#include <openrct2/drawing/Drawing.h>
#include <openrct2/drawing/Font.h>
#include <openrct2/drawing/Rectangle.h>
#include <openrct2/drawing/RenderTarget.h>
#include <openrct2/drawing/Text.h>
#include <openrct2/localisation/Currency.h>
#include <openrct2/localisation/Formatter.h>
#include <openrct2/localisation/Formatting.h>
#include <openrct2/management/Finance.h>
#include <openrct2/ride/Ride.h>
#include <openrct2/ride/RideManager.hpp>
#include <openrct2/ui/WindowManager.h>
#include <openrct2/world/Park.h>

using namespace OpenRCT2::Drawing;

namespace OpenRCT2::Ui::Windows
{
    namespace Vocab = Accessibility::Graph::Vocab;

    static constexpr ScreenSize kSizeObjective = { 450, 122 };
    static constexpr ScreenSize kSizeScenarioDetails = { 450, 169 };
    static constexpr ScreenSize kSizeFinancial = { 380, 233 };
    static constexpr ScreenSize kSizeGuests = { 380, 164 };
    static constexpr ScreenSize kSizeLand = { 268, 134 };
    static constexpr ScreenSize kSizeRides = { 380, 224 };

    static constexpr money64 kObjectiveCurrencyLoanAndValueMax = 2000000.00_GBP;
    static constexpr money64 kObjectiveCurrencyLoanAndValueMin = 1000.00_GBP;
    static constexpr money64 kObjectiveCurrencyLoanAndValueAdjustment = 1000.00_GBP;

    static constexpr money64 kObjectiveCurrencyFoodMax = 2000000.00_GBP;
    static constexpr money64 kObjectiveCurrencyFoodMin = 1000.00_GBP;
    static constexpr money64 kObjectiveCurrencyFoodAdjustment = 100.00_GBP;

    static constexpr uint16_t kObjectiveLengthMax = 5000;
    static constexpr uint16_t kObjectiveLengthMin = 1000;
    static constexpr uint16_t kObjectiveLengthAdjustment = 100;

    static constexpr RideRating_t kObjectiveExcitementMax = RideRating::make(9, 90);
    static constexpr RideRating_t kObjectiveExcitementMin = RideRating::make(4, 00);
    static constexpr RideRating_t kObjectiveExcitementAdjustment = RideRating::make(0, 10);

    // The number has to leave a bit of room for other entities like vehicles, litter and balloons.
    static constexpr uint16_t kObjectiveGuestsMax = 50000;
    static constexpr uint16_t kObjectiveGuestsMin = 250;
    static constexpr uint16_t kObjectiveGuestsAdjustment = 50;

    static constexpr uint8_t kObjectiveYearMax = 25;
    static constexpr uint8_t kObjectiveYearMin = 1;
    static constexpr uint8_t kObjectiveYearAdjustment = 1;

#pragma region Widgets

    enum
    {
        WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_OBJECTIVE,
        WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_SCENARIO_DETAILS,
        WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_FINANCIAL,
        WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_GUESTS,
        WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_PARK,
        WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_RIDES,
        WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_COUNT
    };

    static constexpr StringId ObjectiveDropdownOptionNames[] = {
        STR_OBJECTIVE_DROPDOWN_NONE,
        STR_OBJECTIVE_DROPDOWN_NUMBER_OF_GUESTS_AT_A_GIVEN_DATE,
        STR_OBJECTIVE_DROPDOWN_PARK_VALUE_AT_A_GIVEN_DATE,
        STR_OBJECTIVE_DROPDOWN_HAVE_FUN,
        STR_OBJECTIVE_DROPDOWN_BUILD_THE_BEST_RIDE_YOU_CAN,
        STR_OBJECTIVE_DROPDOWN_BUILD_10_ROLLER_COASTERS,
        STR_OBJECTIVE_DROPDOWN_NUMBER_OF_GUESTS_IN_PARK,
        STR_OBJECTIVE_DROPDOWN_MONTHLY_INCOME_FROM_RIDE_TICKETS,
        STR_OBJECTIVE_DROPDOWN_BUILD_10_ROLLER_COASTERS_OF_A_GIVEN_LENGTH,
        STR_OBJECTIVE_DROPDOWN_FINISH_BUILDING_5_ROLLER_COASTERS,
        STR_OBJECTIVE_DROPDOWN_REPAY_LOAN_AND_ACHIEVE_A_GIVEN_PARK_VALUE,
        STR_OBJECTIVE_DROPDOWN_MONTHLY_PROFIT_FROM_FOOD_MERCHANDISE,
    };

    enum WindowEditorScenarioOptionsWidgetIdx : WidgetIndex
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
        WIDX_PAGE_START,

        // Objective tab
        WIDX_OBJECTIVE_LABEL = WIDX_PAGE_START,
        WIDX_OBJECTIVE,
        WIDX_OBJECTIVE_DROPDOWN,
        WIDX_OBJECTIVE_ARG_1_LABEL,
        WIDX_OBJECTIVE_ARG_1,
        WIDX_OBJECTIVE_ARG_1_INCREASE,
        WIDX_OBJECTIVE_ARG_1_DECREASE,
        WIDX_OBJECTIVE_ARG_2_LABEL,
        WIDX_OBJECTIVE_ARG_2,
        WIDX_OBJECTIVE_ARG_2_INCREASE,
        WIDX_OBJECTIVE_ARG_2_DECREASE,
        WIDX_HARD_PARK_RATING,

        // Scenario details tab
        WIDX_PARK_NAME = WIDX_PAGE_START,
        WIDX_SCENARIO_NAME,
        WIDX_CATEGORY,
        WIDX_CATEGORY_DROPDOWN,
        WIDX_DETAILS,

        // Financial tab
        WIDX_NO_MONEY = WIDX_PAGE_START,
        WIDX_GROUP_LOAN,
        WIDX_INITIAL_LOAN_LABEL,
        WIDX_INITIAL_LOAN,
        WIDX_INITIAL_LOAN_INCREASE,
        WIDX_INITIAL_LOAN_DECREASE,
        WIDX_MAXIMUM_LOAN_LABEL,
        WIDX_MAXIMUM_LOAN,
        WIDX_MAXIMUM_LOAN_INCREASE,
        WIDX_MAXIMUM_LOAN_DECREASE,
        WIDX_INTEREST_RATE_LABEL,
        WIDX_INTEREST_RATE,
        WIDX_INTEREST_RATE_INCREASE,
        WIDX_INTEREST_RATE_DECREASE,
        WIDX_RCT1_INTEREST,
        WIDX_GROUP_BUSINESS_MODEL,
        WIDX_INITIAL_CASH_LABEL,
        WIDX_INITIAL_CASH,
        WIDX_INITIAL_CASH_INCREASE,
        WIDX_INITIAL_CASH_DECREASE,
        WIDX_PAY_FOR_PARK_OR_RIDES_LABEL,
        WIDX_PAY_FOR_PARK_OR_RIDES,
        WIDX_PAY_FOR_PARK_OR_RIDES_DROPDOWN,
        WIDX_ENTRY_PRICE_LABEL,
        WIDX_ENTRY_PRICE,
        WIDX_ENTRY_PRICE_INCREASE,
        WIDX_ENTRY_PRICE_DECREASE,
        WIDX_FORBID_MARKETING,

        // Guests tab
        WIDX_CASH_PER_GUEST_LABEL = WIDX_PAGE_START,
        WIDX_CASH_PER_GUEST,
        WIDX_CASH_PER_GUEST_INCREASE,
        WIDX_CASH_PER_GUEST_DECREASE,
        WIDX_GUEST_INITIAL_HAPPINESS_LABEL,
        WIDX_GUEST_INITIAL_HAPPINESS,
        WIDX_GUEST_INITIAL_HAPPINESS_INCREASE,
        WIDX_GUEST_INITIAL_HAPPINESS_DECREASE,
        WIDX_GUEST_INITIAL_HUNGER_LABEL,
        WIDX_GUEST_INITIAL_HUNGER,
        WIDX_GUEST_INITIAL_HUNGER_INCREASE,
        WIDX_GUEST_INITIAL_HUNGER_DECREASE,
        WIDX_GUEST_INITIAL_THIRST_LABEL,
        WIDX_GUEST_INITIAL_THIRST,
        WIDX_GUEST_INITIAL_THIRST_INCREASE,
        WIDX_GUEST_INITIAL_THIRST_DECREASE,
        WIDX_GUESTS_INTENSITY_PREFERENCE_LABEL,
        WIDX_GUESTS_INTENSITY_PREFERENCE,
        WIDX_GUESTS_INTENSITY_PREFERENCE_DROPDOWN,
        WIDX_HARD_GUEST_GENERATION,

        // Land tab
        WIDX_LAND_COST_LABEL = WIDX_PAGE_START,
        WIDX_LAND_COST,
        WIDX_LAND_COST_INCREASE,
        WIDX_LAND_COST_DECREASE,
        WIDX_CONSTRUCTION_RIGHTS_COST_LABEL,
        WIDX_CONSTRUCTION_RIGHTS_COST,
        WIDX_CONSTRUCTION_RIGHTS_COST_INCREASE,
        WIDX_CONSTRUCTION_RIGHTS_COST_DECREASE,
        WIDX_FORBID_TREE_REMOVAL,
        WIDX_FORBID_LANDSCAPE_CHANGES,
        WIDX_FORBID_HIGH_CONSTRUCTION
    };

    // clang-format off
    static constexpr auto makeOptionsWidgets = [](StringId title, ScreenSize size) {
        return makeWidgets(
            makeWindowShim(title, size),
            makeWidget({  0, 43}, { size.width, 106 }, WidgetType::resize, WindowColour::secondary),
            makeTab   ({  3, 17}, STR_SCENARIO_OPTIONS_OBJECTIVE_TIP             ),
            makeTab   ({ 34, 17}, STR_SCENARIO_OPTIONS_SCENARIO_DETAILS_TIP      ),
            makeTab   ({ 65, 17}, STR_SCENARIO_OPTIONS_FINANCIAL_TIP             ),
            makeTab   ({ 96, 17}, STR_SCENARIO_OPTIONS_GUESTS_TIP                ),
            makeTab   ({127, 17}, STR_SCENARIO_OPTIONS_LAND_RESTRICTIONS_TIP     ),
            makeTab   ({158, 17}, STR_SCENARIO_OPTIONS_PRESERVED_RIDES_TIP       )
        );
    };

    static constexpr auto window_editor_scenario_options_objective_widgets = makeWidgets(
        makeOptionsWidgets(STR_SCENARIO_OPTIONS_OBJECTIVE, kSizeObjective),
        makeWidget        ({  8,  48}, {344,  12}, WidgetType::label,        WindowColour::secondary, STR_OBJECTIVE_DROPDOWN_LABEL                                            ),
        makeWidget        ({ 98,  47}, {344,  14}, WidgetType::dropdownMenu, WindowColour::secondary, kStringIdNone,                STR_SELECT_OBJECTIVE_FOR_THIS_SCENARIO_TIP),
        makeWidget        ({430,  48}, { 11,  12}, WidgetType::button,       WindowColour::secondary, STR_DROPDOWN_GLYPH,           STR_SELECT_OBJECTIVE_FOR_THIS_SCENARIO_TIP),
        makeWidget                ({ 28,  66}, {140,  12}, WidgetType::label,        WindowColour::secondary, kStringIdEmpty                                                          ),
        makeHoldableSpinnerWidgets({158,  65}, {120,  14}, WidgetType::spinner,      WindowColour::secondary                                                                          ), // NB: 3 widgets
        makeWidget                ({ 28,  85}, {140,  12}, WidgetType::label,        WindowColour::secondary, STR_WINDOW_OBJECTIVE_DATE                                               ),
        makeHoldableSpinnerWidgets({158,  84}, {120,  14}, WidgetType::spinner,      WindowColour::secondary                                                                          ), // NB: 3 widgets
        makeWidget        ({ 14, 103}, {340,  12}, WidgetType::checkbox,     WindowColour::secondary, STR_HARD_PARK_RATING,         STR_HARD_PARK_RATING_TIP                  )
    );

    static constexpr auto window_editor_scenario_options_scenario_details_widgets = makeWidgets(
        makeOptionsWidgets(STR_SCENARIO_OPTIONS_SCENARIO_DETAILS, kSizeScenarioDetails),
        makeWidget        ({370,  48}, { 75,  14}, WidgetType::button,       WindowColour::secondary, STR_CHANGE,         STR_CHANGE_NAME_OF_PARK_TIP                    ),
        makeWidget        ({370,  65}, { 75,  14}, WidgetType::button,       WindowColour::secondary, STR_CHANGE,         STR_CHANGE_NAME_OF_SCENARIO_TIP                ),
        makeWidget        ({ 98,  82}, {180,  14}, WidgetType::dropdownMenu, WindowColour::secondary, kStringIdNone,      STR_SELECT_WHICH_GROUP_THIS_SCENARIO_APPEARS_IN),
        makeWidget        ({266,  83}, { 11,  12}, WidgetType::button,       WindowColour::secondary, STR_DROPDOWN_GLYPH, STR_SELECT_WHICH_GROUP_THIS_SCENARIO_APPEARS_IN),
        makeWidget        ({370, 101}, { 75,  14}, WidgetType::button,       WindowColour::secondary, STR_CHANGE,         STR_CHANGE_DETAIL_NOTES_ABOUT_PARK_SCENARIO_TIP)
    );

    static constexpr auto window_editor_scenario_options_financial_widgets = makeWidgets(
        makeOptionsWidgets(STR_SCENARIO_OPTIONS_FINANCIAL, kSizeFinancial),
        makeWidget        ({  8,  48}, {kSizeFinancial.width - 16, 12}, WidgetType::checkbox,     WindowColour::secondary, STR_MAKE_PARK_NO_MONEY,   STR_MAKE_PARK_NO_MONEY_TIP        ),

        makeWidget                ({  5,  63}, {kSizeFinancial.width - 10, 72}, WidgetType::groupbox,     WindowColour::secondary, STR_GROUP_LOAN_OPTIONS                                      ),
        makeWidget                ({  9,  78}, {                      250, 12}, WidgetType::label,        WindowColour::secondary, STR_INIT_LOAN_LABEL                                         ),
        makeHoldableSpinnerWidgets({268,  77}, {                      100, 14}, WidgetType::spinner,      WindowColour::secondary                                                              ), // NB: 3 widgets
        makeWidget                ({  9,  97}, {                      250, 12}, WidgetType::label,        WindowColour::secondary, STR_MAX_LOAN_LABEL                                          ),
        makeHoldableSpinnerWidgets({268,  96}, {                      100, 14}, WidgetType::spinner,      WindowColour::secondary                                                              ), // NB: 3 widgets
        makeWidget                ({  9, 116}, {                      280, 12}, WidgetType::label,        WindowColour::secondary, STR_INTEREST_RATE_LABEL                                     ),
        makeHoldableSpinnerWidgets({298, 115}, {                       70, 14}, WidgetType::spinner,      WindowColour::secondary                                                              ), // NB: 3 widgets
        makeWidget                ({ 10, 115}, {kSizeFinancial.width - 16, 12}, WidgetType::checkbox,     WindowColour::secondary, STR_RCT1_INTEREST,        STR_RCT1_INTEREST_TIP             ),

        makeWidget                ({  5, 137}, {kSizeFinancial.width - 10, 89}, WidgetType::groupbox,     WindowColour::secondary, STR_GROUP_BUSINESS_MODEL                                    ),
        makeWidget                ({  9, 155}, {                      250, 12}, WidgetType::label,        WindowColour::secondary, STR_INIT_CASH_LABEL                                         ),
        makeHoldableSpinnerWidgets({268, 154}, {                      100, 14}, WidgetType::spinner,      WindowColour::secondary                                                              ), // NB: 3 widgets
        makeWidget                ({  9, 173}, {                      150, 12}, WidgetType::label,        WindowColour::secondary, STR_EARNINGS_LABEL                                          ),
        makeWidget                ({158, 172}, {                      210, 14}, WidgetType::dropdownMenu, WindowColour::secondary, kStringIdNone,            STR_PAY_FOR_PARK_PAY_FOR_RIDES_TIP),
        makeWidget                ({356, 173}, {                       11, 12}, WidgetType::button,       WindowColour::secondary, STR_DROPDOWN_GLYPH,       STR_PAY_FOR_PARK_PAY_FOR_RIDES_TIP),
        makeWidget                ({  9, 191}, {                      280, 12}, WidgetType::label,        WindowColour::secondary, STR_ENTRY_PRICE_LABEL                                       ),
        makeHoldableSpinnerWidgets({298, 190}, {                       70, 14}, WidgetType::spinner,      WindowColour::secondary                                                              ), // NB: 3 widgets
        makeWidget                ({ 10, 208}, {kSizeFinancial.width - 16, 12}, WidgetType::checkbox,     WindowColour::secondary, STR_FORBID_MARKETING,     STR_FORBID_MARKETING_TIP          )
    );

    static constexpr auto window_editor_scenario_options_guests_widgets = makeWidgets(
        makeOptionsWidgets(STR_SCENARIO_OPTIONS_GUESTS, kSizeGuests),
        makeWidget                ({  8,  49}, {      280,  12}, WidgetType::label,        WindowColour::secondary, STR_CASH_PER_GUEST_LABEL                                                ),
        makeHoldableSpinnerWidgets({298,  48}, {       70,  14}, WidgetType::spinner,      WindowColour::secondary                                                                          ), // NB: 3 widgets
        makeWidget                ({  8,  68}, {      280,  12}, WidgetType::label,        WindowColour::secondary, STR_GUEST_INIT_HAPPINESS                                                ),
        makeHoldableSpinnerWidgets({298,  67}, {       70,  14}, WidgetType::spinner,      WindowColour::secondary                                                                          ), // NB: 3 widgets
        makeWidget                ({  8,  87}, {      280,  12}, WidgetType::label,        WindowColour::secondary, STR_GUEST_INIT_HUNGER                                                   ),
        makeHoldableSpinnerWidgets({298,  86}, {       70,  14}, WidgetType::spinner,      WindowColour::secondary                                                                          ), // NB: 3 widgets
        makeWidget                ({  8, 106}, {      280,  12}, WidgetType::label,        WindowColour::secondary, STR_GUEST_INIT_THIRST                                                   ),
        makeHoldableSpinnerWidgets({298, 105}, {       70,  14}, WidgetType::spinner,      WindowColour::secondary                                                                          ), // NB: 3 widgets
        makeWidget                ({  8, 124}, {      180,  12}, WidgetType::label,        WindowColour::secondary, STR_GUESTS_PREFER_INTENSITY_LABEL                                       ),
        makeWidget                ({198, 123}, {      170,  14}, WidgetType::dropdownMenu, WindowColour::secondary, kStringIdNone,                        STR_GUESTS_PREFER_INTENSITY_TIP   ),
        makeWidget                ({357, 124}, {       11,  12}, WidgetType::button,       WindowColour::secondary, STR_DROPDOWN_GLYPH,                   STR_GUESTS_PREFER_INTENSITY_TIP   ),
        makeWidget                ({  8, 143}, {      350,  12}, WidgetType::checkbox,     WindowColour::secondary, STR_HARD_GUEST_GENERATION,            STR_HARD_GUEST_GENERATION_TIP     )
    );

    static constexpr auto window_editor_scenario_options_land_widgets = makeWidgets(
        makeOptionsWidgets(STR_SCENARIO_OPTIONS_LAND_RESTRICTIONS, kSizeLand),
        makeWidget                ({  8,  49}, {                 170,  12}, WidgetType::label,    WindowColour::secondary, STR_LAND_COST_LABEL                                             ),
        makeHoldableSpinnerWidgets({188,  48}, {                  70,  14}, WidgetType::spinner,  WindowColour::secondary                                                                  ), // NB: 3 widgets
        makeWidget                ({  8,  66}, {                 170,  12}, WidgetType::label,    WindowColour::secondary, STR_RIGHTS_COST_LABEL                                           ),
        makeHoldableSpinnerWidgets({188,  65}, {                  70,  14}, WidgetType::spinner,  WindowColour::secondary                                                                  ), // NB: 3 widgets
        makeWidget        ({  8,  82}, {kSizeLand.width - 16,  12}, WidgetType::checkbox, WindowColour::secondary, STR_FORBID_TREE_REMOVAL,      STR_FORBID_TREE_REMOVAL_TIP       ),
        makeWidget        ({  8,  99}, {kSizeLand.width - 16,  12}, WidgetType::checkbox, WindowColour::secondary, STR_FORBID_LANDSCAPE_CHANGES, STR_FORBID_LANDSCAPE_CHANGES_TIP  ),
        makeWidget        ({  8, 116}, {kSizeLand.width - 16,  12}, WidgetType::checkbox, WindowColour::secondary, STR_FORBID_HIGH_CONSTRUCTION, STR_FORBID_HIGH_CONSTRUCTION_TIP  )
    );

    static constexpr auto window_editor_scenario_options_rides_widgets = makeWidgets(
        makeOptionsWidgets(STR_SCENARIO_OPTIONS_PRESERVED_RIDES, kSizeRides),
        makeWidget({  6,  46}, {kSizeRides.width - 12,  12}, WidgetType::label,  WindowColour::secondary, STR_WINDOW_PRESERVATION_ORDER),
        makeWidget({  3,  60}, {                  374, 161}, WidgetType::scroll, WindowColour::secondary, SCROLL_VERTICAL              )
    );

    static constexpr std::span<const Widget> window_editor_scenario_options_widgets[] = {
        window_editor_scenario_options_objective_widgets,
        window_editor_scenario_options_scenario_details_widgets,
        window_editor_scenario_options_financial_widgets,
        window_editor_scenario_options_guests_widgets,
        window_editor_scenario_options_land_widgets,
        window_editor_scenario_options_rides_widgets,
    };

    // clang-format on

#pragma endregion

    class EditorScenarioOptionsWindow final : public Window
    {
    private:
        // Not shops or facilities
        std::vector<RideId> _rideableRides;

    public:
        void onOpen() override
        {
            setPage(WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_OBJECTIVE);
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
                case WIDX_TAB_3:
                case WIDX_TAB_4:
                case WIDX_TAB_5:
                case WIDX_TAB_6:
                    setPage(widgetIndex - WIDX_TAB_1);
                    return;
            }

            switch (page)
            {
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_OBJECTIVE:
                    return ObjectiveOnMouseUp(widgetIndex);
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_SCENARIO_DETAILS:
                    return ScenarioDetailsOnMouseUp(widgetIndex);
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_FINANCIAL:
                    return FinancialMouseUp(widgetIndex);
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_PARK:
                    return LandMouseUp(widgetIndex);
            }
        }

        void onResize() override
        {
            switch (page)
            {
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_OBJECTIVE:
                    return ObjectiveOnResize();
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_SCENARIO_DETAILS:
                    return ScenarioDetailsOnResize();
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_FINANCIAL:
                    return FinancialResize();
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_GUESTS:
                    return GuestsResize();
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_PARK:
                    return LandResize();
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_RIDES:
                    return RidesOnResize();
            }
        }

        void onMouseDown(WidgetIndex widgetIndex) override
        {
            switch (page)
            {
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_OBJECTIVE:
                    return ObjectiveOnMouseDown(widgetIndex);
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_SCENARIO_DETAILS:
                    return ScenarioDetailsOnMouseDown(widgetIndex);
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_FINANCIAL:
                    return FinancialMouseDown(widgetIndex);
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_GUESTS:
                    return GuestsMouseDown(widgetIndex);
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_PARK:
                    return LandMouseDown(widgetIndex);
            }
        }

        void onUpdate() override
        {
            switch (page)
            {
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_OBJECTIVE:
                    return ObjectiveOnUpdate();
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_SCENARIO_DETAILS:
                    return ScenarioDetailsOnUpdate();
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_FINANCIAL:
                    return FinancialUpdate();
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_GUESTS:
                    return GuestsUpdate();
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_PARK:
                    return LandUpdate();
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_RIDES:
                    return RidesOnUpdate();
            }
        }

        void onPrepareDraw() override
        {
            HideUnavailableTabs();

            switch (page)
            {
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_OBJECTIVE:
                    return ObjectiveOnPrepareDraw();
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_SCENARIO_DETAILS:
                    return ScenarioDetailsOnPrepareDraw();
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_FINANCIAL:
                    return FinancialPrepareDraw();
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_GUESTS:
                    return GuestsPrepareDraw();
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_PARK:
                    return LandPrepareDraw();
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_RIDES:
                    return RidesOnPrepareDraw();
            }
        }

        void onDraw(RenderTarget& rt) override
        {
            switch (page)
            {
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_OBJECTIVE:
                    return ObjectiveOnDraw(rt);
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_SCENARIO_DETAILS:
                    return ScenarioDetailsOnDraw(rt);
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_FINANCIAL:
                    return FinancialDraw(rt);
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_GUESTS:
                    return GuestsDraw(rt);
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_PARK:
                    return LandDraw(rt);
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_RIDES:
                    return RidesOnDraw(rt);
            }
        }

        void onDropdown(WidgetIndex widgetIndex, int32_t selectedIndex) override
        {
            switch (page)
            {
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_OBJECTIVE:
                    return ObjectiveOnDropdown(widgetIndex, selectedIndex);
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_SCENARIO_DETAILS:
                    return ScenarioDetailsOnDropdown(widgetIndex, selectedIndex);
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_FINANCIAL:
                    return FinancialDropdown(widgetIndex, selectedIndex);
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_GUESTS:
                    return GuestsDropdown(widgetIndex, selectedIndex);
            }
        }

        void onTextInput(WidgetIndex widgetIndex, std::string_view text) override
        {
            if (page == WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_SCENARIO_DETAILS)
            {
                ScenarioDetailsOnTextInput(widgetIndex, text);
            }
        }

        void onScrollMouseDown(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            if (page == WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_RIDES)
            {
                RidesOnScrollMouseDown(scrollIndex, screenCoords);
            }
        }

        void onScrollMouseOver(int32_t scrollIndex, const ScreenCoordsXY& screenCoords) override
        {
            if (page == WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_RIDES)
            {
                RidesOnScrollMouseOver(scrollIndex, screenCoords);
            }
        }

        void onScrollDraw(int32_t scrollIndex, RenderTarget& rt) override
        {
            if (page == WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_RIDES)
            {
                RidesOnScrollDraw(rt, scrollIndex);
            }
        }

        ScreenSize onScrollGetSize(int32_t scrollIndex) override
        {
            if (page == WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_RIDES)
            {
                return RidesOnScrollGetSize(scrollIndex);
            }

            return {};
        }

    private:
        /**
         *
         *  rct2: 0x00672609
         */
        bool AnyRidesExist()
        {
            // Check if there are any rides (not shops or facilities)
            const auto& gameState = getGameState();
            const auto& rideManager = RideManager(gameState);
            return std::any_of(
                rideManager.begin(), rideManager.end(), [](const Ride& rideToCheck) { return rideToCheck.isRide(); });
        }

        void HideUnavailableTabs()
        {
            if (gLegacyScene != LegacyScene::scenarioEditor)
                return;

            auto step = getGameState().editorStep;
            bool isObjectiveSelection = step == Editor::Step::objectiveSelection;
            bool isScenarioDetails = step == Editor::Step::scenarioDetails;
            bool isOtherTab = !isObjectiveSelection && !isScenarioDetails;

            // Disable tabs based on current editor step
            setWidgetDisabled(WIDX_TAB_1, !isObjectiveSelection);
            setWidgetDisabled(WIDX_TAB_2, !isScenarioDetails);
            for (auto i = 2; i < WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_COUNT; i++)
                setWidgetDisabled(WIDX_TAB_1 + i, !isOtherTab);

            setWidgetDisabled(WIDX_TAB_6, !(isOtherTab && AnyRidesExist()));

            // Reposition tabs based on availability so there are no gaps
            widgets[WIDX_TAB_1].left = 3;
            widgets[WIDX_TAB_1].right = widgets[WIDX_TAB_1].left + 30;
            WindowAlignTabs(this, WIDX_TAB_1, WIDX_TAB_6);

            // Switch tabs if our current tab has become unavailable
            if (isWidgetDisabled(WIDX_TAB_1 + page))
            {
                if (isObjectiveSelection)
                    setPage(WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_OBJECTIVE);
                else if (isScenarioDetails)
                    setPage(WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_SCENARIO_DETAILS);
                else
                    setPage(WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_FINANCIAL);
            }
        }

        void SetPressedTab()
        {
            for (int32_t i = 0; i < WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_COUNT; i++)
                setWidgetPressed(WIDX_TAB_1 + i, false);
            setWidgetPressed(WIDX_TAB_1 + page, true);
        }

        void DrawTabImages(RenderTarget& rt)
        {
            Widget* widget;
            int32_t spriteIndex;

            // Tab 1
            if (!isWidgetDisabled(WIDX_TAB_1))
            {
                widget = &widgets[WIDX_TAB_1];
                spriteIndex = SPR_TAB_OBJECTIVE_0;
                if (page == WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_OBJECTIVE)
                    spriteIndex += (currentFrame / 4) % 16;

                GfxDrawSprite(rt, ImageId(spriteIndex), windowPos + ScreenCoordsXY{ widget->left, widget->top });
            }

            // Tab 2
            if (!isWidgetDisabled(WIDX_TAB_2))
            {
                widget = &widgets[WIDX_TAB_2];
                spriteIndex = SPR_TAB_KIOSKS_AND_FACILITIES_0;
                if (page == WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_SCENARIO_DETAILS)
                    spriteIndex += (currentFrame / 4) % 8;

                GfxDrawSprite(rt, ImageId(spriteIndex), windowPos + ScreenCoordsXY{ widget->left, widget->top });
            }

            // Tab 3
            if (!isWidgetDisabled(WIDX_TAB_3))
            {
                widget = &widgets[WIDX_TAB_3];
                spriteIndex = SPR_TAB_FINANCES_SUMMARY_0;
                if (page == WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_FINANCIAL)
                    spriteIndex += (currentFrame / 2) % 8;

                GfxDrawSprite(rt, ImageId(spriteIndex), windowPos + ScreenCoordsXY{ widget->left, widget->top });
            }

            // Tab 4
            if (!isWidgetDisabled(WIDX_TAB_4))
            {
                widget = &widgets[WIDX_TAB_4];
                spriteIndex = SPR_TAB_GUESTS_0;
                if (page == WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_GUESTS)
                    spriteIndex += (currentFrame / 4) % 8;

                GfxDrawSprite(rt, ImageId(spriteIndex), windowPos + ScreenCoordsXY{ widget->left, widget->top });
            }

            // Tab 5
            if (!isWidgetDisabled(WIDX_TAB_5))
            {
                widget = &widgets[WIDX_TAB_5];
                spriteIndex = SPR_G2_MAP_GEN_TERRAIN_TAB;
                GfxDrawSprite(rt, ImageId(spriteIndex), windowPos + ScreenCoordsXY{ widget->left, widget->top });
            }

            // Tab 6
            if (!isWidgetDisabled(WIDX_TAB_6))
            {
                widget = &widgets[WIDX_TAB_6];
                spriteIndex = SPR_TAB_RIDE_0;
                if (page == WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_RIDES)
                    spriteIndex += (currentFrame / 4) % 16;

                GfxDrawSprite(rt, ImageId(spriteIndex), windowPos + ScreenCoordsXY{ widget->left, widget->top });
            }
        }

        void setPage(int32_t newPage)
        {
            // Skip setting page if we're already on this page, unless we're initialising the window
            if (page == newPage && !widgets.empty())
                return;

            page = newPage;
            currentFrame = 0;

            setWidgets(window_editor_scenario_options_widgets[page]);
            invalidate();
            onResize();
            onPrepareDraw();
            initScrollWidgets();
            invalidate();
        }

#pragma region Objective

        /**
         *
         *  rct2: 0x0067201D
         */
        void SetObjective(Scenario::ObjectiveType objective)
        {
            auto& gameState = getGameState();
            auto& scenarioOptions = gameState.scenarioOptions;

            scenarioOptions.objective.Type = objective;
            invalidate();

            // Set default objective arguments
            switch (objective)
            {
                case Scenario::ObjectiveType::none:
                case Scenario::ObjectiveType::haveFun:
                case Scenario::ObjectiveType::buildTheBest:
                case Scenario::ObjectiveType::tenRollercoasters:
                    break;
                case Scenario::ObjectiveType::guestsBy:
                    scenarioOptions.objective.Year = 3;
                    scenarioOptions.objective.NumGuests = 1500;
                    break;
                case Scenario::ObjectiveType::parkValueBy:
                    scenarioOptions.objective.Year = 3;
                    scenarioOptions.objective.Currency = 50000.00_GBP;
                    break;
                case Scenario::ObjectiveType::guestsAndRating:
                    scenarioOptions.objective.NumGuests = 2000;
                    break;
                case Scenario::ObjectiveType::monthlyRideIncome:
                    scenarioOptions.objective.Currency = 10000.00_GBP;
                    break;
                case Scenario::ObjectiveType::tenRollercoastersLength:
                    scenarioOptions.objective.MinimumLength = 1200;
                    break;
                case Scenario::ObjectiveType::finishFiveRollercoasters:
                    scenarioOptions.objective.MinimumExcitement = RideRating::make(6, 70);
                    break;
                case Scenario::ObjectiveType::repayLoanAndParkValue:
                    scenarioOptions.objective.Currency = 50000.00_GBP;
                    break;
                case Scenario::ObjectiveType::monthlyFoodIncome:
                    scenarioOptions.objective.Currency = 1000.00_GBP;
                    break;
                default:
                    break;
            }
        }

        void ShowObjectiveDropdown()
        {
            const auto& gameState = getGameState();
            const auto& park = gameState.park;
            const auto& scenarioOptions = gameState.scenarioOptions;

            auto objectiveType = EnumValue(scenarioOptions.objective.Type);

            int32_t numItems = 0;
            for (auto i = 0; i < EnumValue(Scenario::ObjectiveType::count); i++)
            {
                auto obj = Scenario::ObjectiveType(i);
                if (obj == Scenario::ObjectiveType::none || obj == Scenario::ObjectiveType::buildTheBest)
                    continue;

                const bool objectiveAllowedByMoneyUsage = !gameState.park.flags.has(ParkFlag::noMoney)
                    || !Scenario::ObjectiveNeedsMoney(obj);

                // This objective can only work if the player can ask money for rides.
                const bool objectiveAllowedByPaymentSettings = (obj != Scenario::ObjectiveType::monthlyRideIncome)
                    || Park::RidePricesUnlocked(park);

                if (objectiveAllowedByMoneyUsage && objectiveAllowedByPaymentSettings)
                {
                    gDropdown.items[numItems] = Dropdown::MenuLabel(ObjectiveDropdownOptionNames[i]);
                    gDropdown.items[numItems].value = i;
                    if (i == objectiveType)
                    {
                        gDropdown.items[numItems].setChecked(true);
                    }
                    numItems++;
                }
            }

            Widget* dropdownWidget = &widgets[WIDX_OBJECTIVE];
            WindowDropdownShowTextCustomWidth(
                { windowPos.x + dropdownWidget->left, windowPos.y + dropdownWidget->top }, dropdownWidget->height(), colours[1],
                0, {}, numItems, dropdownWidget->width() - 4);
        }

        void ShowCategoryDropdown()
        {
            for (int32_t i = EnumValue(Scenario::Category::beginner); i <= EnumValue(Scenario::Category::other); i++)
            {
                gDropdown.items[i] = Dropdown::MenuLabel(Scenario::kScenarioCategoryStringIds[i]);
            }

            Widget* dropdownWidget = &widgets[WIDX_CATEGORY];
            WindowDropdownShowTextCustomWidth(
                { windowPos.x + dropdownWidget->left, windowPos.y + dropdownWidget->top }, dropdownWidget->height(), colours[1],
                0, {}, 5, dropdownWidget->width() - 4);

            gDropdown.items[EnumValue(getGameState().scenarioOptions.category)].setChecked(true);
        }

        void Arg1Increase()
        {
            auto& gameState = getGameState();
            auto& scenarioOptions = gameState.scenarioOptions;

            switch (scenarioOptions.objective.Type)
            {
                case Scenario::ObjectiveType::parkValueBy:
                case Scenario::ObjectiveType::monthlyRideIncome:
                case Scenario::ObjectiveType::repayLoanAndParkValue:
                    if (scenarioOptions.objective.Currency >= kObjectiveCurrencyLoanAndValueMax)
                    {
                        ContextShowError(STR_CANT_INCREASE_FURTHER, kStringIdNone, {});
                    }
                    else
                    {
                        scenarioOptions.objective.Currency += kObjectiveCurrencyLoanAndValueAdjustment;
                        invalidate();
                    }
                    break;
                case Scenario::ObjectiveType::monthlyFoodIncome:
                    if (scenarioOptions.objective.Currency >= kObjectiveCurrencyFoodMax)
                    {
                        ContextShowError(STR_CANT_INCREASE_FURTHER, kStringIdNone, {});
                    }
                    else
                    {
                        scenarioOptions.objective.Currency += kObjectiveCurrencyFoodAdjustment;
                        invalidate();
                    }
                    break;
                case Scenario::ObjectiveType::tenRollercoastersLength:
                    if (scenarioOptions.objective.MinimumLength >= kObjectiveLengthMax)
                    {
                        ContextShowError(STR_CANT_INCREASE_FURTHER, kStringIdNone, {});
                    }
                    else
                    {
                        scenarioOptions.objective.MinimumLength += kObjectiveLengthAdjustment;
                        invalidate();
                    }
                    break;
                case Scenario::ObjectiveType::finishFiveRollercoasters:
                    if (scenarioOptions.objective.MinimumExcitement >= kObjectiveExcitementMax)
                    {
                        ContextShowError(STR_CANT_INCREASE_FURTHER, kStringIdNone, {});
                    }
                    else
                    {
                        scenarioOptions.objective.MinimumExcitement += kObjectiveExcitementAdjustment;
                        invalidate();
                    }
                    break;
                default:
                    if (scenarioOptions.objective.NumGuests >= kObjectiveGuestsMax)
                    {
                        ContextShowError(STR_CANT_INCREASE_FURTHER, kStringIdNone, {});
                    }
                    else
                    {
                        scenarioOptions.objective.NumGuests += kObjectiveGuestsAdjustment;
                        invalidate();
                    }
                    break;
            }
        }

        void Arg1Decrease()
        {
            auto& gameState = getGameState();
            auto& scenarioOptions = gameState.scenarioOptions;

            switch (scenarioOptions.objective.Type)
            {
                case Scenario::ObjectiveType::parkValueBy:
                case Scenario::ObjectiveType::monthlyRideIncome:
                case Scenario::ObjectiveType::repayLoanAndParkValue:
                    if (scenarioOptions.objective.Currency <= kObjectiveCurrencyLoanAndValueMin)
                    {
                        ContextShowError(STR_CANT_REDUCE_FURTHER, kStringIdNone, {});
                    }
                    else
                    {
                        scenarioOptions.objective.Currency -= kObjectiveCurrencyLoanAndValueAdjustment;
                        invalidate();
                    }
                    break;
                case Scenario::ObjectiveType::monthlyFoodIncome:
                    if (scenarioOptions.objective.Currency <= kObjectiveCurrencyFoodMin)
                    {
                        ContextShowError(STR_CANT_REDUCE_FURTHER, kStringIdNone, {});
                    }
                    else
                    {
                        scenarioOptions.objective.Currency -= kObjectiveCurrencyFoodAdjustment;
                        invalidate();
                    }
                    break;
                case Scenario::ObjectiveType::tenRollercoastersLength:
                    if (scenarioOptions.objective.MinimumLength <= kObjectiveLengthMin)
                    {
                        ContextShowError(STR_CANT_REDUCE_FURTHER, kStringIdNone, {});
                    }
                    else
                    {
                        scenarioOptions.objective.MinimumLength -= kObjectiveLengthAdjustment;
                        invalidate();
                    }
                    break;
                case Scenario::ObjectiveType::finishFiveRollercoasters:
                    if (scenarioOptions.objective.MinimumExcitement <= kObjectiveExcitementMin)
                    {
                        ContextShowError(STR_CANT_REDUCE_FURTHER, kStringIdNone, {});
                    }
                    else
                    {
                        scenarioOptions.objective.MinimumExcitement -= kObjectiveExcitementAdjustment;
                        invalidate();
                    }
                    break;
                default:
                    if (scenarioOptions.objective.NumGuests <= kObjectiveGuestsMin)
                    {
                        ContextShowError(STR_CANT_REDUCE_FURTHER, kStringIdNone, {});
                    }
                    else
                    {
                        scenarioOptions.objective.NumGuests -= kObjectiveGuestsAdjustment;
                        invalidate();
                    }
                    break;
            }
        }

        void Arg2Increase()
        {
            auto& gameState = getGameState();
            auto& scenarioOptions = gameState.scenarioOptions;

            if (scenarioOptions.objective.Year >= kObjectiveYearMax)
            {
                ContextShowError(STR_CANT_INCREASE_FURTHER, kStringIdNone, {});
            }
            else
            {
                scenarioOptions.objective.Year += kObjectiveYearAdjustment;
                invalidate();
            }
        }

        void Arg2Decrease()
        {
            auto& gameState = getGameState();
            auto& scenarioOptions = gameState.scenarioOptions;

            if (scenarioOptions.objective.Year <= kObjectiveYearMin)
            {
                ContextShowError(STR_CANT_REDUCE_FURTHER, kStringIdNone, {});
            }
            else
            {
                scenarioOptions.objective.Year -= kObjectiveYearAdjustment;
                invalidate();
            }
        }

        /**
         *
         *  rct2: 0x00672254
         */
        void ObjectiveOnResize()
        {
            WindowSetResize(*this, kSizeObjective, kSizeObjective);
        }

        void ObjectiveOnMouseUp(WidgetIndex widgetIndex)
        {
            switch (widgetIndex)
            {
                case WIDX_HARD_PARK_RATING:
                {
                    auto& gameState = getGameState();
                    auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                        GameActions::ScenarioSetSetting::parkRatingHigherDifficultyLevel,
                        gameState.park.flags.has(ParkFlag::difficultParkRating) ? 0 : 1);
                    GameActions::Execute(&scenarioSetSetting, gameState);
                    invalidate();
                    break;
                }
            }
        }

        /**
         *
         *  rct2: 0x00671A0D
         */
        void ObjectiveOnMouseDown(WidgetIndex widgetIndex)
        {
            switch (widgetIndex)
            {
                case WIDX_OBJECTIVE_DROPDOWN:
                    ShowObjectiveDropdown();
                    break;
                case WIDX_OBJECTIVE_ARG_1_INCREASE:
                    Arg1Increase();
                    break;
                case WIDX_OBJECTIVE_ARG_1_DECREASE:
                    Arg1Decrease();
                    break;
                case WIDX_OBJECTIVE_ARG_2_INCREASE:
                    Arg2Increase();
                    break;
                case WIDX_OBJECTIVE_ARG_2_DECREASE:
                    Arg2Decrease();
                    break;
            }
        }

        /**
         *
         *  rct2: 0x00671A54
         */
        void ObjectiveOnDropdown(WidgetIndex widgetIndex, int32_t dropdownIndex)
        {
            auto& gameState = getGameState();

            if (dropdownIndex == -1)
                return;

            switch (widgetIndex)
            {
                case WIDX_OBJECTIVE_DROPDOWN:
                    auto newObjectiveType = static_cast<Scenario::ObjectiveType>(gDropdown.items[dropdownIndex].value);
                    if (gameState.scenarioOptions.objective.Type != newObjectiveType)
                        SetObjective(newObjectiveType);
                    break;
            }
        }

        /**
         *
         *  rct2: 0x006721E7
         */
        void ObjectiveOnUpdate()
        {
            currentFrame++;
            onPrepareDraw();
            invalidateWidget(WIDX_TAB_1);

            auto& gameState = getGameState();
            auto& park = gameState.park;

            auto objectiveType = gameState.scenarioOptions.objective.Type;

            // Check if objective is allowed by money and pay-per-ride settings.
            const bool objectiveAllowedByMoneyUsage = !park.flags.has(ParkFlag::noMoney) || !ObjectiveNeedsMoney(objectiveType);

            // This objective can only work if the player can ask money for rides.
            const bool objectiveAllowedByPaymentSettings = (objectiveType != Scenario::ObjectiveType::monthlyRideIncome)
                || Park::RidePricesUnlocked(park);

            if (!objectiveAllowedByMoneyUsage || !objectiveAllowedByPaymentSettings)
            {
                // Reset objective
                SetObjective(Scenario::ObjectiveType::guestsAndRating);
            }
        }

        /**
         *
         *  rct2: 0x0067161C
         */
        void ObjectiveOnPrepareDraw()
        {
            auto& gameState = getGameState();

            SetPressedTab();

            bool hasPrimaryObjectiveArg = false;
            bool hasSecondaryObjectiveArg = false;

            switch (gameState.scenarioOptions.objective.Type)
            {
                case Scenario::ObjectiveType::guestsBy:
                case Scenario::ObjectiveType::parkValueBy:
                    hasPrimaryObjectiveArg = true;
                    hasSecondaryObjectiveArg = true;
                    break;
                case Scenario::ObjectiveType::guestsAndRating:
                case Scenario::ObjectiveType::monthlyRideIncome:
                case Scenario::ObjectiveType::tenRollercoastersLength:
                case Scenario::ObjectiveType::finishFiveRollercoasters:
                case Scenario::ObjectiveType::repayLoanAndParkValue:
                case Scenario::ObjectiveType::monthlyFoodIncome:
                    hasPrimaryObjectiveArg = true;
                    break;
                default:
                    break;
            }

            widgets[WIDX_OBJECTIVE_ARG_1_LABEL].setVisible(hasPrimaryObjectiveArg);
            widgets[WIDX_OBJECTIVE_ARG_1].setVisible(hasPrimaryObjectiveArg);
            widgets[WIDX_OBJECTIVE_ARG_1_INCREASE].setVisible(hasPrimaryObjectiveArg);
            widgets[WIDX_OBJECTIVE_ARG_1_DECREASE].setVisible(hasPrimaryObjectiveArg);

            widgets[WIDX_OBJECTIVE_ARG_2_LABEL].setVisible(hasSecondaryObjectiveArg);
            widgets[WIDX_OBJECTIVE_ARG_2].setVisible(hasSecondaryObjectiveArg);
            widgets[WIDX_OBJECTIVE_ARG_2_INCREASE].setVisible(hasSecondaryObjectiveArg);
            widgets[WIDX_OBJECTIVE_ARG_2_DECREASE].setVisible(hasSecondaryObjectiveArg);

            auto arg1StringId = kStringIdEmpty;
            if (widgets[WIDX_OBJECTIVE_ARG_1_LABEL].isVisible())
            {
                // Objective argument 1 label
                switch (gameState.scenarioOptions.objective.Type)
                {
                    case Scenario::ObjectiveType::guestsBy:
                    case Scenario::ObjectiveType::guestsAndRating:
                        arg1StringId = STR_WINDOW_OBJECTIVE_GUEST_COUNT;
                        break;
                    case Scenario::ObjectiveType::parkValueBy:
                    case Scenario::ObjectiveType::repayLoanAndParkValue:
                        arg1StringId = STR_WINDOW_OBJECTIVE_PARK_VALUE;
                        break;
                    case Scenario::ObjectiveType::monthlyRideIncome:
                        arg1StringId = STR_WINDOW_OBJECTIVE_MONTHLY_INCOME;
                        break;
                    case Scenario::ObjectiveType::monthlyFoodIncome:
                        arg1StringId = STR_WINDOW_OBJECTIVE_MONTHLY_PROFIT;
                        break;
                    case Scenario::ObjectiveType::tenRollercoastersLength:
                        arg1StringId = STR_WINDOW_OBJECTIVE_MINIMUM_LENGTH;
                        break;
                    default:
                        arg1StringId = STR_WINDOW_OBJECTIVE_EXCITEMENT_RATING;
                        break;
                }
            }

            widgets[WIDX_OBJECTIVE_ARG_1_LABEL].text = arg1StringId;

            widgets[WIDX_CLOSE].setHidden(gLegacyScene == LegacyScene::scenarioEditor);

            setWidgetPressed(WIDX_HARD_PARK_RATING, gameState.park.flags.has(ParkFlag::difficultParkRating));
        }

        /**
         *
         *  rct2: 0x0067161C
         */
        void ObjectiveOnDraw(RenderTarget& rt)
        {
            const auto& gameState = getGameState();
            const auto& scenarioOptions = gameState.scenarioOptions;

            drawWidgets(rt);
            DrawTabImages(rt);

            // Objective value
            auto screenCoords = windowPos + ScreenCoordsXY{ widgets[WIDX_OBJECTIVE].left + 1, widgets[WIDX_OBJECTIVE].top + 1 };
            auto ft = Formatter();
            ft.Add<StringId>(ObjectiveDropdownOptionNames[EnumValue(scenarioOptions.objective.Type)]);
            drawText(rt, screenCoords, STR_WINDOW_COLOUR_2_STRINGID, ft);

            if (widgets[WIDX_OBJECTIVE_ARG_1].isVisible())
            {
                const auto wColour2 = colours[1];
                StringId stringId = kStringIdEmpty;

                // Objective argument 1 value
                screenCoords = windowPos
                    + ScreenCoordsXY{ widgets[WIDX_OBJECTIVE_ARG_1].left + 1, widgets[WIDX_OBJECTIVE_ARG_1].top + 1 };
                ft = Formatter();
                switch (scenarioOptions.objective.Type)
                {
                    case Scenario::ObjectiveType::guestsBy:
                    case Scenario::ObjectiveType::guestsAndRating:
                        stringId = STR_WINDOW_COLOUR_2_COMMA32;
                        ft.Add<int32_t>(scenarioOptions.objective.NumGuests);
                        break;
                    case Scenario::ObjectiveType::parkValueBy:
                    case Scenario::ObjectiveType::repayLoanAndParkValue:
                    case Scenario::ObjectiveType::monthlyRideIncome:
                    case Scenario::ObjectiveType::monthlyFoodIncome:
                        stringId = STR_CURRENCY_FORMAT_LABEL;
                        ft.Add<money64>(scenarioOptions.objective.Currency);
                        break;
                    case Scenario::ObjectiveType::tenRollercoastersLength:
                        stringId = STR_WINDOW_COLOUR_2_LENGTH;
                        ft.Add<uint16_t>(scenarioOptions.objective.MinimumLength);
                        break;
                    case Scenario::ObjectiveType::finishFiveRollercoasters:
                        stringId = STR_WINDOW_COLOUR_2_COMMA2DP32;
                        ft.Add<uint16_t>(scenarioOptions.objective.MinimumExcitement);
                        break;
                    default:
                        stringId = STR_WINDOW_COLOUR_2_COMMA2DP32;
                        ft.Add<money64>(scenarioOptions.objective.Currency);
                        break;
                }
                drawText(rt, screenCoords, stringId, ft, wColour2);
            }

            if (widgets[WIDX_OBJECTIVE_ARG_2].isVisible())
            {
                // Objective argument 2 value
                screenCoords = windowPos
                    + ScreenCoordsXY{ widgets[WIDX_OBJECTIVE_ARG_2].left + 1, widgets[WIDX_OBJECTIVE_ARG_2].top + 1 };
                ft = Formatter();
                ft.Add<uint16_t>((scenarioOptions.objective.Year * MONTH_COUNT) - 1);
                drawText(rt, screenCoords, STR_WINDOW_OBJECTIVE_VALUE_DATE, ft);
            }
        }

#pragma endregion

#pragma region ScenarioDetails

        /**
         *
         *  rct2: 0x006719CA
         */
        void ScenarioDetailsOnMouseUp(WidgetIndex widgetIndex)
        {
            const auto& gameState = getGameState();
            switch (widgetIndex)
            {
                case WIDX_PARK_NAME:
                {
                    WindowTextInputRawOpen(
                        this, WIDX_PARK_NAME, STR_PARK_NAME, STR_ENTER_PARK_NAME, {}, gameState.park.name.c_str(),
                        kParkNameMaxLength);
                    break;
                }
                case WIDX_SCENARIO_NAME:
                    WindowTextInputRawOpen(
                        this, WIDX_SCENARIO_NAME, STR_SCENARIO_NAME, STR_ENTER_SCENARIO_NAME, {},
                        gameState.scenarioOptions.name.c_str(), kScenarioNameMaxLength);
                    break;
                case WIDX_DETAILS:
                    WindowTextInputRawOpen(
                        this, WIDX_DETAILS, STR_PARK_SCENARIO_DETAILS, STR_ENTER_SCENARIO_DESCRIPTION, {},
                        gameState.scenarioOptions.details.c_str(), kScenarioDetailsNameMaxLength);
                    break;
            }
        }

        void ScenarioDetailsOnMouseDown(WidgetIndex widgetIndex)
        {
            switch (widgetIndex)
            {
                case WIDX_CATEGORY_DROPDOWN:
                    ShowCategoryDropdown();
                    break;
            }
        }

        void ScenarioDetailsOnDropdown(WidgetIndex widgetIndex, int32_t dropdownIndex)
        {
            if (dropdownIndex == -1)
                return;

            switch (widgetIndex)
            {
                case WIDX_CATEGORY_DROPDOWN:
                {
                    auto& gameState = getGameState();
                    if (gameState.scenarioOptions.category != static_cast<Scenario::Category>(dropdownIndex))
                    {
                        gameState.scenarioOptions.category = static_cast<Scenario::Category>(dropdownIndex);
                        invalidate();
                    }
                    break;
                }
            }
        }
        void ScenarioDetailsOnPrepareDraw()
        {
            SetPressedTab();
        }

        void ScenarioDetailsOnDraw(RenderTarget& rt)
        {
            drawWidgets(rt);
            DrawTabImages(rt);

            const auto& gameState = getGameState();

            // Land name
            auto screenCoords = windowPos + ScreenCoordsXY{ 8, widgets[WIDX_PARK_NAME].top + 1 };
            int32_t widthToSet = widgets[WIDX_PARK_NAME].left - 16;

            {
                auto parkName = gameState.park.name.c_str();

                auto ft = Formatter();
                ft.Add<StringId>(STR_STRING);
                ft.Add<const char*>(parkName);
                drawTextEllipsised(rt, screenCoords, widthToSet, STR_WINDOW_PARK_NAME, ft);
            }

            // Scenario name
            screenCoords = windowPos + ScreenCoordsXY{ 8, widgets[WIDX_SCENARIO_NAME].top + 1 };
            widthToSet = widgets[WIDX_SCENARIO_NAME].left - 16;

            auto ft = Formatter();
            ft.Add<StringId>(STR_STRING);
            ft.Add<const char*>(gameState.scenarioOptions.name.c_str());
            drawTextEllipsised(rt, screenCoords, widthToSet, STR_WINDOW_SCENARIO_NAME, ft);

            // Scenario details label
            screenCoords = windowPos + ScreenCoordsXY{ 8, widgets[WIDX_DETAILS].top + 1 };
            drawText(rt, screenCoords, STR_WINDOW_PARK_DETAILS);

            // Scenario details value
            screenCoords = windowPos + ScreenCoordsXY{ 16, widgets[WIDX_DETAILS].top + 10 };
            widthToSet = widgets[WIDX_DETAILS].left - 4;

            ft = Formatter();
            ft.Add<StringId>(STR_STRING);
            ft.Add<const char*>(gameState.scenarioOptions.details.c_str());
            drawTextWrapped(rt, screenCoords, widthToSet, STR_BLACK_STRING, ft);

            // Scenario category label
            screenCoords = windowPos + ScreenCoordsXY{ 8, widgets[WIDX_CATEGORY].top + 1 };
            drawText(rt, screenCoords, STR_WINDOW_SCENARIO_GROUP);

            // Scenario category value
            screenCoords = windowPos + ScreenCoordsXY{ widgets[WIDX_CATEGORY].left + 1, widgets[WIDX_CATEGORY].top + 1 };
            ft = Formatter();
            ft.Add<StringId>(Scenario::kScenarioCategoryStringIds[EnumValue(gameState.scenarioOptions.category)]);
            drawText(rt, screenCoords, STR_WINDOW_COLOUR_2_STRINGID, ft);
        }

        /**
         *
         *  rct2: 0x00671A73
         */
        void ScenarioDetailsOnTextInput(WidgetIndex widgetIndex, std::string_view text)
        {
            if (text.empty())
                return;

            auto& gameState = getGameState();
            auto& scenarioOptions = gameState.scenarioOptions;

            switch (widgetIndex)
            {
                case WIDX_PARK_NAME:
                {
                    auto action = GameActions::ParkSetNameAction(std::string(text));
                    GameActions::Execute(&action, gameState);

                    if (scenarioOptions.name.empty())
                    {
                        scenarioOptions.name = gameState.park.name;
                    }
                    break;
                }
                case WIDX_SCENARIO_NAME:
                    scenarioOptions.name = text;
                    invalidate();
                    break;
                case WIDX_DETAILS:
                    scenarioOptions.details = text;
                    invalidate();
                    break;
            }
        }

        void ScenarioDetailsOnUpdate()
        {
            currentFrame++;
            invalidateWidget(WIDX_TAB_1);
        }

        void ScenarioDetailsOnResize()
        {
            WindowSetResize(*this, kSizeScenarioDetails, kSizeScenarioDetails);
        }

#pragma endregion

#pragma region Financial

        void FinancialMouseUp(WidgetIndex widgetIndex)
        {
            auto& gameState = getGameState();
            switch (widgetIndex)
            {
                case WIDX_NO_MONEY:
                {
                    auto newMoneySetting = gameState.park.flags.has(ParkFlag::noMoney) ? 0 : 1;
                    auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                        GameActions::ScenarioSetSetting::noMoney, newMoneySetting);
                    GameActions::Execute(&scenarioSetSetting, gameState);
                    invalidate();
                    break;
                }
                case WIDX_FORBID_MARKETING:
                {
                    auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                        GameActions::ScenarioSetSetting::forbidMarketingCampaigns,
                        gameState.park.flags.has(ParkFlag::forbidMarketingCampaigns) ? 0 : 1);
                    GameActions::Execute(&scenarioSetSetting, gameState);
                    invalidate();
                    break;
                }
                case WIDX_RCT1_INTEREST:
                {
                    auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                        GameActions::ScenarioSetSetting::useRCT1Interest,
                        gameState.park.flags.has(ParkFlag::rct1Interest) ? 0 : 1);
                    GameActions::Execute(&scenarioSetSetting, gameState);
                    invalidate();
                    break;
                }
            }
        }

        void FinancialResize()
        {
            WindowSetResize(*this, kSizeFinancial, kSizeFinancial);
        }

        void FinancialMouseDown(WidgetIndex widgetIndex)
        {
            auto& gameState = getGameState();
            switch (widgetIndex)
            {
                case WIDX_INITIAL_CASH_INCREASE:
                    if (gameState.scenarioOptions.initialCash < 1000000.00_GBP)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::initialCash, gameState.scenarioOptions.initialCash + 500.00_GBP);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_INCREASE_CASH, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_INITIAL_CASH_DECREASE:
                    if (gameState.scenarioOptions.initialCash > 0.00_GBP)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::initialCash, gameState.scenarioOptions.initialCash - 500.00_GBP);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_REDUCE_CASH, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_INITIAL_LOAN_INCREASE:
                    if (gameState.park.bankLoan < 5000000.00_GBP)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::initialLoan, gameState.park.bankLoan + 1000.00_GBP);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_INCREASE_INIT_LOAN, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_INITIAL_LOAN_DECREASE:
                    if (gameState.park.bankLoan > 0.00_GBP)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::initialLoan, gameState.park.bankLoan - 1000.00_GBP);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_REDUCE_INIT_LOAN, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_MAXIMUM_LOAN_INCREASE:
                    if (gameState.park.maxBankLoan < 5000000.00_GBP)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::maximumLoanSize, gameState.park.maxBankLoan + 1000.00_GBP);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_INCREASE_MAX_LOAN, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_MAXIMUM_LOAN_DECREASE:
                    if (gameState.park.maxBankLoan > 0.00_GBP)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::maximumLoanSize, gameState.park.maxBankLoan - 1000.00_GBP);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_REDUCE_MAX_LOAN, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_INTEREST_RATE_INCREASE:
                    if (gameState.park.bankLoanInterestRate < kMaxBankLoanInterestRate)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::annualInterestRate, gameState.park.bankLoanInterestRate + 1);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_INCREASE_INTEREST_RATE, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_INTEREST_RATE_DECREASE:
                    if (gameState.park.bankLoanInterestRate > 0)
                    {
                        auto interest = std::min<uint8_t>(kMaxBankLoanInterestRate, gameState.park.bankLoanInterestRate - 1);
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::annualInterestRate, interest);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_REDUCE_INTEREST_RATE, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_ENTRY_PRICE_INCREASE:
                    if (gameState.park.entranceFee < kMaxEntranceFee)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::parkChargeEntryFee, gameState.park.entranceFee + 1.00_GBP);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_INCREASE_FURTHER, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_ENTRY_PRICE_DECREASE:
                    if (gameState.park.entranceFee > 0.00_GBP)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::parkChargeEntryFee, gameState.park.entranceFee - 1.00_GBP);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_REDUCE_FURTHER, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_PAY_FOR_PARK_OR_RIDES_DROPDOWN:
                {
                    Widget* dropdownWidget = &widgets[widgetIndex - 1];

                    gDropdown.items[0] = Dropdown::MenuLabel(STR_FREE_PARK_ENTER);
                    gDropdown.items[1] = Dropdown::MenuLabel(STR_PAY_PARK_ENTER);
                    gDropdown.items[2] = Dropdown::MenuLabel(STR_PAID_ENTRY_PAID_RIDES);

                    WindowDropdownShowTextCustomWidth(
                        { windowPos.x + dropdownWidget->left, windowPos.y + dropdownWidget->top }, dropdownWidget->height() - 2,
                        colours[1], 0, {}, 3, dropdownWidget->width() - 4);

                    if (gameState.park.flags.has(ParkFlag::unlockAllPrices))
                        gDropdown.items[2].setChecked(true);
                    else if (gameState.park.flags.has(ParkFlag::freeEntry))
                        gDropdown.items[0].setChecked(true);
                    else
                        gDropdown.items[1].setChecked(true);

                    break;
                }
            }

            if (gLegacyScene == LegacyScene::playing)
            {
                auto* windowMgr = GetWindowManager();
                windowMgr->InvalidateByClass(WindowClass::finances);
                windowMgr->InvalidateByClass(WindowClass::parkInfoPanel);
            }
        }

        void FinancialDropdown(WidgetIndex widgetIndex, int32_t dropdownIndex)
        {
            if (dropdownIndex == -1)
            {
                return;
            }

            switch (widgetIndex)
            {
                case WIDX_PAY_FOR_PARK_OR_RIDES_DROPDOWN:
                {
                    auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                        GameActions::ScenarioSetSetting::parkChargeMethod, dropdownIndex);
                    GameActions::Execute(&scenarioSetSetting, getGameState());
                    invalidate();
                    break;
                }
            }
        }

        void FinancialUpdate()
        {
            currentFrame++;
            invalidateWidget(WIDX_TAB_1);
        }

        void FinancialPrepareDraw()
        {
            SetPressedTab();

            auto& gameState = getGameState();
            auto& park = gameState.park;

            bool noMoney = park.flags.has(ParkFlag::noMoney);
            setWidgetPressed(WIDX_NO_MONEY, noMoney);

            setWidgetDisabled(WIDX_GROUP_LOAN, noMoney);
            setWidgetDisabled(WIDX_INITIAL_LOAN_LABEL, noMoney);
            setWidgetDisabled(WIDX_INITIAL_LOAN, noMoney);
            setWidgetDisabled(WIDX_INITIAL_LOAN_INCREASE, noMoney);
            setWidgetDisabled(WIDX_INITIAL_LOAN_DECREASE, noMoney);
            setWidgetDisabled(WIDX_MAXIMUM_LOAN_LABEL, noMoney);
            setWidgetDisabled(WIDX_MAXIMUM_LOAN, noMoney);
            setWidgetDisabled(WIDX_MAXIMUM_LOAN_INCREASE, noMoney);
            setWidgetDisabled(WIDX_MAXIMUM_LOAN_DECREASE, noMoney);

            const bool rct1Interest = park.flags.has(ParkFlag::rct1Interest);
            widgets[WIDX_RCT1_INTEREST].setVisible(rct1Interest);
            widgets[WIDX_INTEREST_RATE_LABEL].setHidden(rct1Interest);
            widgets[WIDX_INTEREST_RATE].setHidden(rct1Interest);
            widgets[WIDX_INTEREST_RATE_INCREASE].setHidden(rct1Interest);
            widgets[WIDX_INTEREST_RATE_DECREASE].setHidden(rct1Interest);

            if (rct1Interest)
            {
                setWidgetPressed(WIDX_RCT1_INTEREST, true);
                setWidgetDisabled(WIDX_RCT1_INTEREST, noMoney);
            }
            else
            {
                setWidgetDisabled(WIDX_INTEREST_RATE_LABEL, noMoney);
                setWidgetDisabled(WIDX_INTEREST_RATE, noMoney);
                setWidgetDisabled(WIDX_INTEREST_RATE_INCREASE, noMoney);
                setWidgetDisabled(WIDX_INTEREST_RATE_DECREASE, noMoney);
            }

            setWidgetDisabled(WIDX_GROUP_BUSINESS_MODEL, noMoney);
            setWidgetDisabled(WIDX_INITIAL_CASH_LABEL, noMoney);
            setWidgetDisabled(WIDX_INITIAL_CASH, noMoney);
            setWidgetDisabled(WIDX_INITIAL_CASH_INCREASE, noMoney);
            setWidgetDisabled(WIDX_INITIAL_CASH_DECREASE, noMoney);
            setWidgetDisabled(WIDX_PAY_FOR_PARK_OR_RIDES_LABEL, noMoney);
            setWidgetDisabled(WIDX_PAY_FOR_PARK_OR_RIDES, noMoney);
            setWidgetDisabled(WIDX_PAY_FOR_PARK_OR_RIDES_DROPDOWN, noMoney);
            setWidgetDisabled(WIDX_FORBID_MARKETING, noMoney);

            const bool entranceFeeUnlocked = Park::EntranceFeeUnlocked(park);
            widgets[WIDX_ENTRY_PRICE_LABEL].setVisible(entranceFeeUnlocked);
            widgets[WIDX_ENTRY_PRICE].setVisible(entranceFeeUnlocked);
            widgets[WIDX_ENTRY_PRICE_INCREASE].setVisible(entranceFeeUnlocked);
            widgets[WIDX_ENTRY_PRICE_DECREASE].setVisible(entranceFeeUnlocked);

            if (entranceFeeUnlocked)
            {
                setWidgetDisabled(WIDX_ENTRY_PRICE_LABEL, noMoney);
                setWidgetDisabled(WIDX_ENTRY_PRICE, noMoney);
                setWidgetDisabled(WIDX_ENTRY_PRICE_INCREASE, noMoney);
                setWidgetDisabled(WIDX_ENTRY_PRICE_DECREASE, noMoney);
            }

            setWidgetPressed(WIDX_FORBID_MARKETING, park.flags.has(ParkFlag::forbidMarketingCampaigns));

            widgets[WIDX_CLOSE].setHidden(gLegacyScene == LegacyScene::scenarioEditor);
        }

        void FinancialDraw(RenderTarget& rt)
        {
            ScreenCoordsXY screenCoords{};

            WindowDrawWidgets(*this, rt);
            DrawTabImages(rt);

            const auto& gameState = getGameState();
            const auto wColour2 = colours[1];

            const auto& initialCashWidget = widgets[WIDX_INITIAL_CASH];
            if (initialCashWidget.isVisible())
            {
                screenCoords = windowPos + ScreenCoordsXY{ initialCashWidget.left + 1, initialCashWidget.top + 1 };
                auto ft = Formatter();
                ft.Add<money64>(getGameState().scenarioOptions.initialCash);
                auto colour = !isWidgetDisabled(WIDX_INITIAL_CASH) ? wColour2 : wColour2.withFlag(ColourFlag::inset, true);
                drawText(rt, screenCoords, STR_CURRENCY_FORMAT_LABEL, ft, colour);
            }

            const auto& initialLoanWidget = widgets[WIDX_INITIAL_LOAN];
            if (initialLoanWidget.isVisible())
            {
                screenCoords = windowPos + ScreenCoordsXY{ initialLoanWidget.left + 1, initialLoanWidget.top + 1 };
                auto ft = Formatter();
                ft.Add<money64>(gameState.park.bankLoan);
                auto colour = !isWidgetDisabled(WIDX_INITIAL_LOAN) ? wColour2 : wColour2.withFlag(ColourFlag::inset, true);
                drawText(rt, screenCoords, STR_CURRENCY_FORMAT_LABEL, ft, colour);
            }

            const auto& maximumLoanWidget = widgets[WIDX_MAXIMUM_LOAN];
            if (maximumLoanWidget.isVisible())
            {
                screenCoords = windowPos + ScreenCoordsXY{ maximumLoanWidget.left + 1, maximumLoanWidget.top + 1 };
                auto ft = Formatter();
                ft.Add<money64>(getGameState().park.maxBankLoan);
                auto colour = !isWidgetDisabled(WIDX_MAXIMUM_LOAN) ? wColour2 : wColour2.withFlag(ColourFlag::inset, true);
                drawText(rt, screenCoords, STR_CURRENCY_FORMAT_LABEL, ft, colour);
            }

            const auto& interestRateWidget = widgets[WIDX_INTEREST_RATE];
            if (interestRateWidget.isVisible())
            {
                screenCoords = windowPos + ScreenCoordsXY{ interestRateWidget.left + 1, interestRateWidget.top + 1 };

                auto ft = Formatter();
                ft.Add<int16_t>(
                    std::clamp<int16_t>(static_cast<int16_t>(gameState.park.bankLoanInterestRate), INT16_MIN, INT16_MAX));
                auto colour = !isWidgetDisabled(WIDX_INTEREST_RATE) ? wColour2 : wColour2.withFlag(ColourFlag::inset, true);
                drawText(rt, screenCoords, STR_PERCENT_FORMAT_LABEL, ft, colour);
            }

            const auto& payForParkOrRidesWidget = widgets[WIDX_PAY_FOR_PARK_OR_RIDES];
            if (payForParkOrRidesWidget.isVisible())
            {
                // Pay for park or rides label
                screenCoords = windowPos + ScreenCoordsXY{ payForParkOrRidesWidget.left + 1, payForParkOrRidesWidget.top + 1 };

                auto ft = Formatter();
                // Pay for park and/or rides value
                if (gameState.park.flags.has(ParkFlag::unlockAllPrices))
                    ft.Add<StringId>(STR_PAID_ENTRY_PAID_RIDES);
                else if (gameState.park.flags.has(ParkFlag::freeEntry))
                    ft.Add<StringId>(STR_FREE_PARK_ENTER);
                else
                    ft.Add<StringId>(STR_PAY_PARK_ENTER);

                auto colour = !isWidgetDisabled(WIDX_PAY_FOR_PARK_OR_RIDES) ? wColour2
                                                                            : wColour2.withFlag(ColourFlag::inset, true);
                drawText(rt, screenCoords, STR_STRINGID, ft, colour);
            }

            const auto& entryPriceWidget = widgets[WIDX_ENTRY_PRICE];
            if (entryPriceWidget.isVisible())
            {
                // Entry price value
                screenCoords = windowPos + ScreenCoordsXY{ entryPriceWidget.left + 1, entryPriceWidget.top + 1 };
                auto ft = Formatter();
                ft.Add<money64>(gameState.park.entranceFee);
                auto colour = !isWidgetDisabled(WIDX_INITIAL_CASH) ? wColour2 : wColour2.withFlag(ColourFlag::inset, true);
                drawText(rt, screenCoords, STR_CURRENCY_FORMAT_LABEL, ft, colour);
            }
        }

#pragma endregion

#pragma region Guests

        void GuestsResize()
        {
            WindowSetResize(*this, kSizeGuests, kSizeGuests);
        }

        void GuestsMouseDown(WidgetIndex widgetIndex)
        {
            auto& gameState = getGameState();
            auto& scenarioOptions = gameState.scenarioOptions;

            switch (widgetIndex)
            {
                case WIDX_CASH_PER_GUEST_INCREASE:
                    if (scenarioOptions.guestInitialCash < 1000.00_GBP)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::averageCashPerGuest, scenarioOptions.guestInitialCash + 1.00_GBP);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_INCREASE_FURTHER, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_CASH_PER_GUEST_DECREASE:
                    if (scenarioOptions.guestInitialCash > 0.00_GBP)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::averageCashPerGuest, scenarioOptions.guestInitialCash - 1.00_GBP);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_REDUCE_FURTHER, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_GUEST_INITIAL_HAPPINESS_INCREASE:
                    if (scenarioOptions.guestInitialHappiness < 250)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::guestInitialHappiness, scenarioOptions.guestInitialHappiness + 4);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_INCREASE_FURTHER, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_GUEST_INITIAL_HAPPINESS_DECREASE:
                    if (scenarioOptions.guestInitialHappiness > 40)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::guestInitialHappiness, scenarioOptions.guestInitialHappiness - 4);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_REDUCE_FURTHER, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_GUEST_INITIAL_HUNGER_INCREASE:
                    if (scenarioOptions.guestInitialHunger > 40)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::guestInitialHunger, scenarioOptions.guestInitialHunger - 4);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_INCREASE_FURTHER, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_GUEST_INITIAL_HUNGER_DECREASE:
                    if (scenarioOptions.guestInitialHunger < 250)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::guestInitialHunger, scenarioOptions.guestInitialHunger + 4);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_REDUCE_FURTHER, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_GUEST_INITIAL_THIRST_INCREASE:
                    if (scenarioOptions.guestInitialThirst > 40)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::guestInitialThirst, scenarioOptions.guestInitialThirst - 4);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_INCREASE_FURTHER, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_GUEST_INITIAL_THIRST_DECREASE:
                    if (scenarioOptions.guestInitialThirst < 250)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::guestInitialThirst, scenarioOptions.guestInitialThirst + 4);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_REDUCE_FURTHER, kStringIdNone, {});
                    }
                    invalidate();
                    break;

                case WIDX_GUESTS_INTENSITY_PREFERENCE_DROPDOWN:
                {
                    auto& dropdownWidget = widgets[widgetIndex - 1];

                    gDropdown.items[0] = Dropdown::MenuLabel(STR_GUESTS_PREFER_INTENSITY_NONE);
                    gDropdown.items[1] = Dropdown::MenuLabel(STR_GUESTS_PREFER_INTENSITY_BALANCED);
                    gDropdown.items[2] = Dropdown::MenuLabel(STR_GUESTS_PREFER_INTENSITY_LESS_INTENSE_RIDES);
                    gDropdown.items[3] = Dropdown::MenuLabel(STR_GUESTS_PREFER_INTENSITY_MORE_INTENSE_RIDES);

                    WindowDropdownShowTextCustomWidth(
                        { windowPos.x + dropdownWidget.left, windowPos.y + dropdownWidget.top }, dropdownWidget.height() - 2,
                        colours[1], 0, {}, 4, dropdownWidget.width() - 4);

                    const auto preferLess = gameState.park.flags.has(ParkFlag::guestPreferLessIntenseRides);
                    const auto preferMore = gameState.park.flags.has(ParkFlag::guestPreferMoreIntenseRides);

                    auto prefItem = 1;
                    if (preferLess && preferMore)
                        prefItem = 0;
                    else if (preferLess && !preferMore)
                        prefItem = 2;
                    else if (!preferLess && preferMore)
                        prefItem = 3;

                    gDropdown.items[prefItem].setChecked(true);
                    break;
                }
                case WIDX_HARD_GUEST_GENERATION:
                {
                    auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                        GameActions::ScenarioSetSetting::guestGenerationHigherDifficultyLevel,
                        gameState.park.flags.has(ParkFlag::difficultGuestGeneration) ? 0 : 1);
                    GameActions::Execute(&scenarioSetSetting, gameState);
                    invalidate();
                    break;
                }
            }
        }

        void GuestsDropdown(WidgetIndex widgetIndex, int32_t dropdownIndex)
        {
            if (dropdownIndex < 0 || dropdownIndex > 3)
                return;

            bool preferLess = dropdownIndex == 0 || dropdownIndex == 2;
            bool preferMore = dropdownIndex == 0 || dropdownIndex == 3;

            auto& gameState = getGameState();

            auto scenarioSetLessSetting = GameActions::ScenarioSetSettingAction(
                GameActions::ScenarioSetSetting::guestsPreferLessIntenseRides, preferLess);
            GameActions::Execute(&scenarioSetLessSetting, gameState);

            auto scenarioSetMoreSetting = GameActions::ScenarioSetSettingAction(
                GameActions::ScenarioSetSetting::guestsPreferMoreIntenseRides, preferMore);
            GameActions::Execute(&scenarioSetMoreSetting, gameState);
        }

        void GuestsUpdate()
        {
            currentFrame++;
            invalidateWidget(WIDX_TAB_2);
        }

        void GuestsPrepareDraw()
        {
            SetPressedTab();

            auto& gameState = getGameState();
            bool noMoney = gameState.park.flags.has(ParkFlag::noMoney);

            setWidgetDisabled(WIDX_CASH_PER_GUEST_LABEL, noMoney);
            setWidgetDisabled(WIDX_CASH_PER_GUEST, noMoney);
            setWidgetDisabled(WIDX_CASH_PER_GUEST_INCREASE, noMoney);
            setWidgetDisabled(WIDX_CASH_PER_GUEST_DECREASE, noMoney);

            widgets[WIDX_CLOSE].setHidden(gLegacyScene == LegacyScene::scenarioEditor);

            setWidgetPressed(WIDX_HARD_GUEST_GENERATION, gameState.park.flags.has(ParkFlag::difficultGuestGeneration));
        }

        void GuestsDraw(RenderTarget& rt)
        {
            ScreenCoordsXY screenCoords{};

            WindowDrawWidgets(*this, rt);
            DrawTabImages(rt);

            const auto& gameState = getGameState();
            const auto wColour2 = colours[1];

            const auto& cashPerGuestWidget = widgets[WIDX_CASH_PER_GUEST];
            if (cashPerGuestWidget.isVisible())
            {
                // Cash per guest value
                screenCoords = windowPos + ScreenCoordsXY{ cashPerGuestWidget.left + 1, cashPerGuestWidget.top + 1 };
                auto ft = Formatter();
                ft.Add<money64>(gameState.scenarioOptions.guestInitialCash);
                auto colour = !isWidgetDisabled(WIDX_CASH_PER_GUEST) ? wColour2 : wColour2.withFlag(ColourFlag::inset, true);
                drawText(rt, screenCoords, STR_CURRENCY_FORMAT_LABEL, ft, colour);
            }

            auto colour = wColour2;

            // Guest initial happiness value
            const auto& initialHappinessWidget = widgets[WIDX_GUEST_INITIAL_HAPPINESS];
            screenCoords = windowPos + ScreenCoordsXY{ initialHappinessWidget.left + 1, initialHappinessWidget.top + 1 };
            auto ft = Formatter();
            ft.Add<uint16_t>((gameState.scenarioOptions.guestInitialHappiness * 100) / 255);
            drawText(rt, screenCoords, STR_PERCENT_FORMAT_LABEL, ft, colour);

            // Guest initial hunger value
            const auto& initialHungerWidget = widgets[WIDX_GUEST_INITIAL_HUNGER];
            screenCoords = windowPos + ScreenCoordsXY{ initialHungerWidget.left + 1, initialHungerWidget.top + 1 };
            ft = Formatter();
            ft.Add<uint16_t>(((255 - gameState.scenarioOptions.guestInitialHunger) * 100) / 255);
            drawText(rt, screenCoords, STR_PERCENT_FORMAT_LABEL, ft, colour);

            // Guest initial thirst value
            const auto& initialThirstWidget = widgets[WIDX_GUEST_INITIAL_THIRST];
            screenCoords = windowPos + ScreenCoordsXY{ initialThirstWidget.left + 1, initialThirstWidget.top + 1 };
            ft = Formatter();
            ft.Add<uint16_t>(((255 - gameState.scenarioOptions.guestInitialThirst) * 100) / 255);
            drawText(rt, screenCoords, STR_PERCENT_FORMAT_LABEL, ft, colour);

            // Guests' intensity value
            {
                const auto& guestsIntensity = widgets[WIDX_GUESTS_INTENSITY_PREFERENCE];
                screenCoords = windowPos + ScreenCoordsXY{ guestsIntensity.left + 1, guestsIntensity.top + 1 };

                const auto preferLess = gameState.park.flags.has(ParkFlag::guestPreferLessIntenseRides);
                const auto preferMore = gameState.park.flags.has(ParkFlag::guestPreferMoreIntenseRides);

                StringId prefString = STR_GUESTS_PREFER_INTENSITY_BALANCED;
                if (preferLess && preferMore)
                    prefString = STR_GUESTS_PREFER_INTENSITY_NONE;
                else if (preferLess && !preferMore)
                    prefString = STR_GUESTS_PREFER_INTENSITY_LESS_INTENSE_RIDES;
                else if (!preferLess && preferMore)
                    prefString = STR_GUESTS_PREFER_INTENSITY_MORE_INTENSE_RIDES;

                ft = Formatter();
                ft.Add<StringId>(prefString);
                drawText(rt, screenCoords, STR_WINDOW_COLOUR_2_STRINGID, ft);
            }
        }

#pragma endregion

#pragma region Land

        void LandMouseUp(WidgetIndex widgetIndex)
        {
            auto& gameState = getGameState();
            switch (widgetIndex)
            {
                case WIDX_FORBID_TREE_REMOVAL:
                {
                    auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                        GameActions::ScenarioSetSetting::forbidTreeRemoval,
                        gameState.park.flags.has(ParkFlag::forbidTreeRemoval) ? 0 : 1);
                    GameActions::Execute(&scenarioSetSetting, gameState);
                    invalidate();
                    break;
                }
                case WIDX_FORBID_LANDSCAPE_CHANGES:
                {
                    auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                        GameActions::ScenarioSetSetting::forbidLandscapeChanges,
                        gameState.park.flags.has(ParkFlag::forbidLandscapeChanges) ? 0 : 1);
                    GameActions::Execute(&scenarioSetSetting, gameState);
                    invalidate();
                    break;
                }
                case WIDX_FORBID_HIGH_CONSTRUCTION:
                {
                    auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                        GameActions::ScenarioSetSetting::forbidHighConstruction,
                        gameState.park.flags.has(ParkFlag::forbidHighConstruction) ? 0 : 1);
                    GameActions::Execute(&scenarioSetSetting, gameState);
                    invalidate();
                    break;
                }
            }
        }

        void LandResize()
        {
            WindowSetResize(*this, kSizeLand, kSizeLand);
        }

        void LandMouseDown(WidgetIndex widgetIndex)
        {
            auto& gameState = getGameState();
            auto& scenarioOptions = gameState.scenarioOptions;

            switch (widgetIndex)
            {
                case WIDX_LAND_COST_INCREASE:
                    if (scenarioOptions.landPrice < 200.00_GBP)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::costToBuyLand, scenarioOptions.landPrice + 1.00_GBP);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_INCREASE_FURTHER, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_LAND_COST_DECREASE:
                    if (scenarioOptions.landPrice > 5.00_GBP)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::costToBuyLand, scenarioOptions.landPrice - 1.00_GBP);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_REDUCE_FURTHER, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_CONSTRUCTION_RIGHTS_COST_INCREASE:
                    if (scenarioOptions.constructionRightsPrice < 200.00_GBP)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::costToBuyConstructionRights,
                            scenarioOptions.constructionRightsPrice + 1.00_GBP);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_INCREASE_FURTHER, kStringIdNone, {});
                    }
                    invalidate();
                    break;
                case WIDX_CONSTRUCTION_RIGHTS_COST_DECREASE:
                    if (scenarioOptions.constructionRightsPrice > 5.00_GBP)
                    {
                        auto scenarioSetSetting = GameActions::ScenarioSetSettingAction(
                            GameActions::ScenarioSetSetting::costToBuyConstructionRights,
                            scenarioOptions.constructionRightsPrice - 1.00_GBP);
                        GameActions::Execute(&scenarioSetSetting, gameState);
                    }
                    else
                    {
                        ContextShowError(STR_CANT_REDUCE_FURTHER, kStringIdNone, {});
                    }
                    invalidate();
                    break;
            }
        }

        void LandUpdate()
        {
            currentFrame++;
            invalidateWidget(WIDX_TAB_3);
        }

        void LandPrepareDraw()
        {
            SetPressedTab();

            auto& gameState = getGameState();
            bool noMoney = gameState.park.flags.has(ParkFlag::noMoney);

            setWidgetDisabled(WIDX_LAND_COST_LABEL, noMoney);
            setWidgetDisabled(WIDX_LAND_COST, noMoney);
            setWidgetDisabled(WIDX_LAND_COST_INCREASE, noMoney);
            setWidgetDisabled(WIDX_LAND_COST_DECREASE, noMoney);
            setWidgetDisabled(WIDX_CONSTRUCTION_RIGHTS_COST_LABEL, noMoney);
            setWidgetDisabled(WIDX_CONSTRUCTION_RIGHTS_COST, noMoney);
            setWidgetDisabled(WIDX_CONSTRUCTION_RIGHTS_COST_INCREASE, noMoney);
            setWidgetDisabled(WIDX_CONSTRUCTION_RIGHTS_COST_DECREASE, noMoney);

            setWidgetPressed(WIDX_FORBID_TREE_REMOVAL, gameState.park.flags.has(ParkFlag::forbidTreeRemoval));
            setWidgetPressed(WIDX_FORBID_LANDSCAPE_CHANGES, gameState.park.flags.has(ParkFlag::forbidLandscapeChanges));
            setWidgetPressed(WIDX_FORBID_HIGH_CONSTRUCTION, gameState.park.flags.has(ParkFlag::forbidHighConstruction));

            widgets[WIDX_CLOSE].setHidden(gLegacyScene == LegacyScene::scenarioEditor);
        }

        void LandDraw(RenderTarget& rt)
        {
            ScreenCoordsXY screenCoords{};

            WindowDrawWidgets(*this, rt);
            DrawTabImages(rt);

            const auto& gameState = getGameState();
            const auto wColour2 = colours[1];

            const auto& landCostWidget = widgets[WIDX_LAND_COST];
            if (landCostWidget.isVisible())
            {
                // Cost to buy land value
                screenCoords = windowPos + ScreenCoordsXY{ landCostWidget.left + 1, landCostWidget.top + 1 };
                auto ft = Formatter();
                ft.Add<money64>(gameState.scenarioOptions.landPrice);
                auto colour = !isWidgetDisabled(WIDX_LAND_COST) ? wColour2 : wColour2.withFlag(ColourFlag::inset, true);
                drawText(rt, screenCoords, STR_CURRENCY_FORMAT_LABEL, ft, colour);
            }

            const auto& constructionRightsCostWidget = widgets[WIDX_CONSTRUCTION_RIGHTS_COST];
            if (constructionRightsCostWidget.isVisible())
            {
                // Cost to buy construction rights value
                screenCoords = windowPos
                    + ScreenCoordsXY{ constructionRightsCostWidget.left + 1, constructionRightsCostWidget.top + 1 };
                auto ft = Formatter();
                ft.Add<money64>(gameState.scenarioOptions.constructionRightsPrice);
                auto colour = !isWidgetDisabled(WIDX_CONSTRUCTION_RIGHTS_COST) ? wColour2
                                                                               : wColour2.withFlag(ColourFlag::inset, true);
                drawText(rt, screenCoords, STR_CURRENCY_FORMAT_LABEL, ft, colour);
            }
        }

#pragma endregion

#pragma region Rides

        /**
         *
         *  rct2: 0x006725A8
         */
        void RidesOnResize()
        {
            WindowSetResize(*this, kSizeRides, kSizeRides);
        }

        static std::string GetRideString(RideId rideId)
        {
            const auto* ride = GetRide(rideId);
            if (ride == nullptr)
                return {};
            return ride->getName();
        }

        /**
         *
         *  rct2: 0x00672544
         */
        void RidesOnUpdate()
        {
            currentFrame++;
            onPrepareDraw();
            onResize();
            invalidateWidget(WIDX_TAB_2);

            const auto oldSize = _rideableRides.size();
            RefreshRideableRides();

            if (oldSize != _rideableRides.size())
            {
                invalidate();
            }
        }

        // The rides listed on the page (not shops or facilities), sorted by name. Also called by
        // the accessibility recipe, so the list is current on the first build after a page change.
        void RefreshRideableRides()
        {
            _rideableRides.clear();

            const auto& gameState = getGameState();
            for (auto& currentRide : RideManager(gameState))
            {
                if (currentRide.isRide())
                {
                    _rideableRides.push_back(currentRide.id);
                }
            }

            // Sort the rides alphabetically
            std::sort(_rideableRides.begin(), _rideableRides.end(), [](const RideId& a, const RideId& b) {
                return String::compare(GetRideString(a), GetRideString(b), false) < 0;
            });
        }

        /**
         *
         *  rct2: 0x006724BF
         */
        ScreenSize RidesOnScrollGetSize(int32_t scrollIndex)
        {
            ScreenSize newSize;
            newSize.height = static_cast<int32_t>(_rideableRides.size()) * 10;

            return newSize;
        }

        /**
         *
         *  rct2: 0x006724FC
         */
        void RidesOnScrollMouseDown(int32_t scrollIndex, const ScreenCoordsXY& screenCoords)
        {
            auto i = screenCoords.y / 12;
            if (i < 0 || i >= static_cast<int32_t>(_rideableRides.size()))
                return;

            auto* currentRide = GetRide(_rideableRides[i]);
            if (currentRide != nullptr)
            {
                currentRide->flags.flip(RideFlag::indestructible);
            }
            invalidate();
        }

        /**
         *
         *  rct2: 0x006724CC
         */
        void RidesOnScrollMouseOver(int32_t scrollIndex, const ScreenCoordsXY& screenCoords)
        {
            int32_t i;

            i = screenCoords.y / 12;
            if (i < 0 || i >= static_cast<int32_t>(_rideableRides.size()))
                return;

            if (selectedListItem != i)
            {
                selectedListItem = i;
                invalidate();
            }
        }

        /**
         *
         *  rct2: 0x006722B5
         */
        void RidesOnPrepareDraw()
        {
            SetPressedTab();

            widgets[WIDX_CLOSE].setHidden(gLegacyScene == LegacyScene::scenarioEditor);
        }

        /**
         *
         *  rct2: 0x00672340
         */
        void RidesOnDraw(RenderTarget& rt)
        {
            drawWidgets(rt);
            DrawTabImages(rt);
        }

        /**
         *
         *  rct2: 0x0067236F
         */
        void RidesOnScrollDraw(RenderTarget& rt, int32_t scrollIndex)
        {
            auto colour = getColourMap(colours[1].colour).midLight;
            Rectangle::fill(rt, { { rt.x, rt.y }, { rt.x + rt.width - 1, rt.y + rt.height - 1 } }, colour);

            for (int32_t i = 0; i < static_cast<int32_t>(_rideableRides.size()); i++)
            {
                int32_t y = i * 12;

                if (y + 12 < rt.y || y >= rt.y + rt.height)
                    continue;

                // Checkbox
                Rectangle::fillInset(
                    rt, { { 2, y }, { 11, y + 10 } }, colours[1], Rectangle::BorderStyle::inset,
                    Rectangle::FillBrightness::dark, Rectangle::FillMode::dontLightenWhenInset);

                // Highlighted
                auto stringId = STR_BLACK_STRING;
                if (i == selectedListItem)
                {
                    stringId = STR_WINDOW_COLOUR_2_STRINGID;
                    Rectangle::filter(rt, { 0, y, width, y + 11 }, FilterPaletteID::paletteDarken1);
                }

                // Checkbox mark
                auto* currentRide = GetRide(_rideableRides[i]);
                if (currentRide != nullptr)
                {
                    if (currentRide->flags.has(RideFlag::indestructible))
                    {
                        auto darkness = stringId == STR_WINDOW_COLOUR_2_STRINGID ? TextDarkness::extraDark : TextDarkness::dark;
                        drawText(
                            rt, { 2, y }, kCheckMarkString,
                            { colours[1].withFlag(ColourFlag::translucent, false), FontStyle::medium, darkness });
                    }

                    // Ride name

                    Formatter ft;
                    currentRide->formatNameTo(ft);
                    drawText(rt, { 15, y }, stringId, ft);
                }
            }
        }

#pragma endregion

#pragma region Accessibility
    public:
        // ---- graph accessibility recipe ----
        //
        // Each page as a form, its controls in the order they stand on screen, named with the game's
        // own captions. Spinners: Left/Right press the game's own arrows, Shift+Left/Right move ten
        // of the game's steps at once, and Enter types a value. Combo boxes open their list with
        // Enter. In the editor the game reopens this window every tick and gives it no close button,
        // so an Editor steps tab follows the last page (see BuildEditorStepsTab).
        void BuildAccessGraph(Accessibility::Graph::GraphBuilder& b)
        {
            using namespace Accessibility::Graph;

            // Re-derive the dropdown state from reality every build - it can be dismissed behind
            // our back, and a stale flag would strand navigation inside a list that is gone.
            auto* windowMgr = GetWindowManager();
            if (_axDropdownOpen && (windowMgr == nullptr || windowMgr->FindByClass(WindowClass::dropdown) == nullptr))
                _axDropdownOpen = false;

            onPrepareDraw(); // pressed, disabled and hidden states, and the objective value's caption

            if (_axStepsTab)
            {
                if (HasEditorStepsTab())
                {
                    BuildEditorStepsTab(b);
                    return;
                }
                _axStepsTab = false; // the step buttons went away (left the editor): back to the page
            }

            if (_axDropdownOpen)
            {
                b.PushContext(axComboLabel(_axDropdownChevron), Vocab::kRoleDropdown);
                for (int32_t i = 0; i < gDropdown.numItems; i++)
                {
                    if (gDropdown.items[i].isSeparator())
                        continue;
                    NodeVtable vt;
                    vt.announcements.emplace_back(NodeAnnouncement::Static(gDropdown.items[i].text));
                    if (gDropdown.items[i].isChecked())
                        vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kSelected, AnnouncementKinds::kSelected));
                    vt.onActivate = [this, i]() { axCommitDropdown(i); };
                    // Keep the engine's drawn highlight on the keyboard's row; it clears that field from
                    // the mouse every tick, so it has to be re-asserted.
                    vt.onFocus = [i]() { Accessibility::SetKeyboardDropdownIndex(i); };
                    b.AddItem(ControlId::Structural("so:dd:" + std::to_string(i)), std::move(vt));
                }
                b.PopContext();
                return;
            }

            b.PushContext(Vocab::kScenarioOptionsPages[page]);
            switch (page)
            {
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_OBJECTIVE:
                    axCombo(b, WIDX_OBJECTIVE_DROPDOWN);
                    axSpinner(b, WIDX_OBJECTIVE_ARG_1);
                    axSpinner(b, WIDX_OBJECTIVE_ARG_2);
                    axCheckbox(b, WIDX_HARD_PARK_RATING);
                    break;
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_SCENARIO_DETAILS:
                    axEdit(b, WIDX_PARK_NAME);
                    axEdit(b, WIDX_SCENARIO_NAME);
                    axCombo(b, WIDX_CATEGORY_DROPDOWN);
                    axEdit(b, WIDX_DETAILS);
                    break;
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_FINANCIAL:
                    axCheckbox(b, WIDX_NO_MONEY);
                    b.PushContext(axText(widgets[WIDX_GROUP_LOAN].text));
                    axSpinner(b, WIDX_INITIAL_LOAN);
                    axSpinner(b, WIDX_MAXIMUM_LOAN);
                    axSpinner(b, WIDX_INTEREST_RATE);
                    axCheckbox(b, WIDX_RCT1_INTEREST);
                    b.PopContext();
                    b.PushContext(axText(widgets[WIDX_GROUP_BUSINESS_MODEL].text));
                    axSpinner(b, WIDX_INITIAL_CASH);
                    axCombo(b, WIDX_PAY_FOR_PARK_OR_RIDES_DROPDOWN);
                    axSpinner(b, WIDX_ENTRY_PRICE);
                    axCheckbox(b, WIDX_FORBID_MARKETING);
                    b.PopContext();
                    break;
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_GUESTS:
                    axSpinner(b, WIDX_CASH_PER_GUEST);
                    axSpinner(b, WIDX_GUEST_INITIAL_HAPPINESS);
                    axSpinner(b, WIDX_GUEST_INITIAL_HUNGER);
                    axSpinner(b, WIDX_GUEST_INITIAL_THIRST);
                    axCombo(b, WIDX_GUESTS_INTENSITY_PREFERENCE_DROPDOWN);
                    axCheckbox(b, WIDX_HARD_GUEST_GENERATION);
                    break;
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_PARK:
                    axSpinner(b, WIDX_LAND_COST);
                    axSpinner(b, WIDX_CONSTRUCTION_RIGHTS_COST);
                    axCheckbox(b, WIDX_FORBID_TREE_REMOVAL);
                    axCheckbox(b, WIDX_FORBID_LANDSCAPE_CHANGES);
                    axCheckbox(b, WIDX_FORBID_HIGH_CONSTRUCTION);
                    break;
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_RIDES:
                    axRideList(b);
                    break;
            }
            b.PopContext();
        }

        // Tab/Shift+Tab: the pages this step offers (the game disables the rest), then the Editor
        // steps tab where there is one, wrapping round.
        void AxChangePage(int32_t delta)
        {
            const int32_t count = WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_COUNT;
            const int32_t dir = delta > 0 ? 1 : -1;

            // Leaving the Editor steps tab: Tab carries on to the first page offered, Shift+Tab back
            // to the last - the steps tab sits after the last page, like one more tab on the end.
            if (_axStepsTab)
            {
                _axStepsTab = false;
                int32_t landing = dir > 0 ? 0 : count - 1;
                for (int32_t step = 0; step < count && isWidgetDisabled(WIDX_TAB_1 + landing); step++)
                    landing = (landing + dir + count) % count;
                setPage(landing);
                return;
            }

            int32_t next = page;
            for (int32_t step = 0; step < count; step++)
            {
                const int32_t unwrapped = next + dir;
                next = (unwrapped + count) % count;
                // Stepping off either end of the pages lands on the Editor steps tab, where there is
                // one (in the editor, not when opened from the Cheats menu).
                if ((unwrapped < 0 || unwrapped >= count) && HasEditorStepsTab())
                {
                    _axStepsTab = true;
                    return;
                }
                if (isWidgetDisabled(WIDX_TAB_1 + next))
                    continue;
                setPage(next);
                return;
            }
        }

        // F1: the page's own help, or the Editor steps tab's.
        std::string AxHelp() const
        {
            return Vocab::Help::ScenarioOptions(page, gLegacyScene == LegacyScene::scenarioEditor, _axStepsTab);
        }

        // Escape closes an open list first. In the editor the window cannot be closed (the game
        // reopens it), so Escape says how to move on; in a park it closes as usual.
        bool AxEscape()
        {
            if (_axDropdownOpen)
            {
                const WidgetIndex chevron = _axDropdownChevron;
                axCloseDropdown();
                axSuggestFocus(axOwner(chevron));
                return true;
            }
            if (gLegacyScene == LegacyScene::scenarioEditor)
            {
                Accessibility::ScreenReaderSpeak(Vocab::kScenarioOptionsEscape);
                return true;
            }
            return false;
        }

    private:
        bool _axDropdownOpen = false;
        WidgetIndex _axDropdownChevron = 0;
        bool _axStepsTab = false;

        // How a spinner's value is shown, typed and stepped.
        enum class AxUnit : uint8_t
        {
            money,
            count,
            length,
            rating,
            percent,      // stored 0-255, shown as a percentage
            plainPercent, // stored and shown as a percentage
            year,
        };

        struct AxSpin
        {
            int64_t value;
            int64_t min;
            int64_t max;
            int64_t step;
            AxUnit unit;
            std::function<void(int64_t)> set;
        };

        // Node keys carry the page: widget indices are reused from page to page.
        std::string axKey(WidgetIndex w) const
        {
            return "so:" + std::to_string(page) + ":" + std::to_string(w);
        }

        // A game caption as speech: format codes and a trailing colon dropped.
        static std::string axText(StringId id)
        {
            if (id == kStringIdNone || id == kStringIdEmpty)
                return {};
            std::string out = axStrip(FormatStringID(id));
            while (!out.empty() && (out.back() == ':' || out.back() == ' '))
                out.pop_back();
            return out;
        }

        std::optional<Accessibility::Graph::GraphRect> axRect(WidgetIndex w) const
        {
            if (w >= widgets.size() || !widgets[w].isVisible())
                return std::nullopt;
            const auto& wd = widgets[w];
            return Accessibility::Graph::GraphRect{ windowPos.x + wd.left, windowPos.y + wd.top, wd.width() + 1,
                                                    wd.height() + 1 };
        }

        std::string axUnavailable(WidgetIndex w)
        {
            return isWidgetDisabled(w) ? Vocab::kUnavailable : "";
        }

        void axSuggestFocus(WidgetIndex w)
        {
            Accessibility::Graph::GraphStateForClass(WindowClass::editorScenarioOptions).nextSuggestedMove
                = Accessibility::Graph::ControlId::Structural(axKey(w));
        }

        // ---- checkboxes ----

        // Enter does what a click does: the game handles some checkboxes on the press and others on
        // the release. Its settings change through game actions, read a tick later, so the state is LIVE.
        void axCheckbox(Accessibility::Graph::GraphBuilder& b, WidgetIndex w)
        {
            using namespace Accessibility::Graph;
            if (!widgets[w].isVisible())
                return;
            NodeVtable vt;
            vt.announcements.emplace_back(NodeAnnouncement::Static(axText(widgets[w].text)));
            vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kRoleCheckbox, AnnouncementKinds::kRole));
            vt.announcements.emplace_back(
                [this, w]() { return std::string(Vocab::Checked(isWidgetPressed(w))); }, true, AnnouncementKinds::kValue);
            vt.announcements.emplace_back([this, w]() { return axUnavailable(w); }, true);
            vt.onActivate = [this, w]() {
                if (isWidgetDisabled(w))
                {
                    Accessibility::ScreenReaderSpeak(Vocab::kUnavailable);
                    return;
                }
                onMouseDown(w);
                onMouseUp(w);
            };
            vt.focusRect = [this, w]() { return axRect(w); };
            b.AddItem(Accessibility::Graph::ControlId::Structural(axKey(w)), std::move(vt));
        }

        // ---- text fields ----

        // Enter presses the field's Change button, which opens the game's own text prompt.
        void axEdit(Accessibility::Graph::GraphBuilder& b, WidgetIndex w)
        {
            using namespace Accessibility::Graph;
            StringId label = kStringIdNone;
            switch (w)
            {
                case WIDX_PARK_NAME:
                    label = STR_PARK_NAME;
                    break;
                case WIDX_SCENARIO_NAME:
                    label = STR_SCENARIO_NAME;
                    break;
                default:
                    label = STR_WINDOW_PARK_DETAILS;
                    break;
            }
            NodeVtable vt;
            vt.announcements.emplace_back(NodeAnnouncement::Static(axText(label)));
            vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kRoleEdit, AnnouncementKinds::kRole));
            vt.announcements.emplace_back(
                [w]() {
                    const auto& gameState = getGameState();
                    std::string text;
                    if (w == WIDX_PARK_NAME)
                        text = gameState.park.name;
                    else if (w == WIDX_SCENARIO_NAME)
                        text = gameState.scenarioOptions.name;
                    else
                        text = gameState.scenarioOptions.details;
                    return text.empty() ? std::string(Vocab::kEmpty) : text;
                },
                true, AnnouncementKinds::kValue);
            vt.onActivate = [this, w]() { onMouseUp(w); };
            vt.focusRect = [this, w]() { return axRect(w); };
            b.AddItem(Accessibility::Graph::ControlId::Structural(axKey(w)), std::move(vt));
        }

        // ---- combo boxes ----

        // The value widget sits just before its chevron.
        static WidgetIndex axOwner(WidgetIndex chevron)
        {
            return static_cast<WidgetIndex>(chevron - 1);
        }

        std::string axComboLabel(WidgetIndex chevron) const
        {
            switch (chevron)
            {
                case WIDX_OBJECTIVE_DROPDOWN:
                    return axText(STR_OBJECTIVE_DROPDOWN_LABEL);
                case WIDX_CATEGORY_DROPDOWN:
                    return axText(STR_WINDOW_SCENARIO_GROUP);
                case WIDX_PAY_FOR_PARK_OR_RIDES_DROPDOWN:
                    return axText(STR_EARNINGS_LABEL);
                default:
                    return axText(STR_GUESTS_PREFER_INTENSITY_LABEL);
            }
        }

        // The value the game draws in the combo box.
        std::string axComboValue(WidgetIndex chevron) const
        {
            const auto& gameState = getGameState();
            const auto& park = gameState.park;
            switch (chevron)
            {
                case WIDX_OBJECTIVE_DROPDOWN:
                    return axText(ObjectiveDropdownOptionNames[EnumValue(gameState.scenarioOptions.objective.Type)]);
                case WIDX_CATEGORY_DROPDOWN:
                    return axText(Scenario::kScenarioCategoryStringIds[EnumValue(gameState.scenarioOptions.category)]);
                case WIDX_PAY_FOR_PARK_OR_RIDES_DROPDOWN:
                    if (park.flags.has(ParkFlag::unlockAllPrices))
                        return axText(STR_PAID_ENTRY_PAID_RIDES);
                    if (park.flags.has(ParkFlag::freeEntry))
                        return axText(STR_FREE_PARK_ENTER);
                    return axText(STR_PAY_PARK_ENTER);
                default:
                {
                    const auto preferLess = park.flags.has(ParkFlag::guestPreferLessIntenseRides);
                    const auto preferMore = park.flags.has(ParkFlag::guestPreferMoreIntenseRides);
                    if (preferLess && preferMore)
                        return axText(STR_GUESTS_PREFER_INTENSITY_NONE);
                    if (preferLess)
                        return axText(STR_GUESTS_PREFER_INTENSITY_LESS_INTENSE_RIDES);
                    if (preferMore)
                        return axText(STR_GUESTS_PREFER_INTENSITY_MORE_INTENSE_RIDES);
                    return axText(STR_GUESTS_PREFER_INTENSITY_BALANCED);
                }
            }
        }

        void axCombo(Accessibility::Graph::GraphBuilder& b, WidgetIndex chevron)
        {
            using namespace Accessibility::Graph;
            if (!widgets[chevron].isVisible())
                return;
            NodeVtable vt;
            vt.announcements.emplace_back(NodeAnnouncement::Static(axComboLabel(chevron)));
            vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kRoleDropdown, AnnouncementKinds::kRole));
            vt.announcements.emplace_back([this, chevron]() { return axComboValue(chevron); }, true, AnnouncementKinds::kValue);
            vt.announcements.emplace_back([this, chevron]() { return axUnavailable(chevron); }, true);
            vt.onActivate = [this, chevron]() {
                if (isWidgetDisabled(chevron))
                {
                    Accessibility::ScreenReaderSpeak(Vocab::kUnavailable);
                    return;
                }
                if (Accessibility::OpenWidgetDropdownFromKeyboard(*this, chevron))
                {
                    _axDropdownOpen = true;
                    _axDropdownChevron = chevron;
                }
            };
            vt.focusRect = [this, chevron]() { return axRect(axOwner(chevron)); };
            b.AddItem(Accessibility::Graph::ControlId::Structural(axKey(axOwner(chevron))), std::move(vt));
        }

        void axCloseDropdown()
        {
            Accessibility::CloseWidgetDropdownFromKeyboard();
            _axDropdownOpen = false;
        }

        // As a click on a list item: the window hears the choice, then the list closes.
        void axCommitDropdown(int32_t idx)
        {
            const WidgetIndex chevron = _axDropdownChevron;
            const bool valid = idx >= 0 && idx < gDropdown.numItems && !gDropdown.items[idx].isSeparator()
                && !gDropdown.items[idx].isDisabled();
            if (valid)
                onDropdown(chevron, idx);
            axCloseDropdown();
            axSuggestFocus(axOwner(chevron));
        }

        // ---- spinners ----

        static void axSetting(GameActions::ScenarioSetSetting setting, int64_t value)
        {
            auto action = GameActions::ScenarioSetSettingAction(setting, static_cast<uint32_t>(value));
            GameActions::Execute(&action, getGameState());
        }

        // What a spinner holds and the game's own step and limits for it - the same numbers its arrow
        // buttons use (see the MouseDown handlers above). Set goes through the same game action, or,
        // for the objective, the same direct write.
        std::optional<AxSpin> axSpin(WidgetIndex w)
        {
            using GameActions::ScenarioSetSetting;
            auto& gameState = getGameState();
            auto& park = gameState.park;
            auto& options = gameState.scenarioOptions;
            auto& objective = options.objective;
            const auto setObjective = [this](auto& field) {
                return [this, &field](int64_t v) {
                    field = static_cast<std::remove_reference_t<decltype(field)>>(v);
                    invalidate();
                };
            };
            const auto setting = [](ScenarioSetSetting s) { return [s](int64_t v) { axSetting(s, v); }; };

            switch (page)
            {
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_OBJECTIVE:
                    if (w == WIDX_OBJECTIVE_ARG_2)
                        return AxSpin{ objective.Year, kObjectiveYearMin, kObjectiveYearMax, kObjectiveYearAdjustment,
                                       AxUnit::year, setObjective(objective.Year) };
                    if (w != WIDX_OBJECTIVE_ARG_1)
                        return std::nullopt;
                    switch (objective.Type)
                    {
                        case Scenario::ObjectiveType::parkValueBy:
                        case Scenario::ObjectiveType::monthlyRideIncome:
                        case Scenario::ObjectiveType::repayLoanAndParkValue:
                            return AxSpin{ objective.Currency, kObjectiveCurrencyLoanAndValueMin,
                                           kObjectiveCurrencyLoanAndValueMax, kObjectiveCurrencyLoanAndValueAdjustment,
                                           AxUnit::money, setObjective(objective.Currency) };
                        case Scenario::ObjectiveType::monthlyFoodIncome:
                            return AxSpin{ objective.Currency,         kObjectiveCurrencyFoodMin,
                                           kObjectiveCurrencyFoodMax,  kObjectiveCurrencyFoodAdjustment,
                                           AxUnit::money,              setObjective(objective.Currency) };
                        case Scenario::ObjectiveType::tenRollercoastersLength:
                            return AxSpin{ objective.MinimumLength, kObjectiveLengthMin,          kObjectiveLengthMax,
                                           kObjectiveLengthAdjustment, AxUnit::length, setObjective(objective.MinimumLength) };
                        case Scenario::ObjectiveType::finishFiveRollercoasters:
                            return AxSpin{ objective.MinimumExcitement,      kObjectiveExcitementMin,
                                           kObjectiveExcitementMax,          kObjectiveExcitementAdjustment,
                                           AxUnit::rating,                   setObjective(objective.MinimumExcitement) };
                        default:
                            return AxSpin{ objective.NumGuests, kObjectiveGuestsMin,     kObjectiveGuestsMax,
                                           kObjectiveGuestsAdjustment, AxUnit::count, setObjective(objective.NumGuests) };
                    }
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_FINANCIAL:
                    switch (w)
                    {
                        case WIDX_INITIAL_LOAN:
                            return AxSpin{ park.bankLoan, 0, 5000000.00_GBP, 1000.00_GBP, AxUnit::money,
                                           setting(ScenarioSetSetting::initialLoan) };
                        case WIDX_MAXIMUM_LOAN:
                            return AxSpin{ park.maxBankLoan, 0, 5000000.00_GBP, 1000.00_GBP, AxUnit::money,
                                           setting(ScenarioSetSetting::maximumLoanSize) };
                        case WIDX_INTEREST_RATE:
                            return AxSpin{ park.bankLoanInterestRate, 0, kMaxBankLoanInterestRate, 1, AxUnit::plainPercent,
                                           setting(ScenarioSetSetting::annualInterestRate) };
                        case WIDX_INITIAL_CASH:
                            return AxSpin{ options.initialCash, 0, 1000000.00_GBP, 500.00_GBP, AxUnit::money,
                                           setting(ScenarioSetSetting::initialCash) };
                        case WIDX_ENTRY_PRICE:
                            return AxSpin{ park.entranceFee, 0, kMaxEntranceFee, 1.00_GBP, AxUnit::money,
                                           setting(ScenarioSetSetting::parkChargeEntryFee) };
                    }
                    return std::nullopt;
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_GUESTS:
                    switch (w)
                    {
                        case WIDX_CASH_PER_GUEST:
                            return AxSpin{ options.guestInitialCash, 0, 1000.00_GBP, 1.00_GBP, AxUnit::money,
                                           setting(ScenarioSetSetting::averageCashPerGuest) };
                        case WIDX_GUEST_INITIAL_HAPPINESS:
                            return AxSpin{ options.guestInitialHappiness, 40, 250, 4, AxUnit::percent,
                                           setting(ScenarioSetSetting::guestInitialHappiness) };
                        // Hunger and thirst are stored the other way up: the game shows 255 minus the stored
                        // value, and its up arrow lowers what is stored. Held here as shown.
                        case WIDX_GUEST_INITIAL_HUNGER:
                            return AxSpin{ 255 - options.guestInitialHunger, 5, 215, 4, AxUnit::percent,
                                           [](int64_t v) { axSetting(ScenarioSetSetting::guestInitialHunger, 255 - v); } };
                        case WIDX_GUEST_INITIAL_THIRST:
                            return AxSpin{ 255 - options.guestInitialThirst, 5, 215, 4, AxUnit::percent,
                                           [](int64_t v) { axSetting(ScenarioSetSetting::guestInitialThirst, 255 - v); } };
                    }
                    return std::nullopt;
                case WINDOW_EDITOR_SCENARIO_OPTIONS_PAGE_PARK:
                    switch (w)
                    {
                        case WIDX_LAND_COST:
                            return AxSpin{ options.landPrice, 5.00_GBP, 200.00_GBP, 1.00_GBP, AxUnit::money,
                                           setting(ScenarioSetSetting::costToBuyLand) };
                        case WIDX_CONSTRUCTION_RIGHTS_COST:
                            return AxSpin{ options.constructionRightsPrice, 5.00_GBP, 200.00_GBP, 1.00_GBP, AxUnit::money,
                                           setting(ScenarioSetSetting::costToBuyConstructionRights) };
                    }
                    return std::nullopt;
            }
            return std::nullopt;
        }

        static bool axImperial()
        {
            return Config::Get().general.measurementFormat == MeasurementFormat::imperial;
        }

        // A value as the game draws it.
        static std::string axFormat(AxUnit unit, int64_t v)
        {
            std::string text;
            switch (unit)
            {
                case AxUnit::money:
                    text = FormatStringID(STR_CURRENCY_FORMAT_LABEL, static_cast<money64>(v));
                    break;
                case AxUnit::count:
                    text = FormatStringID(STR_WINDOW_COLOUR_2_COMMA32, static_cast<int32_t>(v));
                    break;
                case AxUnit::length:
                    text = FormatStringID(STR_WINDOW_COLOUR_2_LENGTH, static_cast<uint16_t>(v));
                    break;
                case AxUnit::rating:
                    text = FormatStringID(STR_WINDOW_COLOUR_2_COMMA2DP32, static_cast<int32_t>(v));
                    break;
                case AxUnit::percent:
                    text = FormatStringID(STR_PERCENT_FORMAT_LABEL, static_cast<uint16_t>((v * 100) / 255));
                    break;
                case AxUnit::plainPercent:
                    text = FormatStringID(STR_PERCENT_FORMAT_LABEL, static_cast<int16_t>(v));
                    break;
                case AxUnit::year:
                    text = FormatStringID(STR_WINDOW_OBJECTIVE_VALUE_DATE, static_cast<uint16_t>(v * MONTH_COUNT - 1));
                    break;
            }
            return axStrip(text);
        }

        static std::string axStrip(const std::string& text)
        {
            std::string out;
            for (size_t i = 0; i < text.size(); i++)
            {
                if (text[i] == '{')
                {
                    const auto end = text.find('}', i);
                    if (end == std::string::npos)
                        break;
                    i = end;
                    continue;
                }
                out.push_back(text[i]);
            }
            return out;
        }

        // The value as the player would type it, to start the prompt with.
        static std::string axEditable(AxUnit unit, int64_t v)
        {
            switch (unit)
            {
                case AxUnit::money:
                {
                    char buffer[kMoneyStringMaxlength + 1]{};
                    MoneyToString(static_cast<money64>(v), buffer, sizeof(buffer), false);
                    return buffer;
                }
                case AxUnit::length:
                    return std::to_string(axImperial() ? MetresToFeet(static_cast<int32_t>(v)) : v);
                case AxUnit::rating:
                {
                    char buffer[16];
                    std::snprintf(buffer, sizeof(buffer), "%.2f", static_cast<double>(v) / 100.0);
                    return buffer;
                }
                case AxUnit::percent:
                    return std::to_string((v * 100) / 255);
                case AxUnit::year:
                case AxUnit::count:
                case AxUnit::plainPercent:
                    return std::to_string(v);
            }
            return {};
        }

        // Typed text to a stored value, in the same units the game shows. Null if it is not a number.
        static std::optional<int64_t> axParse(AxUnit unit, std::string_view text)
        {
            if (unit == AxUnit::money)
            {
                const std::string s(text);
                const auto money = StringToMoney(s.c_str());
                if (money == kMoney64Undefined)
                    return std::nullopt;
                return money;
            }
            std::string number;
            for (const char c : text)
            {
                if (c >= '0' && c <= '9')
                    number.push_back(c);
                else if ((c == '.' || c == ',') && unit == AxUnit::rating)
                    number.push_back('.');
            }
            if (number.empty() || number == ".")
                return std::nullopt;
            const double value = std::stod(number);
            switch (unit)
            {
                case AxUnit::length:
                {
                    const auto shown = static_cast<int32_t>(std::lround(value));
                    return axImperial() ? FeetToMetres(shown) : shown;
                }
                case AxUnit::rating:
                    return static_cast<int64_t>(std::lround(value * 100.0));
                case AxUnit::percent:
                    // The smallest stored value the game shows as this percentage.
                    return static_cast<int64_t>(std::ceil(value * 255.0 / 100.0));
                default:
                    return static_cast<int64_t>(std::lround(value));
            }
        }

        void axSpinner(Accessibility::Graph::GraphBuilder& b, WidgetIndex w)
        {
            using namespace Accessibility::Graph;
            if (!widgets[w].isVisible())
                return;
            NodeVtable vt;
            // The caption sits in the label widget just before the spinner.
            vt.announcements.emplace_back([this, w]() { return axText(widgets[w - 1].text); });
            vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kRoleSpinner, AnnouncementKinds::kRole));
            // LIVE: most of these change through a game action, readable only a tick later.
            vt.announcements.emplace_back(
                [this, w]() {
                    const auto spin = axSpin(w);
                    return spin.has_value() ? axFormat(spin->unit, spin->value) : std::string();
                },
                true, AnnouncementKinds::kValue);
            vt.announcements.emplace_back([this, w]() { return axUnavailable(w); }, true);
            vt.onAdjust = [this, w](int32_t sign, bool large) { axAdjust(w, sign, large); };
            vt.onActivate = [this, w]() { axOpenTyping(w); };
            vt.focusRect = [this, w]() { return axRect(w); };
            b.AddItem(Accessibility::Graph::ControlId::Structural(axKey(w)), std::move(vt));
        }

        // One step is the game's own arrow (with its error at a limit). Ten steps are set in one go:
        // the game reads the current value to work out the next, and a game action only lands a tick
        // later, so ten presses in a row would all start from the same value.
        void axAdjust(WidgetIndex w, int32_t sign, bool large)
        {
            if (isWidgetDisabled(w))
            {
                Accessibility::ScreenReaderSpeak(Vocab::kUnavailable);
                return;
            }
            const auto arrow = static_cast<WidgetIndex>(sign > 0 ? w + 1 : w + 2); // up : down
            const auto spin = axSpin(w);
            if (!large || !spin.has_value())
            {
                onMouseDown(arrow);
                return;
            }
            const bool atLimit = sign > 0 ? spin->value >= spin->max : spin->value <= spin->min;
            if (atLimit)
            {
                onMouseDown(arrow); // the game's own "can't go further"
                return;
            }
            spin->set(std::clamp<int64_t>(spin->value + sign * 10 * spin->step, spin->min, spin->max));
        }

        void axOpenTyping(WidgetIndex w)
        {
            if (isWidgetDisabled(w))
            {
                Accessibility::ScreenReaderSpeak(Vocab::kUnavailable);
                return;
            }
            const auto spin = axSpin(w);
            if (!spin.has_value())
                return;
            const int32_t pageAtOpen = page;
            WindowTextInputOpen(
                axText(widgets[w - 1].text),
                Vocab::TypeValueRange(axFormat(spin->unit, spin->min), axFormat(spin->unit, spin->max)),
                axEditable(spin->unit, spin->value), 16,
                [w, pageAtOpen](std::string_view text) {
                    auto* windowMgr = GetWindowManager();
                    auto* self = windowMgr == nullptr
                        ? nullptr
                        : static_cast<EditorScenarioOptionsWindow*>(windowMgr->FindByClass(WindowClass::editorScenarioOptions));
                    if (self != nullptr && self->page == pageAtOpen)
                        self->axApplyTyped(w, text);
                },
                []() { Accessibility::ScreenReaderSpeak(Vocab::kCancelled); });
        }

        // A typed value, rounded to the game's step and kept within its limits; outside them, the
        // range is said instead and nothing changes.
        void axApplyTyped(WidgetIndex w, std::string_view text)
        {
            auto spin = axSpin(w);
            if (!spin.has_value())
                return;
            auto parsed = axParse(spin->unit, text);
            if (!parsed.has_value())
            {
                Accessibility::ScreenReaderSpeak(Vocab::kNotANumber);
                return;
            }
            int64_t v = *parsed;
            bool inRange;
            if (spin->unit == AxUnit::percent)
            {
                // Compared as shown: several stored values share each percentage.
                const auto shown = [](int64_t x) { return (x * 100) / 255; };
                inRange = shown(v) >= shown(spin->min) && shown(v) <= shown(spin->max);
            }
            else
            {
                v = spin->min + std::llround(static_cast<double>(v - spin->min) / spin->step) * spin->step;
                inRange = v >= spin->min && v <= spin->max;
            }
            if (!inRange)
            {
                Accessibility::ScreenReaderSpeak(Vocab::ValueOutOfRange(
                    axText(widgets[w - 1].text), axFormat(spin->unit, spin->min), axFormat(spin->unit, spin->max)));
                return;
            }
            spin->set(std::clamp(v, spin->min, spin->max));
        }

        // ---- preserved rides ----

        // Each ride as a checkbox; Enter toggles it with the game's own click on that row.
        void axRideList(Accessibility::Graph::GraphBuilder& b)
        {
            using namespace Accessibility::Graph;
            RefreshRideableRides();
            b.PushContext(axText(STR_WINDOW_PRESERVATION_ORDER), Vocab::kRoleList);
            if (_rideableRides.empty())
            {
                NodeVtable vt;
                vt.announcements.emplace_back(NodeAnnouncement::Static(Vocab::kNoRides));
                b.AddItem(ControlId::Structural("so:rides:none"), std::move(vt));
            }
            for (size_t i = 0; i < _rideableRides.size(); i++)
            {
                const RideId id = _rideableRides[i];
                const auto row = static_cast<int32_t>(i);
                NodeVtable vt;
                vt.announcements.emplace_back(NodeAnnouncement::Static(GetRideString(id)));
                vt.announcements.emplace_back(
                    [id]() {
                        const auto* ride = GetRide(id);
                        return std::string(Vocab::Checked(ride != nullptr && ride->flags.has(RideFlag::indestructible)));
                    },
                    true, AnnouncementKinds::kValue);
                vt.onActivate = [this, row]() { onScrollMouseDown(0, { 0, row * 12 + 1 }); };
                vt.onFocus = [this, row]() {
                    selectedListItem = row;
                    invalidate();
                };
                b.AddItem(ControlId::Structural("so:ride:" + std::to_string(id.ToUnderlying())), std::move(vt));
            }
            b.PopContext();
        }
#pragma endregion
    };

    void RegisterEditorScenarioOptionsGraphScreen()
    {
        using namespace Accessibility::Graph;
        GraphScreen screen;
        screen.windowClass = WindowClass::editorScenarioOptions;
        screen.build = [](GraphBuilder& b, WindowBase& w) { static_cast<EditorScenarioOptionsWindow&>(w).BuildAccessGraph(b); };
        screen.onTabKey = [](WindowBase& w, int32_t dir) {
            static_cast<EditorScenarioOptionsWindow&>(w).AxChangePage(dir);
            return true;
        };
        screen.onEscape = [](WindowBase& w) { return static_cast<EditorScenarioOptionsWindow&>(w).AxEscape(); };
        screen.screenName = []() { return std::string(Vocab::kScenarioOptions); };
        screen.helpText = [](WindowBase& w) { return static_cast<EditorScenarioOptionsWindow&>(w).AxHelp(); };
        // A form: Left/Right belong to the spinners. Pages are Tab/Shift+Tab.
        screen.sideArrowsChangePage = false;
        RegisterGraphScreen(std::move(screen));
    }

    WindowBase* EditorScenarioOptionsOpen()
    {
        auto* windowMgr = GetWindowManager();
        return windowMgr->FocusOrCreate<EditorScenarioOptionsWindow>(
            WindowClass::editorScenarioOptions, { 280, 148 }, { WindowFlag::noScrolling, WindowFlag::centreScreen });
    }
} // namespace OpenRCT2::Ui::Windows
