#pragma once

#include "Parameters.h"
#include "dsp/Modules.h"
#include "dsp/PitchDetector.h"
#include "dsp/PitchSelector.h"
#include "dsp/PitchShifter.h"
#include "dsp/Reverb.h"
#include "dsp/SpectralDenoiser.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <cstdint>
#include <vector>

namespace vr
{

struct TransportInfo
{
    bool valid = false;
    double bpm = 120.0;
    bool playing = false;
};

struct PitchStatus
{
    float detectedMidi = 0.0f;
    float targetMidi = -1.0f;
    float cents = 0.0f;
    float confidence = 0.0f;
    bool voiced = false;
    bool active = false;
};

/**
    Host-independent vocal rack engine.

    The whole chain is preallocated in prepare().  process() takes no locks,
    allocates nothing, and is safe for any block size (it chunks to the prepared
    maximum).  Every path is aligned to getLatencySamples().
*/
class Engine : private PitchShifter::PitchProvider
{
public:
    Engine() = default;

    void prepare (double sampleRate, int maximumBlockSize, int inputChannels,
                  int outputChannels, QualityMode quality);
    void releaseResources();
    void reset();

    void process (juce::AudioBuffer<float>& buffer,
                  const RackParams& params,
                  const TransportInfo& transport,
                  int inputChannels,
                  int outputChannels);

    int getLatencySamples() const noexcept { return latencySamples; }
    int getPrelimiterLatencySamples() const noexcept { return prelimiterLatency; }
    int getLimiterLatencySamples() const noexcept { return limiterLatency; }
    int getShifterLatencySamples() const noexcept { return shifter.getLatencySamples(); }
    int getDenoiserLatencySamples() const noexcept { return denoiserLatency; }
    int getSaturationLatencySamples() const noexcept { return saturation.getLatencySamples(); }
    QualityMode getQuality() const noexcept { return quality; }
    bool isPrepared() const noexcept { return prepared; }

    void triggerStateFade() noexcept { stateFadeRequested.store (true, std::memory_order_relaxed); }

    float getInputRmsDb() const noexcept { return inputRmsDb.load (std::memory_order_relaxed); }
    float getOutputRmsDb() const noexcept { return outputRmsDb.load (std::memory_order_relaxed); }
    float getOutputPeakDb() const noexcept { return outputPeakDb.load (std::memory_order_relaxed); }
    float getCompressorGainReductionDb() const noexcept { return compGrDb.load (std::memory_order_relaxed); }
    float getLimiterGainReductionDb() const noexcept { return limiterGrDb.load (std::memory_order_relaxed); }
    float getDeEsserGainReductionDb() const noexcept { return deEsserGrDb.load (std::memory_order_relaxed); }
    float getDelayMs() const noexcept { return delayMs.load (std::memory_order_relaxed); }

    PitchStatus getPitchStatus() const noexcept;

private:
    PitchShifter::PitchInfo getPitchAt (double analysisTimeSamples) override;

    void pushDetectorFeed (const juce::AudioBuffer<float>& buffer, int numSamples);
    void processChunk (int numSamples, const RackParams& params, const TransportInfo& transport);
    void updatePitchSettings (const RackParams& params);

    double sampleRate = 48000.0;
    int maximumBlock = 512;
    int preparedInputChannels = 2;
    int preparedOutputChannels = 2;
    QualityMode quality = QualityMode::track;
    bool prepared = false;

    int shifterFrameSize = 512;
    int shifterDelay = 704;
    int denoiserFrameSize = 0;
    int denoiserLatency = 0;
    int limiterLatency = 16;
    int prelimiterLatency = 704;
    int latencySamples = 720;

    Expander expander;
    SpectralDenoiser denoiser;
    PitchDetector detector;
    PitchSelector selector;
    PitchShifter shifter;
    CleanupEQ cleanupEq;
    Compressor compressor;
    DeEsser deEsser;
    ToneEQ toneEq;
    Saturation saturation;
    StereoDelay delay;
    Reverb reverb;
    LookaheadLimiter limiter;

    juce::AudioBuffer<float> workBuffer;
    juce::AudioBuffer<float> denoiseBuffer;
    juce::AudioBuffer<float> shifterWet;
    juce::AudioBuffer<float> sendBuffer;
    juce::AudioBuffer<float> delayWet;
    juce::AudioBuffer<float> reverbWet;
    juce::AudioBuffer<float> monoScratch;

    DelayLine dryRaw[2];
    DelayLine dryTrimmed[2];
    DelayLine shifterDry[2];

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outputGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mixGain;

    RackParams activeParams;
    double lastProviderTime = -1.0;
    double ratioLog2 = 0.0;
    bool ratioInitialised = false;
    int lastRangeIndex = -1;
    int lastMask = -1;
    int lastScale = -1;
    int lastTonic = -1;
    bool pitchActive = false;
    float lastDetectedFrequency = 0.0f;
    float lastTargetMidi = -1.0f;
    float lastCents = 0.0f;
    float lastConfidence = 0.0f;
    bool lastVoiced = false;

    std::atomic<bool> stateFadeRequested { false };
    float masterFade = 1.0f;
    float masterFadeStep = 1.0f;

    std::atomic<float> inputRmsDb { -120.0f };
    std::atomic<float> outputRmsDb { -120.0f };
    std::atomic<float> outputPeakDb { -120.0f };
    std::atomic<float> compGrDb { 0.0f };
    std::atomic<float> limiterGrDb { 0.0f };
    std::atomic<float> deEsserGrDb { 0.0f };
    std::atomic<float> delayMs { 375.0f };

    std::atomic<float> statusDetectedMidi { 0.0f };
    std::atomic<float> statusTargetMidi { -1.0f };
    std::atomic<float> statusCents { 0.0f };
    std::atomic<float> statusConfidence { 0.0f };
    std::atomic<bool> statusVoiced { false };
    std::atomic<bool> statusActive { false };

    double meterOutputSum = 0.0;
    float meterOutputPeak = 0.0f;
    int64_t meterOutputSamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Engine)
};

} // namespace vr
