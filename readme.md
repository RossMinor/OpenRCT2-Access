# OpenRCT2-Access

**A blind-accessibility mod for [OpenRCT2](https://github.com/OpenRCT2/OpenRCT2).**

OpenRCT2-Access is a fork of OpenRCT2 that adds screen-reader support and
full keyboard control, so blind and low-vision players can play
RollerCoaster Tycoon 2 without a mouse or sight. It speaks the interface
through the **NVDA** screen reader and lets you navigate the game's menus,
windows, and map entirely from the keyboard.

> This repository only covers the **accessibility additions**. For
> everything about the underlying game — what OpenRCT2 is, how to build it
> from source, contributing to the base game, etc. — see the upstream
> project: **https://github.com/OpenRCT2/OpenRCT2** and its
> [website](https://openrct2.io) and [wiki](https://github.com/OpenRCT2/OpenRCT2/wiki).

---

## Requirements

- **Windows** with the **[NVDA screen reader](https://www.nvaccess.org/download/)** running.
- The original **RollerCoaster Tycoon 2** game files (from
  [Steam](https://store.steampowered.com/app/285330/) or
  [GOG](https://www.gog.com/game/rollercoaster_tycoon_2)). As with regular
  OpenRCT2, you must own RCT2 — its assets are **not** included here.

---

## Installing and updating

1. Make sure NVDA is installed and running.
2. Download the latest [release](https://github.com/RossMinor/OpenRCT2-Access/releases)
   and unzip it somewhere.
3. Launch it and point it at your RollerCoaster Tycoon 2 files when asked
   (same as standard OpenRCT2).

**Automatic updates:** at launch the mod checks for a newer release. If one
is found, it announces it and you can press **F5** to download and install
the update in place. Your settings and saved games are preserved.

---

## How it works: modes and getting help

The mod has two input modes, toggled with **Ctrl + Space**:

- **Keyboard mode (default)** — the arrow keys drive a *tile cursor* around
  the park, the camera follows it, and each tile is spoken as you land on
  it. This is the main way blind players explore and build.
- **Mouse mode** — the arrow-key cursor is turned off and the mouse controls
  the game normally. Moving the mouse still reads whatever tile it hovers
  over, which is useful for sighted helpers.

At any time, press **F1** to hear **context-sensitive help** — a short spoken
summary of the controls available for whatever you are currently doing (map
cursor, a menu, ride construction, placing scenery, and so on). If you forget
a key, F1 is the fastest reminder.

Every menu and list also speaks an **"item X of Y"** position so you always
know where you are in a list.

---

## Command reference

Keys are grouped by task below. Unless noted, these work on the **map cursor**
during normal play. Modifier keys (Shift, Ctrl) are written with `+`.

### Moving the map cursor

| Key | What it does | When to use |
| --- | --- | --- |
| Arrow keys | Move the tile cursor one tile; the view follows and the new tile is spoken | Exploring and positioning for any build/tool action |
| Shift + Left / Shift + Right | Rotate the camera 90° and announce the new facing | To view the park from another angle (arrow directions rotate with it) |
| E | Jump the cursor to the park entrance | Quickly return to a known reference point |
| K | Cycle through your area markers | Revisit spots you flagged (see *Markers* below) |
| Shift + K | Snap the cursor to the next area marker | Jump straight to a flagged spot |
| Ctrl + Space | Toggle keyboard-cursor mode / free-mouse mode | Switch control schemes |

### Reading information

| Key | What it does | When to use |
| --- | --- | --- |
| C | Read the cursor's tile description, coordinates, and elevation | Find out exactly what is on the current tile |
| T | Open the status readout: date, park rating, guests, cash, recent messages (arrow to cycle, Enter opens messages, Escape/T closes) | A quick overview of how the park is doing |
| M | Read your current cash | Check funds before spending |
| F | Report the direction the camera is facing | Orient yourself after rotating |
| F1 | Speak context-sensitive help for the current situation | When you're unsure what keys do right now |
| `[` / `]` | Step back / forward through recent spoken announcements | Re-hear something you missed |

### Building paths and terraforming

| Key | What it does | When to use |
| --- | --- | --- |
| Space | Build a footpath on the current tile (auto-slopes to follow terrain) | Lay walkways tile by tile |
| D | Remove the path on the current tile | Undo or reroute a path |
| L | Cycle the path **slope** mode (flat / ramp) | Control how paths climb terrain |
| `,` / `.` (comma / period) | Lower / raise the footpath **build height** | Build bridges over water or gaps |
| Page Up / Page Down | Raise / lower **land** over the brush area (one step) | Shape terrain |
| Ctrl + Page Up / Ctrl + Page Down | Raise / lower **water** over the brush area | Add or drain water |
| X | Clear scenery (trees, bushes, etc.) over the brush area | Clear ground before building |
| B | Cycle the terraform/clear **brush size** (1×1, 3×3, 5×5, 7×7) | Work faster over larger areas |
| O | Buy **land** ownership over the brush area | Expand your buildable park |
| Shift + O | Buy **construction rights** over the brush area | Build above land you don't own outright |

> **Choosing a path type or queue:** open the Footpath window from the
> toolbar (Tab). There, Up/Down pick the option (path type, queue type,
> railings, build mode) and Left/Right change it. Close it with Escape, then
> build with the map cursor and Space.

### Placing rides, stalls, and scenery

Rides, stalls, and scenery are chosen from their build windows (opened from
the toolbar). Once you start placing, the map cursor positions the object:

| Key | What it does |
| --- | --- |
| Arrow keys | Move the object's position |
| Enter | Place the object at the cursor |
| R | Rotate the object |
| Escape | Cancel / finish placing |
| Shift + W / A / S / D | (Scenery) pick the tile **edge** — top / left / bottom / right — for walls and banners |
| Shift + Q / E / Z / C | (Scenery) pick the tile **corner** — top-left / top-right / bottom-left / bottom-right — for small scenery |

### Opening a ride, stall, or gate

| Key | What it does | When to use |
| --- | --- | --- |
| Enter | Open the information window for the ride, stall, or gate under the cursor | Inspect or manage an existing ride |
| Ctrl + Enter | Open that ride/stall in **construction mode** | Continue building or modifying it |

### Ride construction (while the build window is open)

Plain arrow keys still move the map cursor, so hold **Ctrl** to drive the
build menu:

| Key | What it does |
| --- | --- |
| Ctrl + Up / Ctrl + Down | Choose a build option |
| Ctrl + Left / Ctrl + Right | Change the selected option's value |
| Ctrl + Enter | Build the current piece at the cursor |
| Ctrl + B | Read the current build state |
| Escape | Exit construction (with confirmation) |

### Land / Water / Land-rights / Clear-scenery tool windows

Same scheme as construction — the cursor still positions the tool, Ctrl
drives the window:

| Key | What it does |
| --- | --- |
| Ctrl + Up / Ctrl + Down | Choose a tool option |
| Ctrl + Left / Ctrl + Right | Change it |
| Ctrl + B | Read the current option |
| Escape | Close the window |

### Waypoints (10 slots)

| Key | What it does | When to use |
| --- | --- | --- |
| Shift + 1…9, 0 | Set / move a waypoint at the cursor in that slot (0 is the tenth) | Bookmark an important location |
| Ctrl + 1…9, 0 | Jump the cursor to that waypoint | Return there instantly |

### Markers

| Key | What it does |
| --- | --- |
| K | Cycle through area markers |
| Shift + K | Snap the cursor to the next marker |
| Shift + B | Report track breaks (for coaster building) |

### Opening game windows quickly

Hold **Shift** with a letter to jump straight to a window from the map:

| Key | Opens |
| --- | --- |
| Shift + F | Finances |
| Shift + R | Rides list |
| Shift + P | Park information |
| Shift + G | Guest list |
| Shift + S | Staff |
| Shift + D | Research |
| Shift + M | Recent messages |
| Shift + F1 | Land tool |
| Tab | The top toolbar menu (Up/Down to move, Enter to open, Escape to leave) |

### Menus and windows (general navigation)

| Key | What it does |
| --- | --- |
| Arrow keys | Move between items; Left/Right also change values or switch tabs |
| A letter | Jump to the next item starting with that letter (in menus/lists) |
| Enter | Activate the focused item |
| Escape | Go up one level / close the current menu or window |

Combo boxes and colour pickers behave like **sliders**: Left/Right change the
value in place and speak the new value (colours are read by name), so you
never have to open a separate dropdown grid.

### Rescuing lost guests

| Key | What it does | When to use |
| --- | --- | --- |
| Ctrl + H | Find guests stranded on footpaths cut off from any park exit (or not on a path at all) and teleport them to the park entrance, announcing how many were rescued | When guests are complaining they're lost or can't find the exit |

This is multiplayer-safe. Available in any mode.

### Accessibility settings

| Key | What it does |
| --- | --- |
| Ctrl + F1 | Open the mod's own settings window |

The settings window (navigate with Up/Down, change with Left/Right, activate
with Enter) offers:

- **Accessibility sounds volume** — adjust the volume of the mod's own sound
  cues in 5% steps.
- **Step sounds** — footstep cues on every step / only on a change of tile
  type / off.
- **Tile reading** — speak the tile under the cursor on every tile / only
  when it changes / off.
- **Support Ross** — opens the Patreon page (press Enter once to hear the
  message, again to open the link in your browser).

---

## Sound cues

Beyond speech, the mod plays short sound cues (all scaled by the
Accessibility sounds volume, and 3D-positioned where relevant):

- **Elevation tone** — a beep whose pitch rises with terrain height, played
  when the cursor moves to a tile at a different elevation.
- **Footsteps** — distinct sounds for **dirt** paths, **hard** paths
  (tarmac/stone), **queues**, and **water** tiles, each with random
  variations. Frequency is controlled by the *Step sounds* setting.
- **Terraform** — separate cues for raising/lowering land and water.
- **Place object** — when a ride, path, or scenery is placed.
- **Guest drowning** — an alert when a guest starts to drown.
- **Park boundary** — spoken when the cursor crosses into or out of the park.

---

## What's accessible so far

- **Speech via NVDA** for menus, windows, lists, and in-game text (news
  messages and error dialogs), with a reviewable history.
- **Keyboard navigation** everywhere, presented as simple linear lists.
- **The map cursor** with spoken tile descriptions (rides, paths and their
  surface type, scenery, water, park and ride entrances/exits, borders).
- **Building:** footpaths and queues, terraforming land and water, buying
  land and construction rights, clearing scenery, placing rides/stalls and
  scenery, and ride/maze **construction**.
- **Windows:** Scenario select; Ride, Staff, and Guest lists; individual
  Ride, Staff, and Guest windows; Park information; Finances; Research;
  Recent messages/News; Marketing campaigns; the Construct-a-new-ride and
  ride Track-design lists; Options; Cheats; Shortcut keys; Sign and Banner
  editors; and the confirmation prompts (demolish, refurbish, fire staff,
  save).
- **File handling:** an accessible Load / Save browser (spoken names and
  dates, typed-filename echo, save confirmation), the quit/save prompt, and
  text-entry dialogs.
- **Multiplayer** server-list browsing and joining.

---

## Known limitations and not-yet-accessible areas

- **Windows + NVDA only.** No macOS/Linux support, and other screen readers
  (JAWS, Narrator, VoiceOver) are not supported.
- **Ride graphs** (the Graphs tab) are visual charts and are not adapted for
  screen readers.
- **The scenario / map editor and track designer** (building custom
  scenarios, the object-selection window, map generator) are not adapted.
- **The Tile Inspector** and other advanced/debug tools are not adapted.
- **The minimap / map window** is a visual overview and is not spoken.
- **Colour selection** groups similar colours together but reads them by
  name only — the exact RGB/hex value is not announced.
- **Multiplayer chat and the player list** are not adapted, though joining
  and playing on a server work.
- The mod is designed to interoperate with **standard OpenRCT2 0.5.2**
  clients in multiplayer, but a non-modded host is required to have the same
  base version.

## Not fully tested yet

These work in normal use but haven't been exhaustively verified — feedback is
welcome:

- **Multiplayer** beyond joining and the lost-guest rescue: extended
  co-op sessions and edge cases.
- The **dirt vs. hard path** footstep classification on non-RCT2 path
  objects (RCT1 imports, custom/downloaded path sets).
- **Complex tracked-coaster construction** via the keyboard build menu for
  very large or unusual layouts.
- Behaviour in **scenarios with unusual object sets** or heavy customisation.

If something is unclear, missing, or misbehaving, please open an issue on the
[repository](https://github.com/RossMinor/OpenRCT2-Access/issues).

---

## Licence

OpenRCT2-Access is a fork of OpenRCT2 and, like it, is licensed under the
**GNU General Public License version 3** (or, at your option, any later
version). See [`licence.txt`](licence.txt). All credit for the underlying
game engine goes to the [OpenRCT2 developers](https://github.com/OpenRCT2/OpenRCT2/blob/develop/contributors.md);
this project only adds the accessibility layer on top.
