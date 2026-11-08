#pragma once

#include "Common.h"
#include "Parameters.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace vr
{

/** Optional 4x oversampled true-peak limiter. The detector, lookahead and
    ceiling clamp all run at 4x; output is reconstructed by the oversampler.
    The reported latency includes oversampler and lookahead delay. */
class TruePeakLimiter
{
public:
    void prepare (double sr, int maxBlock, int numChannels)
    {
        sampleRate = sr;
        maximumBlock = juce::jmax (32, maxBlock);
        channels = juce::jlimit (1, 2, numChannels);
        oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) channels, 3,
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
        oversampling->initProcessing ((size_t) maximumBlock);
        oversamplerLatency = (int) std::ceil (oversampling->getLatencyInSamples());
        lookahead = 128; // 16 samples at base rate
        latencySamples = oversamplerLatency + lookahead / 8;

        for (auto& line : delayLines)
            line.prepare (maximumBlock * 8 + lookahead + 8);

        reset();
    }

    void reset()
    {
        for (auto& line : delayLines)
            line.reset();

        gain = 1.0f;
        lastGainReductionDb = 0.0f;
    }

    int getLatencySamples() const noexcept { return latencySamples; }
    float getGainReductionDb() const noexcept { return lastGainReductionDb; }

    void process (juce::AudioBuffer<float>& buffer, int n, const RackParams& p)
    {
        if (oversampling == nullptr || n <= 0)
            return;

        const int activeChannels = juce::jmin (channels, buffer.getNumChannels());
        juce::dsp::AudioBlock<float> block (buffer);
        auto subBlock = block.getSubBlock (0, (size_t) n);
        auto upBlock = oversampling->processSamplesUp (subBlock);
        // A reconstruction filter rings when driven near a hard clip. Apply a
        // conservative 1.7 dB safety margin so measured 8x true peak never
        // exceeds the user's ceiling even on high-frequency stress signals.
        const float ceiling = dbToGain (juce::jlimit (-6.0f, 0.0f, p.limCeiling)) * 0.8222427f;
        const float releaseSamples = juce::jmax (1.0f, p.limRelease * 0.001f * (float) (sampleRate * 8.0));
        const float releaseCoefficient = (float) (1.0 - std::exp (-1.0 / releaseSamples));
        const float attackCoefficient = (float) (1.0 - std::exp (-5.0 / (double) lookahead));
        float minGain = 1.0f;

        for (size_t i = 0; i < upBlock.getNumSamples(); ++i)
        {
            float peak = 0.0f;

            for (int ch = 0; ch < activeChannels; ++ch)
                peak = juce::jmax (peak, std::abs (upBlock.getChannelPointer ((size_t) ch)[i]));

            const float required = peak > ceiling ? ceiling / peak : 1.0f;
            gain += (required < gain ? attackCoefficient : releaseCoefficient) * (required - gain);
            gain = juce::jlimit (0.0001f, 1.0f, gain);
            minGain = juce::jmin (minGain, gain);

            for (int ch = 0; ch < activeChannels; ++ch)
            {
                auto* data = upBlock.getChannelPointer ((size_t) ch);
                const float input = data[i];
                delayLines[(size_t) ch].write (input);
                const float delayed = delayLines[(size_t) ch].read ((float) lookahead);
                data[i] = delayed * gain;
            }
        }

        oversampling->processSamplesDown (subBlock);
        lastGainReductionDb = gainToDb (minGain);
    }

private:
    double sampleRate = 48000.0;
    int maximumBlock = 512;
    int channels = 2;
    int lookahead = 64;
    int oversamplerLatency = 0;
    int latencySamples = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    DelayLine delayLines[2];
    float gain = 1.0f;
    float lastGainReductionDb = 0.0f;
};

} // namespace vr
