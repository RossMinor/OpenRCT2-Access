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
    inline constexpr const char* kRoleCheckbox = "checkbox";
    inline constexpr const char* kRoleComboBox = "combo box";
    inline constexpr const char* kRoleEdit = "edit";

    // State words. An "off" state always has a word: an empty part is never spoken, so a control
    // that only ever said "checked" would answer an uncheck with silence.
    inline constexpr const char* kChecked = "checked";
    inline constexpr const char* kUnchecked = "unchecked";
    inline constexpr const char* kSelected = "selected";
    inline constexpr const char* kUnselected = "unselected";
    inline constexpr const char* kOn = "on";
    inline constexpr const char* kOff = "off";
    inline constexpr const char* kEmpty = "empty";
    inline constexpr const char* kNothingChosen = "not selected"; // a combo box with no value yet

    inline const char* Checked(bool checked)
    {
        return checked ? kChecked : kUnchecked;
    }

    inline const char* Selected(bool selected)
    {
        return selected ? kSelected : kUnselected;
    }

    inline const char* OnOff(bool on)
    {
        return on ? kOn : kOff;
    }

    // ---- per-window wording ----
    // Layer-authored text for individual windows, kept here with the rest (A8). Window content the
    // game draws (names, descriptions, its own strings) is passed through, not listed.

    // Editor step windows (the shared "Editor steps" tab).
    inline constexpr const char* kEditorSteps = "Editor steps";
    inline constexpr const char* kEditorStep = "Editor step";

    // Object selection.
    inline constexpr const char* kObjCategory = "Category";
    inline constexpr const char* kObjFilter = "Filter";
    inline constexpr const char* kObjSearch = "Search";
    inline constexpr const char* kObjClearSearch = "Clear search";
    inline constexpr const char* kObjSortByType = "Sort by type";
    inline constexpr const char* kObjSortByName = "Sort by name";
    inline constexpr const char* kObjSortAscending = "A to Z";
    inline constexpr const char* kObjSortDescending = "Z to A";
    inline constexpr const char* kObjSelectedCount = "Objects selected";
    inline constexpr const char* kObjList = "Objects";
    inline constexpr const char* kObjNoMatches = "No objects match the current filter";
    inline constexpr const char* kObjRequired = "required";
    inline constexpr const char* kObjUnavailable = "unavailable";
    inline constexpr const char* kObjSource = "Source";
    inline constexpr const char* kObjAuthor = "Author";
    inline constexpr const char* kObjAuthors = "Authors";
    inline constexpr const char* kObjReloaded = "Reloaded";
    inline constexpr const char* kObjReloadOnlySelected = "Only selected objects can be reloaded";

    inline std::string ObjCount(size_t count)
    {
        return std::to_string(count) + (count == 1 ? " object" : " objects");
    }

    // Inventions list.
    inline constexpr const char* kInvPreInvented = "Items pre-invented at start of game";
    inline constexpr const char* kInvToInvent = "Items to invent during game";
    inline constexpr const char* kInvEndOfList = "End of list";
    inline constexpr const char* kInvEmpty = "Empty";
    inline constexpr const char* kInvAlwaysInvented = "always invented";
    inline constexpr const char* kInvCannotMove = "Always invented, cannot be moved";
    inline constexpr const char* kInvCarried = "picked up";
    inline constexpr const char* kInvPickedUp = "Picked up";
    inline constexpr const char* kInvPutBack = "Put back";
    inline constexpr const char* kInvMoved = "Moved";
    inline constexpr const char* kInvAllToPreInvented = "All items moved to pre-invented";
    inline constexpr const char* kInvAllToInvent = "All items moved to items to invent";
    inline constexpr const char* kInvShuffled = "Items to invent shuffled";

    inline std::string InvMovedTo(const std::string& item, const char* list)
    {
        return item + " moved to " + list;
    }
} // namespace OpenRCT2::Ui::Accessibility::Graph::Vocab
