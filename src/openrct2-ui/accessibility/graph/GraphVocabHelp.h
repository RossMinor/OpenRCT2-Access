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

// F1 help: what each window or mode is for and the keys that are special there. Part of the one
// vocabulary module (spec A8) - GraphVocab.h includes it - kept in its own file only because it is
// long. Each text says what the place is, its own keys, then how to leave. Keys every window shares
// (arrows, Enter, Escape) are only named where they do something different.
namespace OpenRCT2::Ui::Accessibility::Graph::Vocab::Help
{
    // ---- shared ----

    // Any window without its own help, and a combo box's list open inside a window: the keys are the
    // same (Ross, 2026-10-27).
    inline constexpr const char* kWindow
        = "Up and down arrows: Move through the menu. Enter: Select the menu option you are currently on. Escape: "
          "Close menu.";

    // The game's text prompt.
    inline constexpr const char* kTextPrompt
        = "Type your text here. Enter: Confirms your input. Escape: Cancels what you typed. Backspace: Delete last "
          "character.";

    // The file browser.
    inline constexpr const char* kLoadSave
        = "File browser. Up and down move through files and folders. Right or Enter opens a folder, left goes "
          "up one folder. Enter on a file loads it. When saving, type a file name and press Enter; if a file "
          "with that name exists, you are asked before it is replaced. Escape cancels.";

    inline constexpr const char* kOverwritePrompt
        = "A file with this name already exists. Enter replaces it, Escape goes back without saving.";

    inline constexpr const char* kChangelogPrompt
        = "Up and down arrows: Move to next menu item. Enter: Activates the menu option selected. Escape: Activates "
          "no and closes the menu.";

    // ---- windows ----

    inline constexpr const char* kToolbar
        = "Toolbar. Tab, Shift + Tab, or up and down arrows: Move along the toolbar options. Enter: Open the "
          "option's window or menu. Escape: Return to the map.";

    inline constexpr const char* kTitleMenu
        = "Title menu. Up and down arrows: Move through the options. Enter: Select the option you are on.";

    inline constexpr const char* kScenarioSelect
        = "Tab and Shift + Tab: Move between the groups of scenarios. Up and down arrows: Move through the scenarios "
          "in a group. Enter: Start the scenario you are on. Letters: Jump to the next scenario starting with that "
          "letter. Escape: Go back.";

    inline constexpr const char* kOptions
        = "Tab and Shift + Tab: Move between pages. Up and down arrows: Move between settings. Left and right "
          "arrows: Change a setting. Enter: Toggle a checkbox or open a list of choices. Escape: Close the window.";

    inline constexpr const char* kAccessibilityOptions
        = "Up and down arrows: Move between settings. Left and right arrows: Change a setting; changes take effect "
          "at once. Escape: Close the window.";

    inline constexpr const char* kShortcutKeys
        = "Tab and Shift + Tab: Move between the groups of shortcuts. Up and down arrows: Move through the "
          "shortcuts. Enter: Change the shortcut you are on, then press its new key. Escape: Cancel a change, or "
          "close the window.";

    inline constexpr const char* kCheats
        = "Tab and Shift + Tab: Move between pages. Up and down arrows: Move between cheats. Enter: Turn a cheat on "
          "or off, select an option, or open a list. Left and right arrows: Change a number. Escape: Close the "
          "window.";

    inline constexpr const char* kPark
        = "Tab and Shift + Tab: Move between the pages: entrance, rating, guests, price, statistics, objective and "
          "awards. Enter: Open or close the park. Left and right arrows: Change the entry price. Escape: Close the "
          "window.";

    inline constexpr const char* kFinances
        = "Tab and Shift + Tab: Move between the pages: summary, the three graphs, marketing and research. Up and "
          "down arrows: Read the figures. Left and right arrows: Borrow or repay money. Enter: Set up a marketing "
          "campaign, or change the research funding and priorities. Escape: Close the window.";

    inline constexpr const char* kResearch
        = "Tab and Shift + Tab: Move between development and funding. Enter: Step the funding level through none, "
          "minimum, normal and maximum, or turn a research category on or off. Escape: Close the window.";

    inline constexpr const char* kNews
        = "Tab and Shift + Tab: Move between the messages and the notification settings. Enter: Go to what a message "
          "is about, or turn a setting on or off. Escape: Close the window.";

    inline constexpr const char* kRideList
        = "Tab and Shift + Tab, or left and right arrows: Move between rides, shops and stalls, and facilities. Up "
          "and down arrows: Move through the list. Enter: Open the window of the ride you are on. Shift + Enter: "
          "Move the map cursor to the ride. Letters: Jump to the next ride starting with that letter. Escape: Close "
          "the window.";

    inline constexpr const char* kRide
        = "Tab and Shift + Tab: Move between the pages: main, vehicle, operating, maintenance, colour, music, "
          "measurements, graphs, income and customers. Up and down arrows: Move between items. Left and right "
          "arrows: Change a setting. Enter: Select an option or open a list. Escape: Close the window.";

    inline constexpr const char* kNewRide
        = "Tab and Shift + Tab: Move between the kinds of ride. Up and down arrows: Move through the rides. Letters: "
          "Jump to the next ride starting with that letter. Enter: Choose the ride you are on, then pick a design or "
          "build your own. Escape: Close the window.";

    inline constexpr const char* kTrackList
        = "Up and down arrows: Move through the ready-made designs, and building your own. Enter: Choose a design "
          "and start placing it on the map. Escape: Go back to the ride list.";

    inline constexpr const char* kGuestList
        = "Tab and Shift + Tab, or left and right arrows: Move between the groups of guests. Up and down arrows: "
          "Move through the guests. Enter: Open the guest's window. Shift + Enter: Follow the guest with the camera "
          "until you press Escape. Escape: Close the window.";

    inline constexpr const char* kGuest
        = "Tab and Shift + Tab, or left and right arrows: Move between the pages: overview, statistics, rides, "
          "finances, thoughts and inventory. Up and down arrows: Read each page. Enter: Select an option. Escape: "
          "Close the window.";

    inline constexpr const char* kStaffList
        = "Tab and Shift + Tab, or left and right arrows: Move between handymen, mechanics, security guards and "
          "entertainers. Up and down arrows: Move through the staff. Enter: Hire a staff member, or open a staff "
          "member's window. Shift + Enter: Follow the staff member with the camera until you press Escape. Escape: "
          "Close the window.";

    inline constexpr const char* kStaff
        = "Tab and Shift + Tab, or left and right arrows: Move between the pages: overview, options and statistics. "
          "Up and down arrows: Move between items. Enter: Select an option, turn it on or off, or open a list. "
          "Escape: Close the window.";

    inline constexpr const char* kBanner
        = "Up and down arrows: Move between items. Enter: Change the text, turn no entry on or off, or remove the "
          "banner. Left and right arrows: Change a colour. Escape: Close the window.";

    inline constexpr const char* kSign
        = "Up and down arrows: Move between items. Enter: Change the text or remove the sign. Left and right arrows: "
          "Change a colour. Escape: Close the window.";

    inline constexpr const char* kScenery
        = "Tab and Shift + Tab: Move between the scenery groups. Up and down arrows: Move through the items. "
          "Letters: Jump to the next item starting with that letter. Enter: Close the window and start placing the "
          "item on the map. Escape: Close the window.";

    inline constexpr const char* kFootpath
        = "Up and down arrows: Choose an option: path type, queue type, railings, or build mode. Left and right "
          "arrows: Change the option. Enter: Select the option. Escape: Close the window, then build paths on the "
          "map with Space.";

    inline constexpr const char* kNewCampaign
        = "Up and down arrows: Move through the ride or item to advertise, the length in weeks, the cost, and Start. "
          "Enter: Open the list of rides or items, or begin the campaign. Left and right arrows: Change the number "
          "of weeks. Escape: Close the window.";

    inline constexpr const char* kPlayer
        = "Tab and Shift + Tab: Move between overview and statistics. Up and down arrows: Move between items. Enter: "
          "Open the list of groups, or select an option. Escape: Close the window.";

    inline constexpr const char* kMultiplayer
        = "Tab and Shift + Tab: Move between information, players, groups and options. Up and down arrows: Move "
          "between items. Enter: Use an item or open a list. Left and right arrows: Change a setting. Escape: Close "
          "the window.";

    inline constexpr const char* kServerStart
        = "Up and down arrows: Move between settings. Enter: Type into a text field, check or uncheck a checkbox, or "
          "select an option. Left and right arrows: Change the number of players. Escape: Close the window.";

    inline constexpr const char* kServerList
        = "Up and down arrows: Move through the servers, then the options to fetch the list, add a server, and start "
          "your own. Enter: Join the server or select the option you are on. Escape: Close the window.";

    inline constexpr const char* kPrompt
        = "Up and down arrows: Move between the answers. Enter: Choose the answer you are on. Escape: Cancel.";

    // ---- scenario editor windows ----

    inline constexpr const char* kObjectSelection
        = "The rides, scenery and other objects this scenario includes. Tab and Shift + Tab: Move between the pages "
          "of objects and the Editor steps tab, which holds Back and Forward. Up and down arrows: Move through the "
          "controls and the objects. Enter: Add or remove an object, open the filter list, or type a search. "
          "Letters: Jump to the next object starting with that letter. Left and right arrows: Change the category.";

    inline constexpr const char* kObjectFilterList
        = "Filter list. Up and down arrows: Move through the options. Enter: Check or uncheck an option and keep the "
          "list open, so you can set several. Escape: Close the list and keep your choices.";

    inline constexpr const char* kMap
        = "Left and right arrows: Change the map size. Enter: Type a map size, keep the map square, hear how to set "
          "land rights, start placing guest starting positions or the park entrance, or open the Map generator. "
          "Escape: Close the window. Shift + M: Open it again.";

    // Map generator: what the page holds, then the same keys on every page.
    inline std::string MapGen(int32_t page)
    {
        std::string text;
        switch (page)
        {
            case 0:
                text = "Generator page: the map's size, and where its land comes from: flatland, simplex noise for "
                       "random hills, or a height map image chosen with Browse. ";
                break;
            case 1:
                text = "Terrain page: the lowest and highest land, the land and edge styles or random ones, and "
                       "smooth tile edges. ";
                break;
            case 2:
                text = "Water page: the water level, and beaches around the water. ";
                break;
            default:
                text = "Forests page: whether to place trees, how much of the land they cover, and the heights they "
                       "grow between. ";
                break;
        }
        text += "Tab and Shift + Tab: Move between pages. Up and down arrows: Move between settings. Left and right "
                "arrows: Change a number by one step. Shift + left or right arrows: Change a number by ten steps. "
                "Enter: Type a number, open a list, check or uncheck a checkbox, or generate a new map, which "
                "replaces the whole map. Escape: Close the window.";
        return text;
    }

    inline constexpr const char* kLandRights
        = "Up and down arrows: Choose a setting. Enter: Apply the setting to the tiles named at the top of the list "
          "and close the window. Escape: Close without changing anything.";

    inline constexpr const char* kParkEntranceStyles
        = "Up and down arrows: Choose a style. Enter: Pick the style and start placing the entrance on the map. "
          "Escape: Close the window.";

    inline constexpr const char* kInventionsList
        = "Tab: Move between the items invented at the start, the items to invent during the game, the options, and "
          "in the editor the Editor steps tab. Enter: Pick up an item, then put it in front of another item, in "
          "either list, or at End of list. Escape: Put back the item you carry. Shift + Enter: Send an item to the "
          "end of the other list. Letters: Jump to the next item starting with that letter.";

    // Scenario options: what the page holds, then its keys.
    inline std::string ScenarioOptions(int32_t page, bool inEditor, bool onStepsTab)
    {
        if (onStepsTab)
            return "Editor steps. Up and down arrows: Move between the current step, Back and Forward. Enter: Go to "
                   "the previous or next step. Tab: Move back to the options.";
        std::string text;
        switch (page)
        {
            case 0:
                text = "Objective page: choose the scenario's goal, then its target and date. Up and down arrows: "
                       "Move between settings. Left and right arrows: Change a number by one step. Shift + left or "
                       "right arrows: Change a number by ten steps. Enter: Type a value, open a list, or check or "
                       "uncheck a checkbox. ";
                break;
            case 1:
                text = "Scenario details page: the park and scenario names, the group the scenario appears in, and "
                       "its description. Enter: Type a name or the details, open a list, or check or uncheck a "
                       "checkbox. ";
                break;
            case 5:
                text = "Preserved rides page. A checked ride cannot be demolished in the scenario. Enter: Check or "
                       "uncheck a ride. ";
                break;
            default:
                text = "Money, guest and land settings. Up and down arrows: Move between settings. Left and right "
                       "arrows: Change a number by one step. Shift + left or right arrows: Change a number by ten "
                       "steps. Enter: Type a value, open a list, or check or uncheck a checkbox. ";
                break;
        }
        text += inEditor ? "Tab and Shift + Tab: Move between pages and the Editor steps tab, where the options "
                           "continue to the next step."
                         : "Tab and Shift + Tab: Move between pages. Escape: Close the window.";
        return text;
    }

    // ---- map modes ----

    inline constexpr const char* kMap_Park
        = "Map. Arrow keys: Move the cursor. C: Report your coordinates. F: Report the direction you are facing. "
          "Shift + left or right arrows: Rotates the camera 90 degrees left or right. Control + arrow keys: Jumps "
          "the cursor to the nearest currently selected category. Control + shift + up or down arrows: Cycle between "
          "the various categories to jump to. E: Jump to the entrance of the park. Control + E: Jump between the "
          "entrance and exit of a ride. Control + G: Jumps to a specific coordinate. Shift + 1 - 0: Mark a waypoint "
          "on the map. Control + 1 - 0: Jump to that waypoint. Q: Find the entrance to a ride while on a queue. "
          "Shift + Q: Pathfinds to the entrance of a ride while on a queue. Space: Build a path tile. D: Remove a "
          "path tile. L: Change the slope of a tile to be placed. Home and End: Change the height of your cursor. "
          "Page up and page down: Raise or lower land. Control + page up or page down: Raise or lower water. X: "
          "Clear the scenery under the cursor. B: Changes the size of the brush. K: Places a first and then a second "
          "marker if pressed again. Z: Create a zone with a marked area. O: Buys the land under the cursor, or under "
          "the brush or marked area. Shift + O: Buys construction rights. Delete Key: Removes the item under the "
          "cursor, or opens a menu when there are several. Control + P: Reports if the path under the cursor "
          "connects to the entrance of the park. Control + H: Rescues all stranded guests. Enter: Interacts with "
          "whatever is under the cursor. Control + Enter: Build on the ride under the cursor. T: Opens the status "
          "readout: date, park rating, guests, cash and recent messages. M: Reports your cash amount. P: Pauses the "
          "game. Control + equals or dash: Change the game speed. Shift + C: Opens the construction window. Shift + "
          "F: Open the finances window. Shift + R: Open the rides window. Shift + I: Opens the Park Overview window. "
          "Shift + G: Opens the guest list window. Shift + S: Opens the Staff window. Shift + D: Opens the Research "
          "window. Shift + M: Opens the Recent Messages window. Shift + P: Opens the Paths window. Shift + X: Opens "
          "the Scenery window. Tab: Opens the toolbar. Control + F1: Open the Mod Settings window. Control + F2: "
          "Toggle reading of scenery on or off. Control + Space: Enables mouse mode for low-vision users.";

    inline constexpr const char* kMap_Editor
        = "Arrow keys: Move the cursor. C: Report your coordinates. F: Report the direction you are facing. Shift + "
          "left or right arrows: Rotates the camera 90 degrees left or right. Control + arrow keys: Jumps the cursor "
          "to the nearest currently selected category. Control + shift + up or down arrows: Cycle between the "
          "various categories to jump to. E: Jump to the entrance of the park. Control + G: Jumps to a specific "
          "coordinate. Shift + 1 - 0: Mark a waypoint on the map. Control + 1 - 0: Jump to that waypoint. Space: "
          "Build a path tile. D: Remove a path tile. L: Change the slope of a tile to be placed. Home and End: "
          "Change the height of your cursor. Page up and page down: Raise or lower land. Control + page up or page "
          "down: Raise or lower water. X: Clear the scenery under the cursor. B: Changes the size of the brush. K: "
          "Places a first and then a second marker if pressed again. O: Opens the land rights menu. Delete Key: "
          "Removes the item under the cursor, or opens a menu when there are several. Control + P: Reports if the "
          "path under the cursor connects to the entrance of the park. Shift + M: Opens the Map window, which builds "
          "the park entrance and sets guest starting positions. Shift + P: Opens the Paths window. Shift + X: Opens "
          "the Scenery window. Tab: Opens the toolbar. Control + F1: Open the Mod Settings window. Control + F2: "
          "Toggle reading of scenery on or off. Control + Space: Enables mouse mode for low-vision users.";

    inline constexpr const char* kMouseMode
        = "Mouse mode. The keys go to the game. Control + Space: Switch back to the keyboard cursor.";

    inline constexpr const char* kStatusReadout
        = "Status readout. Up and down arrows: Move through the date, park rating, guests, cash and recent messages. "
          "Enter: Open the recent messages. Escape: Close the status readout.";

    inline constexpr const char* kZoneList
        = "Zone list. Up and down: Move through your zones. Enter: Jump to the zone you have selected. R: Renames "
          "the zone you have selected. Delete twice: Removes the zone you have selected. Escape: Close the list.";

    inline constexpr const char* kZoneOverlap
        = "Zone overlaps another. Enter: Take the shared tiles from the other zone. Any other key: Cancel naming the "
          "zone.";

    inline constexpr const char* kDeletePicker
        = "Choosing what to remove. Arrows: Navigate the list. Enter: Delete an item. Escape: Close menu.";

    inline constexpr const char* kAreaConfirm
        = "Waiting for confirmation. The same key again: Do it across the marked area. Any other key: Cancel.";

    inline constexpr const char* kPlacingDesign
        = "Placing a ride design. Arrow keys: Move the ride tile by tile. R: Rotates the ride. Enter: Places the "
          "ride for viewing. Pressing enter again will build it. Backspace: Picks the ride back up if it hasn't been "
          "built. Escape: Cancels building the ride.";

    inline constexpr const char* kPlacingRide
        = "Placing a ride or stall. Arrow keys to move it around. R: Rotate by 90 degrees clockwise. Enter: Places "
          "the ride or stall to be used. Enter again: Builds the ride or stall. Space: Works like Enter. Backspace: "
          "Picks the ride or stall back up if it has not been built yet. Escape: Cancel.";

    inline constexpr const char* kPlacingScenery
        = "Placing scenery. Arrow keys: Move around. R: Rotate the scenery 90 degrees. Shift + W: Places focus on "
          "the top edge of the tile. Shift + D: Places focus on the right edge of the tile. Shift + S: Places focus "
          "on the bottom edge of the tile. Shift + A: Places focus on the left edge of the tile. The edges and "
          "corners are as you see the tile on screen, so they turn with the camera. Shift + Q: Places focus on the "
          "top left corner of the tile. Shift + E: Places focus on the top right of the tile. Shift + C: Places "
          "focus on the bottom right corner of the tile. Shift + Z: Places focus on the bottom left corner of the "
          "tile. Enter or Space: Places the scenery, and you can keep placing. Escape: Exits the scenery builder.";

    inline constexpr const char* kPlacingGuestStarts
        = "Placing guest starting positions. Arrow keys: Move the cursor. R: Turn the way guests walk. Enter: Place "
          "a start on a path outside the park, or remove the start already there. Escape: Finish placing.";

    inline constexpr const char* kPlacingParkEntrance
        = "Placing the park entrance. Arrow keys: Move it around. R: Rotate it. Enter: Show where it would go. Enter "
          "again: Build it. Backspace: Pick it back up. Escape: Cancel.";

    inline constexpr const char* kRideConstruction
        = "Building a ride. Arrow keys: Move the map cursor. Control + up or down arrows: Choose a build option. "
          "Control + left or right arrows: Change the build option. Control + Enter: Build the piece. Control + B: "
          "Report the build state. Delete: Remove the track piece under the cursor. Insert: Move the build helper to "
          "the track under the cursor. Escape: Stop building.";

    inline constexpr const char* kMazeConstruction
        = "Building a maze. Arrow keys: Move the map cursor. Control + up or down arrows: Choose an action: build a "
          "section, place the entrance, or place the exit. Control + Enter: Do the chosen action at the cursor. "
          "Escape: Stop building.";

    inline constexpr const char* kFollowing
        = "Following with the camera, which moves with them. Escape: Stop following.";

    inline constexpr const char* kNetworkStatus
        = "Escape: Cancels the connection.";
} // namespace OpenRCT2::Ui::Accessibility::Graph::Vocab::Help
