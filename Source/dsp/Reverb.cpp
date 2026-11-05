#include "Reverb.h"

#include <algorithm>
#include <cmath>

namespace vr
{

namespace
{
inline float hadamardScale (float value) noexcept
{
    return value * 0.35355339059f; // 1 / sqrt(8)
}
} // namespace

void Reverb::prepare (double newSampleRate, int maximumBlockSize, int numChannels)
{
    sampleRate = newSampleRate > 1000.0 ? newSampleRate : 48000.0;
    maximumBlock = juce::jlimit (16, 8192, maximumBlockSize);
    channels = juce::jlimit (1, 2, numChannels);

    lineSeconds = { 0.0297f, 0.0371f, 0.0411f, 0.0437f, 0.0479f, 0.0531f, 0.0587f, 0.0613f };

    for (size_t i = 0; i < lines.size(); ++i)
    {
        baseLength[i] = (int) std::lround ((double) lineSeconds[i] * sampleRate);
        lines[i].prepare (baseLength[i] + 16);
        dampingState[i] = 0.0f;
        modulationPhase[i] = (float) i * 0.78539816f;
    }

    static const double diffusionMs[] = { 4.7, 3.6, 12.5, 9.2 };

    for (size_t i = 0; i < diffusion.size(); ++i)
    {
        diffusionLength[i] = (int) std::lround (diffusionMs[i] * 0.001 * sampleRate);
        diffusion[i].prepare (diffusionLength[i] + 8);
    }

    predelay.prepare ((int) std::ceil (0.2 * sampleRate));
    earlyLine.prepare ((int) std::ceil (0.05 * sampleRate));

    earlyBuffer.setSize (2, maximumBlock, false, true, false);

    highPassL.setHighPass (sampleRate, 120.0, 0.707);
    highPassR.setHighPass (sampleRate, 120.0, 0.707);
    lowPassL.setLowPass (sampleRate, 9000.0, 0.707);
    lowPassR.setLowPass (sampleRate, 9000.0, 0.707);

    duckFollower.prepare (sampleRate, 4.0f, 120.0f);
    typeFadeCoefficient = (float) (1.0 - std::exp (-1.0 / (0.025 * sampleRate)));

    currentType = -1;
    pendingType = -1;
    typeGain = 1.0f;
    lfoPhase = 0.0f;

    reset();
}

void Reverb::reset()
{
    for (auto& line : lines)
        line.reset();

    for (auto& ap : diffusion)
        ap.reset();

    predelay.reset();
    earlyLine.reset();
    std::fill (dampingState.begin(), dampingState.end(), 0.0f);
    std::fill (modulationPhase.begin(), modulationPhase.end(), 0.0f);

    highPassL.reset();
    highPassR.reset();
    lowPassL.reset();
    lowPassR.reset();
    duckFollower.reset();
    earlyBuffer.clear();

    typeGain = 1.0f;
    lfoPhase = 0.0f;
}

void Reverb::applyType (int typeIndex)
{
    currentType = juce::jlimit (0, 1, typeIndex);
    pendingType = -1;

    if (currentType == 0)
    {
        diffusionCoefficient = 0.52f;
        modulationDepth = 0.7f;
        outputScale = 0.34f;
        inputScale = 0.5f;
        diffusion[0].coefficient = 0.52f;
        diffusion[1].coefficient = 0.48f;
        diffusion[2].coefficient = 0.56f;
        diffusion[3].coefficient = 0.5f;
    }
    else
    {
        diffusionCoefficient = 0.72f;
        modulationDepth = 2.4f;
        outputScale = 0.28f;
        inputScale = 0.42f;
        diffusion[0].coefficient = 0.7f;
        diffusion[1].coefficient = 0.66f;
        diffusion[2].coefficient = 0.73f;
        diffusion[3].coefficient = 0.68f;
    }
}

void Reverb::updateFilters (const RackParams& params) noexcept
{
    highPassL.setHighPass (sampleRate, params.revLowCut, 0.707);
    highPassR.setHighPass (sampleRate, params.revLowCut, 0.707);
    lowPassL.setLowPass (sampleRate, params.revHighCut, 0.707);
    lowPassR.setLowPass (sampleRate, params.revHighCut, 0.707);

    duckAmount = juce::jlimit (0.0f, 1.0f, params.revDuck * 0.01f);
    duckFollower.setTimes (3.0f, 140.0f);

    const float damping = juce::jlimit (0.0f, 100.0f, params.revDamping) * 0.01f;

    // Ensure a finite, decaying tail for every parameter combination.
    for (size_t i = 0; i < lines.size(); ++i)
    {
        const double rt60 = juce::jlimit (0.3, 8.0, (double) params.revDecay);
        const double lengthSeconds = (double) baseLength[i] / sampleRate;
        const double feedback = std::pow (10.0, -3.0 * lengthSeconds / rt60);
        decayGain[i] = (float) juce::jlimit (0.0, 0.995, feedback);
    }

    dampingFilter = 0.06f + 0.92f * (1.0f - damping);
    juce::ignoreUnused (damping);
}

float Reverb::readEarly (float delaySamples) const noexcept
{
    return earlyLine.read (delaySamples);
}

void Reverb::process (const juce::AudioBuffer<float>& send,
                      juce::AudioBuffer<float>& wetOutput,
                      int numSamples,
                      const RackParams& params) noexcept
{
    wetOutput.clear();

    if (numSamples <= 0)
        return;

    const float blockFade = juce::jlimit (0.0f, 1.0f,
        (float) (1.0 - std::exp (-(double) numSamples / (0.025 * sampleRate))));

    if (pendingType >= 0)
    {
        typeGain -= blockFade;

        if (typeGain <= 0.001f)
        {
            typeGain = 0.0f;
            applyType (pendingType);
            reset();
            typeGain = 0.001f;
        }
    }
    else
    {
        typeGain = juce::jmin (1.0f, typeGain + blockFade * 2.0f);
    }

    if (currentType < 0)
        applyType (params.revType);

    if (params.revType != currentType && pendingType < 0)
        pendingType = juce::jlimit (0, 1, params.revType);

    updateFilters (params);

    const float targetPredelaySamples = juce::jlimit (0.0f, 0.15f, params.revPredelay * 0.001f) * (float) sampleRate;
    const float predelayCoefficient = (float) (1.0 - std::exp (-1.0 / (0.03 * sampleRate)));
    const float targetGain = params.revBypass ? 0.0f : dbToGain (params.revLevel);
    const float gainCoefficient = (float) (1.0 - std::exp (-1.0 / (0.02 * sampleRate)));

    const int sendChannels = juce::jmin (send.getNumChannels(), 2);
    const int outChannels = juce::jmin (wetOutput.getNumChannels(), 2);
    const float channelScale = sendChannels > 0 ? 1.0f / (float) sendChannels : 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        float input = 0.0f;

        for (int channel = 0; channel < sendChannels; ++channel)
            input += send.getReadPointer (channel)[i];

        input *= channelScale;

        predelaySmoothed += predelayCoefficient * (targetPredelaySamples - predelaySmoothed);
        const float delayed = predelay.read (predelaySmoothed);
        predelay.write (input);

        earlyLine.write (delayed);

        float diffused = delayed;

        for (size_t d = 0; d < diffusion.size(); ++d)
            diffused = diffusion[d].process (diffused, diffusionLength[d]);

        std::array<float, 8> read {};

        lfoPhase += kTwoPi * (currentType == 0 ? 0.13f : 0.21f) / (float) sampleRate;

        if (lfoPhase > kTwoPi)
            lfoPhase -= kTwoPi;

        for (size_t l = 0; l < lines.size(); ++l)
        {
            const float modulation = std::sin (lfoPhase * (float) (l + 1) + modulationPhase[l])
                                     * modulationDepth;
            const float delay = (float) baseLength[l] + modulation;
            read[l] = lines[l].read (delay);
            dampingState[l] += dampingFilter * (read[l] - dampingState[l]);
            read[l] = dampingState[l];
        }

        std::array<float, 8> feedback = read;

        // Fast Hadamard transform (unnormalised), then normalise and decay.
        for (int stride = 1; stride < 8; stride <<= 1)
        {
            for (int base = 0; base < 8; base += stride << 1)
            {
                for (int j = 0; j < stride; ++j)
                {
                    const float a = feedback[(size_t) (base + j)];
                    const float b = feedback[(size_t) (base + j + stride)];
                    feedback[(size_t) (base + j)] = a + b;
                    feedback[(size_t) (base + j + stride)] = a - b;
                }
            }
        }

        for (size_t l = 0; l < lines.size(); ++l)
            lines[l].write (diffused * inputScale + hadamardScale (feedback[l]) * decayGain[l]);

        const float earlyL = readEarly (0.0071f * (float) sampleRate) * 0.22f
                             + readEarly (0.0129f * (float) sampleRate) * -0.18f
                             + readEarly (0.0193f * (float) sampleRate) * 0.14f
                             + readEarly (0.0271f * (float) sampleRate) * 0.10f;
        const float earlyR = readEarly (0.0083f * (float) sampleRate) * 0.20f
                             + readEarly (0.0147f * (float) sampleRate) * 0.16f
                             + readEarly (0.0217f * (float) sampleRate) * -0.13f
                             + readEarly (0.0311f * (float) sampleRate) * 0.09f;

        float wetL = (read[0] + read[2] + read[4] + read[6]) * outputScale;
        float wetR = (read[1] + read[3] + read[5] + read[7]) * outputScale;

        if (currentType == 0)
        {
            wetL += earlyL;
            wetR += earlyR;
        }

        wetL = lowPassL.process (highPassL.process (wetL));
        wetR = lowPassR.process (highPassR.process (wetR));

        const float dryEnvelope = duckFollower.process (input);
        const float duck = 1.0f - duckAmount * juce::jlimit (0.0f, 1.0f, dryEnvelope * 2.5f);

        returnGainSmoothed += gainCoefficient * (targetGain - returnGainSmoothed);
        const float gain = returnGainSmoothed * duck * typeGain;

        if (outChannels > 0)
            wetOutput.getWritePointer (0)[i] = wetL * gain;

        if (outChannels > 1)
            wetOutput.getWritePointer (1)[i] = wetR * gain;
    }
}

} // namespace vr
