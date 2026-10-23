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

    inline constexpr const char* kNoTooltip = "No tooltip";

    // Any window without its own help.
    inline constexpr const char* kWindow = "Window. Up and down move between items, Enter uses the one you are on, "
                                           "Escape closes. Shift F1 reads an item's tooltip, and the backtick key "
                                           "says where you are.";

    // A combo box's list, open inside a window.
    inline constexpr const char* kDropdownList = "List open. Up and down move through the choices, Enter picks one, "
                                                 "Escape closes the list without changing anything.";

    // The game's text prompt.
    inline constexpr const char* kTextPrompt = "Typing. Type the text, then Enter confirms it and Escape cancels. "
                                               "Backspace deletes the last character.";

    // The file browser.
    inline constexpr const char* kLoadSave
        = "File browser. Up and down move through files and folders. Right or Enter opens a folder, left goes "
          "up one folder. Enter on a file loads it. When saving, type a file name and press Enter; if a file "
          "with that name exists, you are asked before it is replaced. Escape cancels.";

    inline constexpr const char* kOverwritePrompt
        = "A file with this name already exists. Enter replaces it, Escape goes back without saving.";

    inline constexpr const char* kChangelogPrompt
        = "Up and down choose yes or no, Enter confirms. Escape answers no.";

    // ---- windows ----

    inline constexpr const char* kToolbar
        = "Toolbar. Tab, Shift Tab, or up and down move along the toolbar buttons. Enter opens a button's window "
          "or its menu. In a menu, up and down move and Enter chooses. Escape returns to the map.";

    inline constexpr const char* kTitleMenu
        = "Title menu. Up and down move through the buttons, Enter chooses one. Game tools opens a list of the "
          "scenario editor, the track designer and the other tools.";

    inline constexpr const char* kScenarioSelect
        = "Scenario select. Tab and Shift Tab move between the groups of scenarios, up and down move through "
          "the scenarios in a group, and Enter starts the one you are on. Letters jump by first letter. Escape "
          "goes back.";

    inline constexpr const char* kOptions
        = "Options. Tab and Shift Tab move between the pages. Up and down move between settings, left and right "
          "change one, Enter toggles a checkbox or opens a list of choices. Escape closes.";

    inline constexpr const char* kAccessibilityOptions
        = "Accessibility settings, the mod's own. Up and down move between settings and left and right change "
          "them; changes take effect at once. Escape closes.";

    inline constexpr const char* kShortcutKeys
        = "Shortcut keys. Tab and Shift Tab move between the groups of shortcuts, up and down move through "
          "them. Enter on a shortcut waits for the new key to give it; Escape cancels that. Escape closes the "
          "window.";

    inline constexpr const char* kCheats
        = "Cheats. Tab and Shift Tab move between the pages. Up and down move between cheats. Enter turns a "
          "cheat on or off, presses a button, or opens a list; left and right change a number. Escape closes.";

    inline constexpr const char* kPark
        = "Park information. Tab and Shift Tab move between the pages: entrance, rating, guests, price, "
          "statistics, objective and awards. On the entrance page, Enter opens or closes the park. On the price "
          "page, left and right change the entry price. Escape closes.";

    inline constexpr const char* kFinances
        = "Finances. Tab and Shift Tab move between the pages: summary, the three graphs, marketing and "
          "research. Up and down read the figures. On the summary page, left and right on the loan borrow and "
          "repay. On marketing, Enter sets up a campaign. On research, Enter changes the funding and the "
          "priorities. Escape closes.";

    inline constexpr const char* kResearch
        = "Research. Tab and Shift Tab move between development and funding. On funding, Enter on the funding "
          "level steps through none, minimum, normal and maximum, and Enter on a research category turns it on "
          "or off. Escape closes.";

    inline constexpr const char* kNews
        = "Recent messages. Tab and Shift Tab move between the messages and the notification settings. Enter "
          "on a message goes to what it is about, and Enter on a setting turns it on or off. Escape closes.";

    inline constexpr const char* kRideList
        = "Ride list. Tab and Shift Tab, or left and right, move between rides, shops and stalls, and "
          "facilities. Up and down move through them. Enter opens one's window, Shift Enter takes the map "
          "cursor to it. Letters jump by first letter. Escape closes.";

    inline constexpr const char* kRide
        = "Ride window. Tab and Shift Tab move between the pages: main, vehicle, operating, maintenance, "
          "colour, music, measurements, graphs, income and customers. Up and down move between items, left and "
          "right change a setting, Enter uses a button or opens a list. Escape closes.";

    inline constexpr const char* kNewRide
        = "New ride. Tab and Shift Tab move between the kinds of ride. Up and down move through them, letters "
          "jump by first letter. Enter chooses one: you then pick a design or build your own. Escape closes.";

    inline constexpr const char* kTrackList
        = "Ride designs. Up and down move through the ready-made designs, and building your own. Enter chooses "
          "one and starts placing it on the map. Escape goes back to the ride list.";

    inline constexpr const char* kGuestList
        = "Guest list. Tab and Shift Tab, or left and right, move between the groups of guests. Up and down move "
          "through the guests. Enter opens a guest's window, Shift Enter follows them with the camera until you "
          "press Escape. Escape closes.";

    inline constexpr const char* kGuest
        = "Guest. Tab and Shift Tab, or left and right, move between the pages: overview, statistics, rides, "
          "finances, thoughts and inventory. Up and down read each page, Enter uses a button. Escape closes.";

    inline constexpr const char* kStaffList
        = "Staff list. Tab and Shift Tab, or left and right, move between handymen, mechanics, security guards "
          "and entertainers. Up and down move through them; Enter on the first item hires one. Enter opens a "
          "staff member's window, Shift Enter follows them with the camera until you press Escape. Escape "
          "closes.";

    inline constexpr const char* kStaff
        = "Staff member. Tab and Shift Tab, or left and right, move between the pages: overview, options and "
          "statistics. Up and down move between items, Enter uses a button, turns an option on or off, or opens "
          "a list. Escape closes.";

    inline constexpr const char* kBanner
        = "Banner. Up and down move between items. Enter changes the text, turns no entry on or off, or removes "
          "the banner. Left and right change a colour. Escape closes.";

    inline constexpr const char* kSign
        = "Sign. Up and down move between items. Enter changes the text or removes the sign. Left and right "
          "change a colour. Escape closes.";

    inline constexpr const char* kScenery
        = "Scenery. Tab and Shift Tab move between the scenery groups. Up and down move through the items, "
          "letters jump by first letter. Enter closes the window and starts placing the item on the map. Escape "
          "closes.";

    inline constexpr const char* kFootpath
        = "Footpath. Up and down choose an option: path type, queue type, railings, or build mode. Left and "
          "right change it, Enter selects it. Escape closes the window; then build paths on the map with Space.";

    inline constexpr const char* kNewCampaign
        = "Marketing campaign. Up and down move through it: the ride or item to advertise where the campaign "
          "has one, the length in weeks, the cost, and Start. Enter opens the list of rides or items, left and "
          "right change the length, and Enter on Start begins the campaign. Escape closes.";

    inline constexpr const char* kPlayer
        = "Player. Tab and Shift Tab move between overview and statistics. Up and down move between items; "
          "Enter on the group opens a list of groups, and Enter on a button uses it. Escape closes.";

    inline constexpr const char* kMultiplayer
        = "Multiplayer. Tab and Shift Tab, or left and right on the tab selector, move between information, "
          "players, groups and options. Up and down move between items, Enter uses one or opens a list, left "
          "and right change a setting. Escape closes.";

    inline constexpr const char* kServerStart
        = "Start a server. Up and down move between the settings. Enter on a text field lets you type it, left "
          "and right change the number of players, Enter toggles a checkbox, and Enter on a button uses it. "
          "Escape closes.";

    inline constexpr const char* kServerList
        = "Server list. Up and down move through the servers, then the buttons to fetch the list, add a "
          "server, and start your own. Enter joins a server or uses a button. Escape closes.";

    inline constexpr const char* kPrompt
        = "Question. Up and down move between the answers, Enter chooses one. Escape cancels.";

    // ---- scenario editor windows ----

    inline constexpr const char* kObjectSelection
        = "Object selection: the rides, scenery and other objects this scenario includes. Tab and Shift Tab move "
          "between the pages of objects and then the Editor steps tab, which holds Back and Forward. Up and down "
          "move through the controls and the objects. Enter adds or removes an object; letters jump by first "
          "letter. On Category, left and right change it. Enter on Filter opens its list, and Enter on Search "
          "lets you type.";

    inline constexpr const char* kObjectFilterList
        = "Filter list. Up and down move through the options. Enter checks or unchecks one and keeps the list "
          "open, so you can set several. Escape closes the list and keeps your choices.";

    inline constexpr const char* kMap
        = "Map. Map size X and map size Y: left and right change them, Enter types a size. Keep map square "
          "links the two. Set land rights explains how to set land from the map. Set guest starting positions "
          "and Build park entrance start placing them on the map. Map generator opens the generator, which is "
          "not accessible yet. Escape closes the window, and Shift M opens it again.";

    inline constexpr const char* kLandRights
        = "Land rights. Up and down choose a setting. Enter applies it to the tiles named at the top of the "
          "list and closes the window. To change which tiles, close this, then use B for the brush size or K to "
          "mark an area. Escape closes without changing anything.";

    inline constexpr const char* kParkEntranceStyles
        = "Park entrance styles. Up and down choose a style. Enter picks it and starts placing the entrance on "
          "the map. Escape closes.";

    inline constexpr const char* kInventionsList
        = "Inventions list. Tab moves between the items invented at the start, the items to invent during the "
          "game, the buttons, and in the editor the Editor steps tab. Enter picks up an item; Enter on another "
          "item puts it in front of that one, in either list, or at End of list. Escape, or Enter on the item "
          "you carry, puts it back. Shift Enter sends an item to the end of the other list. Letters jump by "
          "first letter.";

    // Scenario options: what the page holds, then the shared keys.
    inline std::string ScenarioOptions(int32_t page, bool inEditor, bool onStepsTab)
    {
        if (onStepsTab)
            return "Editor steps. Back and Forward move between the scenario editor's steps. Tab moves back to "
                   "the options.";
        std::string text = "Scenario options. ";
        switch (page)
        {
            case 0:
                text += "Objective: choose the scenario's goal, then its target and date. ";
                break;
            case 1:
                text += "Scenario details: the park and scenario names, the group the scenario appears in, and "
                        "its description. Enter on a name or the details lets you type it. ";
                break;
            case 5:
                text += "Preserved rides: Enter checks or unchecks a ride. A checked ride cannot be demolished "
                        "in the scenario. ";
                break;
            default:
                text += "Money, guest and land settings. ";
                break;
        }
        if (page != 1 && page != 5)
            text += "On a number, left and right change it one step, Shift with left and right ten steps, and "
                    "Enter types a value. ";
        text += "Enter on a list opens it, and Enter on a checkbox checks or unchecks it. ";
        text += inEditor ? "Tab and Shift Tab move between the pages and the Editor steps tab, where the buttons "
                           "continue to the next step."
                         : "Tab and Shift Tab move between the pages. Escape closes.";
        return text;
    }

    // ---- map modes ----

    inline constexpr const char* kMap_Park
        = "Map. Arrow keys move the cursor. C reads coordinates, F the way the camera faces, Shift with left and "
          "right turns the camera. Control with an arrow jumps to the nearest thing that way, and Control Shift "
          "up and down choose what it finds. E goes to the park entrance, Control E between a ride's entrance "
          "and exit, Control G to typed coordinates. Shift with a number sets a waypoint, Control with a number "
          "jumps to it. Q gives a queue's route. Space builds a path, D removes one, L sets its slope, Home and "
          "End change the building height. Page Up and Page Down raise and lower land, with Control for water. "
          "X clears scenery, B sets the brush size, K marks an area, Z names it as a zone. O buys land, Shift O "
          "construction rights. Delete removes one thing here. Control P checks whether a path reaches the park "
          "entrance, Control H rescues stranded guests. Enter opens what is here, Control Enter builds on a "
          "ride. T reads the park's status, M your cash. P pauses, Control equals and Control minus change the "
          "game speed. Shift with C, F, R, I, G, S, D or M opens the game's windows. Tab opens the toolbar. Control F1 opens the mod settings, Control F2 turns scenery reading on or off, "
          "Control Space switches to the mouse.";

    inline constexpr const char* kMap_Editor
        = "Scenario editor, landscape. Arrow keys move around the map, C reads the tile and coordinates, Control "
          "G jumps to typed coordinates. Space builds a footpath, D removes one, L sets its slope, Home and End "
          "change the building height. Page Up and Page Down raise and lower land, with Control for water. X "
          "clears scenery, B sets the brush size, K marks an area. Delete removes one thing here. Control with "
          "an arrow jumps to the nearest thing that way. Shift with a number sets a waypoint, Control with a "
          "number jumps to it. Control P checks whether a path reaches the park "
          "entrance. O sets land rights. Shift M opens the Map window, which builds the park entrance and sets "
          "guest starting positions. Shift P opens the path menu, Shift X the scenery menu. Tab opens the "
          "toolbar, where the editor's steps are. Control F1 opens the mod settings.";

    inline constexpr const char* kMouseMode
        = "Mouse mode is on: the keys go to the game. Control Space switches back to the keyboard cursor.";

    inline constexpr const char* kStatusReadout
        = "Status readout. Up and down move through the date, park rating, guests, cash and recent messages. "
          "Enter on recent messages opens them. Escape closes.";

    inline constexpr const char* kZoneList
        = "Zone list. Up and down move through your zones. Enter jumps to one, R renames it, Delete twice "
          "removes it. Escape closes the list.";

    inline constexpr const char* kZoneOverlap
        = "Naming a zone that overlaps another. Enter takes those tiles from the other zone; any other key "
          "cancels the naming.";

    inline constexpr const char* kDeletePicker
        = "Choosing what to remove. Up and down move through the things here, Enter removes the one you are on. "
          "If you are asked to confirm, up and down choose yes or no and Enter answers. Escape cancels.";

    inline constexpr const char* kAreaConfirm
        = "Waiting for confirmation. Press the same key again to do it across the marked area. Any other key "
          "cancels.";

    inline constexpr const char* kPlacingDesign
        = "Placing a ride design. Arrow keys move it, R rotates it. Space or Enter shows where it would go; arrow "
          "around to check it, then Space or Enter again builds it. Backspace picks it back up. Escape cancels.";

    inline constexpr const char* kPlacingRide
        = "Placing a ride or stall. Arrow keys move it, R rotates it. Space or Enter shows where it would go; "
          "arrow around to check it, then Space or Enter again builds it. Backspace picks it back up. Escape "
          "cancels.";

    inline constexpr const char* kPlacingScenery
        = "Placing scenery. Arrow keys move it, R rotates it, Space or Enter places it; you can keep placing. "
          "Shift with W, A, S or D puts it on that edge of the tile, Shift with Q, E, Z or C in that corner. "
          "Escape finishes.";

    inline constexpr const char* kRideConstruction
        = "Building a ride. Arrow keys move the map cursor. Control up and down choose a build option, Control "
          "left and right change it, Control Enter builds the piece. Control B reads the build state. Delete "
          "removes the piece under the cursor, Insert moves the build helper to the track under the cursor. "
          "Escape stops building.";

    inline constexpr const char* kMazeConstruction
        = "Building a maze. Arrow keys move the map cursor. Control up and down choose an action: build a "
          "section, place the entrance, or place the exit. Control Enter does it at the cursor. Escape stops "
          "building.";

    inline constexpr const char* kFollowing
        = "Following someone with the camera, which moves with them. Escape stops following.";

    inline constexpr const char* kNetworkStatus
        = "Connection status. Each step of connecting is read as it happens; Enter repeats the latest. Escape "
          "cancels the connection.";
} // namespace OpenRCT2::Ui::Accessibility::Graph::Vocab::Help
