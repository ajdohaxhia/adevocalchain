#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <cstdint>
#include <vector>

namespace vr
{

enum class QualityMode
{
    track = 0,
    mix = 1
};

enum class ParamType
{
    floating = 0,
    boolean,
    choice,
    intValue
};

enum class ParamGroup
{
    global = 0,
    cleanup,
    pitch,
    eq,
    dynamicEq,
    comp,
    deesser,
    saturation,
    delay,
    reverb,
    limiter
};

struct ParamSpec
{
    const char* id;
    const char* name;
    const char* unit;
    float minValue;
    float maxValue;
    float defaultValue;
    float skew;
    ParamType type;
    int numChoices;
    const char* choices;
    ParamGroup group;
    const char* description;
};

const std::vector<ParamSpec>& getParamSpecs();
const ParamSpec* findParamSpec (const juce::String& id);
juce::String paramGroupName (ParamGroup group);

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Plain, allocation-free snapshot of every automatable parameter. */
struct RackParams
{
    // Global
    float inputDb = 0.0f;
    float outputDb = 0.0f;
    float mix = 1.0f;
    bool bypass = false;

    // Downward expander
    bool expBypass = true;
    float expThreshold = -50.0f;
    float expRange = 18.0f;
    float expAttack = 2.0f;
    float expHold = 80.0f;
    float expRelease = 200.0f;

    // Spectral denoiser
    bool dnBypass = true;
    float dnAmount = 35.0f;
    float dnMaxReduction = 12.0f;

    // Pitch correction
    bool pitchBypass = true;
    float pitchAmount = 60.0f;
    float pitchRetune = 80.0f;
    float pitchHumanize = 35.0f;
    float pitchA4 = 440.0f;
    int pitchTonic = 0;
    int pitchScale = 0;
    int pitchMask = 0x0FFF;
    int pitchRange = 1;
    bool pitchFormantPreserve = false;
    float pitchFormantShift = 0.0f;

    // Dynamic EQ
    bool deq1On = false;
    float deq1Freq = 300.0f;
    float deq1Q = 1.0f;
    float deq1Gain = 0.0f;
    float deq1Threshold = -30.0f;
    float deq1Ratio = 2.0f;
    float deq1Range = 8.0f;
    float deq1Attack = 10.0f;
    float deq1Release = 120.0f;
    bool deq2On = false;
    float deq2Freq = 3000.0f;
    float deq2Q = 1.0f;
    float deq2Gain = 0.0f;
    float deq2Threshold = -30.0f;
    float deq2Ratio = 2.0f;
    float deq2Range = 8.0f;
    float deq2Attack = 5.0f;
    float deq2Release = 80.0f;

    // Cleanup EQ
    bool ceqBypass = false;
    float ceqHpfFreq = 80.0f;
    float ceqCut1Freq = 300.0f;
    float ceqCut1Gain = 0.0f;
    float ceqCut1Q = 1.0f;
    float ceqCut2Freq = 2500.0f;
    float ceqCut2Gain = 0.0f;
    float ceqCut2Q = 1.0f;

    // Tone EQ
    bool teqBypass = false;
    float teqLowFreq = 120.0f;
    float teqLowGain = 0.0f;
    float teqPresFreq = 3000.0f;
    float teqPresGain = 0.0f;
    float teqPresQ = 0.9f;
    float teqHighFreq = 9000.0f;
    float teqHighGain = 0.0f;

    // Compressor
    bool compBypass = false;
    float compThreshold = -24.0f;
    float compRatio = 3.0f;
    float compAttack = 12.0f;
    float compRelease = 120.0f;
    float compKnee = 8.0f;
    float compMakeup = 0.0f;
    float compMix = 100.0f;
    float compDetectorHpf = 120.0f;

    // De-esser
    bool dsBypass = true;
    float dsFreq = 6500.0f;
    float dsThreshold = -30.0f;
    float dsRange = 8.0f;
    float dsRelease = 80.0f;
    bool dsAudition = false;

    // Saturation
    bool satBypass = true;
    int satMode = 0;
    float satDrive = 4.0f;
    float satBlend = 100.0f;
    float satOutput = 0.0f;

    // Delay
    bool dlyBypass = true;
    bool dlySync = true;
    float dlyMs = 375.0f;
    int dlyDivision = 2;
    int dlyFeel = 0;
    float dlyFeedback = 25.0f;
    float dlyLevel = -12.0f;
    float dlyLowCut = 180.0f;
    float dlyHighCut = 9000.0f;
    bool dlyPingPong = false;
    float dlyDuck = 0.0f;
    float dlyManualBpm = 120.0f;

    // Reverb
    bool revBypass = true;
    int revType = 0;
    float revPredelay = 20.0f;
    float revDecay = 1.6f;
    float revDamping = 40.0f;
    float revLowCut = 120.0f;
    float revHighCut = 9000.0f;
    float revLevel = -14.0f;
    float revDuck = 0.0f;

    // Limiter
    bool limBypass = false;
    float limCeiling = -0.3f;
    float limRelease = 80.0f;
};

/** Caches the APVTS atomic values so the audio thread can snapshot them without lookups. */
class ParamCache
{
public:
    explicit ParamCache (juce::AudioProcessorValueTreeState& state);

    void snapshot (RackParams& out) const noexcept;

private:
    struct P
    {
        std::atomic<float>* inputDb = nullptr;
        std::atomic<float>* outputDb = nullptr;
        std::atomic<float>* mix = nullptr;
        std::atomic<float>* bypass = nullptr;

        std::atomic<float>* expBypass = nullptr;
        std::atomic<float>* expThreshold = nullptr;
        std::atomic<float>* expRange = nullptr;
        std::atomic<float>* expAttack = nullptr;
        std::atomic<float>* expHold = nullptr;
        std::atomic<float>* expRelease = nullptr;

        std::atomic<float>* dnBypass = nullptr;
        std::atomic<float>* dnAmount = nullptr;
        std::atomic<float>* dnMaxReduction = nullptr;

        std::atomic<float>* pitchBypass = nullptr;
        std::atomic<float>* pitchAmount = nullptr;
        std::atomic<float>* pitchRetune = nullptr;
        std::atomic<float>* pitchHumanize = nullptr;
        std::atomic<float>* pitchA4 = nullptr;
        std::atomic<float>* pitchTonic = nullptr;
        std::atomic<float>* pitchScale = nullptr;
        std::atomic<float>* pitchMask = nullptr;
        std::atomic<float>* pitchRange = nullptr;
        std::atomic<float>* pitchFormant = nullptr;
        std::atomic<float>* pitchFormantShift = nullptr;

        std::atomic<float>* deq1On = nullptr;
        std::atomic<float>* deq1Freq = nullptr;
        std::atomic<float>* deq1Q = nullptr;
        std::atomic<float>* deq1Gain = nullptr;
        std::atomic<float>* deq1Threshold = nullptr;
        std::atomic<float>* deq1Ratio = nullptr;
        std::atomic<float>* deq1Range = nullptr;
        std::atomic<float>* deq1Attack = nullptr;
        std::atomic<float>* deq1Release = nullptr;
        std::atomic<float>* deq2On = nullptr;
        std::atomic<float>* deq2Freq = nullptr;
        std::atomic<float>* deq2Q = nullptr;
        std::atomic<float>* deq2Gain = nullptr;
        std::atomic<float>* deq2Threshold = nullptr;
        std::atomic<float>* deq2Ratio = nullptr;
        std::atomic<float>* deq2Range = nullptr;
        std::atomic<float>* deq2Attack = nullptr;
        std::atomic<float>* deq2Release = nullptr;

        std::atomic<float>* ceqBypass = nullptr;
        std::atomic<float>* ceqHpfFreq = nullptr;
        std::atomic<float>* ceqCut1Freq = nullptr;
        std::atomic<float>* ceqCut1Gain = nullptr;
        std::atomic<float>* ceqCut1Q = nullptr;
        std::atomic<float>* ceqCut2Freq = nullptr;
        std::atomic<float>* ceqCut2Gain = nullptr;
        std::atomic<float>* ceqCut2Q = nullptr;

        std::atomic<float>* teqBypass = nullptr;
        std::atomic<float>* teqLowFreq = nullptr;
        std::atomic<float>* teqLowGain = nullptr;
        std::atomic<float>* teqPresFreq = nullptr;
        std::atomic<float>* teqPresGain = nullptr;
        std::atomic<float>* teqPresQ = nullptr;
        std::atomic<float>* teqHighFreq = nullptr;
        std::atomic<float>* teqHighGain = nullptr;

        std::atomic<float>* compBypass = nullptr;
        std::atomic<float>* compThreshold = nullptr;
        std::atomic<float>* compRatio = nullptr;
        std::atomic<float>* compAttack = nullptr;
        std::atomic<float>* compRelease = nullptr;
        std::atomic<float>* compKnee = nullptr;
        std::atomic<float>* compMakeup = nullptr;
        std::atomic<float>* compMix = nullptr;
        std::atomic<float>* compDetectorHpf = nullptr;

        std::atomic<float>* dsBypass = nullptr;
        std::atomic<float>* dsFreq = nullptr;
        std::atomic<float>* dsThreshold = nullptr;
        std::atomic<float>* dsRange = nullptr;
        std::atomic<float>* dsRelease = nullptr;
        std::atomic<float>* dsAudition = nullptr;

        std::atomic<float>* satBypass = nullptr;
        std::atomic<float>* satMode = nullptr;
        std::atomic<float>* satDrive = nullptr;
        std::atomic<float>* satBlend = nullptr;
        std::atomic<float>* satOutput = nullptr;

        std::atomic<float>* dlyBypass = nullptr;
        std::atomic<float>* dlySync = nullptr;
        std::atomic<float>* dlyMs = nullptr;
        std::atomic<float>* dlyDivision = nullptr;
        std::atomic<float>* dlyFeel = nullptr;
        std::atomic<float>* dlyFeedback = nullptr;
        std::atomic<float>* dlyLevel = nullptr;
        std::atomic<float>* dlyLowCut = nullptr;
        std::atomic<float>* dlyHighCut = nullptr;
        std::atomic<float>* dlyPingPong = nullptr;
        std::atomic<float>* dlyDuck = nullptr;
        std::atomic<float>* dlyManualBpm = nullptr;

        std::atomic<float>* revBypass = nullptr;
        std::atomic<float>* revType = nullptr;
        std::atomic<float>* revPredelay = nullptr;
        std::atomic<float>* revDecay = nullptr;
        std::atomic<float>* revDamping = nullptr;
        std::atomic<float>* revLowCut = nullptr;
        std::atomic<float>* revHighCut = nullptr;
        std::atomic<float>* revLevel = nullptr;
        std::atomic<float>* revDuck = nullptr;

        std::atomic<float>* limBypass = nullptr;
        std::atomic<float>* limCeiling = nullptr;
        std::atomic<float>* limRelease = nullptr;
    };

    P p;
};

// --- Musical helpers shared by DSP, UI and tests -----------------------------

inline int positiveModulo12 (int value) noexcept
{
    const int r = value % 12;
    return r < 0 ? r + 12 : r;
}

/** Scale indices: 0 chromatic, 1 major, 2 minor, 3 custom. */
int maskForScale (int scaleIndex, int tonic) noexcept;

juce::String pitchClassName (int pitchClass);
juce::String midiNoteName (int midiNote);
juce::String noteNameForFrequency (double hz, double a4);

inline float mixGain (float mixPercent) noexcept
{
    return juce::jlimit (0.0f, 1.0f, mixPercent * 0.01f);
}

} // namespace vr
