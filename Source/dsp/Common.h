#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <vector>

namespace vr
{

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float dbToGain (float db) noexcept
{
    return std::pow (10.0f, db * 0.05f);
}

inline float gainToDb (float gain) noexcept
{
    return 20.0f * std::log10 (juce::jmax (1.0e-9f, gain));
}

inline float safeDenominator (float value) noexcept
{
    return std::abs (value) < 1.0e-12f ? (value < 0.0f ? -1.0e-12f : 1.0e-12f) : value;
}

/** RBJ cookbook biquad in transposed direct form II. Allocation-free updates. */
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    void reset() noexcept { z1 = z2 = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setCoefficients (float nb0, float nb1, float nb2, float na0, float na1, float na2) noexcept
    {
        const float inv = 1.0f / safeDenominator (na0);
        b0 = nb0 * inv;
        b1 = nb1 * inv;
        b2 = nb2 * inv;
        a1 = na1 * inv;
        a2 = na2 * inv;
    }

    void setLowPass (double sr, double freq, double q = 0.70710678) noexcept
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (10.0, sr * 0.49, freq) / sr;
        const double c = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * juce::jmax (0.05, q));
        const float b0f = (float) ((1.0 - c) * 0.5);
        const float b1f = (float) (1.0 - c);
        const float b2f = b0f;
        const float a0f = (float) (1.0 + alpha);
        const float a1f = (float) (-2.0 * c);
        const float a2f = (float) (1.0 - alpha);
        setCoefficients (b0f, b1f, b2f, a0f, a1f, a2f);
    }

    void setHighPass (double sr, double freq, double q = 0.70710678) noexcept
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (5.0, sr * 0.49, freq) / sr;
        const double c = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * juce::jmax (0.05, q));
        const float b0f = (float) ((1.0 + c) * 0.5);
        const float b1f = (float) (-(1.0 + c));
        const float b2f = b0f;
        const float a0f = (float) (1.0 + alpha);
        const float a1f = (float) (-2.0 * c);
        const float a2f = (float) (1.0 - alpha);
        setCoefficients (b0f, b1f, b2f, a0f, a1f, a2f);
    }

    void setPeak (double sr, double freq, double gainDb, double q) noexcept
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (10.0, sr * 0.49, freq) / sr;
        const double c = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * juce::jmax (0.05, q));
        const double a = std::pow (10.0, gainDb / 40.0);
        const float b0f = (float) (1.0 + alpha * a);
        const float b1f = (float) (-2.0 * c);
        const float b2f = (float) (1.0 - alpha * a);
        const float a0f = (float) (1.0 + alpha / a);
        const float a1f = (float) (-2.0 * c);
        const float a2f = (float) (1.0 - alpha / a);
        setCoefficients (b0f, b1f, b2f, a0f, a1f, a2f);
    }

    void setLowShelf (double sr, double freq, double gainDb, double s = 1.0) noexcept
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (10.0, sr * 0.49, freq) / sr;
        const double c = std::cos (w0);
        const double sn = std::sin (w0);
        const double a = std::pow (10.0, gainDb / 40.0);
        const double alpha = sn * 0.5 * std::sqrt ((a + 1.0 / a) * (1.0 / s - 1.0) + 2.0);
        const double beta = 2.0 * std::sqrt (a) * alpha;

        const float b0f = (float) (a * ((a + 1.0) - (a - 1.0) * c + beta));
        const float b1f = (float) (2.0 * a * ((a - 1.0) - (a + 1.0) * c));
        const float b2f = (float) (a * ((a + 1.0) - (a - 1.0) * c - beta));
        const float a0f = (float) ((a + 1.0) + (a - 1.0) * c + beta);
        const float a1f = (float) (-2.0 * ((a - 1.0) + (a + 1.0) * c));
        const float a2f = (float) ((a + 1.0) + (a - 1.0) * c - beta);
        setCoefficients (b0f, b1f, b2f, a0f, a1f, a2f);
    }

    void setHighShelf (double sr, double freq, double gainDb, double s = 1.0) noexcept
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (10.0, sr * 0.49, freq) / sr;
        const double c = std::cos (w0);
        const double sn = std::sin (w0);
        const double a = std::pow (10.0, gainDb / 40.0);
        const double alpha = sn * 0.5 * std::sqrt ((a + 1.0 / a) * (1.0 / s - 1.0) + 2.0);
        const double beta = 2.0 * std::sqrt (a) * alpha;

        const float b0f = (float) (a * ((a + 1.0) + (a - 1.0) * c + beta));
        const float b1f = (float) (-2.0 * a * ((a - 1.0) + (a + 1.0) * c));
        const float b2f = (float) (a * ((a + 1.0) + (a - 1.0) * c - beta));
        const float a0f = (float) ((a + 1.0) - (a - 1.0) * c + beta);
        const float a1f = (float) (2.0 * ((a - 1.0) - (a + 1.0) * c));
        const float a2f = (float) ((a + 1.0) - (a - 1.0) * c - beta);
        setCoefficients (b0f, b1f, b2f, a0f, a1f, a2f);
    }

    void setBandPass (double sr, double freq, double q) noexcept
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * juce::jlimit (10.0, sr * 0.49, freq) / sr;
        const double c = std::cos (w0);
        const double alpha = std::sin (w0) / (2.0 * juce::jmax (0.05, q));
        const float b0f = (float) alpha;
        const float b1f = 0.0f;
        const float b2f = (float) -alpha;
        const float a0f = (float) (1.0 + alpha);
        const float a1f = (float) (-2.0 * c);
        const float a2f = (float) (1.0 - alpha);
        setCoefficients (b0f, b1f, b2f, a0f, a1f, a2f);
    }
};

struct OnePoleLowPass
{
    float coefficient = 0.1f;
    float state = 0.0f;

    void setCutoff (double sr, double freq) noexcept
    {
        const double f = juce::jlimit (1.0, sr * 0.49, freq);
        coefficient = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * f / sr));
    }

    void reset() noexcept { state = 0.0f; }

    inline float process (float x) noexcept
    {
        state += coefficient * (x - state);
        return state;
    }
};

struct OnePoleHighPass
{
    OnePoleLowPass low;
    float lastInput = 0.0f;

    void setCutoff (double sr, double freq) noexcept { low.setCutoff (sr, freq); }
    void reset() noexcept { low.reset(); lastInput = 0.0f; }

    inline float process (float x) noexcept
    {
        const float y = x - low.process (x);
        lastInput = x;
        return y;
    }
};

/** Exponential peak/RMS envelope with independent attack and release. */
class EnvelopeFollower
{
public:
    void prepare (double sampleRate, float attackMs, float releaseMs) noexcept
    {
        sr = sampleRate > 1000.0 ? sampleRate : 48000.0;
        setTimes (attackMs, releaseMs);
        reset();
    }

    void setTimes (float attackMs, float releaseMs) noexcept
    {
        attackCoefficient = coefficientFor (attackMs);
        releaseCoefficient = coefficientFor (releaseMs);
    }

    void reset() noexcept { envelope = 0.0f; }

    inline float process (float x) noexcept
    {
        const float target = std::abs (x);
        const float coefficient = target > envelope ? attackCoefficient : releaseCoefficient;
        envelope += coefficient * (target - envelope);
        return envelope;
    }

    float get() const noexcept { return envelope; }

private:
    float coefficientFor (float ms) const noexcept
    {
        const double samples = juce::jmax (1.0, (double) ms * 0.001 * sr);
        return (float) (1.0 - std::exp (-1.0 / samples));
    }

    double sr = 48000.0;
    float attackCoefficient = 0.1f;
    float releaseCoefficient = 0.001f;
    float envelope = 0.0f;
};

/** Interpolating mono delay line. */
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        size = juce::jmax (4, maxSamples + 4);
        buffer.assign ((size_t) size, 0.0f);
        mask = 1;
        while (mask < size)
            mask <<= 1;
        buffer.assign ((size_t) mask, 0.0f);
        writeIndex = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
    }

    inline void write (float x) noexcept
    {
        buffer[(size_t) writeIndex] = x;
        writeIndex = (writeIndex + 1) & (mask - 1);
    }

    inline float read (float delaySamples) const noexcept
    {
        const float clamped = juce::jlimit (0.0f, (float) (mask - 3), delaySamples);
        const float readPosition = (float) writeIndex - clamped;
        int i0 = (int) std::floor (readPosition);
        const float fraction = readPosition - (float) i0;
        int i1 = i0 + 1;
        i0 &= mask - 1;
        i1 &= mask - 1;
        return buffer[(size_t) i0] + fraction * (buffer[(size_t) i1] - buffer[(size_t) i0]);
    }

    int getSize() const noexcept { return mask; }

private:
    std::vector<float> buffer;
    int mask = 1;
    int size = 1;
    int writeIndex = 0;
};

/** Fixed-point free, RT-safe smooth random source used for humanize drift. */
class SmoothRandom
{
public:
    void prepare (double sampleRate, float rateHz) noexcept
    {
        const double samples = juce::jmax (1.0, sampleRate / juce::jmax (0.01, (double) rateHz));
        coefficient = (float) (1.0 - std::exp (-1.0 / samples));
    }

    void reset (float seed = 0.37f) noexcept
    {
        state = seed;
        value = 0.0f;
    }

    inline float process() noexcept
    {
        // Cheap deterministic pseudo-noise, heavily smoothed.
        state = state * 1.6180339887f + 0.12345f;
        state -= std::floor (state);
        const float target = state * 2.0f - 1.0f;
        value += coefficient * (target - value);
        return value;
    }

private:
    float coefficient = 0.0001f;
    float state = 0.37f;
    float value = 0.0f;
};

} // namespace vr
