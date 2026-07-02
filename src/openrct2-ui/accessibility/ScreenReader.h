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
#include <string_view>

struct CoordsXYZ;

namespace OpenRCT2::Audio
{
    enum class SoundId : uint8_t;
}

namespace OpenRCT2::Ui::Accessibility
{
    // Plays one of the accessibility mod's own sound cues (e.g. placement confirmation, error) at a
    // world location, scaled by the mod's cue-volume setting (Config sound.accessibilityCueVolume).
    // No-op when the cue volume is 0. Use this instead of Audio::Play3D for the mod's feedback sounds
    // so they all honour the single volume control in the accessibility settings window.
    void PlayCue(Audio::SoundId soundId, const CoordsXYZ& loc);

    // Loads the screen reader bridge (nvdaControllerClient64.dll on Windows).
    // Safe to call multiple times; only the first call has an effect.
    void ScreenReaderInit();

    // Frees the screen reader library.
    void ScreenReaderShutdown();

    // Returns true if a supported screen reader (NVDA) is loaded and currently running.
    bool ScreenReaderIsAvailable();

    // Speaks UTF-8 text through the screen reader. When interrupt is true, any
    // in-progress speech is cancelled first. No-op when no screen reader is available.
    void ScreenReaderSpeak(std::string_view utf8Text, bool interrupt = true);

    // Speaks a menu/list item followed by its position, e.g. "Rides, 2 of 5". index is
    // zero-based; the position suffix is omitted when count is not positive.
    void ScreenReaderSpeakItem(std::string_view text, int32_t index, int32_t count);

    // Records an in-game announcement (e.g. a news message or error) so the player can
    // review it later. Does not speak it.
    void LogAnnouncement(std::string_view text);

    // Speaks a previously logged announcement, stepping older (direction -1) or newer
    // (direction +1) through the history.
    void CycleAnnouncementHistory(int direction);
} // namespace OpenRCT2::Ui::Accessibility
