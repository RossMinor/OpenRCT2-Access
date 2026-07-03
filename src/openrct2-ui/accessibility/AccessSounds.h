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

namespace OpenRCT2::Ui::Accessibility
{
    // When the map cursor's footstep-style cues (path, water) play. Stored in config as
    // sound.accessibilityStepSoundMode and chosen from the Ctrl+F1 settings window.
    enum class StepSoundMode : uint8_t
    {
        everyStep = 0,    // play on every cursor move onto a path/water tile
        onTransition = 1, // play only when the tile type changes (like the spoken tile announcements)
        off = 2,          // never play step cues
    };

    // When the screen reader announces the tile under the map cursor. Stored in config as
    // sound.accessibilityTileSpeechMode and chosen from the Ctrl+F1 settings window.
    enum class TileSpeechMode : uint8_t
    {
        everyTile = 0,    // announce the tile on every cursor move
        onTransition = 1, // announce only when the tile changes (the mod's original behaviour)
        off = 2,          // never announce the tile under the cursor
    };

    // Custom accessibility sound cues, loaded from <data>/sounds/access/ and played through the
    // mixer at the "Accessibility Sounds Volume" setting (Ctrl+F1). See that folder's README for the
    // file each entry maps to. These are single-clip cues; footstep cues live in StepSound below.
    enum class AccessSound
    {
        land,        // raising or lowering land
        water,       // raising or lowering water
        place,       // placing any object (ride, scenery, path)
        drownFemale, // a guest drowning (female clip)
        drownMale,   // a guest drowning (male clip)
    };

    // Footstep-style cues for the tile the map cursor moves onto. Each category has up to three
    // variations (named <Name>1.wav .. <Name>3.wav) played at random so repeated steps vary.
    enum class StepSound
    {
        dirt,  // a dirt/soil footpath
        hard,  // a hard-surfaced footpath (tarmac, stone, etc.)
        queue, // a queue line
        water, // an open water tile
    };

    // Plays the given custom sound, scaled by the accessibility cue-volume setting. Lazily loads the
    // sound files on first use. No-op if the file is missing or the cue volume is 0.
    void PlayAccessSound(AccessSound sound);

    // Plays a footstep cue for the given tile category, randomly choosing one of the loaded variations.
    // Scaled by the cue-volume setting; no-op if no variation is loaded or the cue volume is 0.
    void PlayStepSound(StepSound category);

    // Plays a drowning sound, randomly choosing the male or female clip. Uses a local RNG (not the
    // game's deterministic RNG), so it is safe to call without affecting a multiplayer simulation.
    void PlayDrownSound();

    // Per-frame watcher that plays a drowning sound the moment a guest begins to drown. Detected by
    // polling from the UI layer (the game's drowning code lives in the core library, which cannot call
    // up into this UI module). Call once per frame.
    void TickDrownWatch();
} // namespace OpenRCT2::Ui::Accessibility
