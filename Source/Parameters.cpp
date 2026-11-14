#include "Parameters.h"

#include <cmath>

namespace vr
{

namespace
{
const std::vector<ParamSpec>& specTable()
{
    using PT = ParamType;
    using PG = ParamGroup;

    static const std::vector<ParamSpec> specs = {
        // id, name, unit, min, max, default, skew, type, choices, group, description
        { "global.input",  "Input",       "dB", -24.0f, 24.0f,   0.0f, 1.0f, PT::floating, 0, nullptr, PG::global, "Input trim before the vocal chain." },
        { "global.output", "Output",      "dB", -24.0f, 24.0f,   0.0f, 1.0f, PT::floating, 0, nullptr, PG::global, "Master output trim, before the limiter." },
        { "global.mix",    "Mix",         "%",    0.0f, 100.0f, 100.0f, 1.0f, PT::floating, 0, nullptr, PG::global, "Blend between the latency-aligned dry input and the processed vocal." },
        { "global.bypass", "Bypass",      "",     0.0f,   1.0f,   0.0f, 1.0f, PT::boolean,  0, nullptr, PG::global, "Global bypass. Returns the latency-aligned original input." },

        { "exp.bypass",    "Expander",    "",     0.0f,   1.0f,   1.0f, 1.0f, PT::boolean,  0, nullptr, PG::cleanup, "Bypass the gentle downward expander." },
        { "exp.threshold", "Threshold",   "dB", -80.0f, -20.0f, -50.0f, 1.0f, PT::floating, 0, nullptr, PG::cleanup, "Level below which the expander starts closing." },
        { "exp.range",     "Range",       "dB",   0.0f,  40.0f,  18.0f, 1.0f, PT::floating, 0, nullptr, PG::cleanup, "Maximum attenuation applied between phrases." },
        { "exp.attack",    "Attack",      "ms",   0.1f,  50.0f,   2.0f, 0.5f, PT::floating, 0, nullptr, PG::cleanup, "How quickly the expander opens again." },
        { "exp.hold",      "Hold",        "ms",   0.0f, 300.0f,  80.0f, 1.0f, PT::floating, 0, nullptr, PG::cleanup, "Keeps the gate open through short gaps." },
        { "exp.release",   "Release",     "ms",  10.0f, 1000.0f, 200.0f, 0.6f, PT::floating, 0, nullptr, PG::cleanup, "How quickly the expander closes after the hold." },

        { "dn.bypass",     "Denoise",     "",     0.0f,   1.0f,   1.0f, 1.0f, PT::boolean,  0, nullptr, PG::cleanup, "Bypass continuous spectral noise suppression." },
        { "dn.amount",     "Amount",      "%",    0.0f, 100.0f,  35.0f, 1.0f, PT::floating, 0, nullptr, PG::cleanup, "Strength of steady broadband noise suppression." },
        { "dn.maxReduction","Max Reduce", "dB",    0.0f,  30.0f,  12.0f, 1.0f, PT::floating, 0, nullptr, PG::cleanup, "Bounded attenuation so the voice is never erased." },

        { "pitch.bypass",  "Pitch",       "",     0.0f,   1.0f,   1.0f, 1.0f, PT::boolean,  0, nullptr, PG::pitch, "Bypass automatic pitch correction." },
        { "pitch.amount",  "Amount",      "%",    0.0f, 100.0f,  60.0f, 1.0f, PT::floating, 0, nullptr, PG::pitch, "How far detected notes are pulled toward the scale." },
        { "pitch.retune",  "Retune",      "ms",   0.5f, 1000.0f, 80.0f, 0.35f, PT::floating, 0, nullptr, PG::pitch, "Retune time constant. Short values snap, long values preserve movement." },
        { "pitch.humanize","Humanize",    "%",    0.0f, 100.0f,  35.0f, 1.0f, PT::floating, 0, nullptr, PG::pitch, "Leaves fast expression and gentle drift intact." },
        { "pitch.a4",      "A4",          "Hz", 400.0f, 480.0f, 440.0f, 1.0f, PT::floating, 0, nullptr, PG::pitch, "Reference for A4 / MIDI 69." },
        { "pitch.tonic",   "Key",          "",    0.0f,  11.0f,   0.0f, 1.0f, PT::choice,  12, "C|C#|D|D#|E|F|F#|G|G#|A|A#|B", PG::pitch, "Tonic of the song key." },
        { "pitch.scale",   "Scale",        "",    0.0f,   3.0f,   0.0f, 1.0f, PT::choice,   4, "Chromatic|Major|Minor|Custom", PG::pitch, "Scale used for target selection." },
        { "pitch.mask",    "Notes",        "",    0.0f, 4095.0f, 4095.0f, 1.0f, PT::intValue, 0, nullptr, PG::pitch, "12-bit mask of enabled pitch classes. Empty means no correction." },
        { "pitch.range",   "Range",        "",    0.0f,   2.0f,   1.0f, 1.0f, PT::choice,   3, "Low (65-350 Hz)|Full (65-1100 Hz)|High (150-1500 Hz)", PG::pitch, "Input range for pitch tracking." },
        { "pitch.formant", "Formant",       "",    0.0f,   1.0f,   0.0f, 1.0f, PT::boolean,  0, nullptr, PG::pitch, "Preserve the vocal-tract formant envelope (Mix only)." },
        { "pitch.formantShift","Formant Shift","st",-12.0f, 12.0f, 0.0f, 1.0f, PT::floating, 0, nullptr, PG::pitch, "Move the preserved formant envelope relative to the shifted pitch (Mix only)." },

        { "deq1.on",        "B1 On",       "",     0.0f,  1.0f,  0.0f, 1.0f, PT::boolean,  0, nullptr, PG::dynamicEq, "Enable dynamic EQ band 1." },
        { "deq1.freq",      "B1 Freq",     "Hz",  40.0f, 16000.0f, 300.0f, 0.3f, PT::floating, 0, nullptr, PG::dynamicEq, "Dynamic EQ band 1 frequency." },
        { "deq1.q",         "B1 Q",        "",     0.2f,  10.0f,  1.0f, 1.0f, PT::floating, 0, nullptr, PG::dynamicEq, "Dynamic EQ band 1 Q." },
        { "deq1.gain",      "B1 Gain",     "dB", -12.0f, 12.0f,  0.0f, 1.0f, PT::floating, 0, nullptr, PG::dynamicEq, "Static peaking gain for dynamic EQ band 1." },
        { "deq1.threshold", "B1 Threshold","dB", -60.0f, 0.0f, -30.0f, 1.0f, PT::floating, 0, nullptr, PG::dynamicEq, "Dynamic reduction threshold for band 1." },
        { "deq1.ratio",     "B1 Ratio",    ":1",   1.0f, 10.0f,  2.0f, 1.0f, PT::floating, 0, nullptr, PG::dynamicEq, "Dynamic reduction ratio for band 1." },
        { "deq1.range",     "B1 Range",    "dB",   0.0f, 24.0f,  8.0f, 1.0f, PT::floating, 0, nullptr, PG::dynamicEq, "Maximum dynamic cut for band 1." },
        { "deq1.attack",    "B1 Attack",   "ms",   0.1f, 100.0f, 10.0f, 1.0f, PT::floating, 0, nullptr, PG::dynamicEq, "Dynamic EQ band 1 attack." },
        { "deq1.release",   "B1 Release",  "ms",  10.0f, 1000.0f,120.0f, 1.0f, PT::floating, 0, nullptr, PG::dynamicEq, "Dynamic EQ band 1 release." },
        { "deq2.on",        "B2 On",       "",     0.0f,  1.0f,  0.0f, 1.0f, PT::boolean,  0, nullptr, PG::dynamicEq, "Enable dynamic EQ band 2." },
        { "deq2.freq",      "B2 Freq",     "Hz",  40.0f, 16000.0f, 3000.0f, 0.3f, PT::floating, 0, nullptr, PG::dynamicEq, "Dynamic EQ band 2 frequency." },
        { "deq2.q",         "B2 Q",        "",     0.2f,  10.0f,  1.0f, 1.0f, PT::floating, 0, nullptr, PG::dynamicEq, "Dynamic EQ band 2 Q." },
        { "deq2.gain",      "B2 Gain",     "dB", -12.0f, 12.0f,  0.0f, 1.0f, PT::floating, 0, nullptr, PG::dynamicEq, "Static peaking gain for dynamic EQ band 2." },
        { "deq2.threshold", "B2 Threshold","dB", -60.0f, 0.0f, -30.0f, 1.0f, PT::floating, 0, nullptr, PG::dynamicEq, "Dynamic reduction threshold for band 2." },
        { "deq2.ratio",     "B2 Ratio",    ":1",   1.0f, 10.0f,  2.0f, 1.0f, PT::floating, 0, nullptr, PG::dynamicEq, "Dynamic reduction ratio for band 2." },
        { "deq2.range",     "B2 Range",    "dB",   0.0f, 24.0f,  8.0f, 1.0f, PT::floating, 0, nullptr, PG::dynamicEq, "Maximum dynamic cut for band 2." },
        { "deq2.attack",    "B2 Attack",   "ms",   0.1f, 100.0f, 5.0f, 1.0f, PT::floating, 0, nullptr, PG::dynamicEq, "Dynamic EQ band 2 attack." },
        { "deq2.release",   "B2 Release",  "ms",  10.0f, 1000.0f,80.0f, 1.0f, PT::floating, 0, nullptr, PG::dynamicEq, "Dynamic EQ band 2 release." },
        { "ceq.bypass",    "Cleanup EQ","",     0.0f,   1.0f,   0.0f, 1.0f, PT::boolean,  0, nullptr, PG::eq, "Bypass the high-pass and two corrective cuts." },
        { "ceq.hpf",       "High-pass",  "Hz",  20.0f, 500.0f,  80.0f, 0.5f, PT::floating, 0, nullptr, PG::eq, "Variable 12 dB/octave high-pass." },
        { "ceq.cut1Freq",  "Cut 1 Freq", "Hz",  80.0f, 2000.0f, 300.0f, 0.5f, PT::floating, 0, nullptr, PG::eq, "First corrective cut frequency." },
        { "ceq.cut1Gain",  "Cut 1 Gain", "dB", -18.0f,   0.0f,   0.0f, 1.0f, PT::floating, 0, nullptr, PG::eq, "First corrective cut depth." },
        { "ceq.cut1Q",     "Cut 1 Q",    "",    0.3f,   8.0f,   1.0f, 0.7f, PT::floating, 0, nullptr, PG::eq, "First corrective cut Q." },
        { "ceq.cut2Freq",  "Cut 2 Freq", "Hz", 300.0f, 8000.0f, 2500.0f, 0.5f, PT::floating, 0, nullptr, PG::eq, "Second corrective cut frequency." },
        { "ceq.cut2Gain",  "Cut 2 Gain", "dB", -18.0f,   0.0f,   0.0f, 1.0f, PT::floating, 0, nullptr, PG::eq, "Second corrective cut depth." },
        { "ceq.cut2Q",     "Cut 2 Q",    "",    0.3f,   8.0f,   1.0f, 0.7f, PT::floating, 0, nullptr, PG::eq, "Second corrective cut Q." },

        { "teq.bypass",    "Tone EQ",    "",     0.0f,   1.0f,   0.0f, 1.0f, PT::boolean,  0, nullptr, PG::eq, "Bypass the low shelf, presence bell and high shelf." },
        { "teq.lowFreq",   "Low Freq",   "Hz",  30.0f, 500.0f, 120.0f, 0.5f, PT::floating, 0, nullptr, PG::eq, "Low shelf frequency." },
        { "teq.lowGain",   "Low Gain",   "dB", -12.0f,  12.0f,   0.0f, 1.0f, PT::floating, 0, nullptr, PG::eq, "Low shelf gain." },
        { "teq.presFreq",  "Presence",   "Hz", 500.0f, 8000.0f, 3000.0f, 0.5f, PT::floating, 0, nullptr, PG::eq, "Presence bell frequency." },
        { "teq.presGain",  "Pres Gain",  "dB", -12.0f,  12.0f,   0.0f, 1.0f, PT::floating, 0, nullptr, PG::eq, "Presence bell gain." },
        { "teq.presQ",     "Pres Q",     "",    0.3f,   8.0f,   0.9f, 0.7f, PT::floating, 0, nullptr, PG::eq, "Presence bell Q." },
        { "teq.highFreq",  "High Freq",  "Hz", 2000.0f, 16000.0f, 9000.0f, 0.5f, PT::floating, 0, nullptr, PG::eq, "High shelf frequency." },
        { "teq.highGain",  "High Gain",  "dB", -12.0f,  12.0f,   0.0f, 1.0f, PT::floating, 0, nullptr, PG::eq, "High shelf gain." },

        { "comp.bypass",   "Compressor", "",     0.0f,   1.0f,   0.0f, 1.0f, PT::boolean,  0, nullptr, PG::comp, "Bypass the compressor." },
        { "comp.threshold","Threshold",  "dB", -60.0f,   0.0f, -24.0f, 1.0f, PT::floating, 0, nullptr, PG::comp, "Compressor threshold." },
        { "comp.ratio",    "Ratio",      ":1",   1.0f,  20.0f,   3.0f, 0.6f, PT::floating, 0, nullptr, PG::comp, "Compression ratio." },
        { "comp.attack",   "Attack",     "ms",   0.1f, 100.0f,  12.0f, 0.4f, PT::floating, 0, nullptr, PG::comp, "Compressor attack." },
        { "comp.release",  "Release",    "ms",  10.0f, 1000.0f, 120.0f, 0.6f, PT::floating, 0, nullptr, PG::comp, "Compressor release." },
        { "comp.knee",     "Knee",       "dB",   0.0f,  30.0f,   8.0f, 1.0f, PT::floating, 0, nullptr, PG::comp, "Soft-knee width." },
        { "comp.makeup",   "Makeup",     "dB", -12.0f,  24.0f,   0.0f, 1.0f, PT::floating, 0, nullptr, PG::comp, "Intentional makeup gain." },
        { "comp.mix",      "Mix",        "%",    0.0f, 100.0f, 100.0f, 1.0f, PT::floating, 0, nullptr, PG::comp, "Parallel compression blend." },
        { "comp.detHPF",   "Det HPF",    "Hz",  20.0f, 500.0f, 120.0f, 0.5f, PT::floating, 0, nullptr, PG::comp, "High-pass in the detector path." },

        { "ds.bypass",     "De-esser",   "",     0.0f,   1.0f,   1.0f, 1.0f, PT::boolean,  0, nullptr, PG::deesser, "Bypass split-band de-essing." },
        { "ds.freq",       "Detect",     "Hz", 3000.0f, 12000.0f, 6500.0f, 0.5f, PT::floating, 0, nullptr, PG::deesser, "Detection band centre." },
        { "ds.threshold",  "Threshold",  "dB", -60.0f,   0.0f, -30.0f, 1.0f, PT::floating, 0, nullptr, PG::deesser, "Sibilance threshold." },
        { "ds.range",      "Range",      "dB",   0.0f,  30.0f,   8.0f, 1.0f, PT::floating, 0, nullptr, PG::deesser, "Maximum high-band attenuation." },
        { "ds.release",    "Release",    "ms",   1.0f, 500.0f,  80.0f, 0.5f, PT::floating, 0, nullptr, PG::deesser, "De-esser release." },
        { "ds.audition",   "Audition",   "",     0.0f,   1.0f,   0.0f, 1.0f, PT::boolean,  0, nullptr, PG::deesser, "Solo the detection band for tuning." },

        { "sat.bypass",    "Saturation", "",     0.0f,   1.0f,   1.0f, 1.0f, PT::boolean,  0, nullptr, PG::saturation, "Bypass saturation." },
        { "sat.mode",      "Mode",       "",     0.0f,   1.0f,   0.0f, 1.0f, PT::choice,   2, "Tape|Edge", PG::saturation, "Tape rounding or bright edge." },
        { "sat.drive",     "Drive",      "dB",   0.0f,  24.0f,   4.0f, 1.0f, PT::floating, 0, nullptr, PG::saturation, "Drive into the non-linearity." },
        { "sat.blend",     "Blend",      "%",    0.0f, 100.0f, 100.0f, 1.0f, PT::floating, 0, nullptr, PG::saturation, "Dry/wet blend, latency aligned." },
        { "sat.output",    "Output",     "dB", -12.0f,  12.0f,   0.0f, 1.0f, PT::floating, 0, nullptr, PG::saturation, "Saturation output compensation." },

        { "dly.bypass",    "Delay",      "",     0.0f,   1.0f,   1.0f, 1.0f, PT::boolean,  0, nullptr, PG::delay, "Bypass the parallel delay return." },
        { "dly.sync",      "Sync",       "",     0.0f,   1.0f,   1.0f, 1.0f, PT::boolean,  0, nullptr, PG::delay, "Lock delay time to host tempo." },
        { "dly.ms",        "Time",       "ms",  10.0f, 2000.0f, 375.0f, 0.5f, PT::floating, 0, nullptr, PG::delay, "Manual delay time." },
        { "dly.division",  "Division",   "",     0.0f,   4.0f,   2.0f, 1.0f, PT::choice,   5, "1/32|1/16|1/8|1/4|1/2", PG::delay, "Tempo division." },
        { "dly.feel",      "Feel",       "",     0.0f,   2.0f,   0.0f, 1.0f, PT::choice,   3, "Straight|Dotted|Triplet", PG::delay, "Straight, dotted or triplet feel." },
        { "dly.feedback",  "Feedback",   "%",    0.0f,  95.0f,  25.0f, 1.0f, PT::floating, 0, nullptr, PG::delay, "Delay feedback, bounded below self-oscillation." },
        { "dly.level",     "Return",     "dB", -60.0f,   0.0f, -12.0f, 1.0f, PT::floating, 0, nullptr, PG::delay, "Delay return level." },
        { "dly.lowCut",    "Low Cut",    "Hz",  20.0f, 1000.0f, 180.0f, 0.5f, PT::floating, 0, nullptr, PG::delay, "High-pass in the delay path." },
        { "dly.highCut",   "High Cut",   "Hz", 1000.0f, 20000.0f, 9000.0f, 0.5f, PT::floating, 0, nullptr, PG::delay, "Low-pass in the delay path." },
        { "dly.pingpong",  "Ping-Pong",  "",     0.0f,   1.0f,   0.0f, 1.0f, PT::boolean,  0, nullptr, PG::delay, "Cross-feed the delay between channels." },
        { "dly.duck",      "Ducking",    "%",    0.0f, 100.0f,   0.0f, 1.0f, PT::floating, 0, nullptr, PG::delay, "Duck the return from the dry vocal." },
        { "dly.manualBpm", "Manual BPM", "BPM", 40.0f, 240.0f, 120.0f, 1.0f, PT::floating, 0, nullptr, PG::delay, "Tempo fallback when the host provides none." },

        { "rev.bypass",    "Reverb",     "",     0.0f,   1.0f,   1.0f, 1.0f, PT::boolean,  0, nullptr, PG::reverb, "Bypass the parallel reverb return." },
        { "rev.type",      "Type",       "",     0.0f,   1.0f,   0.0f, 1.0f, PT::choice,   2, "Room|Plate", PG::reverb, "Room or plate algorithm." },
        { "rev.predelay",  "Pre-delay",  "ms",   0.0f, 150.0f,  20.0f, 1.0f, PT::floating, 0, nullptr, PG::reverb, "Reverb pre-delay." },
        { "rev.decay",     "Decay",      "s",    0.3f,   8.0f,   1.6f, 0.5f, PT::floating, 0, nullptr, PG::reverb, "Reverb decay time (RT60 approximation)." },
        { "rev.damping",   "Damping",    "%",    0.0f, 100.0f,  40.0f, 1.0f, PT::floating, 0, nullptr, PG::reverb, "High-frequency damping in the tail." },
        { "rev.lowCut",    "Low Cut",    "Hz",  20.0f, 1000.0f, 120.0f, 0.5f, PT::floating, 0, nullptr, PG::reverb, "High-pass on the reverb return." },
        { "rev.highCut",   "High Cut",   "Hz", 1000.0f, 20000.0f, 9000.0f, 0.5f, PT::floating, 0, nullptr, PG::reverb, "Low-pass on the reverb return." },
        { "rev.level",     "Return",     "dB", -60.0f,   0.0f, -14.0f, 1.0f, PT::floating, 0, nullptr, PG::reverb, "Reverb return level." },
        { "rev.duck",      "Ducking",    "%",    0.0f, 100.0f,   0.0f, 1.0f, PT::floating, 0, nullptr, PG::reverb, "Duck the return from the dry vocal." },

        { "lim.bypass",    "Limiter",    "",     0.0f,   1.0f,   0.0f, 1.0f, PT::boolean,  0, nullptr, PG::limiter, "Bypass the final limiter." },
        { "lim.truePeak",  "True Peak",  "",     0.0f,   1.0f,   0.0f, 1.0f, PT::boolean,  0, nullptr, PG::limiter, "Enable the 8x oversampled True Peak limiter (Mix only)." },
        { "lim.ceiling",   "Ceiling",    "dBFS", -6.0f,   0.0f,  -0.3f, 1.0f, PT::floating, 0, nullptr, PG::limiter, "Sample-peak ceiling, enforced with lookahead." },
        { "lim.release",   "Release",    "ms",   1.0f, 500.0f,  80.0f, 0.5f, PT::floating, 0, nullptr, PG::limiter, "Limiter release." },
    };

    return specs;
}
} // namespace

const std::vector<ParamSpec>& getParamSpecs()
{
    return specTable();
}

const ParamSpec* findParamSpec (const juce::String& id)
{
    for (const auto& spec : specTable())
        if (id == spec.id)
            return &spec;

    return nullptr;
}

juce::String paramGroupName (ParamGroup group)
{
    switch (group)
    {
        case ParamGroup::global:     return "Master";
        case ParamGroup::cleanup:    return "Cleanup";
        case ParamGroup::pitch:      return "Pitch";
        case ParamGroup::eq:         return "EQ";
        case ParamGroup::dynamicEq:  return "Dynamic EQ";
        case ParamGroup::comp:       return "Dynamics";
        case ParamGroup::deesser:    return "De-esser";
        case ParamGroup::saturation: return "Color";
        case ParamGroup::delay:      return "Delay";
        case ParamGroup::reverb:     return "Reverb";
        case ParamGroup::limiter:    return "Limiter";
    }

    return {};
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& spec : specTable())
    {
        const juce::String id (spec.id);
        const juce::String name (spec.name);

        switch (spec.type)
        {
            case ParamType::floating:
            {
                juce::NormalisableRange<float> range (spec.minValue, spec.maxValue, 0.0f, spec.skew);
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { id, 1 }, name, range, spec.defaultValue,
                    juce::AudioParameterFloatAttributes().withLabel (spec.unit)));
                break;
            }

            case ParamType::boolean:
                layout.add (std::make_unique<juce::AudioParameterBool> (
                    juce::ParameterID { id, 1 }, name, spec.defaultValue >= 0.5f));
                break;

            case ParamType::choice:
            {
                juce::StringArray choices;
                choices.addTokens (juce::String (spec.choices), "|", "");
                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    juce::ParameterID { id, 1 }, name, choices, (int) spec.defaultValue));
                break;
            }

            case ParamType::intValue:
                layout.add (std::make_unique<juce::AudioParameterInt> (
                    juce::ParameterID { id, 1 }, name, (int) spec.minValue, (int) spec.maxValue,
                    (int) spec.defaultValue));
                break;
        }
    }

    return layout;
}

ParamCache::ParamCache (juce::AudioProcessorValueTreeState& state)
{
    auto get = [&state] (const char* id)
    {
        auto* value = state.getRawParameterValue (id);
        jassert (value != nullptr);
        return value;
    };

    p.inputDb = get ("global.input");
    p.outputDb = get ("global.output");
    p.mix = get ("global.mix");
    p.bypass = get ("global.bypass");

    p.expBypass = get ("exp.bypass");
    p.expThreshold = get ("exp.threshold");
    p.expRange = get ("exp.range");
    p.expAttack = get ("exp.attack");
    p.expHold = get ("exp.hold");
    p.expRelease = get ("exp.release");

    p.dnBypass = get ("dn.bypass");
    p.dnAmount = get ("dn.amount");
    p.dnMaxReduction = get ("dn.maxReduction");

    p.pitchBypass = get ("pitch.bypass");
    p.pitchAmount = get ("pitch.amount");
    p.pitchRetune = get ("pitch.retune");
    p.pitchHumanize = get ("pitch.humanize");
    p.pitchA4 = get ("pitch.a4");
    p.pitchTonic = get ("pitch.tonic");
    p.pitchScale = get ("pitch.scale");
    p.pitchMask = get ("pitch.mask");
    p.pitchRange = get ("pitch.range");
    p.pitchFormant = get ("pitch.formant");
    p.pitchFormantShift = get ("pitch.formantShift");

    p.deq1On = get ("deq1.on");
    p.deq1Freq = get ("deq1.freq");
    p.deq1Q = get ("deq1.q");
    p.deq1Gain = get ("deq1.gain");
    p.deq1Threshold = get ("deq1.threshold");
    p.deq1Ratio = get ("deq1.ratio");
    p.deq1Range = get ("deq1.range");
    p.deq1Attack = get ("deq1.attack");
    p.deq1Release = get ("deq1.release");
    p.deq2On = get ("deq2.on");
    p.deq2Freq = get ("deq2.freq");
    p.deq2Q = get ("deq2.q");
    p.deq2Gain = get ("deq2.gain");
    p.deq2Threshold = get ("deq2.threshold");
    p.deq2Ratio = get ("deq2.ratio");
    p.deq2Range = get ("deq2.range");
    p.deq2Attack = get ("deq2.attack");
    p.deq2Release = get ("deq2.release");

    p.ceqBypass = get ("ceq.bypass");
    p.ceqHpfFreq = get ("ceq.hpf");
    p.ceqCut1Freq = get ("ceq.cut1Freq");
    p.ceqCut1Gain = get ("ceq.cut1Gain");
    p.ceqCut1Q = get ("ceq.cut1Q");
    p.ceqCut2Freq = get ("ceq.cut2Freq");
    p.ceqCut2Gain = get ("ceq.cut2Gain");
    p.ceqCut2Q = get ("ceq.cut2Q");

    p.teqBypass = get ("teq.bypass");
    p.teqLowFreq = get ("teq.lowFreq");
    p.teqLowGain = get ("teq.lowGain");
    p.teqPresFreq = get ("teq.presFreq");
    p.teqPresGain = get ("teq.presGain");
    p.teqPresQ = get ("teq.presQ");
    p.teqHighFreq = get ("teq.highFreq");
    p.teqHighGain = get ("teq.highGain");

    p.compBypass = get ("comp.bypass");
    p.compThreshold = get ("comp.threshold");
    p.compRatio = get ("comp.ratio");
    p.compAttack = get ("comp.attack");
    p.compRelease = get ("comp.release");
    p.compKnee = get ("comp.knee");
    p.compMakeup = get ("comp.makeup");
    p.compMix = get ("comp.mix");
    p.compDetectorHpf = get ("comp.detHPF");

    p.dsBypass = get ("ds.bypass");
    p.dsFreq = get ("ds.freq");
    p.dsThreshold = get ("ds.threshold");
    p.dsRange = get ("ds.range");
    p.dsRelease = get ("ds.release");
    p.dsAudition = get ("ds.audition");

    p.satBypass = get ("sat.bypass");
    p.satMode = get ("sat.mode");
    p.satDrive = get ("sat.drive");
    p.satBlend = get ("sat.blend");
    p.satOutput = get ("sat.output");

    p.dlyBypass = get ("dly.bypass");
    p.dlySync = get ("dly.sync");
    p.dlyMs = get ("dly.ms");
    p.dlyDivision = get ("dly.division");
    p.dlyFeel = get ("dly.feel");
    p.dlyFeedback = get ("dly.feedback");
    p.dlyLevel = get ("dly.level");
    p.dlyLowCut = get ("dly.lowCut");
    p.dlyHighCut = get ("dly.highCut");
    p.dlyPingPong = get ("dly.pingpong");
    p.dlyDuck = get ("dly.duck");
    p.dlyManualBpm = get ("dly.manualBpm");

    p.revBypass = get ("rev.bypass");
    p.revType = get ("rev.type");
    p.revPredelay = get ("rev.predelay");
    p.revDecay = get ("rev.decay");
    p.revDamping = get ("rev.damping");
    p.revLowCut = get ("rev.lowCut");
    p.revHighCut = get ("rev.highCut");
    p.revLevel = get ("rev.level");
    p.revDuck = get ("rev.duck");

    p.limBypass = get ("lim.bypass");
    p.limTruePeak = get ("lim.truePeak");
    p.limCeiling = get ("lim.ceiling");
    p.limRelease = get ("lim.release");
}

void ParamCache::snapshot (RackParams& out) const noexcept
{
    auto f = [] (const std::atomic<float>* v) noexcept { return v->load (std::memory_order_relaxed); };
    auto b = [] (const std::atomic<float>* v) noexcept { return v->load (std::memory_order_relaxed) > 0.5f; };
    auto i = [] (const std::atomic<float>* v) noexcept { return (int) std::lround (v->load (std::memory_order_relaxed)); };

    out.inputDb = f (p.inputDb);
    out.outputDb = f (p.outputDb);
    out.mix = f (p.mix) * 0.01f;
    out.bypass = b (p.bypass);

    out.expBypass = b (p.expBypass);
    out.expThreshold = f (p.expThreshold);
    out.expRange = f (p.expRange);
    out.expAttack = f (p.expAttack);
    out.expHold = f (p.expHold);
    out.expRelease = f (p.expRelease);

    out.dnBypass = b (p.dnBypass);
    out.dnAmount = f (p.dnAmount);
    out.dnMaxReduction = f (p.dnMaxReduction);

    out.pitchBypass = b (p.pitchBypass);
    out.pitchAmount = f (p.pitchAmount);
    out.pitchRetune = f (p.pitchRetune);
    out.pitchHumanize = f (p.pitchHumanize);
    out.pitchA4 = f (p.pitchA4);
    out.pitchTonic = i (p.pitchTonic);
    out.pitchScale = i (p.pitchScale);
    out.pitchMask = i (p.pitchMask);
    out.pitchRange = i (p.pitchRange);
    out.pitchFormantPreserve = b (p.pitchFormant);
    out.pitchFormantShift = f (p.pitchFormantShift);

    out.deq1On = b (p.deq1On);
    out.deq1Freq = f (p.deq1Freq);
    out.deq1Q = f (p.deq1Q);
    out.deq1Gain = f (p.deq1Gain);
    out.deq1Threshold = f (p.deq1Threshold);
    out.deq1Ratio = f (p.deq1Ratio);
    out.deq1Range = f (p.deq1Range);
    out.deq1Attack = f (p.deq1Attack);
    out.deq1Release = f (p.deq1Release);
    out.deq2On = b (p.deq2On);
    out.deq2Freq = f (p.deq2Freq);
    out.deq2Q = f (p.deq2Q);
    out.deq2Gain = f (p.deq2Gain);
    out.deq2Threshold = f (p.deq2Threshold);
    out.deq2Ratio = f (p.deq2Ratio);
    out.deq2Range = f (p.deq2Range);
    out.deq2Attack = f (p.deq2Attack);
    out.deq2Release = f (p.deq2Release);

    out.ceqBypass = b (p.ceqBypass);
    out.ceqHpfFreq = f (p.ceqHpfFreq);
    out.ceqCut1Freq = f (p.ceqCut1Freq);
    out.ceqCut1Gain = f (p.ceqCut1Gain);
    out.ceqCut1Q = f (p.ceqCut1Q);
    out.ceqCut2Freq = f (p.ceqCut2Freq);
    out.ceqCut2Gain = f (p.ceqCut2Gain);
    out.ceqCut2Q = f (p.ceqCut2Q);

    out.teqBypass = b (p.teqBypass);
    out.teqLowFreq = f (p.teqLowFreq);
    out.teqLowGain = f (p.teqLowGain);
    out.teqPresFreq = f (p.teqPresFreq);
    out.teqPresGain = f (p.teqPresGain);
    out.teqPresQ = f (p.teqPresQ);
    out.teqHighFreq = f (p.teqHighFreq);
    out.teqHighGain = f (p.teqHighGain);

    out.compBypass = b (p.compBypass);
    out.compThreshold = f (p.compThreshold);
    out.compRatio = f (p.compRatio);
    out.compAttack = f (p.compAttack);
    out.compRelease = f (p.compRelease);
    out.compKnee = f (p.compKnee);
    out.compMakeup = f (p.compMakeup);
    out.compMix = f (p.compMix);
    out.compDetectorHpf = f (p.compDetectorHpf);

    out.dsBypass = b (p.dsBypass);
    out.dsFreq = f (p.dsFreq);
    out.dsThreshold = f (p.dsThreshold);
    out.dsRange = f (p.dsRange);
    out.dsRelease = f (p.dsRelease);
    out.dsAudition = b (p.dsAudition);

    out.satBypass = b (p.satBypass);
    out.satMode = i (p.satMode);
    out.satDrive = f (p.satDrive);
    out.satBlend = f (p.satBlend);
    out.satOutput = f (p.satOutput);

    out.dlyBypass = b (p.dlyBypass);
    out.dlySync = b (p.dlySync);
    out.dlyMs = f (p.dlyMs);
    out.dlyDivision = i (p.dlyDivision);
    out.dlyFeel = i (p.dlyFeel);
    out.dlyFeedback = f (p.dlyFeedback);
    out.dlyLevel = f (p.dlyLevel);
    out.dlyLowCut = f (p.dlyLowCut);
    out.dlyHighCut = f (p.dlyHighCut);
    out.dlyPingPong = b (p.dlyPingPong);
    out.dlyDuck = f (p.dlyDuck);
    out.dlyManualBpm = f (p.dlyManualBpm);

    out.revBypass = b (p.revBypass);
    out.revType = i (p.revType);
    out.revPredelay = f (p.revPredelay);
    out.revDecay = f (p.revDecay);
    out.revDamping = f (p.revDamping);
    out.revLowCut = f (p.revLowCut);
    out.revHighCut = f (p.revHighCut);
    out.revLevel = f (p.revLevel);
    out.revDuck = f (p.revDuck);

    out.limBypass = b (p.limBypass);
    out.limTruePeak = b (p.limTruePeak);
    out.limCeiling = f (p.limCeiling);
    out.limRelease = f (p.limRelease);
}

int maskForScale (int scaleIndex, int tonic) noexcept
{
    if (scaleIndex == 0)
        return 0x0FFF;

    if (scaleIndex == 3)
        return 0; // caller should use the explicit custom mask

    static constexpr int major[] = { 0, 2, 4, 5, 7, 9, 11 };
    static constexpr int minor[] = { 0, 2, 3, 5, 7, 8, 10 };

    int mask = 0;
    const int root = positiveModulo12 (tonic);
    const auto* intervals = scaleIndex == 1 ? major : minor;

    for (int i = 0; i < 7; ++i)
        mask |= 1 << positiveModulo12 (root + intervals[i]);

    return mask & 0x0FFF;
}

juce::String pitchClassName (int pitchClass)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[positiveModulo12 (pitchClass)];
}

juce::String midiNoteName (int midiNote)
{
    const int pc = positiveModulo12 (midiNote);
    const int octave = (int) std::floor ((double) midiNote / 12.0) - 1;
    return pitchClassName (pc) + juce::String (octave);
}

juce::String noteNameForFrequency (double hz, double a4)
{
    if (hz <= 1.0 || a4 <= 1.0)
        return "--";

    const double midi = 69.0 + 12.0 * std::log2 (hz / a4);
    const int nearest = (int) std::lround (midi);
    const int cents = (int) std::lround ((midi - (double) nearest) * 100.0);

    juce::String result = midiNoteName (nearest);

    if (cents != 0)
        result << (cents > 0 ? " +" : " ") << cents << "c";

    return result;
}

} // namespace vr
