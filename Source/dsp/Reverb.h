#pragma once

#include "Common.h"
#include "../Parameters.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <vector>

namespace vr
{

/**
    Stereo algorithmic reverb.

    Room: discrete early reflections plus a short 8-line FDN.
    Plate: no discrete early taps, heavier input diffusion and deeper
    modulation for a smoother, denser tail.  Both are algorithmic; no assets.
*/
class Reverb
{
public:
    void prepare (double sampleRate, int maximumBlockSize, int numChannels);
    void reset();

    /** Adds the wet return into wetOutput; send contains the dry processed vocal. */
    void process (const juce::AudioBuffer<float>& send,
                  juce::AudioBuffer<float>& wetOutput,
                  int numSamples,
                  const RackParams& params) noexcept;

    int getTailSamples (double decaySeconds) const noexcept
    {
        return (int) std::ceil (decaySeconds * sampleRate);
    }

private:
    struct Allpass
    {
        DelayLine line;
        float coefficient = 0.5f;

        void prepare (int maxSamples)
        {
            line.prepare (maxSamples);
        }

        void reset() { line.reset(); }

        inline float process (float x, int delaySamples) noexcept
        {
            const float delayed = line.read ((float) delaySamples);
            const float y = -coefficient * x + delayed;
            line.write (x + coefficient * delayed);
            return y;
        }
    };

    void applyType (int typeIndex);
    void updateFilters (const RackParams& params) noexcept;
    float readEarly (float delaySamples) const noexcept;

    double sampleRate = 48000.0;
    int channels = 2;
    int maximumBlock = 512;

    std::array<DelayLine, 8> lines;
    std::array<float, 8> lineSeconds {};
    std::array<float, 8> decayGain {};
    std::array<float, 8> dampingState {};
    std::array<float, 8> modulationPhase {};
    std::array<int, 8> baseLength {};

    std::array<Allpass, 4> diffusion;
    std::array<int, 4> diffusionLength {};

    DelayLine predelay;
    DelayLine earlyLine;

    float diffusionCoefficient = 0.55f;
    float modulationDepth = 1.0f;
    float outputScale = 0.32f;
    float inputScale = 0.45f;

    int currentType = -1;
    int pendingType = -1;
    float typeGain = 1.0f;
    float typeFadeCoefficient = 0.001f;

    float predelaySmoothed = 20.0f;
    float returnGainSmoothed = 0.2f;
    float duckAmount = 0.0f;
    float dampingFilter = 0.5f;

    Biquad highPassL, highPassR, lowPassL, lowPassR;
    EnvelopeFollower duckFollower;
    float lfoPhase = 0.0f;

    juce::AudioBuffer<float> earlyBuffer;
};

} // namespace vr
