# Accessibility mod custom sounds

Drop the custom sound files in this folder using the exact filenames below. They are loaded
at runtime from the game's data directory (`<data>/sounds/access/`) and played through the
mixer, scaled by the "Accessibility Sounds Volume" setting (Ctrl+F1).

Preferred format: WAV, 16-bit PCM, 44.1/48 kHz. OGG and FLAC also work. Keep clips short.

## Single-clip cues

| File                   | Plays when...                                    |
|------------------------|--------------------------------------------------|
| `Terraform.wav`        | You raise or lower land                          |
| `Waterform.wav`        | You raise or lower water                         |
| `PlaceObject.wav`      | You place any object (ride, scenery, path, etc.) |
| `WaterDeathFemale.wav` | A guest drowns (see gender note below)           |
| `WaterDeathMale.wav`   | A guest drowns (see gender note below)           |

## Footstep cues (random variation)

Each footstep category plays one of up to three variations chosen at random, so repeated
steps vary. Provide `1`, `2` and/or `3` — any that are missing are simply skipped.

| Files                                              | Plays when the map cursor moves onto...    |
|----------------------------------------------------|--------------------------------------------|
| `DirtStep1.wav`, `DirtStep2.wav`, `DirtStep3.wav`  | a dirt/soil footpath tile                  |
| `HardStep1.wav`, `HardStep2.wav`, `HardStep3.wav`  | a hard footpath tile (tarmac, stone, etc.) |
| `QueueStep1.wav`, `QueueStep2.wav`, `QueueStep3.wav` | a queue line tile                        |
| `WaterStep1.wav`, `WaterStep2.wav`, `WaterStep3.wav` | a water tile                             |

A footpath is treated as "dirt" when its surface object identifier contains `dirt`
(e.g. `rct2.footpath_surface.dirt`); every other non-queue path is a "hard" path.

Note: OpenRCT2 does not track guest gender, so male vs. female drowning is decided by a
chosen rule (see the MapNavigation implementation), not by an actual gender.
