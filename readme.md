# OpenRCT2-Access

**A blind-accessibility mod for [OpenRCT2](https://github.com/OpenRCT2/OpenRCT2).**

OpenRCT2-Access is a fork of OpenRCT2 that adds screen-reader support and
full keyboard control, so blind and low-vision players can play
Rollercoaster Tycoon 2 without a mouse or sight. It speaks the interface
through the **NVDA** screen reader and lets you navigate the game's menus,
windows, and map entirely from the keyboard.

## Important Note About This Page

This repository is a fork of Open RCT2. Below are instructions on how to use the mod, but I would still research the game on your own and how to play. I will explain how certain aspects of the game works to illustrate how they interact with the mod itself, but it would be too much for me to write detailed walkthroughs on how the game itself works.

A great place to start is the **[Getting Started page](https://docs.openrct2.io/en/latest/playing/getting_started/index.html)** on the Open RCT 2 [website](https://openrct2.io). While the documentation will be oriented toward sighted players that use a mouse, it does explain in detail how the game works, where you can then apply that knowledge to the mod documentation here.

---

## Requirements

- Windows and the [NVDA screen reader](https://www.nvaccess.org/download/).
- The Rollercoaster Tycoon 2 game from [Steam](https://store.steampowered.com/app/285330/).

---

## Installing and updating

1. Make sure NVDA is installed and running.
2. Download the latest release of the mod under the **Releases** heading.
3. Launch the game from Steam to generate any needed files, then close the game. It will likely ask you if you would like to install Direct Play, which you want to do.
4. Unzip the mod folder anywhere on your computer.
5. Run OpenRCT2Access.exe in the folder and the game will be accessible. **Note:** You will need to launch the game with this method for the time being.
6. The mod has an updater, so you will be alerted when there's a new update.

---

## Using the Mod

### Important

At any point, you can press F1 to hear what commands you currently have at your disposal and what you can do with them in the game.

### Currently Not Accessible or Not Fully Tested

- Building custom rollercoasters
- Online Multiplayer
- Importing extensions.
- MacOS compatibility

### Navigation

- Arrow Keys: Moves around the map tile by tile. They can also be held to scroll across the map.
- C: Read your current coordinates.
- F: Read the direction you are currently facing.
- Shift + Left or Right: Snaps the camera 90 degrees in either direction.
- E: Jumps focus to the entrance of the park.
- Control + arrow keys: Jumps to the nearest ride or stall in that direction.
- Control + E: Jumps between the entrance and exit of the ride you are hovering over.
- Shift + 1 ... 9, 0: Place a waypoint at the current location.
- Control + 1 ... 9, 0: Jump to the waypoint of that number.

### Information

- Tab: Opens the Tools Menu.
- F1: Mod/game help. Reports actions or commands that can be used anywhere in the game.
- Control + F1: Opens the mod settings.
- T: Open the park stats window.
- M: Report your cash amount.
- []: Moves back or forward through game announcements.
- Enter: While hovering over a ride or stall, it will open that ride/stall's information page.
- Shift + C: Opens the Construct a New Ride window.
- Shift + F: Open the Finances window.
- Shift + R: Opens the Rides window.
- Shift + P: Open the Park Information Window.
- Shift + G: Open the Guest List window.
- Shift + S: Opens the Staff window.
- Shift + D: Opens the Research and Design window.
- Shift + M: Opens the Recent Messages window.

### Building

- Space: Builds a footpath of the selected type on the current tile.
- Control + P: Tells the user if the path they are currently on connects all the way to the entrance. Useful for knowing if guests can get to a ride or other location.
- P: Pause or unpause the game.
- F4: Opens the path menu.
- D: Deletes the path on the current tile and elivation.
- L: Change the slope of the path. Once your desired slope is selected, you can resume using space to build the path. The sloped path will build int he direction you are facing. So if you are facing north and have slope set to up, the path will gain elivation going north.
- . or ,: Raise and lower the elivation you are working on. This is useful for building elivated paths.
- shift + . or ,: Snaps the cursor to any objects above or below you, respectively.
- Page up or down: Raises and lower the land tile you are currently on. **Note:** There is a sound that plays with terrain to indicate elevation. The higher pitch, the higher the elevation. The lower the pitch, the lower the elevation.
- Control page up or down: Raise or lower the water tile you are currently on.
- X: Clear the scenery on the tile and elivation you are currently on. Examples are trees, bushes, fences, and other objects.
- B: Cycle the size of the brush from 1x1, 3x3, 5x5, and 7x7.
- O: Buy land ownership over the tile you're on.
- Shift + O: Buy construction rights over the tile you're on.
- K: Place a first and then second marker to determine an area to be modified. Actions that can be performed are terraforming, laying and deleting paths, clearing scenery, and purchasing land and construction rights.
- Shift + K: Snaps your focus between the two markers.
- Control + Enter: If hovering over a ride or stall, it will open its construction/build menu.
- Shift + B: Report the break status of the custom ride tile you are hovering over.


#### While in Build Mode

Build mode is activated when you construct pre-built or custom rides. It will cause the mod to behave differently than it normally does while you're in this mode.

- Arrow Keys: Moves the ride you are wanting to construct around the map.
- R: Rotate the ride you are constructing 90 degrees right.
- Enter: Attempts to place the ride. If there is scenery in the location area where the ride will be placed, it will automatically remove the scenery. If there is scenery in the area, but it is still an invalid construction area, the scenery will stay and the game will report the error that is preventing you from constructing and tell you how and where it needs to be fixed.
- Escape: Cancels construction or exits build mode.
- Shift + W, A, S, or D: Focusses the scenery you are building on the tile's edge of that direction. For example, if you are placing a bench, you can press shift + W to put it on the north side of the tile you are currently on and then press space to place it.
- Shift + Q, E, Z, or C: Focusses the scenery you are building on the tile's corner you are currently on. For example, if you were placing flowers, you can press shift + Q to point a flower at the top left corner of the tile you are on and then can press space to place the flower.
- Control + Up or Down: Cycles through the custom ride builder menu.
- Control + Left or Right: Cycle between the options for that menu item.
- Control + Enter: Builds the ride piece with the attributes selected.
- Control + B: Reports the build status of the custom ride.

### Other Useful Commands

- Control + - or =: Speeds up or slows down time.
- Control + H: Teleports all stranded guests on the map back to the entrance and onto a path.
- Control + Space: Toggle between keyboard and mouse mode. When in keyboard mode, the mouse is modified to not interfere with the keyboard focus. With mouse mode on, it allows the mouse to behave as it would in Open RCT2 normally.

## Tips for Playing the Game

### Important

It’s highly recommended to have your first park be a blank slate because it is much more easy to understand the layout of a park when you’ve placed everything yourself. It will also give you an opportunity to learn the commands and mechanics without worrying about making mistakes.

To create a blank map:

1. Select new game from the main menu.
2. Move left one to tab to the Extras menu.
3. Choose any of these maps to build your park.

### Placing Paths and Queues

Paths and queues are everything in this game because they direct your guests as to where they can and cannot go. Queues are just as important because they show guests where to line up at the entrance to a ride. Paths and queues are 1 tile each and must be placed with no breaks in order for guests to recognize them as a continuous path or queue. If a path leads to the entrance of a ride, a guests would not view it as a place they can stand and wait for their turn on the ride, and therefore would begin complaining about not being able to get on the ride. By placing queues, guests will know where to begin waiting and will begin to forma  line. You also do not need to use queues for the exits of your rides because guests will simply exit the ride and walk off to do whatever else they want to do.
Guests will never walk off the path or queue into empty space. However, if a path or queue is deleted while they are on It, then they will be placed on empty space and wander around aimlessly. Additionally, if you delete a path or queue and guests are on the other side of it that isn’t connected to the entrance of the park, they are now more or less stranded on an island and cannot leave the park. If either of these scenarios happen, guests will begin to complain and lower the rating of your park. To avoid this, you can press control + “p” while on a path and it will trace all the way back to the entrance of your park to insure guests are able to leave. If there is a break in the path, the mod will tell you where that break occurs. If a guests does happen to get stranded off the path, you can press control + “h” from anywhere to teleport them back to the entrance of the park. This does not mean they will automatically leave the park once being placed there, there are many other factors that determine  when a guests wants to eat, go on a ride, leave the park, etc.

### Placing Sloped Paths and Queues

Sloped paths and queues are important because they allow your guests to reach higher elevations. You can change the slope of the path or queue you are placing by pressing “l”. Each slope tile elevates up or down one unit, so if you are at elevation 7 and a ride entrance is at elevation 12, you would need to place 6 tiles sloping up to properly reach the same elevation as the entrance. The reason you would need 6 and not 5 is because in order for a path to connect to an entrance or exit, the path or queue tyle directly outside of it needs to be flat and not a slope. When placing a sloped path, you must be at the same elevation as the current path for it to connect properly. It’s also important to remember that paths and queues can be placed below or above other objects, but the game will let you know if you are unable to place a path or queue and tell you specifically why. For example, some pre-built rides have tricky entrances or exits for guests to access, so sometimes you may need to snake a path or queue above or below the ride itself for guests to get in or out.
