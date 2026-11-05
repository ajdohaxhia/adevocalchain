#pragma once

#include "Common.h"
#include "../Parameters.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace vr
{

//==============================================================================
/** Gentle, stereo-linked downward expander with hold and soft transition. */
class Expander
{
public:
    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        follower.prepare (sr, 2.0f, 200.0f);
        gainDb = 0.0f;
        holdCounter = 0;
        holdSamples = 0;
        juce::ignoreUnused (maxBlock);
    }

    void reset() noexcept
    {
        follower.reset();
        gainDb = 0.0f;
        holdCounter = 0;
    }

    void process (juce::AudioBuffer<float>& buffer, int numSamples, const RackParams& p) noexcept
    {
        follower.setTimes (p.expAttack, p.expRelease);
        holdSamples = (int) (p.expHold * 0.001f * sampleRate);
        const float threshold = juce::jlimit (-80.0f, -20.0f, p.expThreshold);
        const float range = juce::jlimit (0.0f, 40.0f, p.expRange);
        const bool enabled = ! p.expBypass;

        const int channels = juce::jmin (2, buffer.getNumChannels());
        const float attackCoefficient = (float) (1.0 - std::exp (-1.0 / (0.002 * sampleRate)));
        const float releaseCoefficient = (float) (1.0 - std::exp (-1.0 / (0.080 * sampleRate)));

        for (int i = 0; i < numSamples; ++i)
        {
            float detector = 0.0f;

            for (int channel = 0; channel < channels; ++channel)
                detector = juce::jmax (detector, std::abs (buffer.getReadPointer (channel)[i]));

            const float envelope = follower.process (detector);
            const float levelDb = gainToDb (envelope);

            float targetDb = 0.0f;

            if (! enabled || levelDb >= threshold)
            {
                holdCounter = holdSamples;
            }
            else if (holdCounter > 0)
            {
                // Hold the current gain through short gaps so breaths and
                // word tails are not chopped.
                targetDb = gainDb;
                --holdCounter;
            }
            else
            {
                const float depth = juce::jlimit (0.0f, 1.0f, (threshold - levelDb) / 24.0f);
                const float shaped = depth * depth * (3.0f - 2.0f * depth);
                targetDb = -range * shaped;
            }

            const float coefficient = targetDb > gainDb ? attackCoefficient : releaseCoefficient;
            gainDb += coefficient * (targetDb - gainDb);
            const float gain = dbToGain (gainDb);

            for (int channel = 0; channel < channels; ++channel)
                buffer.getWritePointer (channel)[i] *= gain;
        }
    }

private:
    double sampleRate = 48000.0;
    EnvelopeFollower follower;
    float gainDb = 0.0f;
    int holdCounter = 0;
    int holdSamples = 0;
};

//==============================================================================
struct EqSection
{
    void prepare (double sr, int maxBlock)
    {
        dryBuffer.setSize (2, juce::jmax (32, maxBlock), false, true, false);
        enableGain.reset (sr, 0.008);
        enableGain.setCurrentAndTargetValue (1.0f);
    }

    void captureDry (const juce::AudioBuffer<float>& buffer, int n)
    {
        const int channels = juce::jmin (2, buffer.getNumChannels());

        for (int ch = 0; ch < channels; ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, n);
    }

    void blend (juce::AudioBuffer<float>& buffer, int n, bool bypass)
    {
        enableGain.setTargetValue (bypass ? 0.0f : 1.0f);
        const int channels = juce::jmin (2, buffer.getNumChannels());

        for (int i = 0; i < n; ++i)
        {
            const float mix = enableGain.getNextValue();

            for (int ch = 0; ch < channels; ++ch)
            {
                const float wet = buffer.getReadPointer (ch)[i];
                const float dry = dryBuffer.getReadPointer (ch)[i];
                buffer.getWritePointer (ch)[i] = dry + (wet - dry) * mix;
            }
        }
    }

    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> enableGain;
};

class CleanupEQ
{
public:
    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        section.prepare (sr, maxBlock);
        for (auto& f : filters)
            f.reset();
    }

    void reset()
    {
        for (auto& f : filters)
            f.reset();

        section.enableGain.setCurrentAndTargetValue (1.0f);
    }

    void process (juce::AudioBuffer<float>& buffer, int n, const RackParams& p)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            filters[ch * 3 + 0].setHighPass (sampleRate, p.ceqHpfFreq, 0.707);
            filters[ch * 3 + 1].setPeak (sampleRate, p.ceqCut1Freq, p.ceqCut1Gain, p.ceqCut1Q);
            filters[ch * 3 + 2].setPeak (sampleRate, p.ceqCut2Freq, p.ceqCut2Gain, p.ceqCut2Q);
        }

        const int channels = juce::jmin (2, buffer.getNumChannels());
        section.captureDry (buffer, n);

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int i = 0; i < n; ++i)
            {
                float x = data[i];
                x = filters[ch * 3 + 0].process (x);
                x = filters[ch * 3 + 1].process (x);
                x = filters[ch * 3 + 2].process (x);
                data[i] = x;
            }
        }

        section.blend (buffer, n, p.ceqBypass);
    }

private:
    double sampleRate = 48000.0;
    Biquad filters[6];
    EqSection section;
};

class ToneEQ
{
public:
    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        section.prepare (sr, maxBlock);
        for (auto& f : filters)
            f.reset();
    }

    void reset()
    {
        for (auto& f : filters)
            f.reset();

        section.enableGain.setCurrentAndTargetValue (1.0f);
    }

    void process (juce::AudioBuffer<float>& buffer, int n, const RackParams& p)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            filters[ch * 3 + 0].setLowShelf (sampleRate, p.teqLowFreq, p.teqLowGain);
            filters[ch * 3 + 1].setPeak (sampleRate, p.teqPresFreq, p.teqPresGain, p.teqPresQ);
            filters[ch * 3 + 2].setHighShelf (sampleRate, p.teqHighFreq, p.teqHighGain);
        }

        const int channels = juce::jmin (2, buffer.getNumChannels());
        section.captureDry (buffer, n);

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);

            for (int i = 0; i < n; ++i)
            {
                float x = data[i];
                x = filters[ch * 3 + 0].process (x);
                x = filters[ch * 3 + 1].process (x);
                x = filters[ch * 3 + 2].process (x);
                data[i] = x;
            }
        }

        section.blend (buffer, n, p.teqBypass);
    }

private:
    double sampleRate = 48000.0;
    Biquad filters[6];
    EqSection section;
};

//==============================================================================
class Compressor
{
public:
    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        detectorHpf.setHighPass (sr, 120.0, 0.707);
        follower.prepare (sr, 12.0f, 120.0f);
        dryBuffer.setSize (2, juce::jmax (32, maxBlock), false, true, false);
        mixSmoother.reset (sr, 0.01);
        mixSmoother.setCurrentAndTargetValue (1.0f);
        lastGainReductionDb = 0.0f;
    }

    void reset()
    {
        detectorHpf.reset();
        follower.reset();
        mixSmoother.setCurrentAndTargetValue (1.0f);
        lastGainReductionDb = 0.0f;
    }

    void process (juce::AudioBuffer<float>& buffer, int n, const RackParams& p)
    {
        const int channels = juce::jmin (2, buffer.getNumChannels());
        detectorHpf.setHighPass (sampleRate, p.compDetectorHpf, 0.707);
        follower.setTimes (p.compAttack, p.compRelease);
        mixSmoother.setTargetValue (p.compBypass ? 0.0f : juce::jlimit (0.0f, 1.0f, p.compMix * 0.01f));

        const float threshold = p.compThreshold;
        const float ratio = juce::jmax (1.0f, p.compRatio);
        const float knee = juce::jmax (0.0f, p.compKnee);
        const float makeupGain = dbToGain (p.compMakeup);
        float minGainDb = 0.0f;

        for (int ch = 0; ch < channels; ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, n);

        for (int i = 0; i < n; ++i)
        {
            float detector = 0.0f;

            for (int ch = 0; ch < channels; ++ch)
                detector += buffer.getReadPointer (ch)[i];

            detector /= (float) juce::jmax (1, channels);
            detector = detectorHpf.process (detector);

            const float envelope = follower.process (detector);
            const float levelDb = gainToDb (envelope);

            float reduction = 0.0f;

            if (levelDb > threshold)
            {
                const float over = levelDb - threshold;

                if (knee > 0.0f && over < knee * 0.5f)
                {
                    const float x = over + knee * 0.5f;
                    reduction = (1.0f - 1.0f / ratio) * x * x / (2.0f * knee);
                }
                else
                {
                    reduction = (1.0f - 1.0f / ratio) * over;
                }
            }

            const float gain = dbToGain (-reduction) * makeupGain;
            minGainDb = juce::jmin (minGainDb, -reduction);
            const float mix = mixSmoother.getNextValue();

            for (int ch = 0; ch < channels; ++ch)
            {
                const float dry = dryBuffer.getReadPointer (ch)[i];
                const float wet = buffer.getReadPointer (ch)[i] * gain;
                buffer.getWritePointer (ch)[i] = dry + (wet - dry) * mix;
            }
        }

        lastGainReductionDb = minGainDb;
    }

    float getGainReductionDb() const noexcept { return lastGainReductionDb; }

private:
    double sampleRate = 48000.0;
    Biquad detectorHpf;
    EnvelopeFollower follower;
    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixSmoother;
    float lastGainReductionDb = 0.0f;
};

//==============================================================================
class DeEsser
{
public:
    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        for (auto& f : filters)
            f.reset();

        detectorBand.reset();
        follower.prepare (sr, 1.0f, 80.0f);
        dryBuffer.setSize (2, juce::jmax (32, maxBlock), false, true, false);
        auditionGain.reset (sr, 0.02);
        auditionGain.setCurrentAndTargetValue (0.0f);
        lastReductionDb = 0.0f;
    }

    void reset()
    {
        for (auto& f : filters)
            f.reset();

        detectorBand.reset();
        follower.reset();
        auditionGain.setCurrentAndTargetValue (0.0f);
        lastReductionDb = 0.0f;
    }

    void process (juce::AudioBuffer<float>& buffer, int n, const RackParams& p)
    {
        const int channels = juce::jmin (2, buffer.getNumChannels());
        const float splitHz = juce::jlimit (3500.0f, 6000.0f, p.dsFreq * 0.55f);

        for (int ch = 0; ch < channels; ++ch)
        {
            filters[ch * 4 + 0].setLowPass (sampleRate, splitHz, 0.707);
            filters[ch * 4 + 1].setLowPass (sampleRate, splitHz, 0.707);
            filters[ch * 4 + 2].setHighPass (sampleRate, splitHz, 0.707);
            filters[ch * 4 + 3].setHighPass (sampleRate, splitHz, 0.707);
        }

        detectorBand.setBandPass (sampleRate, p.dsFreq, 1.4);
        follower.setTimes (1.0f, p.dsRelease);
        auditionGain.setTargetValue (p.dsAudition && ! p.dsBypass ? 1.0f : 0.0f);

        for (int ch = 0; ch < channels; ++ch)
            dryBuffer.copyFrom (ch, 0, buffer, ch, 0, n);

        const float threshold = p.dsThreshold;
        const float range = juce::jmax (0.0f, p.dsRange);
        float minReduction = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            float low[2] {};
            float high[2] {};
            float detect = 0.0f;

            for (int ch = 0; ch < channels; ++ch)
            {
                const float x = buffer.getReadPointer (ch)[i];
                low[ch] = filters[ch * 4 + 1].process (filters[ch * 4 + 0].process (x));
                high[ch] = filters[ch * 4 + 3].process (filters[ch * 4 + 2].process (x));
                detect += high[ch];
            }

            detect /= (float) juce::jmax (1, channels);
            detect = detectorBand.process (detect);
            const float envelope = follower.process (detect);
            const float levelDb = gainToDb (envelope);
            const float reductionDb = juce::jlimit (0.0f, range, (levelDb - threshold) * 1.2f);
            const float highGain = dbToGain (-reductionDb);
            minReduction = juce::jmin (minReduction, -reductionDb);

            const float auditionMix = auditionGain.getNextValue();

            // Apply the split-band result, then optionally replace it with the
            // audition signal for the UI's detector audition switch.
            for (int ch = 0; ch < channels; ++ch)
            {
                const float dry = dryBuffer.getReadPointer (ch)[i];
                const float deessed = low[ch] + high[ch] * highGain;
                const float monitor = detect * 0.5f;
                const float out = dry + (deessed - dry) * (p.dsBypass ? 0.0f : 1.0f);
                buffer.getWritePointer (ch)[i] = out + (monitor - out) * auditionMix;
            }
        }

        lastReductionDb = minReduction;
    }

    float getReductionDb() const noexcept { return lastReductionDb; }

private:
    double sampleRate = 48000.0;
    Biquad filters[8];
    Biquad detectorBand;
    EnvelopeFollower follower;
    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> auditionGain;
    float lastReductionDb = 0.0f;
};

//==============================================================================
class Saturation
{
public:
    void prepare (double sr, int maxBlock, int numChannels, bool useOversampling)
    {
        sampleRate = sr;
        channels = juce::jlimit (1, 2, numChannels);
        maximumBlock = juce::jmax (32, maxBlock);
        oversampling.reset();

        if (useOversampling)
        {
            oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
                (size_t) channels, 1,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
            oversampling->initProcessing ((size_t) maximumBlock);
            latencySamples = (int) std::ceil (oversampling->getLatencyInSamples());
        }
        else
        {
            latencySamples = 0;
        }

        for (auto& line : dryDelay)
            line.prepare (latencySamples + maximumBlock + 8);

        dryBuffer.setSize (2, maximumBlock, false, true, false);
        blendSmoother.reset (sr, 0.01);
        blendSmoother.setCurrentAndTargetValue (1.0f);
    }

    void reset()
    {
        for (auto& line : dryDelay)
            line.reset();

        blendSmoother.setCurrentAndTargetValue (1.0f);
        latencySamples = oversampling != nullptr ? (int) std::ceil (oversampling->getLatencyInSamples()) : 0;
    }

    int getLatencySamples() const noexcept { return latencySamples; }

    void process (juce::AudioBuffer<float>& buffer, int n, const RackParams& p)
    {
        const int activeChannels = juce::jmin (channels, buffer.getNumChannels());
        const float drive = dbToGain (p.satDrive);
        const float manualOutput = dbToGain (p.satOutput);
        const float autoCompensation = 1.0f / (1.0f + 0.22f * drive);
        blendSmoother.setTargetValue (p.satBypass ? 0.0f : juce::jlimit (0.0f, 1.0f, p.satBlend * 0.01f));

        // Store the dry signal before processing. It is delayed by the
        // oversampler's own latency so the blend is phase aligned.
        for (int ch = 0; ch < activeChannels; ++ch)
        {
            auto* dry = buffer.getReadPointer (ch);
            auto* saved = dryBuffer.getWritePointer (ch);

            for (int i = 0; i < n; ++i)
            {
                saved[i] = dry[i];
                dryDelay[ch].write (dry[i]);
            }
        }

        if (oversampling != nullptr)
        {
            juce::dsp::AudioBlock<float> block (buffer);
            auto subBlock = block.getSubBlock (0, (size_t) n);
            auto upBlock = oversampling->processSamplesUp (subBlock);

            for (size_t ch = 0; ch < upBlock.getNumChannels(); ++ch)
            {
                auto* data = upBlock.getChannelPointer (ch);

                for (size_t i = 0; i < upBlock.getNumSamples(); ++i)
                    data[i] = saturate (data[i] * drive, p.satMode);
            }

            oversampling->processSamplesDown (subBlock);
        }
        else
        {
            for (int ch = 0; ch < activeChannels; ++ch)
            {
                auto* data = buffer.getWritePointer (ch);

                for (int i = 0; i < n; ++i)
                    data[i] = saturate (data[i] * drive, p.satMode);
            }
        }

        for (int i = 0; i < n; ++i)
        {
            const float blend = blendSmoother.getNextValue();

            for (int ch = 0; ch < activeChannels; ++ch)
            {
                const float delayedDry = latencySamples > 0
                                             ? dryDelay[ch].read ((float) latencySamples + (float) (n - i))
                                             : dryBuffer.getReadPointer (ch)[i];
                const float wet = buffer.getReadPointer (ch)[i] * autoCompensation * manualOutput;
                buffer.getWritePointer (ch)[i] = delayedDry + (wet - delayedDry) * blend;
            }
        }
    }

private:
    static inline float saturate (float x, int mode) noexcept
    {
        if (mode == 0)
            return std::tanh (1.25f * x);

        const float clipped = juce::jlimit (-3.0f, 3.0f, x);
        float y = clipped / (1.0f + 0.55f * std::abs (clipped));
        y += 0.16f * clipped * clipped * (clipped >= 0.0f ? 1.0f : -1.0f);
        return std::tanh (y);
    }

    double sampleRate = 48000.0;
    int channels = 2;
    int maximumBlock = 512;
    int latencySamples = 0;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    DelayLine dryDelay[2];
    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> blendSmoother;
};

//==============================================================================
class StereoDelay
{
public:
    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        maximumDelay = (int) std::ceil (2.2 * sr);
        for (auto& line : lines)
            line.prepare (maximumDelay + maxBlock + 8);

        for (auto& filter : feedbackHp)
            filter.reset();

        for (auto& filter : feedbackLp)
            filter.reset();

        duckFollower.prepare (sr, 2.0f, 120.0f);
        levelGain.reset (sr, 0.02);
        levelGain.setCurrentAndTargetValue (0.0f);
        delaySmoothed = 0.375 * sr;
        delayTarget = delaySmoothed;
        delayInitialised = false;
        lastDelayMs = 375.0;
    }

    void reset()
    {
        for (auto& line : lines)
            line.reset();

        delayInitialised = false;

        for (auto& filter : feedbackHp)
            filter.reset();

        for (auto& filter : feedbackLp)
            filter.reset();

        duckFollower.reset();
        levelGain.setCurrentAndTargetValue (0.0f);
        delaySmoothed = 0.375 * sampleRate;
        delayTarget = delaySmoothed;
    }

    void process (juce::AudioBuffer<float>& send,
                  juce::AudioBuffer<float>& wetOutput,
                  int n,
                  const RackParams& p,
                  double hostBpm,
                  bool bypassed)
    {
        wetOutput.clear();
        const int channels = juce::jmin (2, send.getNumChannels());

        for (int ch = 0; ch < 2; ++ch)
        {
            feedbackHp[ch].setHighPass (sampleRate, p.dlyLowCut, 0.707);
            feedbackLp[ch].setLowPass (sampleRate, p.dlyHighCut, 0.707);
        }

        const double bpm = (hostBpm > 20.0 && hostBpm < 400.0) ? hostBpm : (double) p.dlyManualBpm;
        double delayMs = p.dlyMs;

        if (p.dlySync)
        {
            const double beatMs = 60000.0 / juce::jmax (20.0, bpm);
            static const double divisions[] = { 0.125, 0.25, 0.5, 1.0, 2.0 };
            const int division = juce::jlimit (0, 4, p.dlyDivision);
            delayMs = beatMs * divisions[division];

            if (p.dlyFeel == 1)
                delayMs *= 1.5;
            else if (p.dlyFeel == 2)
                delayMs *= 2.0 / 3.0;
        }

        delayMs = juce::jlimit (10.0, 2000.0, delayMs);
        lastDelayMs = (float) delayMs;
        delayTarget = (float) (delayMs * 0.001 * sampleRate);

        if (! delayInitialised)
        {
            delaySmoothed = delayTarget;
            delayInitialised = true;
        }

        const float feedback = juce::jlimit (0.0f, 0.95f, p.dlyFeedback * 0.01f);
        const float targetGain = bypassed ? 0.0f : dbToGain (p.dlyLevel);
        levelGain.setTargetValue (targetGain);
        const float duck = juce::jlimit (0.0f, 1.0f, p.dlyDuck * 0.01f);
        const float smoothCoefficient = (float) (1.0 - std::exp (-1.0 / (0.035 * sampleRate)));

        for (int i = 0; i < n; ++i)
        {
            delaySmoothed += smoothCoefficient * (delayTarget - delaySmoothed);

            float in[2] { 0.0f, 0.0f };

            for (int ch = 0; ch < channels; ++ch)
                in[ch] = send.getReadPointer (ch)[i];

            if (channels == 1)
                in[1] = in[0];

            const float readL = lines[0].read (delaySmoothed);
            const float readR = lines[1].read (delaySmoothed);

            float wetL = readL;
            float wetR = readR;

            if (p.dlyPingPong)
            {
                wetL = readR;
                wetR = readL;
            }

            wetL = feedbackHp[0].process (feedbackLp[0].process (wetL));
            wetR = feedbackHp[1].process (feedbackLp[1].process (wetR));
            const float fbL = wetL * feedback;
            const float fbR = wetR * feedback;

            lines[0].write (in[0] + (p.dlyPingPong ? fbR : fbL));
            lines[1].write (in[1] + (p.dlyPingPong ? fbL : fbR));

            float detector = 0.0f;

            for (int ch = 0; ch < channels; ++ch)
                detector = juce::jmax (detector, std::abs (in[ch]));

            const float envelope = duckFollower.process (detector);
            const float duckGain = 1.0f - duck * juce::jlimit (0.0f, 1.0f, envelope * 2.0f);
            const float gain = levelGain.getNextValue() * duckGain;

            if (wetOutput.getNumChannels() > 0)
                wetOutput.getWritePointer (0)[i] = wetL * gain;

            if (wetOutput.getNumChannels() > 1)
                wetOutput.getWritePointer (1)[i] = wetR * gain;
        }
    }

    float getDelayMs() const noexcept { return lastDelayMs; }

private:
    double sampleRate = 48000.0;
    int maximumDelay = 96000;
    DelayLine lines[2];
    Biquad feedbackHp[2];
    Biquad feedbackLp[2];
    EnvelopeFollower duckFollower;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> levelGain;
    float delaySmoothed = 1.0f;
    float delayTarget = 1.0f;
    float lastDelayMs = 375.0f;
    bool delayInitialised = false;
};

//==============================================================================
class LookaheadLimiter
{
public:
    void prepare (double sr, int maxBlock, int lookaheadSamples)
    {
        sampleRate = sr;
        lookahead = juce::jmax (0, lookaheadSamples);
        maximumBlock = juce::jmax (32, maxBlock);

        for (auto& line : delayLines)
            line.prepare (lookahead + maximumBlock + 8);

        gain = 1.0f;
        lastGainReductionDb = 0.0f;
    }

    void reset()
    {
        for (auto& line : delayLines)
            line.reset();

        gain = 1.0f;
        lastGainReductionDb = 0.0f;
    }

    int getLatencySamples() const noexcept { return lookahead; }

    void setForceUnity (bool shouldForce) noexcept { forceUnity = shouldForce; }

    void process (juce::AudioBuffer<float>& buffer, int n, const RackParams& p)
    {
        const int channels = juce::jmin (2, buffer.getNumChannels());
        const float ceiling = dbToGain (juce::jlimit (-6.0f, 0.0f, p.limCeiling));
        const float releaseSamples = juce::jmax (1.0f, p.limRelease * 0.001f * (float) sampleRate);
        const float releaseCoefficient = (float) (1.0 - std::exp (-1.0 / releaseSamples));
        const float attackCoefficient = lookahead > 0
                                            ? (float) (1.0 - std::exp (-5.0 / (double) lookahead))
                                            : 1.0f;
        const bool enabled = ! p.limBypass;
        float minGain = 1.0f;

        for (int i = 0; i < n; ++i)
        {
            float peak = 0.0f;

            for (int ch = 0; ch < channels; ++ch)
                peak = juce::jmax (peak, std::abs (buffer.getReadPointer (ch)[i]));

            const float required = peak > ceiling ? ceiling / peak : 1.0f;

            if (forceUnity)
                gain = 1.0f;
            else if (! enabled)
                gain += (1.0f - gain) * releaseCoefficient;
            else if (required < gain)
                gain += (required - gain) * attackCoefficient;
            else
                gain += (required - gain) * releaseCoefficient;

            gain = juce::jlimit (0.0001f, 1.0f, gain);
            minGain = juce::jmin (minGain, gain);

            for (int ch = 0; ch < channels; ++ch)
            {
                const float input = buffer.getReadPointer (ch)[i];
                const float delayedInput = lookahead > 0
                                               ? delayLines[ch].read ((float) lookahead)
                                               : input;
                delayLines[ch].write (input);
                float output = delayedInput * gain;

                if (enabled && ! forceUnity)
                    output = juce::jlimit (-ceiling, ceiling, output);

                buffer.getWritePointer (ch)[i] = output;
            }
        }

        lastGainReductionDb = gainToDb (minGain);
    }

    float getGainReductionDb() const noexcept { return lastGainReductionDb; }

private:
    double sampleRate = 48000.0;
    int lookahead = 0;
    int maximumBlock = 512;
    DelayLine delayLines[2];
    float gain = 1.0f;
    float lastGainReductionDb = 0.0f;
    bool forceUnity = false;
};

} // namespace vr
