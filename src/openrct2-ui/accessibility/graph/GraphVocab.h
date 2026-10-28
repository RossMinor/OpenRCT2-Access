/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "GraphVocabHelp.h"

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
    inline constexpr const char* kRoleSpinner = "spinner";
    inline constexpr const char* kUnavailable = "unavailable";

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

    // Map window (scenario editor and sandbox mode). Its tool buttons are pictures with long
    // tooltips, so they get short names; F1 still reads the game's tooltip.
    inline constexpr const char* kMapSizeX = "Map size X";
    inline constexpr const char* kMapSizeY = "Map size Y";
    inline constexpr const char* kMapKeepSquare = "Keep map square";
    inline constexpr const char* kMapSetLandRights = "Set land rights";
    inline constexpr const char* kMapGuestStarts = "Set guest starting positions";
    inline constexpr const char* kMapBuildEntrance = "Build park entrance";
    inline constexpr const char* kMapGenerator = "Map generator";
    // Ross's wording (2026-10-22): Set land rights closes the Map window and says how it is done.
    inline constexpr const char* kMapLandRightsHint
        = "Use O to set the land rights of the tile you are on. Use K to set rights within a specified area.";

    // Map generator (scenario editor). Its four pages, and the two style buttons, which are pictures.
    inline constexpr const char* kMapGenPages[] = { "Generator", "Terrain", "Water", "Forests" };
    inline constexpr const char* kMapGenLandStyle = "Land style";
    inline constexpr const char* kMapGenEdgeStyle = "Edge style";
    inline constexpr const char* kMapGenNeedImage = "Choose an image file with Browse first";
    inline constexpr const char* kMapGenImageFailed = "That image could not be used as a height map";
    // Said on Generate while the map already has a park entrance or guest starting positions.
    inline constexpr const char* kMapGenReplacesMap = "replaces the whole map and everything on it";

    // A height as the map cursor speaks it, after the window's own metres or feet.
    inline std::string ElevationPart(const std::string& elevation)
    {
        return "elevation " + elevation;
    }

    // What Generate made: the map's size, the range of its land, how much is under water, its trees.
    inline std::string MapGenerated(
        int32_t sizeX, int32_t sizeY, const std::string& lowest, const std::string& highest, int64_t waterTiles,
        int64_t waterPercent, int64_t trees)
    {
        std::string text = "Map generated, " + std::to_string(sizeX) + " by " + std::to_string(sizeY) + ". ";
        text += lowest == highest ? "Land at elevation " + lowest + ". "
                                  : "Land from elevation " + lowest + " to " + highest + ". ";
        if (waterTiles == 0)
            text += "No water. ";
        else if (waterPercent == 0)
            text += "Less than 1 percent under water. ";
        else
            text += std::to_string(waterPercent) + " percent under water. ";
        if (trees == 0)
            text += "No trees.";
        else
            text += std::to_string(trees) + (trees == 1 ? " tree." : " trees.");
        return text;
    }

    // Land rights: the editor's five ownership settings, or a park's two buying options, applied
    // from the keyboard to the map cursor's brush or marked area.
    inline constexpr const char* kBuyLand = "Buy land";
    inline constexpr const char* kBuyConstructionRights = "Buy construction rights";
    inline constexpr const char* kNothingForSaleHere = "nothing for sale here";

    inline std::string LandRightsAppliesTo(const std::string& area)
    {
        return "Applies to " + area;
    }

    inline std::string LandRightsApplied(const std::string& setting, const std::string& area)
    {
        return setting + " set on " + area;
    }

    // Park entrance window (scenario editor).
    inline constexpr const char* kEntranceStyles = "Entrance styles";
    inline constexpr const char* kEntranceNoStyles = "No entrance styles. Choose one in object selection.";

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

    // Scenario options (the editor's options, objective and details steps; the Cheats menu in a park).
    inline constexpr const char* kScenarioOptions = "Scenario options";
    inline constexpr const char* kScenarioOptionsPages[] = {
        "Objective", "Scenario details", "Financial", "Guests", "Land restrictions", "Preserved rides",
    };
    inline constexpr const char* kScenarioOptionsEscape = "Tab over to the buttons to continue."; // Ross's wording
    inline constexpr const char* kNoRides = "No rides";
    inline constexpr const char* kNotANumber = "Not a number";
    inline constexpr const char* kCancelled = "Cancelled";

    // The game's mouse tools for land, water, clearing scenery and buying land: from the keyboard the
    // map cursor does their jobs, so opening one says which keys to use.
    inline constexpr const char* kLandToolHint
        = "Use Page Up and Page Down to raise and lower the land you are on. Use B to change the brush size, and K "
          "to work on a marked area.";
    inline constexpr const char* kWaterToolHint
        = "Use Control Page Up and Control Page Down to raise and lower the water you are on. Use B to change the "
          "brush size, and K to work on a marked area.";
    inline constexpr const char* kClearSceneryToolHint
        = "Use X to clear the scenery on the tile you are on. Use B to change the brush size, and K to work on a "
          "marked area.";

    // Connection status window.
    inline constexpr const char* kConnectionStatus = "Connection status";

    inline std::string TypeValueRange(const std::string& from, const std::string& to)
    {
        return "Type a value from " + from + " to " + to;
    }

    inline std::string ValueOutOfRange(const std::string& label, const std::string& from, const std::string& to)
    {
        return label + " runs from " + from + " to " + to;
    }
} // namespace OpenRCT2::Ui::Accessibility::Graph::Vocab
