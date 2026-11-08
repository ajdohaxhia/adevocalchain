#pragma once

#include "Common.h"
#include "Parameters.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace vr
{

/** Two-band vocal dynamic EQ. Static peaking gain plus a stereo-linked
    band-pass detector and smooth dynamic cut. All storage is prepared up
    front; process() allocates nothing. */
class DynamicEQ
{
public:
    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        maximumBlock = juce::jmax (32, maxBlock);
        reset();
    }

    void reset()
    {
        for (auto& band : bands)
        {
            for (auto& filter : band.peak)
                filter.reset();
            for (auto& filter : band.bandpass)
                filter.reset();
            band.follower.reset();
            band.gainReductionDb = 0.0f;
        }
    }

    void process (juce::AudioBuffer<float>& buffer, int n, const RackParams& p)
    {
        if (n <= 0)
            return;

        const int channels = juce::jmin (2, buffer.getNumChannels());
        float minimumReduction = 0.0f;

        for (int b = 0; b < 2; ++b)
        {
            auto& band = bands[(size_t) b];
            const bool enabled = b == 0 ? p.deq1On : p.deq2On;
            const float frequency = b == 0 ? p.deq1Freq : p.deq2Freq;
            const float q = b == 0 ? p.deq1Q : p.deq2Q;
            const float gainDb = b == 0 ? p.deq1Gain : p.deq2Gain;
            const float threshold = b == 0 ? p.deq1Threshold : p.deq2Threshold;
            const float ratio = b == 0 ? p.deq1Ratio : p.deq2Ratio;
            const float range = b == 0 ? p.deq1Range : p.deq2Range;
            const float attack = b == 0 ? p.deq1Attack : p.deq2Attack;
            const float release = b == 0 ? p.deq1Release : p.deq2Release;

            band.follower.setTimes (attack, release);

            for (int ch = 0; ch < channels; ++ch)
            {
                band.peak[ch].setPeak (sampleRate, frequency, gainDb, q);
                band.bandpass[ch].setBandPass (sampleRate, frequency, q);
            }

            float localReduction = 0.0f;

            for (int i = 0; i < n; ++i)
            {
                float detector = 0.0f;

                for (int ch = 0; ch < channels; ++ch)
                {
                    const float input = buffer.getReadPointer (ch)[i];
                    const float staticGain = band.peak[ch].process (input);
                    const float bandSignal = band.bandpass[ch].process (input);
                    band.bandSignal[ch] = bandSignal;
                    band.staticSignal[ch] = staticGain;
                    detector = juce::jmax (detector, std::abs (bandSignal));
                }

                const float envelope = band.follower.process (detector);
                const float levelDb = gainToDb (envelope);
                float reduction = 0.0f;

                if (enabled && levelDb > threshold)
                    reduction = juce::jmin (range, (levelDb - threshold) * (1.0f - 1.0f / juce::jmax (1.0f, ratio)));

                localReduction = juce::jmin (localReduction, -reduction);
                const float dynamicGain = enabled ? dbToGain (-reduction) : 1.0f;

                for (int ch = 0; ch < channels; ++ch)
                {
                    const float input = buffer.getReadPointer (ch)[i];
                    const float output = enabled
                                             ? band.staticSignal[ch]
                                                   + band.bandSignal[ch] * (dynamicGain - 1.0f)
                                             : input;
                    buffer.getWritePointer (ch)[i] = output;
                }
            }

            band.gainReductionDb = localReduction;
            minimumReduction = juce::jmin (minimumReduction, localReduction);
        }

        lastGainReductionDb = minimumReduction;
    }

    float getGainReductionDb() const noexcept { return lastGainReductionDb; }
    float getBandGainReductionDb (int band) const noexcept
    {
        return bands[(size_t) juce::jlimit (0, 1, band)].gainReductionDb;
    }

private:
    struct Band
    {
        Biquad peak[2];
        Biquad bandpass[2];
        EnvelopeFollower follower;
        float bandSignal[2] {};
        float staticSignal[2] {};
        float gainReductionDb = 0.0f;
    };

    double sampleRate = 48000.0;
    int maximumBlock = 512;
    Band bands[2];
    float lastGainReductionDb = 0.0f;
};

} // namespace vr
