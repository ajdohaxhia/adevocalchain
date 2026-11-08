#include "Engine.h"

#include <algorithm>
#include <cmath>

namespace vr
{

void Engine::prepare (double newSampleRate, int maximumBlockSize, int inputChannels,
                      int outputChannels, QualityMode newQuality)
{
    sampleRate = newSampleRate > 1000.0 ? newSampleRate : 48000.0;
    maximumBlock = juce::jlimit (32, 8192, maximumBlockSize);
    preparedInputChannels = juce::jlimit (1, 2, inputChannels);
    preparedOutputChannels = juce::jlimit (1, 2, outputChannels);
    quality = newQuality;
    prepared = false;

    if (quality == QualityMode::track)
    {
        shifterFrameSize = sampleRate >= 72000.0 ? 1024 : 512;
        shifterDelay = (int) (std::lround ((0.0146666667 * sampleRate) / 64.0) * 64);
        shifterDelay = juce::jmax (shifterDelay, shifterFrameSize + 64);
        denoiserFrameSize = 0;
        limiterLatency = juce::jmax (8, (int) std::lround (sampleRate * 0.000333));
    }
    else
    {
        shifterFrameSize = sampleRate >= 72000.0 ? 4096 : 2048;
        shifterDelay = shifterFrameSize + shifterFrameSize / 4;
        denoiserFrameSize = sampleRate >= 72000.0 ? 2048 : 1024;
        limiterLatency = juce::jmax (32, (int) std::lround (sampleRate * 0.002667));
    }

    const int activeChannels = 2;

    saturation.prepare (sampleRate, maximumBlock, activeChannels, quality == QualityMode::mix);
    denoiserLatency = 0;

    if (quality == QualityMode::mix)
    {
        denoiser.prepare (sampleRate, activeChannels, maximumBlock, denoiserFrameSize);
        denoiserLatency = denoiser.getLatencySamples();
    }

    latencySamples = denoiserLatency + shifterDelay + saturation.getLatencySamples() + limiterLatency;
    prelimiterLatency = latencySamples - limiterLatency;

    shifter.prepare (sampleRate, activeChannels, maximumBlock, shifterFrameSize, shifterDelay);
    shifter.setPitchProvider (this);

    detector.prepare (sampleRate, quality);
    detector.setRange (1);
    lastRangeIndex = 1;

    expander.prepare (sampleRate, maximumBlock);
    cleanupEq.prepare (sampleRate, maximumBlock);
    dynamicEq.prepare (sampleRate, maximumBlock);
    compressor.prepare (sampleRate, maximumBlock);
    deEsser.prepare (sampleRate, maximumBlock);
    toneEq.prepare (sampleRate, maximumBlock);
    delay.prepare (sampleRate, maximumBlock);
    reverb.prepare (sampleRate, maximumBlock, activeChannels);
    limiter.prepare (sampleRate, maximumBlock, limiterLatency);

    for (auto& line : dryRaw)
        line.prepare (prelimiterLatency + maximumBlock + 8);

    for (auto& line : dryTrimmed)
        line.prepare (prelimiterLatency + maximumBlock + 8);

    for (auto& line : shifterDry)
        line.prepare (shifterDelay + maximumBlock + 8);

    workBuffer.setSize (2, maximumBlock, false, true, false);
    denoiseBuffer.setSize (2, maximumBlock, false, true, false);
    shifterWet.setSize (2, maximumBlock, false, true, false);
    sendBuffer.setSize (2, maximumBlock, false, true, false);
    delayWet.setSize (2, maximumBlock, false, true, false);
    reverbWet.setSize (2, maximumBlock, false, true, false);
    monoScratch.setSize (1, maximumBlock, false, true, false);

    inputGain.reset (sampleRate, 0.01);
    inputGain.setCurrentAndTargetValue (1.0f);
    outputGain.reset (sampleRate, 0.01);
    outputGain.setCurrentAndTargetValue (1.0f);
    mixGain.reset (sampleRate, 0.02);
    mixGain.setCurrentAndTargetValue (1.0f);

    masterFadeStep = (float) (1.0 / juce::jmax (1.0, 0.015 * sampleRate));
    masterFade = 1.0f;

    prepared = true;
    reset();
}

void Engine::releaseResources()
{
    prepared = false;
}

void Engine::reset()
{
    expander.reset();
    denoiser.reset();
    detector.reset();
    selector.reset();
    shifter.reset();
    cleanupEq.reset();
    dynamicEq.reset();
    compressor.reset();
    deEsser.reset();
    toneEq.reset();
    saturation.reset();
    delay.reset();
    reverb.reset();
    limiter.reset();

    for (auto& line : dryRaw)
        line.reset();

    for (auto& line : dryTrimmed)
        line.reset();

    for (auto& line : shifterDry)
        line.reset();

    inputGain.setCurrentAndTargetValue (1.0f);
    outputGain.setCurrentAndTargetValue (1.0f);
    mixGain.setCurrentAndTargetValue (1.0f);

    masterFade = 1.0f;
    stateFadeRequested.store (false, std::memory_order_relaxed);

    lastProviderTime = -1.0;
    ratioLog2 = 0.0;
    ratioInitialised = false;
    lastDetectedFrequency = 0.0f;
    lastTargetMidi = -1.0f;
    lastCents = 0.0f;
    lastConfidence = 0.0f;
    lastVoiced = false;
    pitchActive = false;

    statusDetectedMidi.store (0.0f, std::memory_order_relaxed);
    statusTargetMidi.store (-1.0f, std::memory_order_relaxed);
    statusCents.store (0.0f, std::memory_order_relaxed);
    statusConfidence.store (0.0f, std::memory_order_relaxed);
    statusVoiced.store (false, std::memory_order_relaxed);
    statusActive.store (false, std::memory_order_relaxed);
}

void Engine::updatePitchSettings (const RackParams& params)
{
    const double amount = params.pitchBypass ? 0.0 : juce::jlimit (0.0, 1.0, (double) params.pitchAmount * 0.01);

    PitchSelectionSettings settings;
    settings.scaleIndex = params.pitchScale;
    settings.tonic = params.pitchTonic;
    settings.mask = params.pitchMask;
    settings.a4 = params.pitchA4;
    settings.amount = amount;
    settings.retuneMs = params.pitchRetune;
    settings.humanize = juce::jlimit (0.0, 1.0, (double) params.pitchHumanize * 0.01);
    settings.rangeIndex = params.pitchRange;
    selector.setSettings (settings);

    if (params.pitchRange != lastRangeIndex)
    {
        detector.setRange (params.pitchRange);
        lastRangeIndex = params.pitchRange;
    }

    lastMask = params.pitchMask;
    lastScale = params.pitchScale;
    lastTonic = params.pitchTonic;

    shifter.setFormantOptions (quality == QualityMode::mix && params.pitchFormantPreserve,
                               params.pitchFormantShift);

    pitchActive = ! params.pitchBypass
                  && params.pitchAmount > 0.001f
                  && selector.getEffectiveMask() != 0;

    statusActive.store (pitchActive, std::memory_order_relaxed);

    if (! pitchActive)
    {
        statusVoiced.store (false, std::memory_order_relaxed);
        statusTargetMidi.store (-1.0f, std::memory_order_relaxed);
        statusCents.store (0.0f, std::memory_order_relaxed);
    }
}

void Engine::pushDetectorFeed (const juce::AudioBuffer<float>& buffer, int numSamples)
{
    const int channels = juce::jmin (2, buffer.getNumChannels());

    if (channels <= 0)
        return;

    double midEnergy = 0.0;
    double sideEnergy = 0.0;
    double leftEnergy = 0.0;
    double rightEnergy = 0.0;

    const auto* left = buffer.getReadPointer (0);
    const auto* right = channels > 1 ? buffer.getReadPointer (1) : left;

    for (int i = 0; i < numSamples; ++i)
    {
        const double l = left[i];
        const double r = right[i];
        const double mid = 0.5 * (l + r);
        const double side = 0.5 * (l - r);
        midEnergy += mid * mid;
        sideEnergy += side * side;
        leftEnergy += l * l;
        rightEnergy += r * r;
    }

    const double maxChannel = juce::jmax (leftEnergy, rightEnergy);
    const bool useSide = midEnergy < 0.12 * maxChannel && sideEnergy > 0.05 * maxChannel;
    auto* mono = monoScratch.getWritePointer (0);

    if (channels == 1)
    {
        for (int i = 0; i < numSamples; ++i)
            mono[i] = left[i];
    }
    else if (useSide)
    {
        for (int i = 0; i < numSamples; ++i)
            mono[i] = 0.5f * (left[i] - right[i]);
    }
    else
    {
        for (int i = 0; i < numSamples; ++i)
            mono[i] = 0.5f * (left[i] + right[i]);
    }

    detector.process (mono, numSamples);
}

PitchShifter::PitchInfo Engine::getPitchAt (double analysisTimeSamples)
{
    PitchShifter::PitchInfo info;

    const double dt = lastProviderTime >= 0.0
                          ? juce::jlimit (0.0005, 0.05, (analysisTimeSamples - lastProviderTime) / sampleRate)
                          : 0.005;
    lastProviderTime = analysisTimeSamples;

    if (! pitchActive)
    {
        if (ratioInitialised)
            ratioLog2 *= std::exp (-dt / 0.025);

        if (std::abs (ratioLog2) < 1.0e-4)
            ratioLog2 = 0.0;

        info.ratio = (float) std::exp2 (ratioLog2);
        info.voiced = std::abs (ratioLog2) > 1.0e-4;
        statusVoiced.store (false, std::memory_order_relaxed);
        return info;
    }

    const PitchEstimate estimate = detector.analyseAt (analysisTimeSamples);
    const double rawMidi = PitchSelector::frequencyToMidi ((double) estimate.frequency, activeParams.pitchA4);

    if (! estimate.voiced)
    {
        if (! ratioInitialised)
        {
            ratioLog2 = 0.0;
            ratioInitialised = true;
        }
        else
        {
            const double alpha = 1.0 - std::exp (-dt / 0.025);
            ratioLog2 += alpha * (0.0 - ratioLog2);
        }

        if (std::abs (ratioLog2) < 1.0e-4)
            ratioLog2 = 0.0;

        info.ratio = (float) std::exp2 (ratioLog2);
        info.voiced = std::abs (ratioLog2) > 1.0e-4;
        info.ratio = juce::jlimit (PitchShifter::minimumRatio, PitchShifter::maximumRatio, info.ratio);

        statusVoiced.store (false, std::memory_order_relaxed);
        statusConfidence.store (estimate.confidence, std::memory_order_relaxed);
        statusCents.store (0.0f, std::memory_order_relaxed);
        statusTargetMidi.store (-1.0f, std::memory_order_relaxed);
        return info;
    }

    const PitchSelection selection = selector.process ((double) estimate.frequency, dt);
    const double targetLog2 = std::log2 (juce::jlimit ((double) PitchShifter::minimumRatio,
                                                      (double) PitchShifter::maximumRatio,
                                                      selection.valid ? selection.ratio : 1.0));

    if (! ratioInitialised)
    {
        ratioLog2 = targetLog2;
        ratioInitialised = true;
    }
    else
    {
        const double tau = juce::jmax (0.0005, (double) activeParams.pitchRetune * 0.001);
        const double alpha = 1.0 - std::exp (-dt / tau);
        ratioLog2 += alpha * (targetLog2 - ratioLog2);
    }

    ratioLog2 = juce::jlimit (-1.0, 1.0, ratioLog2);
    info.ratio = (float) std::exp2 (ratioLog2);
    info.voiced = true;

    lastDetectedFrequency = estimate.frequency;
    lastTargetMidi = (float) selection.targetNote;

    // Report the applied correction in cents at the raw detected pitch.
    if (selection.valid)
    {
        const double amount = juce::jlimit (0.0, 1.0, (double) activeParams.pitchAmount * 0.01);
        lastCents = (float) (((double) selection.targetNote - rawMidi) * 100.0 * amount);
    }
    else
    {
        lastCents = 0.0f;
    }

    lastConfidence = estimate.confidence;
    lastVoiced = true;

    statusDetectedMidi.store ((float) rawMidi, std::memory_order_relaxed);
    statusTargetMidi.store ((float) selection.targetNote, std::memory_order_relaxed);
    statusCents.store (lastCents, std::memory_order_relaxed);
    statusConfidence.store (estimate.confidence, std::memory_order_relaxed);
    statusVoiced.store (true, std::memory_order_relaxed);
    return info;
}

void Engine::process (juce::AudioBuffer<float>& buffer,
                      const RackParams& params,
                      const TransportInfo& transport,
                      int inputChannels,
                      int outputChannels)
{
    if (! prepared || buffer.getNumSamples() <= 0)
        return;

    const int totalSamples = buffer.getNumSamples();
    const int inChannels = juce::jlimit (1, 2, inputChannels > 0 ? inputChannels : buffer.getNumChannels());
    const int outChannels = juce::jlimit (1, 2, outputChannels > 0 ? outputChannels : buffer.getNumChannels());
    const int bufferChannels = buffer.getNumChannels();

    activeParams = params;
    inputGain.setTargetValue (dbToGain (params.inputDb));
    outputGain.setTargetValue (dbToGain (params.outputDb));
    mixGain.setTargetValue (juce::jlimit (0.0f, 1.0f, params.mix));
    updatePitchSettings (params);

    if (stateFadeRequested.exchange (false, std::memory_order_relaxed))
        masterFade = 0.0f;

    meterOutputSum = 0.0;
    meterOutputPeak = 0.0f;
    meterOutputSamples = 0;

    double inputSum = 0.0;
    int64_t inputSamples = 0;

    for (int offset = 0; offset < totalSamples; offset += maximumBlock)
    {
        const int n = juce::jmin (maximumBlock, totalSamples - offset);
        const auto* left = buffer.getReadPointer (0) + offset;
        const auto* right = inChannels > 1 && bufferChannels > 1
                                ? buffer.getReadPointer (1) + offset
                                : left;

        auto* workL = workBuffer.getWritePointer (0);
        auto* workR = workBuffer.getWritePointer (1);

        for (int i = 0; i < n; ++i)
        {
            workL[i] = left[i];
            workR[i] = right[i];
            inputSum += (double) left[i] * (double) left[i]
                        + (double) right[i] * (double) right[i];
            inputSamples += 2;
        }

        processChunk (n, params, transport);

        if (outChannels == 1)
        {
            auto* out = buffer.getWritePointer (0) + offset;
            const auto* outL = workBuffer.getReadPointer (0);
            const auto* outR = workBuffer.getReadPointer (1);

            for (int i = 0; i < n; ++i)
                out[i] = 0.5f * (outL[i] + outR[i]);
        }
        else
        {
            auto* outL = buffer.getWritePointer (0) + offset;
            auto* outR = bufferChannels > 1 ? buffer.getWritePointer (1) + offset : nullptr;
            const auto* srcL = workBuffer.getReadPointer (0);
            const auto* srcR = workBuffer.getReadPointer (1);

            for (int i = 0; i < n; ++i)
            {
                outL[i] = srcL[i];

                if (outR != nullptr)
                    outR[i] = srcR[i];
            }
        }
    }

    const double inputRms = std::sqrt (inputSum / (double) juce::jmax ((int64_t) 1, inputSamples));
    inputRmsDb.store (gainToDb ((float) inputRms), std::memory_order_relaxed);

    const double outputRms = std::sqrt (meterOutputSum / (double) juce::jmax ((int64_t) 1, meterOutputSamples));
    outputRmsDb.store (gainToDb ((float) outputRms), std::memory_order_relaxed);
    outputPeakDb.store (gainToDb (meterOutputPeak), std::memory_order_relaxed);
}

void Engine::processChunk (int numSamples, const RackParams& params, const TransportInfo& transport)
{
    const int n = numSamples;

    // Input trim, plus the two delay-aligned dry taps used by mix and bypass.
    for (int i = 0; i < n; ++i)
    {
        const float gain = inputGain.getNextValue();
        const float rawL = workBuffer.getReadPointer (0)[i];
        const float rawR = workBuffer.getReadPointer (1)[i];
        const float trimL = rawL * gain;
        const float trimR = rawR * gain;

        dryRaw[0].write (rawL);
        dryRaw[1].write (rawR);
        dryTrimmed[0].write (trimL);
        dryTrimmed[1].write (trimR);
        workBuffer.getWritePointer (0)[i] = trimL;
        workBuffer.getWritePointer (1)[i] = trimR;
    }

    expander.process (workBuffer, n, params);

    if (quality == QualityMode::mix)
    {
        denoiser.setParameters (params.dnBypass ? 0.0f : params.dnAmount, params.dnMaxReduction);
        denoiser.pushInput (workBuffer, 0, n);
        denoiser.renderNextBlock (denoiseBuffer, n);

        for (int ch = 0; ch < 2; ++ch)
            workBuffer.copyFrom (ch, 0, denoiseBuffer, ch, 0, n);
    }

    if (pitchActive)
        pushDetectorFeed (workBuffer, n);

    // Keep a latency-aligned dry path. When tuning is inactive the phase
    // vocoder is bypassed completely; this avoids its low-frequency smearing
    // and "flanged" character on tracks that are not being corrected.
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* work = workBuffer.getWritePointer (ch);

        for (int i = 0; i < n; ++i)
            shifterDry[ch].write (work[i]);
    }

    shifter.pushInput (workBuffer, 0, n);
    shifter.renderNextBlock (shifterWet, n);

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* work = workBuffer.getWritePointer (ch);

        for (int i = 0; i < n; ++i)
        {
            if (pitchActive)
                work[i] = shifterWet.getReadPointer (ch)[i];
            else
                work[i] = shifterDry[ch].read ((float) shifterDelay + (float) (n - i));
        }
    }

    cleanupEq.process (workBuffer, n, params);
    dynamicEq.process (workBuffer, n, params);
    compressor.process (workBuffer, n, params);
    deEsser.process (workBuffer, n, params);
    toneEq.process (workBuffer, n, params);
    saturation.process (workBuffer, n, params);

    for (int ch = 0; ch < 2; ++ch)
        sendBuffer.copyFrom (ch, 0, workBuffer, ch, 0, n);

    delay.process (sendBuffer, delayWet, n, params, transport.valid ? transport.bpm : -1.0, params.dlyBypass);
    reverb.process (sendBuffer, reverbWet, n, params);

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* wet = workBuffer.getWritePointer (ch);
        const auto* d = delayWet.getReadPointer (ch);
        const auto* r = reverbWet.getReadPointer (ch);

        for (int i = 0; i < n; ++i)
            wet[i] += d[i] + r[i];
    }

    // Global dry/processed mix, output trim, then the final limiter.
    float fade = masterFade;

    for (int i = 0; i < n; ++i)
    {
        const float mix = mixGain.getNextValue();
        const float output = outputGain.getNextValue();
        const float delaySamples = (float) prelimiterLatency + (float) (n - i);

        for (int ch = 0; ch < 2; ++ch)
        {
            const float raw = dryRaw[ch].read (delaySamples);
            const float trimmed = dryTrimmed[ch].read (delaySamples);
            const float processed = workBuffer.getReadPointer (ch)[i];
            const float value = params.bypass
                                    ? raw
                                    : (trimmed + (processed - trimmed) * mix) * output;
            workBuffer.getWritePointer (ch)[i] = value * fade;
        }

        fade = juce::jmin (1.0f, fade + masterFadeStep);
    }

    masterFade = fade;

    limiter.setForceUnity (params.bypass);
    limiter.process (workBuffer, n, params);

    compGrDb.store (compressor.getGainReductionDb(), std::memory_order_relaxed);
    deEsserGrDb.store (deEsser.getReductionDb(), std::memory_order_relaxed);
    dynamicEqGrDb.store (dynamicEq.getGainReductionDb(), std::memory_order_relaxed);
    dynamicEqBandGrDb[0].store (dynamicEq.getBandGainReductionDb (0), std::memory_order_relaxed);
    dynamicEqBandGrDb[1].store (dynamicEq.getBandGainReductionDb (1), std::memory_order_relaxed);
    limiterGrDb.store (limiter.getGainReductionDb(), std::memory_order_relaxed);
    delayMs.store (delay.getDelayMs(), std::memory_order_relaxed);

    for (int i = 0; i < n; ++i)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            const float value = workBuffer.getReadPointer (ch)[i];
            const float absolute = std::abs (value);
            meterOutputPeak = juce::jmax (meterOutputPeak, absolute);
            meterOutputSum += (double) value * (double) value;
            ++meterOutputSamples;
        }
    }
}

PitchStatus Engine::getPitchStatus() const noexcept
{
    PitchStatus status;
    status.detectedMidi = statusDetectedMidi.load (std::memory_order_relaxed);
    status.targetMidi = statusTargetMidi.load (std::memory_order_relaxed);
    status.cents = statusCents.load (std::memory_order_relaxed);
    status.confidence = statusConfidence.load (std::memory_order_relaxed);
    status.voiced = statusVoiced.load (std::memory_order_relaxed);
    status.active = statusActive.load (std::memory_order_relaxed);
    return status;
}

} // namespace vr
