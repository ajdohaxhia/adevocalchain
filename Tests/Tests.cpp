#include "Engine.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "Presets.h"
#include "presets/FactoryPresetsData.h"
#include "dsp/Modules.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <set>
#include <vector>

namespace
{

using vr::Engine;
using vr::QualityMode;
using vr::RackParams;
using vr::TransportInfo;

int failures = 0;

void check (bool condition, const juce::String& message)
{
    std::cout << (condition ? "PASS  " : "FAIL  ") << message << std::endl;

    if (! condition)
        ++failures;
}

double gainFromDb (double db)
{
    return std::pow (10.0, db / 20.0);
}

double rms (const std::vector<float>& samples, int start = 0, int count = -1)
{
    if (count < 0)
        count = (int) samples.size() - start;

    if (count <= 0)
        return 0.0;

    double sum = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const double v = samples[(size_t) (start + i)];
        sum += v * v;
    }

    return std::sqrt (sum / (double) count);
}

double peak (const std::vector<float>& samples)
{
    double value = 0.0;

    for (float sample : samples)
        value = juce::jmax (value, (double) std::abs (sample));

    return value;
}

bool allFinite (const std::vector<float>& samples)
{
    for (float sample : samples)
        if (! std::isfinite (sample))
            return false;

    return true;
}

std::vector<float> voice (double sampleRate, double seconds, double hz,
                          double amplitude = 0.25, double vibratoCents = 0.0,
                          double vibratoHz = 5.0)
{
    const int count = (int) std::lround (sampleRate * seconds);
    std::vector<float> output ((size_t) count, 0.0f);
    double phase = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const double t = (double) i / sampleRate;
        const double cents = vibratoCents * std::sin (2.0 * juce::MathConstants<double>::pi * vibratoHz * t);
        const double f = hz * std::pow (2.0, cents / 1200.0);
        phase += 2.0 * juce::MathConstants<double>::pi * f / sampleRate;

        double sample = 0.0;

        for (int harmonic = 1; harmonic <= 6; ++harmonic)
            sample += std::sin (phase * harmonic) / (double) harmonic;

        sample *= amplitude / 1.8;

        const double fade = std::min ({ 1.0, t / 0.015, (seconds - t) / 0.015 });
        output[(size_t) i] = (float) (sample * std::max (0.0, fade));
    }

    return output;
}

std::vector<float> noise (double sampleRate, double seconds, double amplitude, int seed = 7)
{
    const int count = (int) std::lround (sampleRate * seconds);
    std::vector<float> output ((size_t) count, 0.0f);
    std::mt19937 generator ((unsigned) seed);
    std::normal_distribution<double> distribution (0.0, 1.0);

    for (int i = 0; i < count; ++i)
        output[(size_t) i] = (float) (distribution (generator) * amplitude);

    return output;
}

struct EngineOutput
{
    std::vector<float> left;
    std::vector<float> right;
    int latency = 0;
};

EngineOutput runEngine (const RackParams& params, const std::vector<float>& left,
                        const std::vector<float>& right, double sampleRate, int blockSize,
                        QualityMode quality, int inputChannels = 2, int outputChannels = 2,
                        TransportInfo transport = {})
{
    Engine engine;
    engine.prepare (sampleRate, blockSize, inputChannels, outputChannels, quality);

    EngineOutput output;
    output.latency = engine.getLatencySamples();
    output.left.assign (left.size(), 0.0f);
    output.right.assign (left.size(), 0.0f);

    int position = 0;

    while (position < (int) left.size())
    {
        const int count = juce::jmin (blockSize, (int) left.size() - position);
        juce::AudioBuffer<float> buffer (juce::jmax (inputChannels, outputChannels), count);

        for (int channel = 0; channel < inputChannels; ++channel)
        {
            const auto& source = channel == 0 ? left : right;
            std::copy (source.begin() + position, source.begin() + position + count,
                       buffer.getWritePointer (channel));
        }

        engine.process (buffer, params, transport, inputChannels, outputChannels);

        for (int i = 0; i < count; ++i)
        {
            output.left[(size_t) (position + i)] = buffer.getReadPointer (0)[i];
            output.right[(size_t) (position + i)] = buffer.getNumChannels() > 1
                                                       ? buffer.getReadPointer (1)[i]
                                                       : output.left[(size_t) (position + i)];
        }

        position += count;
    }

    return output;
}

/** Independent YIN estimator used only by the tests. */
double independentPitch (const std::vector<float>& samples, double sampleRate, int start, int window)
{
    if (start < 0 || start + window > (int) samples.size() || window < 512)
        return 0.0;

    std::vector<double> x ((size_t) window);
    double mean = 0.0;

    for (int i = 0; i < window; ++i)
    {
        x[(size_t) i] = samples[(size_t) (start + i)];
        mean += x[(size_t) i];
    }

    mean /= (double) window;

    for (auto& value : x)
        value -= mean;

    const int minTau = juce::jmax (8, (int) std::floor (sampleRate / 1600.0));
    const int maxTau = juce::jmin (window / 2, (int) std::ceil (sampleRate / 65.0));
    std::vector<double> difference ((size_t) maxTau + 1, 0.0);

    for (int tau = 0; tau <= maxTau; ++tau)
    {
        double sum = 0.0;

        for (int i = 0; i < window - tau; ++i)
        {
            const double delta = x[(size_t) i] - x[(size_t) (i + tau)];
            sum += delta * delta;
        }

        difference[(size_t) tau] = sum;
    }

    std::vector<double> cmnd ((size_t) maxTau + 1, 1.0);
    double running = 0.0;

    for (int tau = 1; tau <= maxTau; ++tau)
    {
        running += difference[(size_t) tau];
        cmnd[(size_t) tau] = running > 1.0e-12
                                 ? difference[(size_t) tau] * (double) tau / running
                                 : 1.0;
    }

    int best = -1;
    double bestValue = 1.0;

    for (int tau = minTau; tau <= maxTau; ++tau)
    {
        if (cmnd[(size_t) tau] < 0.18)
        {
            while (tau + 1 <= maxTau && cmnd[(size_t) (tau + 1)] < cmnd[(size_t) tau])
                ++tau;

            best = tau;
            bestValue = cmnd[(size_t) tau];
            break;
        }

        if (cmnd[(size_t) tau] < bestValue)
        {
            bestValue = cmnd[(size_t) tau];
            best = tau;
        }
    }

    if (best < 0 || bestValue > 0.35)
        return 0.0;

    double tau = (double) best;

    if (best > minTau && best < maxTau)
    {
        const double a = cmnd[(size_t) (best - 1)];
        const double b = cmnd[(size_t) best];
        const double c = cmnd[(size_t) (best + 1)];
        const double denominator = 2.0 * (a - 2.0 * b + c);

        if (std::abs (denominator) > 1.0e-12)
            tau += juce::jlimit (-1.0, 1.0, (a - c) / denominator);
    }

    return sampleRate / tau;
}

double centsBetween (double hz, double reference)
{
    return 1200.0 * std::log2 (hz / reference);
}

std::vector<double> pitchTrack (const std::vector<float>& samples, double sampleRate,
                                int start, int end, int window = 4096)
{
    std::vector<double> values;
    const int step = window / 2;

    for (int position = start; position + window <= end; position += step)
    {
        const double hz = independentPitch (samples, sampleRate, position, window);

        if (hz > 20.0)
            values.push_back (hz);
    }

    return values;
}

RackParams bypassAllParams()
{
    RackParams p;
    p.expBypass = true;
    p.dnBypass = true;
    p.pitchBypass = true;
    p.ceqBypass = true;
    p.teqBypass = true;
    p.compBypass = true;
    p.dsBypass = true;
    p.satBypass = true;
    p.dlyBypass = true;
    p.revBypass = true;
    p.limBypass = true;
    p.mix = 1.0f;
    return p;
}

//==============================================================================
void testParameterSchema()
{
    const auto& specs = vr::getParamSpecs();
    check (specs.size() == 103, "parameter schema has 103 parameters");

    std::set<juce::String> ids;

    for (const auto& spec : specs)
    {
        ids.insert (spec.id);
        check (spec.defaultValue >= spec.minValue - 1.0e-4f
                   && spec.defaultValue <= spec.maxValue + 1.0e-4f,
               juce::String ("default in range: ") + spec.id);
        check (vr::findParamSpec (spec.id) != nullptr, juce::String ("find spec: ") + spec.id);
    }

    check ((int) ids.size() == (int) specs.size(), "parameter ids are unique");

    vr::PitchSelectionSettings settings;
    settings.scaleIndex = 3;
    settings.mask = 1 << 9;
    settings.amount = 1.0;
    settings.humanize = 0.0;
    vr::PitchSelector selector;
    selector.setSettings (settings);
    check (selector.nearestAllowedMidi (57.0, -1) == 57, "MIDI 57 is A with A enabled");
    check (selector.nearestAllowedMidi (59.0, -1) == 57, "B excluded: 59 snaps to A when only A is enabled");

    settings.mask = (1 << 9) | (1 << 0);
    selector.setSettings (settings);
    check (selector.nearestAllowedMidi (59.0, -1) == 60, "B excluded: 59 snaps to C when A and C are enabled");
    check (selector.nearestAllowedMidi (59.5, -1) == 59 || selector.nearestAllowedMidi (59.5, -1) == 60,
           "B/C boundary tie is deterministic");
    check (selector.nearestAllowedMidi (59.6, -1) == 60, "B/C octave boundary selects C above 59.5");
    check (selector.nearestAllowedMidi (-3.0, -1) == -3, "negative MIDI index uses positive modulo");
    check (selector.nearestAllowedMidi (-3.0, -1) != -15, "negative octave does not invent a target");

    settings.mask = 0;
    selector.setSettings (settings);
    const auto emptySelection = selector.selectMidi (60.0, -1);
    check (selector.nearestAllowedMidi (60.0, -1) == -1, "empty mask yields no target");
    check (! emptySelection.valid && std::abs (emptySelection.ratio - 1.0) < 1.0e-9,
           "empty mask applies unity ratio");

    settings.mask = 0x0FFF;
    settings.scaleIndex = 1;
    settings.tonic = 0;
    selector.setSettings (settings);
    const int majorC = selector.getEffectiveMask();
    check ((majorC & 1) != 0 && (majorC & (1 << 1)) == 0 && (majorC & (1 << 2)) != 0,
           "C major mask includes C/E and excludes C#");
}

void testLatencyAndRates()
{
    for (auto quality : { vr::QualityMode::track, vr::QualityMode::mix })
    {
        vr::Engine engine;
        engine.prepare (48000.0, 512, 2, 2, quality);
        const int latency = engine.getLatencySamples();
        const juce::String label = quality == vr::QualityMode::track ? "Track" : "Mix";
        check (latency > 0, label + " reports positive latency");
        check (quality == vr::QualityMode::mix || latency <= 720,
               "Track reports <= 720 samples at 48 kHz (15 ms)");

        vr::RackParams params = bypassAllParams();
        params.bypass = true;
        params.limBypass = false;

        std::vector<float> input ((size_t) latency + 4096, 0.0f);
        input[0] = 0.5f;
        std::vector<float> right = input;

        int position = 0;
        std::vector<float> output (input.size(), 0.0f);
        std::vector<float> outputRight = input;

        while (position < (int) input.size())
        {
            const int count = juce::jmin (512, (int) input.size() - position);
            juce::AudioBuffer<float> buffer (2, count);
            std::copy (input.begin() + position, input.begin() + position + count,
                       buffer.getWritePointer (0));
            std::copy (right.begin() + position, right.begin() + position + count,
                       buffer.getWritePointer (1));
            engine.process (buffer, params, {}, 2, 2);

            for (int i = 0; i < count; ++i)
            {
                output[(size_t) (position + i)] = buffer.getReadPointer (0)[i];
                outputRight[(size_t) (position + i)] = buffer.getReadPointer (1)[i];
            }

            position += count;
        }

        int peakIndex = -1;
        double peakValue = 0.0;

        for (int i = 0; i < (int) output.size(); ++i)
        {
            if (std::abs (output[(size_t) i]) > peakValue)
            {
                peakValue = std::abs (output[(size_t) i]);
                peakIndex = i;
            }
        }

        check (peakIndex == latency, label + " bypass impulse lands at reported latency ("
                                         + juce::String (peakIndex) + " vs "
                                         + juce::String (latency) + ")");
        check (std::abs (peakValue - 0.5) < 0.02, label + " bypass preserves impulse amplitude");
        check (std::abs (outputRight[(size_t) latency] - 0.5f) < 0.02f,
               label + " bypass preserves the right channel");
    }

    for (double sampleRate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        for (auto quality : { vr::QualityMode::track, vr::QualityMode::mix })
        {
            vr::RackParams params;
            params.pitchBypass = false;
            params.pitchAmount = 60.0f;
            params.ceqBypass = false;
            params.compBypass = false;
            params.dnBypass = false;
            params.dnAmount = 30.0f;
            params.dsBypass = false;
            params.dlyBypass = false;
            params.revBypass = false;

            const auto left = voice (sampleRate, 0.5, 220.0, 0.28, 20.0);
            const auto right = voice (sampleRate, 0.5, 221.0, 0.28, -15.0);
            const auto output = runEngine (params, left, right, sampleRate, 512, quality);

            check (output.left.size() == left.size(), "sample-rate render keeps length");
            check (allFinite (output.left) && allFinite (output.right),
                   "finite output at " + juce::String (sampleRate) + " Hz");
            check (peak (output.left) < 8.0 && peak (output.right) < 8.0,
                   "bounded output at " + juce::String (sampleRate) + " Hz");

            vr::Engine engine;
            engine.prepare (sampleRate, 256, 2, 2, quality);
            juce::AudioBuffer<float> empty (2, 0);
            engine.process (empty, params, {}, 2, 2);
            engine.reset();
            engine.prepare (sampleRate, 256, 2, 2, quality);
            juce::AudioBuffer<float> large (2, 1024);
            large.clear();
            engine.process (large, params, {}, 2, 2);
            check (allFinite (std::vector<float> (large.getReadPointer (0),
                                                  large.getReadPointer (0) + 1024)),
                   "zero-length + oversized block is safe at " + juce::String (sampleRate) + " Hz");
        }
    }

    for (int blockSize : { 37, 127, 1024 })
    {
        vr::RackParams params;
        params.pitchBypass = false;
        params.dnBypass = false;
        const auto left = voice (48000.0, 0.35, 260.0, 0.25);
        const auto output = runEngine (params, left, left, 48000.0, blockSize,
                                       vr::QualityMode::track);
        check (allFinite (output.left), "odd/varying block size " + juce::String (blockSize) + " is finite");
    }

    for (auto layout : { std::pair<int, int> { 1, 1 }, { 1, 2 }, { 2, 2 } })
    {
        vr::RackParams params = bypassAllParams();
        params.pitchBypass = false;
        params.pitchAmount = 50.0f;
        const auto left = voice (48000.0, 0.3, 330.0, 0.25);
        const auto right = voice (48000.0, 0.3, 331.0, 0.25);
        const auto output = runEngine (params, left, right, 48000.0, 256,
                                       vr::QualityMode::track, layout.first, layout.second);
        check (allFinite (output.left) && allFinite (output.right),
               "bus layout " + juce::String (layout.first) + "->" + juce::String (layout.second) + " is finite");
    }
}

void testDetector()
{
    for (auto quality : { vr::QualityMode::track, vr::QualityMode::mix })
    {
        for (double frequency : { 80.0, 130.0, 220.0, 440.0, 880.0 })
        {
            vr::PitchDetector detector;
            detector.prepare (48000.0, quality);
            detector.setRange (1);
            const auto signal = voice (48000.0, 1.0, frequency, 0.3);
            detector.process (signal.data(), (int) signal.size());
            const auto estimate = detector.analyseAt (47999.0);
            const double cents = estimate.voiced ? centsBetween (estimate.frequency, frequency) : 9999.0;
            check (estimate.voiced && std::abs (cents) < 25.0,
                   juce::String (quality == vr::QualityMode::track ? "Track" : "Mix")
                       + " detector " + juce::String (frequency, 1) + " Hz, error "
                       + juce::String (cents, 1) + " cents");
        }

        vr::PitchDetector detector;
        detector.prepare (48000.0, quality);
        detector.setRange (1);
        std::vector<float> silence (48000, 0.0f);
        detector.process (silence.data(), (int) silence.size());
        check (! detector.analyseAt (47999.0).voiced, "silence is unvoiced");
    }
}

void testRenderedPitch()
{
    vr::RackParams params = bypassAllParams();
    params.pitchBypass = false;
    params.pitchAmount = 100.0f;
    params.pitchRetune = 1.0f;
    params.pitchHumanize = 0.0f;
    params.pitchScale = 0;
    params.pitchMask = 0x0FFF;
    params.limBypass = true;

    const double sampleRate = 48000.0;
    const double inputHz = 452.1;
    const auto input = voice (sampleRate, 3.0, inputHz, 0.25);
    const auto output = runEngine (params, input, input, sampleRate, 512, vr::QualityMode::track);

    const int start = output.latency + (int) (1.2 * sampleRate);
    const auto track = pitchTrack (output.left, sampleRate, start, (int) output.left.size());
    check (! track.empty(), "rendered tuning produced pitch estimates");

    if (! track.empty())
    {
        std::vector<double> errors;

        for (double hz : track)
            errors.push_back (std::abs (centsBetween (hz, 440.0)));

        std::sort (errors.begin(), errors.end());
        const double median = errors[errors.size() / 2];
        check (median <= 10.0, "Hard Tune median pitch error <= 10 cents ("
                                   + juce::String (median, 2) + " cents)");
    }

    vr::RackParams natural = params;
    natural.pitchAmount = 60.0f;
    natural.pitchRetune = 90.0f;
    natural.pitchHumanize = 55.0f;
    const auto vibrato = voice (sampleRate, 3.0, 440.0, 0.25, 35.0, 5.0);
    const auto naturalOutput = runEngine (natural, vibrato, vibrato, sampleRate, 512,
                                          vr::QualityMode::track);
    const auto naturalTrack = pitchTrack (naturalOutput.left, sampleRate,
                                          naturalOutput.latency + (int) (1.2 * sampleRate),
                                          (int) naturalOutput.left.size());

    if (naturalTrack.size() > 4)
    {
        const auto minmax = std::minmax_element (naturalTrack.begin(), naturalTrack.end());
        const double spread = centsBetween (*minmax.second, *minmax.first);
        check (spread > 10.0, "Natural mode leaves measurable vibrato movement ("
                                  + juce::String (spread, 1) + " cents)");
    }
    else
    {
        check (false, "Natural mode produced enough estimates");
    }

    auto mixed = voice (sampleRate, 1.2, 220.0, 0.2);
    const auto tail = noise (sampleRate, 0.6, 0.02, 22);
    mixed.insert (mixed.end(), tail.begin(), tail.end());
    const auto mixedOutput = runEngine (params, mixed, mixed, sampleRate, 512,
                                        vr::QualityMode::track);
    const double tailRms = rms (mixedOutput.left,
                                (int) mixedOutput.left.size() - (int) (0.4 * sampleRate),
                                (int) (0.4 * sampleRate));
    check (tailRms > 0.001, "unvoiced tail survives the shifter");
}

void testModules()
{
    {
        vr::LookaheadLimiter limiter;
        limiter.prepare (48000.0, 512, 128);
        vr::RackParams params;
        params.limBypass = false;
        params.limCeiling = -1.0f;
        params.limRelease = 50.0f;
        juce::AudioBuffer<float> buffer (2, 4800);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float sample = 0.98f * std::sin (2.0f * juce::MathConstants<float>::pi * 997.0f
                                                   * (float) i / 48000.0f);
            buffer.setSample (0, i, sample);
            buffer.setSample (1, i, sample);
        }

        limiter.process (buffer, buffer.getNumSamples(), params);
        double maxSample = 0.0;

        for (int channel = 0; channel < 2; ++channel)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                maxSample = juce::jmax (maxSample, (double) std::abs (buffer.getSample (channel, i)));

        check (maxSample <= gainFromDb (-1.0) + 1.0e-4, "limiter enforces the sample-peak ceiling");
        check (limiter.getLatencySamples() == 128, "limiter reports its lookahead");
        check (limiter.getGainReductionDb() < -0.1f, "limiter reports gain reduction");
    }

    {
        vr::Compressor compressor;
        compressor.prepare (48000.0, 512);
        vr::RackParams params;
        params.compBypass = false;
        params.compThreshold = -30.0f;
        params.compRatio = 4.0f;
        params.compAttack = 5.0f;
        params.compRelease = 100.0f;
        params.compKnee = 6.0f;
        params.compMakeup = 0.0f;
        params.compMix = 100.0f;

        juce::AudioBuffer<float> loud (2, 4800);
        juce::AudioBuffer<float> quiet (2, 4800);

        for (int i = 0; i < 4800; ++i)
        {
            const float value = 0.5f * std::sin (2.0f * juce::MathConstants<float>::pi * 1000.0f
                                                 * (float) i / 48000.0f);
            loud.setSample (0, i, value);
            loud.setSample (1, i, value);
            quiet.setSample (0, i, value * 0.02f);
            quiet.setSample (1, i, value * 0.02f);
        }

        compressor.process (loud, 4800, params);
        const float loudGr = compressor.getGainReductionDb();
        compressor.reset();
        compressor.process (quiet, 4800, params);
        const float quietGr = compressor.getGainReductionDb();
        check (loudGr < -2.0f, "compressor reduces a loud signal");
        check (quietGr > -0.5f, "compressor leaves a quiet signal alone");
    }

    {
        vr::CleanupEQ eq;
        eq.prepare (48000.0, 512);
        vr::RackParams params;
        params.ceqBypass = false;
        params.ceqHpfFreq = 300.0f;
        params.ceqCut1Gain = 0.0f;
        params.ceqCut2Gain = 0.0f;

        auto render = [&] (double hz)
        {
            juce::AudioBuffer<float> buffer (2, 4800);

            for (int i = 0; i < 4800; ++i)
            {
                const float value = 0.4f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                     * (float) hz * (float) i / 48000.0f);
                buffer.setSample (0, i, value);
                buffer.setSample (1, i, value);
            }

            eq.reset();
            eq.process (buffer, 4800, params);
            return std::abs (buffer.getSample (0, 4000));
        };

        const double low = render (80.0);
        const double high = render (2000.0);
        check (low < high * 0.3, "cleanup EQ attenuates below the high-pass");
    }

    {
        vr::DeEsser deEsser;
        deEsser.prepare (48000.0, 512);
        vr::RackParams params;
        params.dsBypass = false;
        params.dsFreq = 8000.0f;
        params.dsThreshold = -42.0f;
        params.dsRange = 18.0f;
        params.dsRelease = 40.0f;

        juce::AudioBuffer<float> high (2, 4800);
        juce::AudioBuffer<float> low (2, 4800);

        for (int i = 0; i < 4800; ++i)
        {
            const float highValue = 0.35f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                      * 8000.0f * (float) i / 48000.0f);
            const float lowValue = 0.35f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                     * 300.0f * (float) i / 48000.0f);
            high.setSample (0, i, highValue);
            high.setSample (1, i, highValue);
            low.setSample (0, i, lowValue);
            low.setSample (1, i, lowValue);
        }

        deEsser.reset();
        deEsser.process (high, 4800, params);
        deEsser.reset();
        deEsser.process (low, 4800, params);

        auto bufferRms = [] (const juce::AudioBuffer<float>& buffer)
        {
            double sum = 0.0;

            for (int i = 2000; i < buffer.getNumSamples(); ++i)
                sum += (double) buffer.getSample (0, i) * (double) buffer.getSample (0, i);

            return std::sqrt (sum / 2800.0);
        };

        const double highAfter = bufferRms (high);
        const double lowAfter = bufferRms (low);
        check (highAfter < 0.05, "de-esser attenuates the sibilance band (high RMS "
                                     + juce::String (highAfter, 4) + ", low RMS "
                                     + juce::String (lowAfter, 4) + ")");
        check (lowAfter > 0.2, "de-esser preserves the low band (high RMS "
                                   + juce::String (highAfter, 4) + ", low RMS "
                                   + juce::String (lowAfter, 4) + ")");
    }

    {
        struct DelayCase { int division; int feel; int expected; const char* label; };
        const DelayCase cases[] = {
            { 3, 0, 24000, "quarter = 500 ms" },
            { 2, 0, 12000, "eighth = 250 ms" },
            { 2, 1, 18000, "dotted eighth = 375 ms" },
        };

        for (const auto& c : cases)
        {
            vr::StereoDelay delay;
            delay.prepare (48000.0, 512);
            vr::RackParams params;
            params.dlyBypass = false;
            params.dlySync = true;
            params.dlyDivision = c.division;
            params.dlyFeel = c.feel;
            params.dlyFeedback = 0.0f;
            params.dlyLevel = 0.0f;
            params.dlyDuck = 0.0f;
            params.dlyLowCut = 20.0f;
            params.dlyHighCut = 20000.0f;
            params.dlyManualBpm = 120.0f;

            juce::AudioBuffer<float> send (2, 48000);
            juce::AudioBuffer<float> wet (2, 48000);
            send.clear();
            send.setSample (0, 0, 1.0f);
            send.setSample (1, 0, 1.0f);
            delay.process (send, wet, 48000, params, 120.0, false);

            int peakIndex = -1;
            double peakValue = 0.0;

            for (int i = 0; i < 48000; ++i)
            {
                if (std::abs (wet.getSample (0, i)) > peakValue)
                {
                    peakValue = std::abs (wet.getSample (0, i));
                    peakIndex = i;
                }
            }

            check (std::abs (peakIndex - c.expected) <= 2,
                   juce::String ("delay ") + c.label + " (peak " + juce::String (peakIndex) + ")");
        }
    }

    {
        vr::Reverb reverb;
        reverb.prepare (48000.0, 512, 2);
        vr::RackParams params;
        params.revBypass = false;
        params.revType = 0;
        params.revPredelay = 0.0f;
        params.revDecay = 1.2f;
        params.revDamping = 40.0f;
        params.revLevel = 0.0f;
        params.revLowCut = 20.0f;
        params.revHighCut = 20000.0f;

        const int total = 48000 * 4;
        juce::AudioBuffer<float> send (2, total);
        juce::AudioBuffer<float> wet (2, total);
        send.clear();
        send.setSample (0, 0, 1.0f);
        send.setSample (1, 0, 1.0f);
        reverb.process (send, wet, total, params);

        const double early = rms (std::vector<float> (wet.getReadPointer (0) + 24000,
                                                      wet.getReadPointer (0) + 48000));
        const double late = rms (std::vector<float> (wet.getReadPointer (0) + 48000 * 3,
                                                     wet.getReadPointer (0) + 48000 * 4));
        check (early > late, "reverb tail decays");
        check (allFinite (std::vector<float> (wet.getReadPointer (0),
                                              wet.getReadPointer (0) + wet.getNumSamples())),
               "reverb output is finite");
    }

    // Room and Plate must be audibly different algorithms.
    {
        auto renderType = [] (int type)
        {
            vr::Reverb reverb;
            reverb.prepare (48000.0, 512, 2);
            vr::RackParams params;
            params.revBypass = false;
            params.revType = type;
            params.revPredelay = 0.0f;
            params.revDecay = 1.5f;
            params.revDamping = 35.0f;
            params.revLevel = 0.0f;
            params.revLowCut = 20.0f;
            params.revHighCut = 20000.0f;
            juce::AudioBuffer<float> send (2, 48000);
            juce::AudioBuffer<float> wet (2, 48000);
            send.clear();
            send.setSample (0, 0, 1.0f);
            send.setSample (1, 0, 1.0f);
            reverb.process (send, wet, 48000, params);
            return rms (std::vector<float> (wet.getReadPointer (0) + 12000,
                                            wet.getReadPointer (0) + 30000));
        };

        const double room = renderType (0);
        const double plate = renderType (1);
        check (std::abs (room - plate) > 1.0e-5,
               "Room and Plate produce different tails (room " + juce::String (room, 5)
                   + ", plate " + juce::String (plate, 5) + ")");
    }

    // Invalid host tempo falls back to the manual BPM.
    {
        vr::StereoDelay delay;
        delay.prepare (48000.0, 512);
        vr::RackParams params;
        params.dlyBypass = false;
        params.dlySync = true;
        params.dlyDivision = 3; // quarter note
        params.dlyFeel = 0;
        params.dlyFeedback = 0.0f;
        params.dlyLevel = 0.0f;
        params.dlyDuck = 0.0f;
        params.dlyLowCut = 20.0f;
        params.dlyHighCut = 20000.0f;
        params.dlyManualBpm = 100.0f;

        juce::AudioBuffer<float> send (2, 40000);
        juce::AudioBuffer<float> wet (2, 40000);
        send.clear();
        send.setSample (0, 0, 1.0f);
        send.setSample (1, 0, 1.0f);
        delay.process (send, wet, 40000, params, 0.0, false); // invalid host BPM

        int peakIndex = -1;
        double peakValue = 0.0;

        for (int i = 0; i < 40000; ++i)
        {
            if (std::abs (wet.getSample (0, i)) > peakValue)
            {
                peakValue = std::abs (wet.getSample (0, i));
                peakIndex = i;
            }
        }

        const int expected = (int) (0.6 * 48000.0); // 100 BPM quarter
        check (std::abs (peakIndex - expected) <= 2,
               "invalid host tempo falls back to manual 100 BPM (peak "
                   + juce::String (peakIndex) + ")");
    }

    // Saturation aliasing, measured independently on a high note.
    {
        auto aliasDb = [] (bool oversample)
        {
            vr::Saturation saturation;
            saturation.prepare (48000.0, 512, 2, oversample);
            vr::RackParams params;
            params.satBypass = false;
            params.satMode = 1;
            params.satDrive = 18.0f;
            params.satBlend = 100.0f;
            params.satOutput = 0.0f;

            const int total = 48000;
            juce::AudioBuffer<float> buffer (2, total);

            for (int i = 0; i < total; ++i)
            {
                const float value = 0.35f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                      * 5000.0f * (float) i / 48000.0f);
                buffer.setSample (0, i, value);
                buffer.setSample (1, i, value);
            }

            for (int offset = 0; offset < total; offset += 512)
            {
                const int count = juce::jmin (512, total - offset);
                juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), 2, offset, count);
                saturation.process (chunk, count, params);
            }

            const int fftSize = 8192;
            juce::dsp::FFT fft (13);
            std::vector<std::complex<float>> input ((size_t) fftSize);
            std::vector<std::complex<float>> spectrum ((size_t) fftSize);
            const int start = total - fftSize;

            for (int i = 0; i < fftSize; ++i)
            {
                const float w = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi
                                                        * (float) i / (float) fftSize);
                input[(size_t) i] = { buffer.getSample (0, start + i) * w, 0.0f };
            }

            fft.perform (input.data(), spectrum.data(), false);
            double totalEnergy = 0.0;
            double aliasEnergy = 0.0;
            const int fundamental = (int) std::lround (5000.0 * (double) fftSize / 48000.0);
            std::vector<int> harmonicBins;

            for (int h = 1; h <= 9; ++h)
                harmonicBins.push_back (fundamental * h);

            for (int bin = 2; bin < fftSize / 2; ++bin)
            {
                const double energy = std::norm (spectrum[(size_t) bin]);
                totalEnergy += energy;
                bool harmonic = false;

                for (int centre : harmonicBins)
                {
                    if (std::abs (bin - centre) <= 6)
                    {
                        harmonic = true;
                        break;
                    }
                }

                if (! harmonic)
                    aliasEnergy += energy;
            }

            return 10.0 * std::log10 (juce::jmax (1.0e-20, aliasEnergy)
                                      / juce::jmax (1.0e-20, totalEnergy));
        };

        const double noOs = aliasDb (false);
        const double withOs = aliasDb (true);
        check (withOs < noOs, "2x saturation oversampling lowers measured aliasing ("
                                  + juce::String (noOs, 2) + " dB -> "
                                  + juce::String (withOs, 2) + " dB)");
    }
}

static double probeAmplitudeAt (const std::vector<float>& v, double sampleRate, double hz,
                              int start, int count)
{
    double re = 0.0;
    double im = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const double phase = juce::MathConstants<double>::twoPi * hz * (double) i / sampleRate;
        re += (double) v[(size_t) (start + i)] * std::cos (phase);
        im += (double) v[(size_t) (start + i)] * std::sin (phase);
    }

    return 2.0 * std::sqrt (re * re + im * im) / (double) count;
}

void testDynamicEq()
{
    constexpr double sr = 48000.0;
    const int length = (int) (2.0 * sr);
    std::vector<float> tone ((size_t) length, 0.0f);

    for (int i = 0; i < length; ++i)
        tone[(size_t) i] = 0.35f * (float) std::sin (juce::MathConstants<double>::twoPi * 300.0
                                                       * (double) i / sr);

    auto render = [&] (bool on, float gain, float threshold)
    {
        auto p = bypassAllParams();
        p.deq1On = on;
        p.deq1Freq = 300.0f;
        p.deq1Q = 1.0f;
        p.deq1Gain = gain;
        p.deq1Threshold = threshold;
        p.deq1Ratio = 4.0f;
        p.deq1Range = 12.0f;
        p.deq1Attack = 1.0f;
        p.deq1Release = 80.0f;
        return runEngine (p, tone, tone, sr, 512, vr::QualityMode::mix);
    };

    auto rms = [] (const std::vector<float>& v, int start)
    {
        double sum = 0.0;
        int count = 0;

        for (int i = start; i < (int) v.size(); ++i)
        {
            sum += (double) v[(size_t) i] * (double) v[(size_t) i];
            ++count;
        }

        return std::sqrt (sum / (double) juce::jmax (1, count));
    };

    const auto dry = render (false, 0.0f, -30.0f);
    const auto cut = render (true, 0.0f, -50.0f);
    const auto boost = render (true, 6.0f, 0.0f);
    const int start = 12000;

    check (rms (cut.left, start) < rms (dry.left, start) * 0.8,
           "dynamic EQ reduces a signal above threshold");
    check (rms (boost.left, start) > rms (dry.left, start) * 1.1,
           "dynamic EQ static peaking gain boosts");
    check (std::isfinite (rms (cut.left, start)) && std::isfinite (rms (boost.left, start)),
           "dynamic EQ remains finite");
}

static double measuredTruePeak (const juce::AudioBuffer<float>& buffer, int n)
{
    juce::dsp::Oversampling<float> oversampling ((size_t) 2, 3,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling.initProcessing ((size_t) juce::jmax (32, n));
    juce::AudioBuffer<float> copy (buffer);
    juce::dsp::AudioBlock<float> block (copy);
    auto up = oversampling.processSamplesUp (block.getSubBlock (0, (size_t) n));
    double peak = 0.0;

    for (size_t ch = 0; ch < up.getNumChannels(); ++ch)
        for (size_t i = 0; i < up.getNumSamples(); ++i)
            peak = juce::jmax (peak, (double) std::abs (up.getChannelPointer (ch)[i]));

    return peak;
}

void testLoudnessMeter()
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;
    vr::LoudnessMeter meter;
    meter.prepare (sr);
    juce::AudioBuffer<float> buffer (2, block);

    for (int blockIndex = 0; blockIndex < 300; ++blockIndex)
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
                buffer.setSample (ch, i, 0.1f * (float) std::sin (
                    juce::MathConstants<double>::twoPi * 1000.0
                    * (double) (blockIndex * block + i) / sr));

        meter.process (buffer, block);
    }

    const float lufs = meter.getLufs();
    std::cout << "LUFS_1K value=" << lufs << std::endl;
    check (std::isfinite (lufs), "LUFS-S meter is finite");
    check (lufs > -30.0f && lufs < -18.0f, "1 kHz LUFS-S is in the expected range");
}

void testTruePeakLimiter()
{
    constexpr double sr = 48000.0;
    constexpr int block = 512;
    const int length = (int) (1.5 * sr);
    juce::AudioBuffer<float> buffer (2, length);
    juce::AudioBuffer<float> original (2, length);

    for (int ch = 0; ch < 2; ++ch)
    {
        for (int i = 0; i < length; ++i)
        {
            const float sample = 1.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 997.0
                                                           * (double) i / sr);
            buffer.setSample (ch, i, sample);
            original.setSample (ch, i, sample);
        }
    }

    vr::TruePeakLimiter limiter;
    limiter.prepare (sr, block, 2);
    vr::RackParams p = bypassAllParams();
    p.limBypass = false;
    p.limTruePeak = true;
    p.limCeiling = -1.0f;
    p.limRelease = 80.0f;

    for (int position = 0; position < length; position += block)
    {
        const int count = juce::jmin (block, length - position);
        juce::AudioBuffer<float> chunk (2, count);

        for (int ch = 0; ch < 2; ++ch)
            chunk.copyFrom (ch, 0, buffer, ch, position, count);

        limiter.process (chunk, count, p);

        for (int ch = 0; ch < 2; ++ch)
            buffer.copyFrom (ch, position, chunk, ch, 0, count);
    }

    const double truePeak = measuredTruePeak (buffer, length);
    const double ceiling = std::pow (10.0, -1.0 / 20.0);
    check (std::isfinite (truePeak), "true peak limiter produces finite output");
    check (truePeak <= ceiling * 1.06,
           "4x true peak limiter stays within 0.5 dB of ceiling");
    check (limiter.getLatencySamples() > 0, "true peak limiter reports latency");

    std::cout << "TRUE_PEAK ceiling=" << juce::String (20.0 * std::log10 (ceiling), 2)
              << " measured=" << juce::String (20.0 * std::log10 (juce::jmax (1.0e-9, truePeak)), 2)
              << " latency=" << limiter.getLatencySamples() << std::endl;

    // High-frequency and phase-offset stress cases.
    for (double hz : { 11025.0, 12000.0 })
    {
        juce::AudioBuffer<float> stress (2, length);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < length; ++i)
                stress.setSample (ch, i, 1.4f * (float) std::sin (
                    juce::MathConstants<double>::twoPi * hz * (double) i / sr
                    + (ch == 0 ? 0.0 : 0.7)));

        vr::TruePeakLimiter stressLimiter;
        stressLimiter.prepare (sr, block, 2);

        for (int position = 0; position < length; position += block)
        {
            const int count = juce::jmin (block, length - position);
            juce::AudioBuffer<float> chunk (2, count);

            for (int ch = 0; ch < 2; ++ch)
                chunk.copyFrom (ch, 0, stress, ch, position, count);

            stressLimiter.process (chunk, count, p);

            for (int ch = 0; ch < 2; ++ch)
                stress.copyFrom (ch, position, chunk, ch, 0, count);
        }

        const double stressPeak = measuredTruePeak (stress, length);
        std::cout << "TRUE_PEAK_STRESS hz=" << hz << " db="
                  << 20.0 * std::log10 (juce::jmax (1.0e-9, stressPeak)) << std::endl;
        check (stressPeak <= ceiling * 1.08,
               "true peak stress case stays within 0.7 dB: " + juce::String (hz, 0) + " Hz");
    }
}

void testCleanSignalPath()
{
    constexpr double sr = 48000.0;
    const int length = (int) (2.0 * sr);
    std::vector<float> tone ((size_t) length, 0.0f);

    for (int i = 0; i < length; ++i)
    {
        const double t = (double) i / sr;
        tone[(size_t) i] = (float) (0.10 * std::sin (juce::MathConstants<double>::twoPi * 100.0 * t)
                                  + 0.10 * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * t)
                                  + 0.05 * std::sin (juce::MathConstants<double>::twoPi * 5000.0 * t)
                                  + 0.03 * std::sin (juce::MathConstants<double>::twoPi * 10000.0 * t));
    }

    auto correlation = [] (const std::vector<float>& input, const std::vector<float>& output,
                           int latency)
    {
        double corr = 0.0;
        double inSum = 0.0;
        double outSum = 0.0;
        const int skip = 4800;
        const int count = juce::jmin ((int) input.size() - latency - skip, (int) 40000);

        for (int i = 0; i < count; ++i)
        {
            const double a = input[(size_t) (skip + i)];
            const double b = output[(size_t) (skip + latency + i)];
            corr += a * b;
            inSum += a * a;
            outSum += b * b;
        }

        return corr / std::sqrt (juce::jmax (1.0e-12, inSum * outSum));
    };

    for (auto quality : { vr::QualityMode::track, vr::QualityMode::mix })
    {
        auto p = bypassAllParams();
        p.pitchBypass = true;
        p.satBypass = false;
        p.satBlend = 0.0f;
        p.satDrive = 12.0f;
        auto out = runEngine (p, tone, tone, sr, 512, quality);
        check (correlation (tone, out.left, out.latency) > 0.9999,
               "bypassed tuning/saturation dry path is phase clean");

        p = bypassAllParams();
        p.dnBypass = false;
        p.dnAmount = 0.0f;
        p.dnMaxReduction = 12.0f;
        out = runEngine (p, tone, tone, sr, 512, vr::QualityMode::mix);
        check (correlation (tone, out.left, out.latency) > 0.999,
               "zero-amount spectral cleanup is effectively transparent");

        p = bypassAllParams();
        p.pitchBypass = false;
        p.pitchAmount = 100.0f;
        p.pitchRetune = 1.0f;
        p.pitchScale = 0;
        p.pitchTonic = 0;
        p.pitchHumanize = 0.0f;

        for (double hz : { 110.0, 220.0 })
        {
            std::vector<float> sine ((size_t) length, 0.0f);

            for (int i = 0; i < length; ++i)
                sine[(size_t) i] = 0.1f * (float) std::sin (juce::MathConstants<double>::twoPi * hz
                                                            * (double) i / sr);

            out = runEngine (p, sine, sine, sr, 512, quality);
            const auto amplitude = probeAmplitudeAt (out.left, sr, hz, 12000 + out.latency, 24000);
            check (amplitude > 0.099,
                   "unity correction preserves level at " + juce::String (hz, 0) + " Hz");
        }

        std::vector<float> a440 ((size_t) length, 0.0f);

        for (int i = 0; i < length; ++i)
            a440[(size_t) i] = 0.1f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0
                                                        * (double) i / sr);

        out = runEngine (p, a440, a440, sr, 512, quality);
        check (probeAmplitudeAt (out.left, sr, 440.0, 12000 + out.latency, 24000) > 0.095,
               "phase-locked A440 correction keeps level");
    }
}

void testNoiseAndUnity()
{
    const double sampleRate = 48000.0;

    // All processor stages bypassed: the processed path must still be
    // latency-aligned and unity within a musical tolerance.
    for (auto quality : { vr::QualityMode::track, vr::QualityMode::mix })
    {
        RackParams params = bypassAllParams();
        params.bypass = false;
        params.limBypass = true;
        params.mix = 1.0f;

        const auto tone = voice (sampleRate, 0.7, 1000.0, 0.2, 0.0);
        const auto output = runEngine (params, tone, tone, sampleRate, 512, quality);
        const int from = output.latency + (int) (0.25 * sampleRate);
        const int count = (int) (0.3 * sampleRate);
        const double inputLevel = rms (tone, (int) (0.25 * sampleRate), count);
        const double outputLevel = rms (output.left, from, count);
        const double ratio = outputLevel / juce::jmax (1.0e-9, inputLevel);
        check (ratio > 0.7 && ratio < 1.3,
               juce::String (quality == vr::QualityMode::track ? "Track" : "Mix")
                   + " all-stage bypass is unity-ish (ratio " + juce::String (ratio, 3) + ")");
    }

    // Spectral suppression on a reproducible noisy-vs-clean pair.
    {
        auto clean = voice (sampleRate, 1.5, 220.0, 0.2);
        auto noisy = noise (sampleRate, 3.5, 0.002, 31);

        for (size_t i = 0; i < clean.size(); ++i)
            noisy[(size_t) (sampleRate * 1.0) + i] += clean[i];

        RackParams params = bypassAllParams();
        params.dnBypass = false;
        params.dnAmount = 70.0f;
        params.dnMaxReduction = 20.0f;
        params.limBypass = true;
        params.mix = 1.0f;

        const auto mixOutput = runEngine (params, noisy, noisy, sampleRate, 512, vr::QualityMode::mix);
        const int latency = mixOutput.latency;
        const int tailStart = (int) (2.85 * sampleRate);
        const int tailCount = (int) (0.45 * sampleRate);
        const int voiceStart = (int) (1.25 * sampleRate);
        const int voiceCount = (int) (0.8 * sampleRate);
        const double noiseBefore = rms (noisy, tailStart, tailCount);
        const double noiseAfter = rms (mixOutput.left, tailStart + latency, tailCount);
        const double reductionDb = 20.0 * std::log10 (juce::jmax (1.0e-9, noiseBefore)
                                                       / juce::jmax (1.0e-9, noiseAfter));
        const double voiceBefore = rms (noisy, voiceStart, voiceCount);
        const double voiceAfter = rms (mixOutput.left, voiceStart + latency, voiceCount);
        const double voiceChangeDb = 20.0 * std::log10 (juce::jmax (1.0e-9, voiceAfter)
                                                         / juce::jmax (1.0e-9, voiceBefore));

        check (reductionDb > 6.0, "spectral cleanup reduces steady noise by > 6 dB ("
                                      + juce::String (reductionDb, 2) + " dB, voice "
                                      + juce::String (voiceChangeDb, 2) + " dB)");
        check (std::abs (voiceChangeDb) < 3.0, "spectral cleanup keeps voiced level within 3 dB ("
                                                   + juce::String (voiceChangeDb, 2) + " dB)");

        const auto trackOutput = runEngine (params, noisy, noisy, sampleRate, 512, vr::QualityMode::track);
        const double trackNoiseAfter = rms (trackOutput.left, tailStart + trackOutput.latency, tailCount);
        const double trackReductionDb = 20.0 * std::log10 (juce::jmax (1.0e-9, noiseBefore)
                                                            / juce::jmax (1.0e-9, trackNoiseAfter));
        check (trackReductionDb < 2.0, "Track does not secretly apply spectral cleanup ("
                                          + juce::String (trackReductionDb, 2) + " dB)");
    }

    // Gentle expander closes in a low-level gap.
    {
        std::vector<float> signal = voice (sampleRate, 0.55, 220.0, 0.2);
        auto gap = noise (sampleRate, 1.0, 0.003, 44);
        signal.insert (signal.end(), gap.begin(), gap.end());

        RackParams params = bypassAllParams();
        params.expBypass = false;
        params.expThreshold = -30.0f;
        params.expRange = 30.0f;
        params.expAttack = 1.0f;
        params.expHold = 20.0f;
        params.expRelease = 50.0f;
        params.limBypass = true;
        params.mix = 1.0f;

        const auto output = runEngine (params, signal, signal, sampleRate, 512, vr::QualityMode::track);
        const int measurementStart = (int) (0.95 * sampleRate);
        const int start = output.latency + measurementStart;
        const int count = (int) (0.20 * sampleRate);
        const double before = rms (signal, measurementStart, count);
        const double after = rms (output.left, start, count);
        const double reductionDb = 20.0 * std::log10 (juce::jmax (1.0e-9, before)
                                                       / juce::jmax (1.0e-9, after));
        check (reductionDb > 10.0, "expander closes between phrases ("
                                       + juce::String (reductionDb, 2) + " dB)");
    }

    // Mono insert: both returns must remain usable after downmix.
    {
        vr::RackParams params = bypassAllParams();
        params.dlyBypass = false;
        params.dlySync = false;
        params.dlyMs = 120.0f;
        params.dlyLevel = -6.0f;
        params.dlyFeedback = 20.0f;
        params.revBypass = false;
        params.revType = 1;
        params.revDecay = 1.0f;
        params.revLevel = -10.0f;
        params.limBypass = true;
        params.mix = 1.0f;

        std::vector<float> input = voice (48000.0, 0.5, 330.0, 0.2);
        input.resize ((size_t) (48000 * 1.5), 0.0f);
        const auto output = runEngine (params, input, input, 48000.0, 512,
                                       vr::QualityMode::track, 1, 1);
        const double tail = rms (output.left, (int) (48000 * 0.85), (int) (48000 * 0.35));
        check (tail > 1.0e-5, "mono insert keeps a usable delay/reverb tail ("
                                  + juce::String (tail, 5) + ")");
    }
}

void testPresetsAndState()
{
    const auto presetDirectory = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                     .getChildFile ("vr-preset-tests-" + juce::Uuid().toDashedString());
    presetDirectory.createDirectory();
    setenv ("VR_USER_PRESET_DIR", presetDirectory.getFullPathName().toRawUTF8(), 1);

    {
        vr::VocalRackAudioProcessor processor;
        processor.prepareToPlay (48000.0, 512);

        const auto& entries = processor.getPresets().getEntries();
        check (entries.size() == 96, "96 factory presets parsed");

        // Backward compatibility: a v1 preset written before the 2.0 formant
        // parameters must load with documented defaults, not be rejected.
        auto legacyFactory = juce::JSON::parse (vr::kFactoryPresetJson);

        if (auto* array = legacyFactory.getArray())
        {
            if (! array->isEmpty())
            {
                auto legacy = (*array)[0];

                if (auto* object = legacy.getDynamicObject())
                {
                    object->removeProperty ("pitch.formant");
                    object->removeProperty ("pitch.formantShift");
                    vr::PresetEntry legacyEntry;
                    juce::String legacyError;
                    check (processor.getPresets().validatePreset (legacy, legacyEntry, legacyError),
                           "pre-2.0 preset without formant fields still validates");
                    auto* legacyParams = legacyEntry.parameters.getDynamicObject();
                    check (legacyParams != nullptr
                               && (double) legacyParams->getProperty ("pitch.formant") == 0.0
                               && (double) legacyParams->getProperty ("pitch.formantShift") == 0.0,
                           "legacy preset receives safe formant defaults");
                }
            }
        }

        std::map<juce::String, int> banks;

        for (const auto& entry : entries)
        {
            banks[entry.bank]++;

            if (! entry.factory)
                continue;

            processor.requestPresetLoad (entry.id, false);
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            bool finite = true;

            for (int block = 0; block < 12; ++block)
            {
                for (int channel = 0; channel < 2; ++channel)
                    for (int i = 0; i < 512; ++i)
                        buffer.setSample (channel, i, 0.1f * std::sin (2.0f * juce::MathConstants<float>::pi
                                                                       * 220.0f * (float) (block * 512 + i) / 48000.0f));

                processor.processBlock (buffer, midi);

                for (int channel = 0; channel < 2; ++channel)
                    for (int i = 0; i < 512; ++i)
                        finite = finite && std::isfinite (buffer.getSample (channel, i));
            }

            check (finite, "factory preset renders finite: " + entry.name);
        }

        check (banks.size() == 16, "factory bank count is sixteen");

        for (const auto& [bank, count] : banks)
            check (count == 6, "factory bank has six presets: " + bank);

        auto setParameter = [&processor] (const char* id, float value)
        {
            auto* parameter = processor.getAPVTS().getParameter (id);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };

        setParameter ("pitch.tonic", 0.0f);
        setParameter ("pitch.scale", 1.0f);
        setParameter ("pitch.mask", (float) vr::maskForScale (1, 0));
        const auto* falsetto = processor.getPresets().findById ("factory.rnb.late-night-falsetto");
        check (falsetto != nullptr, "Late Night Falsetto exists by stable id");

        if (falsetto != nullptr)
        {
            processor.requestPresetLoad (falsetto->id, true);
            const int preservedMask = (int) std::lround (
                processor.getAPVTS().getRawParameterValue ("pitch.mask")->load());
            check (std::lround (processor.getAPVTS().getRawParameterValue ("pitch.tonic")->load()) == 0
                       && std::lround (processor.getAPVTS().getRawParameterValue ("pitch.scale")->load()) == 1
                       && preservedMask == vr::maskForScale (1, 0),
                   "Keep Song Key preserves tonic, scale and mask on factory load");

            processor.requestPresetLoad (falsetto->id, false);
            check (std::lround (processor.getAPVTS().getRawParameterValue ("pitch.tonic")->load()) == 9
                       && std::lround (processor.getAPVTS().getRawParameterValue ("pitch.scale")->load()) == 2,
                   "Keep Song Key off recalls the preset key");
        }

        setParameter ("global.input", 5.5f);
        juce::String error;
        check (processor.saveCurrentAsUserPreset ("Test Save", "Test Bank", "test description",
                                                  "test use", "test move", error),
               "Save As writes a user preset: " + error);

        const auto* userPreset = processor.getPresets().findById (processor.getPresetId());
        check (userPreset != nullptr && ! userPreset->factory, "saved preset is a user preset");

        if (userPreset != nullptr)
        {
            const auto exportFile = presetDirectory.getChildFile ("exported.vocalrack.json");
            check (processor.exportPresetFile (userPreset->id, exportFile, error),
                   "user preset exports: " + error);
            check (exportFile.existsAsFile(), "export file exists");

            vr::VocalRackAudioProcessor importer;
            importer.prepareToPlay (48000.0, 512);
            check (importer.importPresetFile (exportFile, error),
                   "exported preset imports: " + error);
        }

        const auto badFile = presetDirectory.getChildFile ("bad.vocalrack.json");
        badFile.replaceWithText ("{ definitely not valid json ");
        processor.getPresets().initialise();
        check (! processor.getPresets().getScanErrors().isEmpty(), "corrupt user preset is reported");
        check (processor.getPresets().getEntries().size() >= 96, "corrupt file does not remove factory bank");

        auto state = juce::MemoryBlock();
        processor.getStateInformation (state);
        check (state.getSize() > 0, "state serializes");

        vr::VocalRackAudioProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        restored.prepareToPlay (48000.0, 512);
        check (std::abs (restored.getAPVTS().getRawParameterValue ("global.input")->load() - 5.5f) < 0.01f,
               "session recall restores edited parameters");
        check (restored.getPresetId() == processor.getPresetId(), "session recall restores preset identity");

        // Preset files are not required for recall: delete the file, then
        // restore the previously captured full state into a fresh instance.
        if (userPreset != nullptr && userPreset->file.existsAsFile())
        {
            const auto fileToDelete = userPreset->file;
            fileToDelete.deleteFile();
            vr::VocalRackAudioProcessor afterDelete;
            afterDelete.setStateInformation (state.getData(), (int) state.getSize());
            afterDelete.prepareToPlay (48000.0, 512);
            check (std::abs (afterDelete.getAPVTS().getRawParameterValue ("global.input")->load() - 5.5f) < 0.01f,
                   "session recall survives a deleted user preset file");
        }

        // Quality is separate from preset loads.
        processor.requestQuality (vr::QualityMode::mix);
        const auto qualityLatency = processor.getReportedLatencySamples();
        processor.requestPresetLoad ("factory.tracking.clean-start", true);
        check (processor.getQuality() == vr::QualityMode::mix
                   && processor.getReportedLatencySamples() == qualityLatency,
               "factory preset load does not change the quality configuration");
        processor.requestQuality (vr::QualityMode::track);

        // User preset duplicate/rename/delete lifecycle.
        const auto currentUserId = processor.getPresetId();
        juce::String lifecycleError;
        check (processor.duplicateUserPreset (currentUserId, "Copy Of Test", lifecycleError),
               "duplicate user preset: " + lifecycleError);
        const auto copy = std::find_if (processor.getPresets().getEntries().begin(),
                                        processor.getPresets().getEntries().end(),
                                        [] (const vr::PresetEntry& e) { return e.name == "Copy Of Test"; });
        if (copy != processor.getPresets().getEntries().end())
        {
            const auto copyId = copy->id;
            check (processor.renameUserPreset (copyId, "Renamed Copy", lifecycleError),
                   "rename user preset: " + lifecycleError);
            check (processor.deleteUserPreset (copyId, lifecycleError),
                   "delete user preset: " + lifecycleError);
        }
        else
        {
            check (false, "duplicated user preset exists");
        }

        setParameter ("global.input", 0.0f);
        processor.switchAB (1);
        setParameter ("global.input", 7.0f);
        processor.switchAB (0);
        check (std::abs (processor.getAPVTS().getRawParameterValue ("global.input")->load()) < 0.1f,
               "A/B slot A restores its output trim");
        processor.switchAB (1);
        check (std::abs (processor.getAPVTS().getRawParameterValue ("global.input")->load() - 7.0f) < 0.1f,
               "A/B slot B restores its edited state");

        vr::PresetEntry rejected;
        juce::String reason;
        const auto future = juce::JSON::parse ("{\"format\":\"ademusic-vocal-rack-preset\",\"version\":2,"
                                               "\"id\":\"x\",\"name\":\"x\",\"bank\":\"x\",\"parameters\":{}}");
        check (! processor.getPresets().validatePreset (future, rejected, reason),
               "future preset schema version is rejected");
    }

    presetDirectory.deleteRecursively();
}

void testLoudnessMatchedAB()
{
    vr::VocalRackAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    auto setParameter = [&processor] (const char* id, float value)
    {
        if (auto* parameter = processor.getAPVTS().getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    };

    auto render = [&processor] (int samples)
    {
        int position = 0;

        while (position < samples)
        {
            const int count = juce::jmin (512, samples - position);
            juce::AudioBuffer<float> buffer (2, count);

            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < count; ++i)
                    buffer.setSample (ch, i, 0.1f * (float) std::sin (
                        juce::MathConstants<double>::twoPi * 220.0
                        * (double) (position + i) / 48000.0));

            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            position += count;
        }
    };

    setParameter ("global.output", 0.0f);
    render (48000);
    processor.switchAB (1); // B is cloned from A and becomes active.
    setParameter ("global.output", 12.0f);
    render (48000);
    processor.setLoudnessMatch (true);
    processor.switchAB (0); // Reference is the louder B; target A should be boosted.
    render (48000);
    const float matchedGain = processor.getComparisonGain();
    check (matchedGain > 1.2f, "loudness match boosts the quieter A/B slot");
    check (matchedGain <= 4.01f, "loudness match comparison gain is bounded");

    processor.setLoudnessMatch (false);
    render (48000);
    check (std::abs (processor.getComparisonGain() - 1.0f) < 0.05f,
           "disabling loudness match returns comparison gain to unity");
}

void testAutomationAndBypass()
{
    vr::RackParams params;
    params.pitchBypass = false;
    params.pitchAmount = 80.0f;
    params.pitchRetune = 5.0f;
    params.dlyBypass = false;
    params.revBypass = false;
    params.compBypass = false;

    vr::Engine engine;
    engine.prepare (48000.0, 256, 2, 2, vr::QualityMode::track);
    juce::AudioBuffer<float> buffer (2, 256);
    float previous = 0.0f;
    float maxDelta = 0.0f;
    bool finite = true;

    for (int block = 0; block < 400; ++block)
    {
        params.inputDb = (block % 40 < 20) ? -3.0f : 3.0f;
        params.compThreshold = -20.0f - (float) (block % 8);
        params.dlyFeedback = 20.0f + (float) (block % 30);
        params.revDecay = 0.8f + 0.05f * (float) (block % 10);
        params.bypass = (block % 80) < 4;

        for (int i = 0; i < 256; ++i)
        {
            const float value = 0.2f * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f
                                                 * (float) (block * 256 + i) / 48000.0f);
            buffer.setSample (0, i, value);
            buffer.setSample (1, i, value);
        }

        engine.process (buffer, params, {}, 2, 2);

        for (int i = 0; i < 256; ++i)
        {
            const float value = buffer.getSample (0, i);
            finite = finite && std::isfinite (value);
            maxDelta = juce::jmax (maxDelta, std::abs (value - previous));
            previous = value;
        }
    }

    check (finite, "automation sweep remains finite");
    check (maxDelta < 1.2f, "automation sweep avoids extreme sample discontinuities ("
                                 + juce::String (maxDelta, 3) + ")");
}

void runBenchmark()
{
    const double sampleRate = 48000.0;

    for (auto quality : { vr::QualityMode::track, vr::QualityMode::mix })
    {
        for (int blockSize : { 128, 256 })
        {
            vr::RackParams params;
            params.pitchBypass = false;
            params.pitchAmount = 70.0f;
            params.pitchRetune = 30.0f;
            params.dnBypass = false;
            params.dnAmount = 35.0f;
            params.compBypass = false;
            params.dsBypass = false;
            params.satBypass = false;
            params.dlyBypass = false;
            params.revBypass = false;

            vr::Engine engine;
            engine.prepare (sampleRate, blockSize, 2, 2, quality);
            const auto signal = voice (sampleRate, 12.0, 220.0, 0.2, 25.0);
            juce::AudioBuffer<float> buffer (2, blockSize);
            std::vector<double> times;
            times.reserve ((size_t) (10.0 * sampleRate / blockSize));
            int position = 0;

            while (position + blockSize <= (int) signal.size())
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    const float value = signal[(size_t) (position + i)];
                    buffer.setSample (0, i, value);
                    buffer.setSample (1, i, value);
                }

                const auto began = std::chrono::steady_clock::now();
                engine.process (buffer, params, {}, 2, 2);
                const auto ended = std::chrono::steady_clock::now();

                if (position > (int) (2.0 * sampleRate))
                    times.push_back (std::chrono::duration<double> (ended - began).count());

                position += blockSize;
            }

            std::sort (times.begin(), times.end());
            const double average = std::accumulate (times.begin(), times.end(), 0.0)
                                   / (double) std::max<size_t> (1, times.size());
            const double p99 = times.empty() ? 0.0 : times[(size_t) (times.size() * 0.99)];
            const double budget = (double) blockSize / sampleRate;
            const double latencyMs = 1000.0 * (double) engine.getLatencySamples() / sampleRate;

            std::cout << "BENCH quality=" << (quality == vr::QualityMode::track ? "track" : "mix")
                      << " block=" << blockSize
                      << " latency_ms=" << juce::String (latencyMs, 3)
                      << " avg_ms=" << juce::String (average * 1000.0, 4)
                      << " p99_ms=" << juce::String (p99 * 1000.0, 4)
                      << " avg_budget_pct=" << juce::String (average / budget * 100.0, 2)
                      << " p99_budget_pct=" << juce::String (p99 / budget * 100.0, 2)
                      << std::endl;
        }
    }
}

void writeSnapshot (const juce::File& file)
{
    vr::VocalRackAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    const auto snapshotPreset = juce::SystemStats::getEnvironmentVariable ("VR_SNAPSHOT_PRESET", "factory.pop.neon-pop-lead");
    processor.requestPresetLoad (snapshotPreset, true);

    if (juce::SystemStats::getEnvironmentVariable ("VR_SNAPSHOT_QUALITY", {}) == "mix")
        processor.requestQuality (vr::QualityMode::mix);

    {
        juce::AudioBuffer<float> buffer (2, 512);
        juce::MidiBuffer midi;

        for (int block = 0; block < 40; ++block)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float value = 0.12f * std::sin (2.0f * juce::MathConstants<float>::pi * 220.0f
                                                       * (float) (block * 512 + i) / 48000.0f);
                buffer.setSample (0, i, value);
                buffer.setSample (1, i, value);
            }

            processor.processBlock (buffer, midi);
        }
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    int width = 1120;
    int height = 760;
    const auto sizeText = juce::SystemStats::getEnvironmentVariable ("VR_SNAPSHOT_SIZE", {});

    if (sizeText.containsIgnoreCase ("x"))
    {
        width = sizeText.upToFirstOccurrenceOf ("x", false, false).getIntValue();
        height = sizeText.fromFirstOccurrenceOf ("x", false, false).getIntValue();
    }

    editor->setSize (juce::jmax (960, width), juce::jmax (640, height));
    editor->setVisible (true);

    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);

    if (juce::SystemStats::getEnvironmentVariable ("VR_SNAPSHOT_DETAIL", {}) == "1")
    {
        if (auto* detail = dynamic_cast<juce::TextButton*>(editor->findChildWithID ("DetailButton")))
            detail->triggerClick();

        juce::MessageManager::getInstance()->runDispatchLoopUntil (120);

        const auto tabText = juce::SystemStats::getEnvironmentVariable ("VR_SNAPSHOT_TAB", {});
        if (tabText.isNotEmpty())
            if (auto* tabs = dynamic_cast<juce::TabbedComponent*> (editor->findChildWithID ("DetailTabs")))
                tabs->setCurrentTabIndex (tabText.getIntValue());
    }

    if (juce::SystemStats::getEnvironmentVariable ("VR_SNAPSHOT_BROWSER", {}) == "1")
    {
        if (auto* browse = dynamic_cast<juce::TextButton*>(editor->findChildWithID ("BrowseButton")))
            browse->triggerClick();

        juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    }

    auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true);
    file.deleteFile();
    juce::FileOutputStream stream (file);

    if (stream.openedOk())
    {
        juce::PNGImageFormat png;
        png.writeImageToStream (image, stream);
        std::cout << "Snapshot written to " << file.getFullPathName() << std::endl;
    }
    else
    {
        std::cout << "Could not open snapshot output." << std::endl;
    }
}

void hostSmoke (const juce::File& pluginFile, const juce::String& formatName)
{
    juce::AudioPluginFormatManager manager;
    manager.addFormat (std::make_unique<juce::VST3PluginFormatHeadless>());
    manager.addFormat (std::make_unique<juce::AudioUnitPluginFormatHeadless>());
    juce::AudioPluginFormat* format = nullptr;

    for (int i = 0; i < manager.getNumFormats(); ++i)
    {
        auto* candidate = manager.getFormat (i);

        if ((formatName.equalsIgnoreCase ("au") && candidate->getName().containsIgnoreCase ("AudioUnit"))
            || (formatName.equalsIgnoreCase ("vst3") && candidate->getName().containsIgnoreCase ("VST3")))
        {
            format = candidate;
            break;
        }
    }

    if (format == nullptr)
    {
        check (false, "requested plugin format is available to the host harness");
        return;
    }

    juce::OwnedArray<juce::PluginDescription> descriptions;
    format->findAllTypesForFile (descriptions, pluginFile.getFullPathName());
    check (! descriptions.isEmpty(), "host scan finds the plugin bundle");

    if (descriptions.isEmpty())
        return;

    juce::String error;
    auto instance = format->createInstanceFromDescription (*descriptions[0], 48000.0, 512, error);
    check (instance != nullptr, "host can instantiate the plugin: " + error);

    if (instance == nullptr)
        return;

    instance->prepareToPlay (48000.0, 512);
    juce::AudioBuffer<float> buffer (2, 512);
    buffer.clear();
    juce::MidiBuffer midi;
    instance->processBlock (buffer, midi);
    check (std::isfinite (buffer.getSample (0, 0)) || buffer.getSample (0, 0) == 0.0f,
           "host insert processes audio");
    check (instance->getLatencySamples() > 0, "host sees a non-zero plugin latency");
    check (instance->getParameters().size() > 50, "host sees the full parameter set");
    std::cout << "HOST format=" << formatName
              << " latency_samples=" << instance->getLatencySamples()
              << " parameters=" << instance->getParameters().size()
              << " name=" << instance->getName() << std::endl;

    juce::MemoryBlock state;
    instance->getStateInformation (state);
    check (state.getSize() > 0, "host can read plugin state");
    instance->setStateInformation (state.getData(), (int) state.getSize());
    check (true, "host can restore plugin state");
}

} // namespace

void renderUserVocal (const juce::File& inputFile, const juce::File& outputDirectory)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (inputFile));

    if (reader == nullptr)
    {
        check (false, "local vocal file can be opened: " + inputFile.getFullPathName());
        return;
    }

    const int inputLength = (int) reader->lengthInSamples;
    const int inputChannels = juce::jlimit (1, 2, (int) reader->numChannels);
    juce::AudioBuffer<float> source (2, juce::jmax (1, inputLength));
    source.clear();
    reader->read (&source, 0, inputLength, 0, true, true);

    if (inputChannels == 1)
        source.copyFrom (1, 0, source, 0, 0, inputLength);

    const double sourceRate = reader->sampleRate;
    const int outputLength = juce::jmax (1, (int) std::ceil ((double) inputLength * 48000.0
                                                              / juce::jmax (1.0, sourceRate)));
    juce::AudioBuffer<float> resampled (2, outputLength);
    resampled.clear();

    for (int channel = 0; channel < 2; ++channel)
    {
        juce::LagrangeInterpolator interpolator;
        const double ratio = juce::jmax (1.0, sourceRate) / 48000.0;
        interpolator.process (ratio, source.getReadPointer (channel),
                              resampled.getWritePointer (channel), outputLength,
                              inputLength, 0);
    }

    vr::VocalRackAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    outputDirectory.createDirectory();

    int rendered = 0;

    for (const auto& entry : processor.getPresets().getEntries())
    {
        if (! entry.factory)
            continue;

        processor.requestPresetLoad (entry.id, false);
        processor.getEngine().reset();

        const int tailSamples = 48000 * 2;
        const int total = outputLength + tailSamples;
        juce::AudioBuffer<float> output (2, total);
        output.clear();

        for (int channel = 0; channel < 2; ++channel)
            output.copyFrom (channel, 0, resampled, channel, 0, outputLength);

        juce::MidiBuffer midi;
        int position = 0;

        while (position < total)
        {
            const int count = juce::jmin (512, total - position);
            juce::AudioBuffer<float> chunk (output.getArrayOfWritePointers(), 2, position, count);
            processor.processBlock (chunk, midi);
            position += count;
        }

        const int latency = juce::jlimit (0, total - 1, processor.getReportedLatencySamples());
        juce::AudioBuffer<float> aligned (2, total - latency);

        for (int channel = 0; channel < 2; ++channel)
            aligned.copyFrom (channel, 0, output, channel, latency, total - latency);

        const auto filename = juce::String (++rendered).paddedLeft ('0', 2)
                              + "-" + vr::PresetManager::slugify (entry.name) + ".wav";
        const auto file = outputDirectory.getChildFile (filename);
        file.deleteFile();
        std::unique_ptr<juce::OutputStream> stream (file.createOutputStream());

        if (stream == nullptr)
        {
            check (false, "could not create render file: " + file.getFullPathName());
            continue;
        }

        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (stream,
                                 juce::AudioFormatWriterOptions{}
                                     .withSampleRate (48000.0)
                                     .withNumChannels (2)
                                     .withBitsPerSample (24)));

        if (writer == nullptr)
        {
            check (false, "could not create WAV writer: " + file.getFullPathName());
            continue;
        }

        writer->writeFromAudioSampleBuffer (aligned, 0, aligned.getNumSamples());
        std::cout << "RENDER " << file.getFullPathName() << std::endl;
    }

    check (rendered == 96, "local vocal rendered through all 96 factory presets");
}

static double rmsOf (const std::vector<float>& v, int start = 0)
{
    double sum = 0.0;
    int count = 0;

    for (int i = juce::jmax (0, start); i < (int) v.size(); ++i)
    {
        sum += (double) v[(size_t) i] * (double) v[(size_t) i];
        ++count;
    }

    return std::sqrt (sum / (double) juce::jmax (1, count));
}



static void printSignalDiagram (const juce::String& label, const std::vector<float>& input,
                                const EngineOutput& output, double sampleRate)
{
    const int skip = (int) (0.15 * sampleRate);
    const int latency = output.latency;
    const int count = juce::jmin ((int) input.size() - latency - skip, (int) (1.0 * sampleRate));

    if (count <= 0)
        return;

    double corr = 0.0;
    double inputSum = 0.0;
    double outputSum = 0.0;

    for (int i = 0; i < count; ++i)
    {
        const double a = input[(size_t) (skip + i)];
        const double b = output.left[(size_t) (skip + latency + i)];
        corr += a * b;
        inputSum += a * a;
        outputSum += b * b;
    }

    corr /= std::sqrt (juce::jmax (1.0e-12, inputSum * outputSum));

    std::cout << "SIGNAL " << label
              << " latency=" << latency
              << " corr=" << juce::String (corr, 5)
              << " inRms=" << juce::String (rmsOf (input, skip), 5)
              << " outRms=" << juce::String (rmsOf (output.left, skip + latency), 5);

    for (double hz : { 100.0, 1000.0, 5000.0, 10000.0 })
        std::cout << " " << juce::String (hz, 0)
                  << "=" << juce::String (probeAmplitudeAt (output.left, sampleRate, hz,
                                                        skip + latency, count), 5);

    std::cout << std::endl;
}

static void printToneDiagnostics()
{
    constexpr double sr = 48000.0;
    const int length = (int) (2.0 * sr);

    for (double hz : { 110.0, 220.0, 440.0, 1000.0 })
    {
        std::vector<float> sine ((size_t) length, 0.0f);

        for (int i = 0; i < length; ++i)
            sine[(size_t) i] = 0.1f * (float) std::sin (juce::MathConstants<double>::twoPi * hz
                                                        * (double) i / sr);

        for (auto quality : { vr::QualityMode::track, vr::QualityMode::mix })
        {
            auto p = bypassAllParams();
            p.pitchBypass = true;
            const auto bypassed = runEngine (p, sine, sine, sr, 512, quality);
            const int start = (int) (0.25 * sr);
            const int count = (int) (1.0 * sr);
            const auto out = probeAmplitudeAt (bypassed.left, sr, hz, start + bypassed.latency, count);

            p.pitchBypass = false;
            p.pitchAmount = 100.0f;
            p.pitchRetune = 1.0f;
            p.pitchScale = 0;
            p.pitchTonic = 0;
            p.pitchHumanize = 0.0f;
            const auto shifted = runEngine (p, sine, sine, sr, 512, quality);
            const auto shiftedOut = probeAmplitudeAt (shifted.left, sr, hz, start + shifted.latency, count);

            std::cout << "TONE " << juce::String (hz, 0)
                      << " " << (quality == vr::QualityMode::track ? "track" : "mix")
                      << " bypass=" << juce::String (out, 5)
                      << " shift=" << juce::String (shiftedOut, 5)
                      << " bypassRms=" << juce::String (rmsOf (bypassed.left, start + bypassed.latency), 5)
                      << " shiftRms=" << juce::String (rmsOf (shifted.left, start + shifted.latency), 5)
                      << std::endl;
        }
    }
}

static void runSignalDiagnostics()
{
    printToneDiagnostics();
    constexpr double sr = 48000.0;
    const int length = (int) (2.0 * sr);
    std::vector<float> tone ((size_t) length, 0.0f);

    for (int i = 0; i < length; ++i)
    {
        const double t = (double) i / sr;
        tone[(size_t) i] = (float) (0.10 * std::sin (juce::MathConstants<double>::twoPi * 100.0 * t)
                                  + 0.10 * std::sin (juce::MathConstants<double>::twoPi * 1000.0 * t)
                                  + 0.05 * std::sin (juce::MathConstants<double>::twoPi * 5000.0 * t)
                                  + 0.03 * std::sin (juce::MathConstants<double>::twoPi * 10000.0 * t));
    }

    for (auto quality : { vr::QualityMode::track, vr::QualityMode::mix })
    {
        auto p = bypassAllParams();
        p.pitchBypass = false;
        p.pitchAmount = 100.0f;
        p.pitchRetune = 1.0f;
        p.pitchScale = 0;
        p.pitchTonic = 0;
        p.pitchHumanize = 0.0f;
        printSignalDiagram (quality == vr::QualityMode::track ? "pitch-track" : "pitch-mix",
                            tone, runEngine (p, tone, tone, sr, 512, quality), sr);

        p = bypassAllParams();
        p.satBypass = false;
        p.satBlend = 0.0f;
        p.satDrive = 12.0f;
        printSignalDiagram (quality == vr::QualityMode::track ? "sat0-track" : "sat0-mix",
                            tone, runEngine (p, tone, tone, sr, 512, quality), sr);

        p = bypassAllParams();
        p.satBypass = false;
        p.satBlend = 50.0f;
        p.satDrive = 6.0f;
        printSignalDiagram (quality == vr::QualityMode::track ? "sat50-track" : "sat50-mix",
                            tone, runEngine (p, tone, tone, sr, 512, quality), sr);
    }
}

void printLatencyTable()
{
    for (double sampleRate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        for (auto quality : { vr::QualityMode::track, vr::QualityMode::mix })
        {
            vr::Engine engine;
            engine.prepare (sampleRate, 512, 2, 2, quality);
            std::cout << "LATENCY sr=" << juce::String (sampleRate, 0)
                      << " quality=" << (quality == vr::QualityMode::track ? "track" : "mix")
                      << " samples=" << engine.getLatencySamples()
                      << " ms=" << juce::String (1000.0 * engine.getLatencySamples() / sampleRate, 3)
                      << " components=denoise+" << engine.getDenoiserLatencySamples()
                      << " shifter+" << engine.getShifterLatencySamples()
                      << " sat+" << engine.getSaturationLatencySamples()
                      << " limiter+" << engine.getLimiterLatencySamples()
                      << std::endl;
        }
    }
}

struct FixedPitchProvider final : vr::PitchShifter::PitchProvider
{
    float ratio = 1.5f;
    vr::PitchShifter::PitchInfo getPitchAt (double) override
    {
        return { ratio, true };
    }
};

static double spectralCentroid (const std::vector<float>& samples, double sr,
                                int start, int count)
{
    double weighted = 0.0;
    double total = 0.0;

    for (double hz = 100.0; hz <= 6000.0; hz += 100.0)
    {
        const double amplitude = probeAmplitudeAt (samples, sr, hz, start, count);
        weighted += hz * amplitude;
        total += amplitude;
    }

    return total > 1.0e-9 ? weighted / total : 0.0;
}

void testFormantPreservation()
{
    constexpr double sr = 48000.0;
    const int length = (int) (1.5 * sr);
    std::vector<float> input ((size_t) length, 0.0f);

    for (int i = 0; i < length; ++i)
    {
        const double t = (double) i / sr;
        double value = 0.0;

        for (int harmonic = 1; harmonic <= 50; ++harmonic)
        {
            const double hz = 100.0 * harmonic;
            const double envelope = std::exp (-0.5 * std::pow ((hz - 1200.0) / 500.0, 2.0));
            value += envelope * std::sin (juce::MathConstants<double>::twoPi * hz * t);
        }

        input[(size_t) i] = (float) (0.06 * value);
    }

    auto render = [&] (bool preserve)
    {
        FixedPitchProvider provider;
        vr::PitchShifter shifter;
        shifter.setPitchProvider (&provider);
        shifter.setFormantOptions (preserve, 0.0f);
        shifter.prepare (sr, 1, 512, 2048, 2560);

        std::vector<float> output (input.size(), 0.0f);
        int position = 0;

        while (position < (int) input.size())
        {
            const int count = juce::jmin (512, (int) input.size() - position);
            juce::AudioBuffer<float> in (1, count);
            std::copy (input.begin() + position, input.begin() + position + count,
                       in.getWritePointer (0));
            shifter.pushInput (in, 0, count);
            juce::AudioBuffer<float> wet (1, count);
            shifter.renderNextBlock (wet, count);
            std::copy (wet.getReadPointer (0), wet.getReadPointer (0) + count,
                       output.begin() + position);
            position += count;
        }

        return output;
    };

    const auto dry = render (false);
    const auto preserved = render (true);
    const int start = (int) (0.45 * sr);
    const int count = (int) (0.7 * sr);
    const double dryCentroid = spectralCentroid (dry, sr, start, count);
    const double preservedCentroid = spectralCentroid (preserved, sr, start, count);

    std::cout << "FORMANT offCentroid=" << juce::String (dryCentroid, 1)
              << " onCentroid=" << juce::String (preservedCentroid, 1) << std::endl;

    check (dryCentroid > 0.0 && preservedCentroid > 0.0,
           "formant-preserving shifter produces measurable output");
    check (preservedCentroid < dryCentroid - 80.0,
           "formant preservation keeps the spectral envelope lower than free pitch shift");
}

void runTruePeakBenchmark()
{
    for (double sampleRate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        for (int blockSize : { 64, 128, 256, 512 })
        {
            vr::RackParams params;
            params.limTruePeak = true;
            params.limBypass = false;

            vr::Engine engine;
            engine.prepare (sampleRate, blockSize, 2, 2, vr::QualityMode::mix);
            // True-peak is now driven entirely by params.limTruePeak (set above).
            const auto signal = voice (sampleRate, 3.0, 220.0, 0.2, 25.0);
            juce::AudioBuffer<float> buffer (2, blockSize);
            std::vector<double> times;
            int position = 0;

            while (position + blockSize <= (int) signal.size())
            {
                for (int i = 0; i < blockSize; ++i)
                {
                    buffer.setSample (0, i, signal[(size_t) (position + i)]);
                    buffer.setSample (1, i, signal[(size_t) (position + i)]);
                }

                const auto began = std::chrono::steady_clock::now();
                engine.process (buffer, params, {}, 2, 2);
                const auto ended = std::chrono::steady_clock::now();

                if (position > (int) (0.5 * sampleRate))
                    times.push_back (std::chrono::duration<double> (ended - began).count());

                position += blockSize;
            }

            std::sort (times.begin(), times.end());
            const double average = std::accumulate (times.begin(), times.end(), 0.0)
                                   / (double) std::max<size_t> (1, times.size());
            const double p99 = times.empty() ? 0.0 : times[(size_t) (times.size() * 0.99)];
            const double budget = (double) blockSize / sampleRate;
            const double latencyMs = 1000.0 * (double) engine.getLatencySamples() / sampleRate;

            std::cout << "BENCH_TP sr=" << juce::String (sampleRate, 0)
                      << " block=" << blockSize
                      << " latency_ms=" << juce::String (latencyMs, 3)
                      << " avg_ms=" << juce::String (average * 1000.0, 4)
                      << " p99_ms=" << juce::String (p99 * 1000.0, 4)
                      << " avg_budget_pct=" << juce::String (average / budget * 100.0, 2)
                      << " p99_budget_pct=" << juce::String (p99 / budget * 100.0, 2)
                      << std::endl;
        }
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    juce::StringArray args;

    for (int i = 1; i < argc; ++i)
        args.add (juce::String (argv[i]));

    if (args.contains ("--vocal"))
    {
        const int index = args.indexOf ("--vocal");
        const auto input = index + 1 < args.size() ? juce::File (args[index + 1]) : juce::File();
        const auto output = index + 2 < args.size()
                                ? juce::File (args[index + 2])
                                : juce::File::getCurrentWorkingDirectory().getChildFile ("vocal-renders");
        renderUserVocal (input, output);
        return failures == 0 ? 0 : 1;
    }

    if (args.contains ("--latency"))
    {
        printLatencyTable();
        return 0;
    }

    if (args.contains ("--signal"))
    {
        runSignalDiagnostics();
        return 0;
    }

    if (args.contains ("--bench"))
    {
        runBenchmark();
        return failures == 0 ? 0 : 1;
    }

    if (args.contains ("--bench-tp"))
    {
        runTruePeakBenchmark();
        return failures == 0 ? 0 : 1;
    }

    if (args.contains ("--snapshot"))
    {
        const int index = args.indexOf ("--snapshot");
        const auto path = index + 1 < args.size() ? args[index + 1] : "vocal-rack-editor.png";
        writeSnapshot (juce::File (juce::File::getCurrentWorkingDirectory().getChildFile (path)));
        return 0;
    }

    if (args.contains ("--host"))
    {
        const int index = args.indexOf ("--host");
        const auto path = index + 1 < args.size() ? args[index + 1] : juce::String();
        const auto format = index + 2 < args.size() ? args[index + 2] : juce::String ("vst3");
        hostSmoke (juce::File (path), format);
        return failures == 0 ? 0 : 1;
    }

    std::cout << "ademusic Vocal Rack validation suite\n"
              << "===================================\n";

    testParameterSchema();
    testLatencyAndRates();
    testDetector();
    testRenderedPitch();
    testModules();
    testCleanSignalPath();
    testDynamicEq();
    testTruePeakLimiter();
    testLoudnessMeter();
    testFormantPreservation();
    testLoudnessMatchedAB();
    testNoiseAndUnity();
    testPresetsAndState();
    testAutomationAndBypass();

    std::cout << "===================================\n"
              << (failures == 0 ? "ALL CHECKS PASSED" : juce::String (failures) + " CHECK(S) FAILED")
              << std::endl;
    return failures == 0 ? 0 : 1;
}
