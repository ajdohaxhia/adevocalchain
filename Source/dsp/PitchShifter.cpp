// Phase-vocoder shifter adapted from PitchLab / MegaTune
// Copyright (c) 2026 PitchLab, MIT License.
// The original is preserved unchanged at the inspected upstream location; this
// file is a modified, reduced adaptation for ademusic Vocal Rack.

#include "PitchShifter.h"

#include <algorithm>
#include <cmath>

namespace vr
{

namespace
{
constexpr float twoPi = 6.28318530717958647692f;

inline float wrapPhase (float phase) noexcept
{
    return std::remainder (phase, twoPi);
}
} // namespace

void PitchShifter::buildFftState (int frameSize, int numChannels)
{
    state.size = frameSize;
    state.hop = juce::jmax (32, frameSize / 4);
    state.window.assign ((size_t) frameSize, 0.0f);
    state.fftRe.assign ((size_t) frameSize, 0.0f);
    state.fftIm.assign ((size_t) frameSize, 0.0f);
    state.twiddleCos.assign ((size_t) frameSize / 2 + 1, 0.0f);
    state.twiddleSin.assign ((size_t) frameSize / 2 + 1, 0.0f);
    state.bitReverse.assign ((size_t) frameSize, 0);
    state.magnitude.assign ((size_t) frameSize / 2 + 2, 0.0f);
    state.omega.assign ((size_t) frameSize / 2 + 2, 0.0f);
    state.phase.assign ((size_t) frameSize / 2 + 2, 0.0f);

    const double twoPiDouble = juce::MathConstants<double>::twoPi;

    for (int i = 0; i < frameSize; ++i)
        state.window[(size_t) i] = (float) (0.5 - 0.5 * std::cos (twoPiDouble * (double) i / (double) frameSize));

    for (int i = 0; i <= frameSize / 2; ++i)
    {
        state.twiddleCos[(size_t) i] = (float) std::cos (twoPiDouble * (double) i / (double) frameSize);
        state.twiddleSin[(size_t) i] = (float) std::sin (twoPiDouble * (double) i / (double) frameSize);
    }

    int bits = 0;
    while ((1 << bits) < frameSize)
        ++bits;

    for (int i = 0; i < frameSize; ++i)
    {
        int reversed = 0;
        int value = i;

        for (int bit = 0; bit < bits; ++bit)
        {
            reversed = (reversed << 1) | (value & 1);
            value >>= 1;
        }

        state.bitReverse[(size_t) i] = reversed;
    }

    state.previousPhase.assign ((size_t) numChannels, std::vector<float> ((size_t) frameSize / 2 + 2, 0.0f));
    state.phaseAccumulator.assign ((size_t) numChannels, std::vector<float> ((size_t) frameSize / 2 + 2, 0.0f));
    state.phaseActive.assign ((size_t) numChannels, std::vector<uint8_t> ((size_t) frameSize / 2 + 2, 0));
    state.previousPhaseValid.assign ((size_t) numChannels, 0);
}

void PitchShifter::prepare (double newSampleRate, int numChannels, int maximumBlockSize,
                            int frameSize, int newDelaySamples)
{
    sampleRate = newSampleRate > 1000.0 ? newSampleRate : 48000.0;
    channels = juce::jlimit (1, 2, numChannels);
    maximumBlock = juce::jlimit (16, 8192, maximumBlockSize);
    delaySamples = juce::jlimit (frameSize, inputRingSize / 4, newDelaySamples);

    inputRing.assign ((size_t) inputRingSize * (size_t) channels, 0.0f);
    olaRing.assign ((size_t) olaRingSize * (size_t) channels, 0.0f);
    windowSum.assign ((size_t) olaRingSize, 0.0f);

    buildFftState (juce::jmax (256, frameSize), channels);
    processedFrames = 0;
    reset();
}

void PitchShifter::reset()
{
    std::fill (inputRing.begin(), inputRing.end(), 0.0f);
    std::fill (olaRing.begin(), olaRing.end(), 0.0f);
    std::fill (windowSum.begin(), windowSum.end(), 0.0f);

    inputWritePos = 0;
    outputReadPos = 0;
    processedFrames = 0;
    resetPhaseState();
    resetOverlapAdd();
}

void PitchShifter::resetPhaseState() noexcept
{
    for (auto& channelPhase : state.previousPhase)
        std::fill (channelPhase.begin(), channelPhase.end(), 0.0f);

    for (auto& channelPhase : state.phaseAccumulator)
        std::fill (channelPhase.begin(), channelPhase.end(), 0.0f);

    for (auto& channelActive : state.phaseActive)
        std::fill (channelActive.begin(), channelActive.end(), 0);

    std::fill (state.previousPhaseValid.begin(), state.previousPhaseValid.end(), 0);
}

void PitchShifter::resetOverlapAdd() noexcept
{
    std::fill (olaRing.begin(), olaRing.end(), 0.0f);
    std::fill (windowSum.begin(), windowSum.end(), 0.0f);
    nextFrameCentre = (double) outputReadPos - (double) delaySamples - 0.5 * (double) state.size;
}

void PitchShifter::pushInput (const juce::AudioBuffer<float>& input, int startSample, int numSamples) noexcept
{
    const int inputChannels = input.getNumChannels();
    const int samples = juce::jmax (0, numSamples);

    for (int i = 0; i < samples; ++i)
    {
        const int64_t position = inputWritePos + (int64_t) i;
        const int index = (int) (position & inputRingMask);

        for (int channel = 0; channel < channels; ++channel)
        {
            const float sample = channel < inputChannels
                                     ? input.getReadPointer (channel)[startSample + i]
                                     : 0.0f;
            inputRing[(size_t) channel * (size_t) inputRingSize + (size_t) index] = sample;
        }
    }

    inputWritePos += samples;
}

float PitchShifter::readInputSample (int channel, int64_t position) const noexcept
{
    if (position < 0 || position >= inputWritePos)
        return 0.0f;

    const int index = (int) (position & inputRingMask);
    return inputRing[(size_t) channel * (size_t) inputRingSize + (size_t) index];
}

void PitchShifter::fft (bool inverse) noexcept
{
    const int size = state.size;

    for (int i = 0; i < size; ++i)
    {
        const int j = state.bitReverse[(size_t) i];

        if (j > i)
        {
            std::swap (state.fftRe[(size_t) i], state.fftRe[(size_t) j]);
            std::swap (state.fftIm[(size_t) i], state.fftIm[(size_t) j]);
        }
    }

    const float sign = inverse ? 1.0f : -1.0f;

    for (int length = 2; length <= size; length <<= 1)
    {
        const int half = length >> 1;
        const int step = size / length;

        for (int start = 0; start < size; start += length)
        {
            for (int j = 0; j < half; ++j)
            {
                const int twiddle = j * step;
                const float wr = state.twiddleCos[(size_t) twiddle];
                const float wi = sign * state.twiddleSin[(size_t) twiddle];

                const int even = start + j;
                const int odd = even + half;

                const float oddRe = state.fftRe[(size_t) odd];
                const float oddIm = state.fftIm[(size_t) odd];
                const float rotatedRe = oddRe * wr - oddIm * wi;
                const float rotatedIm = oddRe * wi + oddIm * wr;

                state.fftRe[(size_t) odd] = state.fftRe[(size_t) even] - rotatedRe;
                state.fftIm[(size_t) odd] = state.fftIm[(size_t) even] - rotatedIm;
                state.fftRe[(size_t) even] += rotatedRe;
                state.fftIm[(size_t) even] += rotatedIm;
            }
        }
    }

    if (inverse)
    {
        const float scale = 1.0f / (float) size;

        for (int i = 0; i < size; ++i)
        {
            state.fftRe[(size_t) i] *= scale;
            state.fftIm[(size_t) i] *= scale;
        }
    }
}

void PitchShifter::processFrame (int centre, float ratio) noexcept
{
    const int size = state.size;
    const int halfSize = size / 2;
    const int64_t frameStart = (int64_t) centre - (int64_t) halfSize;

    for (int i = 0; i < size; ++i)
    {
        state.fftRe[(size_t) i] = readInputSample (0, frameStart + i) * state.window[(size_t) i];
        state.fftIm[(size_t) i] = 0.0f;
    }

    fft (false);

    for (int channel = 0; channel < channels; ++channel)
    {
        if (channel > 0)
        {
            for (int i = 0; i < size; ++i)
            {
                state.fftRe[(size_t) i] = readInputSample (channel, frameStart + i) * state.window[(size_t) i];
                state.fftIm[(size_t) i] = 0.0f;
            }

            fft (false);
        }

        auto& previous = state.previousPhase[(size_t) channel];
        auto& accumulator = state.phaseAccumulator[(size_t) channel];
        auto& active = state.phaseActive[(size_t) channel];
        const bool havePrevious = state.previousPhaseValid[(size_t) channel] != 0;

        const double twoPiDouble = juce::MathConstants<double>::twoPi;

        for (int k = 0; k <= halfSize; ++k)
        {
            const float re = state.fftRe[(size_t) k];
            const float im = state.fftIm[(size_t) k];
            const float magnitude = std::sqrt (re * re + im * im);
            const float phase = std::atan2 (im, re);

            const float expected = (float) (twoPiDouble * (double) k
                                            * (double) state.hop / (double) size);

            if (havePrevious)
            {
                const float delta = wrapPhase (phase - previous[(size_t) k] - expected);
                state.omega[(size_t) k] = juce::jlimit (0.0f, juce::MathConstants<float>::pi,
                                                        (expected + delta) / (float) state.hop);
            }
            else
            {
                state.omega[(size_t) k] = juce::jlimit (0.0f, juce::MathConstants<float>::pi,
                                                        (float) (twoPiDouble * (double) k / (double) size));
            }

            state.magnitude[(size_t) k] = magnitude;
            state.phase[(size_t) k] = phase;
            previous[(size_t) k] = phase;
        }

        state.previousPhaseValid[(size_t) channel] = 1;

        std::fill (state.fftRe.begin(), state.fftRe.end(), 0.0f);
        std::fill (state.fftIm.begin(), state.fftIm.end(), 0.0f);

        const float limitedRatio = juce::jlimit (minimumRatio, maximumRatio, ratio);

        for (int target = 1; target < halfSize; ++target)
        {
            const double sourcePosition = (double) target / (double) limitedRatio;

            if (sourcePosition >= (double) halfSize - 1.0)
            {
                active[(size_t) target] = 0;
                continue;
            }

            const int source0 = (int) std::floor (sourcePosition);
            const int source1 = juce::jmin (halfSize, source0 + 1);
            const float fraction = (float) (sourcePosition - (double) source0);

            const float magnitude = (1.0f - fraction) * state.magnitude[(size_t) source0]
                                    + fraction * state.magnitude[(size_t) source1];

            if (magnitude < 1.0e-5f)
            {
                active[(size_t) target] = 0;
                continue;
            }

            const float omega = (1.0f - fraction) * state.omega[(size_t) source0]
                                + fraction * state.omega[(size_t) source1];

            float outputPhase = accumulator[(size_t) target];

            if (active[(size_t) target] == 0)
            {
                outputPhase = state.phase[(size_t) source0];
                active[(size_t) target] = 1;
            }
            else
            {
                outputPhase = wrapPhase (outputPhase + omega * limitedRatio * (float) state.hop);
            }

            accumulator[(size_t) target] = outputPhase;

            state.fftRe[(size_t) target] = magnitude * std::cos (outputPhase);
            state.fftIm[(size_t) target] = magnitude * std::sin (outputPhase);
        }

        for (int k = 1; k < halfSize; ++k)
        {
            state.fftRe[(size_t) (size - k)] = state.fftRe[(size_t) k];
            state.fftIm[(size_t) (size - k)] = -state.fftIm[(size_t) k];
        }

        state.fftRe[0] = 0.0f;
        state.fftIm[0] = 0.0f;
        state.fftRe[(size_t) halfSize] = 0.0f;
        state.fftIm[(size_t) halfSize] = 0.0f;

        fft (true);

        const int64_t outputStart = (int64_t) centre + (int64_t) delaySamples - (int64_t) halfSize;

        for (int i = 0; i < size; ++i)
        {
            const int64_t position = outputStart + (int64_t) i;

            if (position < outputReadPos)
                continue;

            if (position - outputReadPos >= (int64_t) olaRingSize - 8)
                break;

            const float window = state.window[(size_t) i];
            const int index = (int) (position & olaRingMask);
            olaRing[(size_t) channel * (size_t) olaRingSize + (size_t) index]
                += state.fftRe[(size_t) i] * window;

            if (channel == 0)
                windowSum[(size_t) index] += window * window;
        }
    }

    ++processedFrames;
}

void PitchShifter::renderNextBlock (juce::AudioBuffer<float>& wetOutput, int numSamples) noexcept
{
    const int samples = juce::jmax (0, numSamples);
    wetOutput.clear();

    if (samples == 0 || state.size <= 0)
        return;

    const double latestNeededCentre = (double) (outputReadPos + (int64_t) samples)
                                      - (double) delaySamples
                                      + 0.5 * (double) state.size + 1.0;

    int guard = 0;

    while (nextFrameCentre <= latestNeededCentre && guard++ < 20000)
    {
        const int centre = (int) std::llround (nextFrameCentre);

        if ((int64_t) centre + (int64_t) state.size / 2 >= inputWritePos)
            break;

        PitchInfo info;

        if (pitchProvider != nullptr)
            info = pitchProvider->getPitchAt ((double) centre);

        const float ratio = info.voiced
                                ? juce::jlimit (minimumRatio, maximumRatio, info.ratio)
                                : 1.0f;

        processFrame (centre, ratio);
        nextFrameCentre += (double) state.hop;
    }

    const int wetChannels = wetOutput.getNumChannels();

    for (int i = 0; i < samples; ++i)
    {
        const int64_t position = outputReadPos + (int64_t) i;
        const int index = (int) (position & olaRingMask);
        const float normaliser = windowSum[(size_t) index];

        if (normaliser > 1.0e-7f)
        {
            const float inverse = 1.0f / normaliser;

            for (int channel = 0; channel < wetChannels; ++channel)
            {
                const int ringChannel = juce::jmin (channel, channels - 1);
                wetOutput.getWritePointer (channel)[i] =
                    olaRing[(size_t) ringChannel * (size_t) olaRingSize + (size_t) index] * inverse;
            }
        }

        for (int channel = 0; channel < channels; ++channel)
            olaRing[(size_t) channel * (size_t) olaRingSize + (size_t) index] = 0.0f;

        windowSum[(size_t) index] = 0.0f;
    }

    outputReadPos += samples;
}

} // namespace vr
