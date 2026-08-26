/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <cstdint>
#include <string>

// The one vocabulary module for everything the graph layer says on its own behalf (spec A8):
// role words, "n of m", expansion state. Hardcoded English at call sites is forbidden - routing
// every layer-authored string through here is what makes later localization safe to defer.
// Application content (ride names, config values) is already localized - passed through, never
// re-translated.
namespace OpenRCT2::Ui::Accessibility::Graph::Vocab
{
    inline std::string Position(int32_t index, int32_t count)
    {
        return std::to_string(index) + " of " + std::to_string(count);
    }

    inline std::string ExpandedState(bool expanded)
    {
        return expanded ? "expanded" : "collapsed";
    }

    inline constexpr const char* kEmptyGroup = "No items";

    // Role words for the shared control types.
    inline constexpr const char* kRoleButton = "button";
    inline constexpr const char* kRoleToggle = "toggle";
    inline constexpr const char* kRoleSlider = "slider";
    inline constexpr const char* kRoleDropdown = "menu";
    inline constexpr const char* kRoleList = "list";
} // namespace OpenRCT2::Ui::Accessibility::Graph::Vocab
