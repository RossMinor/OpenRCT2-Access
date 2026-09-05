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
#include <initializer_list>
#include <string>
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

    // Loads the speech bridge. Speech goes through Prism (prism.dll), which routes it to whichever
    // screen reader is actually running - NVDA, JAWS, Narrator, System Access and others on Windows,
    // VoiceOver on macOS, Speech Dispatcher or Orca on Linux - so nothing above this layer needs to
    // know which reader the player uses. If prism.dll cannot be loaded the mod falls back to driving
    // NVDA directly, and if that fails too the game runs silently rather than refusing to start.
    // Safe to call multiple times; only the first call has an effect.
    void ScreenReaderInit();

    // Frees the speech bridge.
    void ScreenReaderShutdown();

    // Returns true if a screen reader is loaded and currently running. Re-checks on each call, so it
    // becomes true when the player starts a reader after the game.
    bool ScreenReaderIsAvailable();

    // The name of the screen reader currently being spoken through ("NVDA", "JAWS", ...), or an empty
    // string when none is running. The pointer is owned by the speech bridge; copy it if you need to
    // keep it. Useful for reporting to the player which reader the mod found.
    const char* ScreenReaderBackendName();

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

    // Assembles a spoken phrase from fragments, owning the punctuation so no call site has to. Each
    // add() appends a fragment, inserting the separator (", " by default) only between non-empty
    // fragments - empty fragments are dropped. This is the single place responsible for the join, so
    // two call sites can never disagree and leave the player hearing a run-together or double-comma
    // seam. Build incrementally, then read str().
    //
    //   SpeechBuilder sb;
    //   sb.add("Ride entrance").add(facingText).add(connectionText);
    //   ScreenReaderSpeak(sb.str());
    class SpeechBuilder
    {
    public:
        SpeechBuilder() = default;
        explicit SpeechBuilder(std::string_view separator)
            : _separator(separator)
        {
        }

        SpeechBuilder& add(std::string_view fragment)
        {
            if (!fragment.empty())
            {
                if (!_text.empty())
                    _text += _separator;
                _text += fragment;
            }
            return *this;
        }

        bool empty() const
        {
            return _text.empty();
        }

        const std::string& str() const
        {
            return _text;
        }

    private:
        std::string _separator = ", ";
        std::string _text;
    };

    // One-shot convenience: join fragments with ", " (empties dropped). For incremental/conditional
    // assembly use SpeechBuilder directly.
    std::string JoinSpeech(std::initializer_list<std::string_view> fragments);
} // namespace OpenRCT2::Ui::Accessibility
