#include "PitchDetector.h"

#include "Common.h"

#include <algorithm>
#include <cmath>

namespace vr
{

void PitchDetector::prepare (double sampleRate, QualityMode quality)
{
    this->sampleRate = sampleRate > 1000.0 ? sampleRate : 48000.0;
    this->quality = quality;

    decimation = juce::jmax (1, (int) std::lround (this->sampleRate / 16000.0));
    detectorRate = this->sampleRate / (double) decimation;
    // Track uses a 512-sample window at ~15 kHz (about 32 ms) so low rap
    // fundamentals around 80 Hz have more than two periods to analyse.  The
    // detector reads history that is already inside the shifter's lookahead,
    // so this does not increase the reported plugin latency.  Mix uses 768.
    windowSize = quality == QualityMode::track ? 512 : 768;
    windowSize = juce::jmin (windowSize, kMaxWindow);

    int ringSize = 1;
    while (ringSize < 65536)
        ringSize <<= 1;

    ring.assign ((size_t) ringSize, 0.0f);
    ringMask = ringSize - 1;
    window.assign ((size_t) windowSize, 0.0f);
    difference.assign ((size_t) windowSize + 2, 0.0);
    cmnd.assign ((size_t) windowSize + 2, 0.0);

    lowpassA.reset();
    lowpassB.reset();
    updateLowpass();

    setRange (rangeIndex);
    reset();
}

void PitchDetector::updateLowpass() noexcept
{
    // Keep the fundamental and the first harmonics, reject most sibilance.
    const double cutoff = juce::jmin (3600.0, detectorRate * 0.44);
    lowpassA.setLowPass (sampleRate, cutoff, 0.70710678);
    lowpassB.setLowPass (sampleRate, cutoff, 0.70710678);
}

void PitchDetector::reset() noexcept
{
    std::fill (ring.begin(), ring.end(), 0.0f);
    lowpassA.reset();
    lowpassB.reset();
    decimationCounter = 0;
    totalDecimated = 0;
    latest = {};
    cached = {};
    cacheValid = false;
    lastStart = -1000000;
}

void PitchDetector::setRange (int newRange) noexcept
{
    rangeIndex = juce::jlimit (0, 2, newRange);

    double minHz = 65.0;
    double maxHz = 1100.0;

    if (rangeIndex == 0)
    {
        minHz = 65.0;
        maxHz = 350.0;
    }
    else if (rangeIndex == 2)
    {
        minHz = 150.0;
        maxHz = 1500.0;
    }

    tauMin = juce::jmax (2, (int) std::floor (detectorRate / maxHz));
    tauMax = juce::jmin (windowSize - 32, (int) std::ceil (detectorRate / minHz));
    tauMin = juce::jmin (tauMin, tauMax - 2);
    cacheValid = false;
}

void PitchDetector::process (const float* mono, int numSamples) noexcept
{
    if (mono == nullptr || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const float filtered = lowpassB.process (lowpassA.process (mono[i]));

        if (++decimationCounter >= decimation)
        {
            decimationCounter = 0;
            ring[(size_t) ((int) (totalDecimated & ringMask))] = filtered;
            ++totalDecimated;
        }
    }
}

PitchEstimate PitchDetector::analyseAt (double centreFullRateSamples) noexcept
{
    PitchEstimate invalid;
    invalid.timestampSamples = centreFullRateSamples;

    if (totalDecimated < (int64_t) windowSize)
        return invalid;

    const int64_t centreDecimated = (int64_t) std::llround (centreFullRateSamples / (double) decimation);
    const int64_t newest = (int64_t) totalDecimated - 1;

    int64_t end = centreDecimated + windowSize / 2;
    end = juce::jmin (end, newest);
    const int64_t start = end - (int64_t) windowSize;

    if (start < 0)
        return invalid;

    const int64_t stride = quality == QualityMode::track ? 64 : 128;

    if (cacheValid && std::llabs (start - lastStart) < stride)
        return cached;

    for (int i = 0; i < windowSize; ++i)
    {
        const int64_t index = start + i;
        window[(size_t) i] = ring[(size_t) ((int) (index & ringMask))];
    }

    lastStart = start;
    cached = runYin();
    cached.timestampSamples = (double) (start + (int64_t) windowSize / 2) * (double) decimation;
    cacheValid = true;
    latest = cached;
    return cached;
}

PitchEstimate PitchDetector::runYin() noexcept
{
    PitchEstimate result;

    double energy = 0.0;
    int zeroCrossings = 0;
    float previous = window[0];

    for (int i = 0; i < windowSize; ++i)
    {
        const float x = window[(size_t) i];
        energy += (double) x * (double) x;

        if (i > 0 && ((x >= 0.0f) != (previous >= 0.0f)))
            ++zeroCrossings;

        previous = x;
    }

    const double rms = std::sqrt (energy / (double) windowSize);
    result.rms = (float) rms;

    if (rms < 0.0012)
        return result;

    const double zcr = (double) zeroCrossings / (double) windowSize;

    // Sibilants and broadband noise have a high zero-crossing rate.  Keep them
    // out of the voiced path so the shifter does not chase them.
    if (zcr > 0.34)
        return result;

    const int maxTau = juce::jmin (tauMax, windowSize - 8);

    for (int tau = 0; tau <= maxTau; ++tau)
    {
        double sum = 0.0;

        for (int i = 0; i < windowSize - tau; ++i)
        {
            const double delta = (double) window[(size_t) i] - (double) window[(size_t) (i + tau)];
            sum += delta * delta;
        }

        difference[(size_t) tau] = sum;
    }

    cmnd[0] = 1.0;
    double runningSum = 0.0;

    for (int tau = 1; tau <= maxTau; ++tau)
    {
        runningSum += difference[(size_t) tau];

        if (runningSum <= 1.0e-12)
            cmnd[(size_t) tau] = 1.0;
        else
            cmnd[(size_t) tau] = difference[(size_t) tau] * (double) tau / runningSum;
    }

    int bestTau = -1;
    constexpr double threshold = 0.15;

    for (int tau = tauMin; tau <= maxTau; ++tau)
    {
        if (cmnd[(size_t) tau] < threshold)
        {
            while (tau + 1 <= maxTau && cmnd[(size_t) (tau + 1)] < cmnd[(size_t) tau])
                ++tau;

            bestTau = tau;
            break;
        }
    }

    if (bestTau < 0)
    {
        double bestValue = 1.0;

        for (int tau = tauMin; tau <= maxTau; ++tau)
        {
            if (cmnd[(size_t) tau] < bestValue)
            {
                bestValue = cmnd[(size_t) tau];
                bestTau = tau;
            }
        }

        if (bestTau < 0 || bestValue > 0.30)
            return result;
    }

    const double bestValue = cmnd[(size_t) bestTau];

    if (bestValue > 0.32)
        return result;

    double interpolatedTau = (double) bestTau;

    if (bestTau > tauMin && bestTau < maxTau)
    {
        const double s0 = cmnd[(size_t) (bestTau - 1)];
        const double s1 = cmnd[(size_t) bestTau];
        const double s2 = cmnd[(size_t) (bestTau + 1)];
        const double denominator = 2.0 * (s0 - 2.0 * s1 + s2);

        if (std::abs (denominator) > 1.0e-12)
        {
            const double offset = (s0 - s2) / denominator;

            if (std::abs (offset) < 1.0)
                interpolatedTau = (double) bestTau + offset;
        }
    }

    if (interpolatedTau <= 0.0)
        return result;

    const double frequency = detectorRate / interpolatedTau;

    const double minHz = rangeIndex == 2 ? 150.0 : (rangeIndex == 0 ? 65.0 : 65.0);
    const double maxHz = rangeIndex == 0 ? 350.0 : (rangeIndex == 2 ? 1500.0 : 1100.0);

    if (frequency < minHz * 0.98 || frequency > maxHz * 1.02)
        return result;

    const int note = (int) std::lround (69.0 + 12.0 * std::log2 (frequency / 440.0));
    juce::ignoreUnused (note);

    result.frequency = (float) frequency;
    result.confidence = (float) juce::jlimit (0.0, 1.0, 1.0 - bestValue);
    result.voiced = result.confidence > 0.66f;
    return result;
}

} // namespace vr
