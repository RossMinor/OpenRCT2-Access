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

## What's accessible so far

- **Speech via NVDA** for menus, windows, lists, and in-game text (news
  messages and error dialogs), with a history you can review.
- **Keyboard navigation** everywhere, presented as simple linear lists:
  the title menu, the top toolbar and its dropdowns, and the File / Game
  Tools menus.
- **An in-game map cursor** you move tile by tile, with spoken tile
  descriptions (rides, paths and their surface type, scenery, water, park
  and ride entrances/exits, borders, and more).
- **Windows:** Scenario select, Ride / Staff / Guest lists, Park
  information, Finances, Research, Recent messages, and the
  Construct-a-new-ride window (with spoken ride details).
- **File handling:** an accessible Load / Save browser (with spoken file
  names and dates, typed-filename echo, and save confirmation), the
  quit/save prompt, and text-entry dialogs.
- **Multiplayer** server list browsing and joining.
- A spoken **"item X of Y"** position read-out throughout the menus.

This is a work in progress — more of the game is being made accessible
over time.

---

## Key bindings

**In the park (map cursor):**

| Key | Action |
| --- | --- |
| Arrow keys | Move the tile cursor (the view follows) |
| C | Read the cursor's coordinates and elevation |
| T | Read the in-game date (and whether the game is paused) |
| M | Read your current cash |
| E | Jump the cursor to the park entrance |
| F | Report the direction the camera is facing |
| Space | Build a flat footpath on the current tile |
| D | Remove the path on the current tile |
| `[` / `]` | Step back / forward through recent announcements |
| Tab | Enter the top toolbar menu |

**In menus and windows:**

| Key | Action |
| --- | --- |
| Arrow keys | Move between items, tabs, or options |
| Enter | Activate the focused item |
| Escape | Go up one level / close the current menu or window |

---

## Installing

1. Make sure NVDA is installed and running.
2. Download a release (or build from source) and unzip it somewhere.
3. Launch it and point it at your RollerCoaster Tycoon 2 files when asked
   (same as standard OpenRCT2).

---

## Licence

OpenRCT2-Access is a fork of OpenRCT2 and, like it, is licensed under the
**GNU General Public License version 3** (or, at your option, any later
version). See [`licence.txt`](licence.txt). All credit for the underlying
game engine goes to the [OpenRCT2 developers](https://github.com/OpenRCT2/OpenRCT2/blob/develop/contributors.md);
this project only adds the accessibility layer on top.
