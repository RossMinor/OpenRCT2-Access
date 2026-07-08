# OpenRCT2-Access changelog

Accessibility-mod release notes. Add your changes under **Unreleased** as you make them; at
release time that section becomes the version's notes and a new empty Unreleased is started.

(This is separate from `distribution/changelog.txt`, which is the upstream OpenRCT2 game changelog.)

## Unreleased

- Fixed keyboard navigation routing to the wrong window when more than one was open (for example,
  pressing Enter to set the park admission price could adjust the loan in the Finances window
  instead). Keys now always go to the window you raised most recently.
- Footstep and tile-reading cues on the map: added distinct dirt vs. hard (tarmac/stone) path
  sounds, queue sounds, and three random variations per footstep type.
- Added a visible keyboard-focus indicator: it outlines the whole ride when the map cursor is over
  one, and its colour can be changed in the Ctrl+F1 settings menu.
- Internal cleanups for consistency and maintainability (unified list navigation, tidier speech
  assembly, split up oversized source files) — no change to how the mod is used.

## Earlier releases

Release notes for v0.91.1 and earlier are on the GitHub Releases page:
https://github.com/RossMinor/OpenRCT2-Access/releases
