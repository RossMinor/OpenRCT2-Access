/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "ElevationTone.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <openrct2/Context.h>
#include <openrct2/audio/Audio.h>
#include <openrct2/audio/AudioContext.h>
#include <openrct2/audio/AudioMixer.h>
#include <openrct2/audio/AudioSource.h>
#include <openrct2/config/Config.h>
#include <openrct2/core/MemoryStream.h>
#include <vector>

namespace OpenRCT2::Ui::Accessibility
{
    // Pitch range mapped across elevation. Capped at 1 kHz so the highest terrain never gets
    // piercing; kElevToneRange is how many elevation HALF STEPS span the full min..max sweep.
    // Half steps and this range were doubled together, so the pitch of any given whole step is
    // exactly what it always was - the sweep simply gained a note between each pair of steps, which
    // is what makes an off-grid height audible as well as speakable.
    static constexpr double kElevToneMinFreq = 220.0;
    static constexpr double kElevToneMaxFreq = 1000.0;
    static constexpr int32_t kElevToneRange = 100;

    // One synthesised sine source per (clamped) elevation step, generated lazily and cached for
    // the session. Each is rendered at its exact target frequency and played at rate 1.0, which
    // bypasses the mixer's resampler - a crude linear interpolator that adds a faint buzz to any
    // pitch-shifted tone. Index = clamped elevation in [0, kElevToneRange].
    static Audio::IAudioSource* _elevationToneSources[kElevToneRange + 1] = {};

    // Builds a mono 16-bit PCM WAV in memory holding a sine wave at the given frequency, with
    // short fade in/out so the beep starts and ends without an audible click. Returns the raw
    // bytes of a complete .wav file, ready to hand to CreateStreamFromWAV.
    static std::vector<uint8_t> BuildSineWav(double freq, int32_t sampleRate, double seconds, double amplitude)
    {
        const int32_t numSamples = static_cast<int32_t>(sampleRate * seconds);
        const uint16_t numChannels = 1;
        const uint16_t bitsPerSample = 16;
        const uint16_t blockAlign = numChannels * (bitsPerSample / 8);
        const uint32_t byteRate = static_cast<uint32_t>(sampleRate) * blockAlign;
        const uint32_t dataSize = static_cast<uint32_t>(numSamples) * blockAlign;

        std::vector<uint8_t> buf;
        buf.reserve(44 + dataSize);
        const auto put16 = [&](uint16_t v) {
            buf.push_back(v & 0xFF);
            buf.push_back((v >> 8) & 0xFF);
        };
        const auto put32 = [&](uint32_t v) {
            buf.push_back(v & 0xFF);
            buf.push_back((v >> 8) & 0xFF);
            buf.push_back((v >> 16) & 0xFF);
            buf.push_back((v >> 24) & 0xFF);
        };
        const auto putTag = [&](const char* s) {
            for (int32_t i = 0; i < 4; i++)
                buf.push_back(static_cast<uint8_t>(s[i]));
        };

        putTag("RIFF");
        put32(36 + dataSize);
        putTag("WAVE");
        putTag("fmt ");
        put32(16);  // PCM fmt chunk size
        put16(1);   // audio format = PCM
        put16(numChannels);
        put32(static_cast<uint32_t>(sampleRate));
        put32(byteRate);
        put16(blockAlign);
        put16(bitsPerSample);
        putTag("data");
        put32(dataSize);

        constexpr double kPi = 3.14159265358979323846;
        const double step = 2.0 * kPi * freq / sampleRate;
        const int32_t attack = std::max(1, sampleRate / 100); // ~10 ms fade-in
        const int32_t release = std::max(1, sampleRate / 50);  // ~20 ms fade-out
        for (int32_t i = 0; i < numSamples; i++)
        {
            double env = 1.0;
            if (i < attack)
                env = static_cast<double>(i) / attack;
            else if (i >= numSamples - release)
                env = static_cast<double>(numSamples - i) / release;
            const double sample = std::sin(step * i) * amplitude * env;
            put16(static_cast<uint16_t>(static_cast<int16_t>(std::lround(sample * 32767.0))));
        }
        return buf;
    }

    void PlayElevationTone(int32_t halfSteps)
    {
        if (!Audio::IsAvailable())
            return;

        // The elevation tone is an accessibility cue, so it follows the mod's cue-volume setting like
        // the other cues: muted at 0, and scaled by the percentage otherwise.
        const int32_t pct = Config::Get().sound.accessibilityCueVolume;
        if (pct <= 0)
            return;

        const int32_t clamped = std::clamp(halfSteps, 0, kElevToneRange);
        Audio::IAudioSource*& source = _elevationToneSources[clamped];
        if (source == nullptr)
        {
            const double frac = static_cast<double>(clamped) / kElevToneRange;
            // Sweep pitch geometrically (log-frequency) so each elevation step sounds evenly spaced.
            const double freq = kElevToneMinFreq * std::pow(kElevToneMaxFreq / kElevToneMinFreq, frac);
            auto wav = BuildSineWav(freq, 44100, 0.12, 0.35);
            auto stream = std::make_unique<MemoryStream>(wav);
            source = GetContext()->GetAudioContext().CreateStreamFromWAV(std::move(stream));
        }
        if (source == nullptr)
            return;

        // MixerGroup::Accessibility: master volume only, decoupled from the game's sound slider. The
        // cue-volume percentage scales the tone here so the mod's slider controls it.
        const int32_t volume = Audio::kMixerVolumeMax * pct / 100;
        Audio::CreateAudioChannel(source, Audio::MixerGroup::Accessibility, false, volume, 0.5f, 1.0, true);
    }
} // namespace OpenRCT2::Ui::Accessibility
