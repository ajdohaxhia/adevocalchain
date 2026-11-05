#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <complex>
#include <cstdint>
#include <memory>
#include <vector>

namespace vr
{

/**
    Streaming spectral noise suppressor.

    75% overlap STFT with a minimum-tracking noise estimate, Wiener-style
    bounded gain, fast attack and slow release.  The same gain is applied to
    both channels, so the stereo image is preserved.  It handles steady
    broadband noise (fan, hiss, room rumble); it is not a transient or
    click remover.  Latency is exactly one analysis frame.
*/
class SpectralDenoiser
{
public:
    void prepare (double sampleRate, int numChannels, int maximumBlockSize, int frameSize);
    void reset();

    void setParameters (float amountPercent, float maxReductionDb) noexcept;
    void pushInput (const juce::AudioBuffer<float>& input, int startSample, int numSamples) noexcept;
    void renderNextBlock (juce::AudioBuffer<float>& wetOutput, int numSamples) noexcept;

    int getLatencySamples() const noexcept { return frameSize; }
    int getFrameSize() const noexcept { return frameSize; }
    bool isPrepared() const noexcept { return prepared; }

private:
    void processFrame (int centre) noexcept;
    void fftForward (const std::complex<float>* time, std::complex<float>* spectrum) noexcept;
    void fftInverse (const std::complex<float>* spectrum, std::complex<float>* time) noexcept;

    bool prepared = false;
    double sampleRate = 48000.0;
    int channels = 1;
    int frameSize = 1024;
    int hopSize = 256;
    int maximumBlock = 512;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;
    std::vector<float> windowSquared;

    static constexpr int inputRingSize = 1 << 17;
    static constexpr int inputRingMask = inputRingSize - 1;
    static constexpr int olaRingSize = 1 << 16;
    static constexpr int olaRingMask = olaRingSize - 1;

    std::vector<float> inputRing;
    std::vector<float> olaRing;
    std::vector<float> windowSum;

    std::vector<std::complex<float>> timeL;
    std::vector<std::complex<float>> timeR;
    std::vector<std::complex<float>> specL;
    std::vector<std::complex<float>> specR;
    std::vector<float> noiseEstimate;
    std::vector<float> smoothedGain;
    std::vector<float> outputGain;

    int64_t inputWritePos = 0;
    int64_t outputReadPos = 0;
    double nextFrameCentre = 0.0;

    float amount = 0.35f;
    float gainFloor = 0.25f;
    int numBins = 0;
};

} // namespace vr
