#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <cstdint>
#include <vector>

namespace vr
{

/**
    Streaming STFT phase-vocoder pitch shifter.

    This is a purpose-built adaptation of the MIT-licensed PitchLab/MegaTune
    phase-vocoder (Copyright (c) 2026 PitchLab), reduced to the two frame sizes
    the Vocal Rack needs and extended with a 512-sample Track mode, an arbitrary
    reported delay and a provider that can change ratio every hop.

    Frames are Hann-windowed, bins are remapped to their new frequency position,
    synthesis phases are accumulated from instantaneous frequency, and output is
    overlap-added with a window-sum normaliser.  prepare() owns all storage.
*/
class PitchShifter
{
public:
    struct PitchInfo
    {
        float ratio = 1.0f;
        bool voiced = false;
    };

    class PitchProvider
    {
    public:
        virtual ~PitchProvider() = default;
        virtual PitchInfo getPitchAt (double analysisTimeSamples) = 0;
    };

    PitchShifter() = default;

    void prepare (double sampleRate, int numChannels, int maximumBlockSize,
                  int frameSize, int delaySamples);
    void reset();
    void setPitchProvider (PitchProvider* provider) noexcept { pitchProvider = provider; }
    void setFormantOptions (bool preserve, float shiftSemitones) noexcept
    {
        formantPreserve = preserve;
        formantShiftSemitones = juce::jlimit (-12.0f, 12.0f, shiftSemitones);
    }

    void pushInput (const juce::AudioBuffer<float>& input, int startSample, int numSamples) noexcept;
    void renderNextBlock (juce::AudioBuffer<float>& wetOutput, int numSamples) noexcept;

    int getLatencySamples() const noexcept { return delaySamples; }
    int getFrameSize() const noexcept { return state.size; }
    int getHopSize() const noexcept { return state.hop; }
    uint64_t getProcessedFrameCount() const noexcept { return processedFrames; }

    static constexpr float minimumRatio = 0.5f;
    static constexpr float maximumRatio = 2.0f;

private:
    struct FftState
    {
        int size = 0;
        int hop = 0;
        std::vector<float> window;
        std::vector<float> fftRe;
        std::vector<float> fftIm;
        std::vector<float> twiddleCos;
        std::vector<float> twiddleSin;
        std::vector<int> bitReverse;

        std::vector<float> magnitude;
        std::vector<float> omega;
        std::vector<float> phase;

        std::vector<float> logMagnitude;
        std::vector<float> envelope;
        std::vector<float> envelopePrefix;
        std::vector<float> targetMagnitude;
        std::vector<float> targetPhase;
        std::vector<int> targetSource;
        std::vector<uint8_t> targetActive;
        std::vector<int> peaks;

        std::vector<std::vector<float>> previousPhase;
        std::vector<std::vector<float>> phaseAccumulator;
        std::vector<std::vector<uint8_t>> phaseActive;
        std::vector<uint8_t> previousPhaseValid;
    };

    float readInputSample (int channel, int64_t position) const noexcept;
    void buildFftState (int frameSize, int numChannels);
    void resetPhaseState() noexcept;
    void resetOverlapAdd() noexcept;
    void fft (bool inverse) noexcept;
    void processFrame (int centre, float ratio) noexcept;

    double sampleRate = 48000.0;
    int channels = 1;
    int maximumBlock = 512;
    int delaySamples = 0;

    static constexpr int inputRingSize = 1 << 17;
    static constexpr int inputRingMask = inputRingSize - 1;
    static constexpr int olaRingSize = 1 << 16;
    static constexpr int olaRingMask = olaRingSize - 1;

    std::vector<float> inputRing;
    std::vector<float> olaRing;
    std::vector<float> windowSum;

    int64_t inputWritePos = 0;
    int64_t outputReadPos = 0;
    double nextFrameCentre = 0.0;

    FftState state;
    PitchProvider* pitchProvider = nullptr;
    bool formantPreserve = false;
    float formantShiftSemitones = 0.0f;
    uint64_t processedFrames = 0;
};

} // namespace vr
