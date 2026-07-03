/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "AccessSounds.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <openrct2/Context.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/PlatformEnvironment.h>
#include <openrct2/audio/Audio.h>
#include <openrct2/audio/AudioContext.h>
#include <openrct2/audio/AudioMixer.h>
#include <openrct2/audio/AudioSource.h>
#include <openrct2/config/Config.h>
#include <openrct2/core/FileStream.h>
#include <openrct2/entity/EntityList.h>
#include <openrct2/entity/Guest.h>
#include <openrct2/entity/Peep.h>
#include <random>
#include <unordered_set>

namespace OpenRCT2::Ui::Accessibility
{
    using namespace OpenRCT2::Audio;

    static constexpr size_t kCount = 5;

    // Single-clip cue filenames, in AccessSound enum order.
    static constexpr const char* kFileNames[kCount] = {
        "Terraform.wav",        // land
        "Waterform.wav",        // water
        "PlaceObject.wav",      // place
        "WaterDeathFemale.wav", // drownFemale
        "WaterDeathMale.wav",   // drownMale
    };

    // Footstep cues, in StepSound enum order. Each category has up to three variations played at
    // random. A missing variation stays null and is skipped, so 1, 2 or 3 files all work.
    static constexpr size_t kStepVariations = 3;
    static constexpr const char* kStepFileNames[][kStepVariations] = {
        { "DirtStep1.wav", "DirtStep2.wav", "DirtStep3.wav" },     // dirt
        { "HardStep1.wav", "HardStep2.wav", "HardStep3.wav" },     // hard
        { "QueueStep1.wav", "QueueStep2.wav", "QueueStep3.wav" },  // queue
        { "WaterStep1.wav", "WaterStep2.wav", "WaterStep3.wav" },  // water
    };
    static constexpr size_t kStepCategories = std::size(kStepFileNames);

    static std::array<IAudioSource*, kCount> _sources{};
    static IAudioSource* _stepSources[kStepCategories][kStepVariations]{};
    static bool _loaded = false;

    // Loads a WAV into an audio source, or nullptr if missing/unreadable. Missing files are fine: the
    // corresponding cue is simply skipped, so the mod still works if a clip is absent.
    static IAudioSource* LoadSource(const std::filesystem::path& path, IAudioContext& audioContext)
    {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
            return nullptr;
        try
        {
            auto stream = std::make_unique<FileStream>(path, FileMode::open);
            return audioContext.CreateStreamFromWAV(std::move(stream));
        }
        catch (const std::exception&)
        {
            return nullptr;
        }
    }

    // Loads the sound files once, on first play.
    static void EnsureLoaded()
    {
        if (_loaded)
            return;
        _loaded = true;

        auto& env = GetContext()->GetPlatformEnvironment();
        // DirBase::openrct2 already resolves to the data root (where g2.dat, language/, object/ live);
        // do NOT append DirId::data, which would add a second "data" segment.
        const auto dir = std::filesystem::path(env.GetDirectoryPath(DirBase::openrct2)) / "sounds" / "access";
        auto& audioContext = GetContext()->GetAudioContext();

        for (size_t i = 0; i < kCount; i++)
            _sources[i] = LoadSource(dir / kFileNames[i], audioContext);

        for (size_t c = 0; c < kStepCategories; c++)
            for (size_t v = 0; v < kStepVariations; v++)
                _stepSources[c][v] = LoadSource(dir / kStepFileNames[c][v], audioContext);
    }

    static void PlaySource(IAudioSource* source, int32_t pct)
    {
        const int32_t volume = kMixerVolumeMax * pct / 100;
        CreateAudioChannel(source, MixerGroup::Sound, false, volume, 0.5f, 1.0, true);
    }

    void PlayAccessSound(AccessSound sound)
    {
        const int32_t pct = Config::Get().sound.accessibilityCueVolume;
        if (pct <= 0)
            return; // cues muted

        EnsureLoaded();
        const auto idx = static_cast<size_t>(sound);
        if (idx >= kCount || _sources[idx] == nullptr)
            return;

        PlaySource(_sources[idx], pct);
    }

    void PlayStepSound(StepSound category)
    {
        const int32_t pct = Config::Get().sound.accessibilityCueVolume;
        if (pct <= 0)
            return; // cues muted

        EnsureLoaded();
        const auto cat = static_cast<size_t>(category);
        if (cat >= kStepCategories)
            return;

        // Gather the variations that actually loaded and pick one at random. Uses a local RNG (not the
        // game's deterministic RNG) so it never affects a multiplayer simulation.
        static std::mt19937 rng{ std::random_device{}() };
        IAudioSource* choices[kStepVariations];
        size_t n = 0;
        for (size_t v = 0; v < kStepVariations; v++)
            if (_stepSources[cat][v] != nullptr)
                choices[n++] = _stepSources[cat][v];
        if (n == 0)
            return;

        PlaySource(choices[rng() % n], pct);
    }

    void PlayDrownSound()
    {
        static std::mt19937 rng{ std::random_device{}() };
        PlayAccessSound((rng() & 1u) ? AccessSound::drownMale : AccessSound::drownFemale);
    }

    void TickDrownWatch()
    {
        static std::unordered_set<uint32_t> _drowning;

        if (gLegacyScene != LegacyScene::playing)
        {
            _drowning.clear();
            return;
        }

        std::unordered_set<uint32_t> current;
        for (auto* guest : EntityList<Guest>())
        {
            if (guest->Action != PeepActionType::drowning)
                continue;
            const uint32_t id = guest->id.ToUnderlying();
            current.insert(id);
            if (_drowning.find(id) == _drowning.end())
                PlayDrownSound(); // this guest just began drowning
        }
        _drowning = std::move(current);
    }
} // namespace OpenRCT2::Ui::Accessibility
