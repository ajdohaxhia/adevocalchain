#include "SpectralDenoiser.h"

#include "Common.h"

#include <algorithm>
#include <cmath>

namespace vr
{

void SpectralDenoiser::prepare (double newSampleRate, int numChannels, int maximumBlockSize, int newFrameSize)
{
    sampleRate = newSampleRate > 1000.0 ? newSampleRate : 48000.0;
    channels = juce::jlimit (1, 2, numChannels);
    maximumBlock = juce::jlimit (16, 8192, maximumBlockSize);

    frameSize = newFrameSize;
    while ((frameSize & (frameSize - 1)) != 0)
        ++frameSize;

    hopSize = juce::jmax (32, frameSize / 4);
    numBins = frameSize / 2 + 1;

    int order = 0;
    while ((1 << order) < frameSize)
        ++order;

    fft = std::make_unique<juce::dsp::FFT> (order);

    window.resize ((size_t) frameSize);
    windowSquared.resize ((size_t) frameSize);
    const double twoPi = juce::MathConstants<double>::twoPi;

    for (int i = 0; i < frameSize; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (twoPi * (double) i / (double) frameSize);
        window[(size_t) i] = (float) w;
        windowSquared[(size_t) i] = (float) (w * w);
    }

    inputRing.assign ((size_t) inputRingSize * (size_t) channels, 0.0f);
    olaRing.assign ((size_t) olaRingSize * (size_t) channels, 0.0f);
    windowSum.assign ((size_t) olaRingSize, 0.0f);

    timeL.assign ((size_t) frameSize, {});
    timeR.assign ((size_t) frameSize, {});
    specL.assign ((size_t) frameSize, {});
    specR.assign ((size_t) frameSize, {});
    noiseEstimate.assign ((size_t) numBins, 0.0f);
    smoothedGain.assign ((size_t) numBins, 1.0f);
    outputGain.assign ((size_t) numBins, 1.0f);

    prepared = true;
    setParameters (35.0f, 12.0f);
    reset();
}

void SpectralDenoiser::reset()
{
    std::fill (inputRing.begin(), inputRing.end(), 0.0f);
    std::fill (olaRing.begin(), olaRing.end(), 0.0f);
    std::fill (windowSum.begin(), windowSum.end(), 0.0f);
    std::fill (noiseEstimate.begin(), noiseEstimate.end(), 0.0f);
    std::fill (smoothedGain.begin(), smoothedGain.end(), 1.0f);
    std::fill (outputGain.begin(), outputGain.end(), 1.0f);

    inputWritePos = 0;
    outputReadPos = 0;
    nextFrameCentre = -(double) frameSize;
}

void SpectralDenoiser::setParameters (float amountPercent, float maxReductionDb) noexcept
{
    amount = juce::jlimit (0.0f, 1.0f, amountPercent * 0.01f);
    const float reduction = juce::jlimit (0.0f, 30.0f, maxReductionDb);
    gainFloor = dbToGain (-reduction);
}

void SpectralDenoiser::pushInput (const juce::AudioBuffer<float>& input, int startSample, int numSamples) noexcept
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

void SpectralDenoiser::renderNextBlock (juce::AudioBuffer<float>& wetOutput, int numSamples) noexcept
{
    if (! prepared)
        return;

    const int samples = juce::jmax (0, numSamples);
    wetOutput.clear();

    if (samples <= 0)
        return;

    const double latestNeededCentre = (double) (outputReadPos + (int64_t) samples)
                                      - (double) frameSize + 0.5 * (double) frameSize + 1.0;

    int guard = 0;

    while (nextFrameCentre <= latestNeededCentre && guard++ < 4096)
    {
        const int centre = (int) std::llround (nextFrameCentre);

        if ((int64_t) centre + (int64_t) frameSize / 2 >= inputWritePos)
            break;

        processFrame (centre);
        nextFrameCentre += (double) hopSize;
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

void SpectralDenoiser::processFrame (int centre) noexcept
{
    const int half = frameSize / 2;
    const int64_t frameStart = (int64_t) centre - (int64_t) half;

    for (int i = 0; i < frameSize; ++i)
    {
        const int64_t position = frameStart + (int64_t) i;
        const int index = (int) (position & inputRingMask);

        timeL[(size_t) i] = { inputRing[(size_t) index] * window[(size_t) i], 0.0f };

        if (channels > 1)
            timeR[(size_t) i] = { inputRing[(size_t) inputRingSize + (size_t) index] * window[(size_t) i], 0.0f };
        else
            timeR[(size_t) i] = timeL[(size_t) i];
    }

    fftForward (timeL.data(), specL.data());
    fftForward (timeR.data(), specR.data());

    for (int k = 0; k < numBins; ++k)
    {
        const float magL = std::abs (specL[(size_t) k]);
        const float magR = std::abs (specR[(size_t) k]);
        const float mag = 0.5f * (magL + magR);

        float& noise = noiseEstimate[(size_t) k];

        if (mag < noise)
            noise += 0.28f * (mag - noise);
        else
            noise += 0.00035f * (mag - noise);

        // Minimum statistics tracks the low percentile of a noisy signal;
        // apply a bias to recover the mean broadband noise power.
        const float denominator = mag + 1.0e-9f;
        float gain = 1.0f - amount * (noise * 3.0f / denominator);
        gain = juce::jlimit (gainFloor, 1.0f, gain);

        const float previous = smoothedGain[(size_t) k];

        if (gain < previous)
            smoothedGain[(size_t) k] = gain + 0.45f * (previous - gain);  // fast reduction
        else
            smoothedGain[(size_t) k] = gain + 0.94f * (previous - gain);  // slow release
    }

    for (int k = 0; k < numBins; ++k)
    {
        const int left = juce::jmax (0, k - 1);
        const int right = juce::jmin (numBins - 1, k + 1);
        outputGain[(size_t) k] = 0.25f * smoothedGain[(size_t) left]
                                 + 0.5f * smoothedGain[(size_t) k]
                                 + 0.25f * smoothedGain[(size_t) right];
    }

    for (int k = 1; k < half; ++k)
    {
        const float g = outputGain[(size_t) k];
        specL[(size_t) k] *= g;
        specR[(size_t) k] *= g;
        specL[(size_t) (frameSize - k)] *= g;
        specR[(size_t) (frameSize - k)] *= g;
    }

    specL[0] *= outputGain[0];
    specR[0] *= outputGain[0];
    specL[(size_t) half] *= outputGain[(size_t) half];
    specR[(size_t) half] *= outputGain[(size_t) half];

    fftInverse (specL.data(), timeL.data());
    fftInverse (specR.data(), timeR.data());

    const int64_t outputStart = (int64_t) centre + (int64_t) frameSize - (int64_t) half;

    for (int i = 0; i < frameSize; ++i)
    {
        const int64_t position = outputStart + (int64_t) i;

        if (position < outputReadPos)
            continue;

        if (position - outputReadPos >= (int64_t) olaRingSize - 8)
            break;

        const int index = (int) (position & olaRingMask);
        const float w = window[(size_t) i];

        olaRing[(size_t) index] += timeL[(size_t) i].real() * w;

        if (channels > 1)
            olaRing[(size_t) olaRingSize + (size_t) index] += timeR[(size_t) i].real() * w;

        windowSum[(size_t) index] += windowSquared[(size_t) i];
    }
}

void SpectralDenoiser::fftForward (const std::complex<float>* time, std::complex<float>* spectrum) noexcept
{
    fft->perform (time, spectrum, false);
}

void SpectralDenoiser::fftInverse (const std::complex<float>* spectrum, std::complex<float>* time) noexcept
{
    fft->perform (spectrum, time, true);
}

} // namespace vr
