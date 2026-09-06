# OpenRCT2-Access

**A blind-accessibility mod for [OpenRCT2](https://github.com/OpenRCT2/OpenRCT2).**

OpenRCT2-Access adds screen-reader support and full keyboard control to
OpenRCT2, so blind and low-vision players can play Rollercoaster Tycoon 2
without a mouse or sight. It speaks the interface through whichever screen
reader you already use and lets you navigate the game's menus, windows, and
map entirely from the keyboard. It installs into a copy of OpenRCT2 you
already have.

## Important Note About This Page

This repository is a fork of Open RCT2. Below are instructions on how to use the mod, but I would still research the game on your own and how to play. I will explain how certain aspects of the game works to illustrate how they interact with the mod itself, but it would be too much for me to write detailed walkthroughs on how the game itself works.

A great place to start is the **[Getting Started page](https://docs.openrct2.io/en/latest/playing/getting_started/index.html)** on the Open RCT 2 [website](https://openrct2.io). While the documentation will be oriented toward sighted players that use a mouse, it does explain in detail how the game works, where you can then apply that knowledge to the mod documentation here.

---

## Requirements

- Windows.
- A screen reader. The mod speaks through [Prism](https://github.com/ethindp/prism), a library that hands the mod's speech to whichever reader you already run, so NVDA, JAWS, Narrator, System Access, ZDSR and others all work. [NVDA](https://www.nvaccess.org/download/) is free and is the one the mod is developed and tested against, so it is still the safest choice.
- The Rollercoaster Tycoon 2 game from [Steam](https://store.steampowered.com/app/285330/).
- [OpenRCT2](https://openrct2.io/download) itself. The mod installs into the copy of OpenRCT2 you already have rather than setting up a second one, and any version will do - the download brings the game files it needs, so an older OpenRCT2 is brought up to date as part of installing.

---

## Installing and updating

The mod installs itself into your existing OpenRCT2, so your saved parks, settings and scenarios stay exactly where they are and you keep playing the same copy of the game you already had.

1. Make sure your screen reader is installed and running.
2. Install [OpenRCT2](https://openrct2.io/download) if you do not already have it.
3. Launch Rollercoaster Tycoon 2 from Steam once to generate any needed files, then close it. It will likely ask you if you would like to install Direct Play, which you want to do.
4. Download the latest release of the mod under the **Releases** heading.
5. Unzip the download somewhere that is **not** your OpenRCT2 folder. Your Downloads folder is fine.
6. Close OpenRCT2 if it is running.
7. Run **Install-OpenRCT2Access.bat**. It finds your OpenRCT2, tells you exactly what it is going to change, and asks you to type `yes` before it touches anything.
8. Launch OpenRCT2 the way you normally do. It will now be accessible.

If it says it cannot write to the folder, close the window, then right-click **Install-OpenRCT2Access.bat** and choose **Run as administrator**. That happens when OpenRCT2 is installed under Program Files.

To remove the mod, run **Uninstall-OpenRCT2Access.bat**. It puts your original OpenRCT2 back and deletes everything the installer added. The exception is if the installer also updated your OpenRCT2 version: in that case there is no matching original to put back, and the uninstaller will tell you to reinstall OpenRCT2 instead.

### About versions

Each mod release is built against one specific version of OpenRCT2, and the download carries that version's game files with it. So if your OpenRCT2 is older than the release, the installer simply brings it up to date at the same time - there is nothing extra to download and nothing to choose. Your saved parks, settings and RollerCoaster Tycoon 2 files are not touched; they live outside the game folder.

The one case it will not handle is an OpenRCT2 **newer** than the mod release. It stops rather than moving your game backwards, because a park you saved with the newer version might not open afterwards. When that happens, wait for a mod release built for your version - the message names both versions so you know what to look for.

Also worth knowing: **updating OpenRCT2 yourself will remove the mod**, because the update replaces the program file the mod lives in. If speech stops working after OpenRCT2 updates, run the mod installer again.

The mod also has its own updater and will tell you in-game when a new version is out.

---

## Using the Mod

### Important

At any point, you can press F1 to hear what commands you currently have at your disposal and what you can do with them in the game.

### Currently Not Accessible or Not Fully Tested

- Online Multiplayer
- Importing extensions.
- MacOS compatibility

### Navigation

- Arrow Keys: Moves around the map tile by tile. They can also be held to scroll across the map.
- C: Read your current coordinates.
- F: Read the direction you are currently facing.
- Shift + Left or Right: Snaps the camera 90 degrees in either direction.
- E: Jumps focus to the entrance of the park.
- Control + arrow keys: Jumps to the nearest object in that direction and reads the coordinates it lands on. What it looks for is set by the filter below.
- Control + Shift + Up or Down: Chooses what Control + arrows jump to: rides and stalls, scenery, footpath objects (bins, benches, lamps), or hazards (litter, vomit, and vandalised objects). It starts on rides and stalls.
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
- Enter: Opens whatever is under the cursor - a ride or stall's information page, a sign, the park gates, or a guest, staff member or ride vehicle standing there.
- Shift + C: Opens the Construct a New Ride window.
- Shift + F: Open the Finances window.
- Shift + R: Opens the Rides window.
- Shift + P: Open the Park Information Window.
- Shift + G: Open the Guest List window.
- Shift + S: Opens the Staff window.
- Shift + D: Opens the Research and Design window.
- Shift + M: Opens the Recent Messages window.

### Building

- Space: Places whatever you are placing. With nothing selected that means a footpath of the current type on the current tile; while placing a ride, stall or scenery it places that instead. Enter also still works for placing, so either key will do.
- Delete: Removes one thing on the current tile - a path, a bin or bench, scenery, a fence, a sign, a ride entrance or exit, or a piece of track. If there is only one thing there it just goes. If there are several, the mod reads them out and you choose with the up and down arrows, then press Enter. Track always asks you to confirm first, because the game has no undo.
- Control + P: Tells the user if the path they are currently on connects all the way to the entrance. Useful for knowing if guests can get to a ride or other location.
- F4: Opens the path menu.
- D: Deletes the path on the current tile and elivation. Unlike Delete, this works across a whole marked area.
- L: Change the slope of the path. Once your desired slope is selected, you can resume using space to build the path. The sloped path will build int he direction you are facing. So if you are facing north and have slope set to up, the path will gain elivation going north.
- Home or End: Raise and lower the elivation you are working on. This is useful for building elivated paths.
- Shift + Home or End: Snaps the cursor to any objects above or below you, respectively.
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

### About elevation numbers

Elevations are spoken as the same numbers the game itself uses on its height markers, so they match what a sighted player sees on screen. Sea level (the default water level) is elevation 0, and land below it reads as a minus number. When the cursor is on a tile where something stands above the ground - an elevated path or bridge, a raised ride entrance or exit, track passing overhead, or a sloped path - the tile readout ends with that thing's elevation. Bare ground never reports a height, since the ground is the baseline everything else is measured against. If several things overlap at different heights, every height is read: "elevation 2 and 4" for an elevated queue with coaster track flying over it. They are listed in whichever direction your **Tile reading order** setting uses, so the heights follow the same order as the features. The elevation tone follows the same levels, one note each, in the same order - but a note only sounds for a level that has actually changed since the last tile. Walking along under a coaster on level ground beeps once for the track and then goes quiet; the ground note returns only when the ground itself changes height. Note that pressing C reports where **you** are - the ground, or the height you have raised your working elevation to - not the height of something passing overhead. Those come from the tile readout. How often the heights are spoken is set by **Elevation reading** in the mod settings (control + F1): every tile, on change (the default, which stays quiet while you walk a bridge at one height), or off. You will occasionally hear an elevation like "4.5". That means the thing you are on sits **between** two normal steps. Ride track and sloped paths legitimately do this, so hearing it during a Z scan (shift + Home or End) is normal. Hearing it on a **flat path or queue you built** is a warning: a path half a step off the grid can never connect to its neighbours, no matter how far it runs, and control + P will report the network as broken. Delete that stretch and rebuild it from a whole-numbered elevation.


#### While in Build Mode

Build mode is activated when you construct pre-built or custom rides. It will cause the mod to behave differently than it normally does while you're in this mode. The mod says "entering build mode" followed by the ride's name when it starts, whichever way you got there.

- Arrow Keys: Moves the ride you are wanting to construct around the map.
- R: Rotate the ride you are constructing 90 degrees right.
- Space or Enter: Attempts to place the ride. If there is scenery in the location area where the ride will be placed, it will automatically remove the scenery. If there is scenery in the area, but it is still an invalid construction area, the scenery will stay and the game will report the error that is preventing you from constructing and tell you how and where it needs to be fixed.
- Escape: Cancels construction or exits build mode.
- Shift + W, A, S, or D: Focusses the scenery you are building on the tile's edge of that direction. For example, if you are placing a bench, you can press shift + W to put it on the north side of the tile you are currently on and then press space to place it.
- Shift + Q, E, Z, or C: Focusses the scenery you are building on the tile's corner you are currently on. For example, if you were placing flowers, you can press shift + Q to point a flower at the top left corner of the tile you are on and then can press space to place the flower.
- Control + Up or Down: Cycles through the custom ride builder menu. The menu items you get depend on the ride: only the pieces that ride type can actually build are offered. Note that while the builder is open these keys belong to it, so Control + arrow keys will not jump the cursor to a nearby ride until you close the builder.
- Control + Left or Right: Cycle between the options for that menu item.
- Special piece: One of the custom ride builder's menu items, sitting just after Curve. This is where a ride type's own pieces live - vertical loops, corkscrews, helices, reversers, s-bends, on-ride photo sections and so on - so its contents differ from one ride type to the next. Control + Left or Right steps through the pieces you can build from where you are standing; pieces that don't fit your current slope, banking or heading are skipped. The first entry is "none", which takes you back to ordinary curve-and-slope track. Note that a special piece comes with its slope, banking and chain lift already built in, so the game locks those three menu items for as long as one is selected. They will read as "locked" when you arrive on them, and trying to change one will tell you which piece is holding it. Set Special piece back to "none" to get them back.
- Control + Enter: Builds the ride piece with the attributes selected.
- Control + B: Reports the build status of the custom ride.
- Delete: Removes the track piece your map cursor is on. This reaches any piece in the ride, not just the last one you placed, so you can fix a mistake in the middle without undoing everything after it. If two pieces of the ride are stacked on the same tile, the one nearest your cursor's elevation is the one that goes. Afterwards the build helper moves to the end of the run of track still joined to the station, so Control + Enter carries on building from there rather than from the hole you just made.
- Insert: Moves the build helper to the track your map cursor is on, then along that stretch to its open end, so you can pick construction up from anywhere in the ride instead of walking back through it piece by piece. Your cursor needs to be on a piece of the ride you are building. If a piece you build closes a gap, the helper walks itself on to the next open end and says "joined up".

### Other Useful Commands

- p: Pauses and unpauses the game.
- Control + - or =: Speeds up or slows down time.
- Control + H: Teleports all stranded guests on the map back to the entrance and onto a path.
- Control + Space: Toggle between keyboard and mouse mode. When in keyboard mode, the mouse is modified to not interfere with the keyboard focus. With mouse mode on, it allows the mouse to behave as it would in Open RCT2 normally.
- / (slash): Opens the chat box in multiplayer. This is C in stock OpenRCT2, but C is the coordinate read-out here, so the mod moves chat to slash. You can change it back under Options, Controls and Interface, Shortcut keys.

## Tips for Playing the Game

### Important

It’s highly recommended to have your first park be a blank slate because it is much more easy to understand the layout of a park when you’ve placed everything yourself. It will also give you an opportunity to learn the commands and mechanics without worrying about making mistakes.

To create a blank map:

1. Select new game from the main menu.
2. Move left one to tab to the Extras menu.
3. Choose any of these maps to build your park.
4. You can also use cheats to make your play experience easier. Activate them by going to file and then options.

### Placing Paths and Queues

Paths and queues are everything in this game because they direct your guests as to where they can and cannot go. Queues are just as important because they show guests where to line up at the entrance to a ride. Paths and queues are 1 tile each and must be placed with no breaks in order for guests to recognize them as a continuous path or queue. If a path leads to the entrance of a ride, a guests would not view it as a place they can stand and wait for their turn on the ride, and therefore would begin complaining about not being able to get on the ride. By placing queues, guests will know where to begin waiting and will begin to forma  line. You also do not need to use queues for the exits of your rides because guests will simply exit the ride and walk off to do whatever else they want to do.
Guests will never walk off the path or queue into empty space. However, if a path or queue is deleted while they are on It, then they will be placed on empty space and wander around aimlessly. Additionally, if you delete a path or queue and guests are on the other side of it that isn’t connected to the entrance of the park, they are now more or less stranded on an island and cannot leave the park. If either of these scenarios happen, guests will begin to complain and lower the rating of your park. To avoid this, you can press control + “p” while on a path and it will trace all the way back to the entrance of your park to insure guests are able to leave. If there is a break in the path, the mod will tell you where that break occurs. If a guests does happen to get stranded off the path, you can press control + “h” from anywhere to teleport them back to the entrance of the park. This does not mean they will automatically leave the park once being placed there, there are many other factors that determine  when a guests wants to eat, go on a ride, leave the park, etc.

### Placing Sloped Paths and Queues

Sloped paths and queues are important because they allow your guests to reach higher elevations. You can change the slope of the path or queue you are placing by pressing “l”. Each slope tile elevates up or down one unit, so if you are at elevation 0 and a ride entrance is at elevation 5, you would need to place 6 tiles sloping up to properly reach the same elevation as the entrance. The reason you would need 6 and not 5 is because in order for a path to connect to an entrance or exit, the path or queue tyle directly outside of it needs to be flat and not a slope. When placing a sloped path, you must be at the same elevation as the current path for it to connect properly. If you place a slope, the mod will automatically move you up or down int he direction of that slope as you place it, so you don't have to keep manually adjusting your elevation. It’s also important to remember that paths and queues can be placed below or above other objects, but the game will let you know if you are unable to place a path or queue and tell you specifically why. For example, some pre-built rides have tricky entrances or exits for guests to access, so sometimes you may need to snake a path or queue above or below the ride itself for guests to get in or out.

### Building Custom Rides

After you choose a custom ride to build, you will be placed into build mode. While in this mode, some commands may not work as intended, but most of them still do. The main one that doesn’t is control + arrows. I may change this in the future, but for now that’s how it is.
The way building in RCT works is by customizing the features of the piece you would like to build and then placing it. The mod will tell you when the piece you have created is valid or not. The game itself quite literally will not let you place an invalid piece or a piece in an invalid location. Additionally, you do not need to be focused on the spot where you want the piece to be placed, the game will automatically place the piece you are working on at the end of the ride. The game and mod will only let you use the pieces the specific tier of ride lets you build. So for example, you can’t build a loop on a junior rollercoaster.
Here are the general steps to build a rollercoaster, but you’ll learn they don’t technically need to be built in this order:

1.	Navigate the build menu with control up and down and change the specifications of the category you’re on with control left and right. When you have made your desired piece, navigate to construct and activate it with control + enter.
2.	You will first want to place a station platform. This is where the carts sit for guests to board and disembark from the ride. Choosing the direction of your station will determine which way the ride will initially move.
3.	After your station platform is built, it’s a good idea to place your entrance and exit. Select this option from the menu and the mod will walk you through the rest.
4.	After those are placed, you are ready to build the actual ride. In my opinion, a lot of this should be self-explanatory if you’ve gotten this far, but I’ll try to update this documentation with feedback I get. Keep in mind that if you’re starting out, you will want to put chains on your starting hill so the carts can actually go up in order to let gravity take care of the rest. 
5.	If you ever need to delete a section of your ride, just press delete on the piece you want to get rid of. If the piece you are deleting isn’t on the end and will cause a break in the ride, you will need to go to the piece before the gap you just made and then press insert. Insert tells the build helper to readjust itself so it knows where to insert the next piece. After the gap is filled, it should configure itself back to the end of the ride. If it doesn’t for whatever reason, you should be able to go to the last piece in the ride and press insert to realign it. 
6.	Once you’re ride is complete, you can escape out of build mode and then set your ride to test mode. You will know the ride makes a complete circuit if you are able to enter test mode. If it doesn’t let you, it means you haven’t connected something properly. It’s fairly difficult to do this, so if this does happen to you, I would bet that it’s an issue with how you connected the final piece back to the station platform or leading away from the station platform. I plan to make this more clear in future updates.

