#pragma once

#include "Common.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace vr
{

/** Short-term K-weighted loudness meter (approximately LUFS-S, 3 s time
    constant). Not a gated integrated LUFS implementation. */
class LoudnessMeter
{
public:
    void prepare (double sr)
    {
        sampleRate = sr > 1000.0 ? sr : 48000.0;
        reset();
    }

    void reset()
    {
        for (auto& filter : shelf)
            filter.reset();
        for (auto& filter : highPass)
            filter.reset();
        meanSquare = 0.0;
        coefficient = 1.0 - std::exp (-1.0 / (3.0 * sampleRate));
        lastLufs = -70.0f;
    }

    void process (const juce::AudioBuffer<float>& buffer, int n)
    {
        if (n <= 0)
            return;

        const int channels = juce::jmin (2, buffer.getNumChannels());

        for (int ch = 0; ch < channels; ++ch)
        {
            shelf[(size_t) ch].setHighShelf (sampleRate, 1681.974, 3.9998, 0.707175);
            highPass[(size_t) ch].setHighPass (sampleRate, 38.135, 0.500327);
        }

        for (int i = 0; i < n; ++i)
        {
            double sample = 0.0;

            for (int ch = 0; ch < channels; ++ch)
            {
                const float weighted = highPass[(size_t) ch].process (
                    shelf[(size_t) ch].process (buffer.getReadPointer (ch)[i]));
                sample += (double) weighted * weighted;
            }

            sample /= (double) juce::jmax (1, channels);
            meanSquare += coefficient * (sample - meanSquare);
        }

        lastLufs = meanSquare > 1.0e-12
                       ? (float) (-0.691 + 10.0 * std::log10 (meanSquare))
                       : -70.0f;
    }

    float getLufs() const noexcept { return lastLufs; }

private:
    double sampleRate = 48000.0;
    Biquad shelf[2];
    Biquad highPass[2];
    double meanSquare = 0.0;
    double coefficient = 0.0;
    float lastLufs = -70.0f;
};

} // namespace vr
