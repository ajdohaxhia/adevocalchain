#pragma once

#include "../Parameters.h"
#include "Common.h"

#include <cstdint>
#include <vector>

namespace vr
{

struct PitchEstimate
{
    double timestampSamples = 0.0;  // absolute input sample at the analysis window centre
    float frequency = 0.0f;
    float confidence = 0.0f;
    float rms = 0.0f;
    bool voiced = false;
};

/**
    Monophonic YIN detector with a decimated low-pass front end.

    Track uses a 384-sample analysis window at ~15 kHz (about 26 ms) and Mix a
    768-sample window (about 52 ms).  The engine anchors the window to the
    newest available input, which keeps the estimate aligned without an extra
    detector delay beyond the pitch shifter's existing lookahead.
*/
class PitchDetector
{
public:
    static constexpr int kMaxWindow = 1024;

    void prepare (double sampleRate, QualityMode quality);
    void reset() noexcept;
    void setRange (int rangeIndex) noexcept;

    void process (const float* mono, int numSamples) noexcept;

    PitchEstimate analyseAt (double centreFullRateSamples) noexcept;
    PitchEstimate getLatest() const noexcept { return latest; }

    double getSampleRate() const noexcept { return sampleRate; }
    double getDetectorRate() const noexcept { return detectorRate; }
    int getWindowSize() const noexcept { return windowSize; }
    int getAnalyzedSamples() const noexcept { return windowSize * decimation; }

private:
    void updateLowpass() noexcept;
    PitchEstimate runYin() noexcept;

    double sampleRate = 48000.0;
    double detectorRate = 16000.0;
    int decimation = 3;
    int windowSize = 384;
    int tauMin = 10;
    int tauMax = 240;
    int rangeIndex = 1;
    QualityMode quality = QualityMode::track;

    Biquad lowpassA;
    Biquad lowpassB;
    int decimationCounter = 0;

    std::vector<float> ring;
    int ringMask = 0;
    int64_t totalDecimated = 0;

    std::vector<float> window;
    std::vector<double> difference;
    std::vector<double> cmnd;

    PitchEstimate latest;
    PitchEstimate cached;
    bool cacheValid = false;
    int64_t lastStart = -1000000;
};

} // namespace vr
