/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "../actions/CommandFlag.h"
#include "../core/Money.hpp"
#include "Research.h"

enum class ExpenditureType : int32_t
{
    rideConstruction = 0,
    rideRunningCosts,
    landPurchase,
    landscaping,
    parkEntranceTickets,
    parkRideTickets,
    shopSales,
    shopStock,
    foodDrinkSales,
    foodDrinkStock,
    wages,
    marketing,
    research,
    interest,
    count
};

constexpr uint8_t kExpenditureTableMonthCount = 16;
constexpr uint8_t kFinanceHistorySize = 128;

constexpr uint8_t kMaxBankLoanInterestRate = 255;

extern const money64 kResearchCosts[RESEARCH_FUNDING_COUNT];

bool FinanceCheckMoneyRequired(OpenRCT2::GameActions::CommandFlags flags);
bool FinanceCheckAffordability(money64 cost, OpenRCT2::GameActions::CommandFlags flags);
void FinancePayment(money64 amount, ExpenditureType type);

// Accessibility: if a player-initiated transaction (building, demolishing, land, marketing) has
// occurred since the last call, returns true and writes the net amount (positive = spent,
// negative = earned), clearing the pending state. Used to announce money changes to the screen
// reader without reading out every guest transaction.
bool FinanceAccessConsumePending(money64& outAmount);
void FinancePayWages();
void FinancePayResearch();
void FinancePayInterest();
void FinancePayRideUpkeep();
void FinanceResetHistory();
void FinanceInit();
void FinanceUpdateDailyProfit();
void FinanceShiftExpenditureTable();
void FinanceResetCashToInitial();

money64 FinanceGetLastMonthShopProfit();
