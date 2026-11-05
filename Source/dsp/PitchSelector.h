#pragma once

#include "Common.h"
#include "../Parameters.h"

#include <cstdint>

namespace vr
{

struct PitchSelectionSettings
{
    int scaleIndex = 0;
    int tonic = 0;
    int mask = 0x0FFF;
    double a4 = 440.0;
    double amount = 0.6;       // 0..1
    double retuneMs = 80.0;
    double humanize = 0.35;    // 0..1
    int rangeIndex = 1;
};

struct PitchSelection
{
    double detectedMidi = 0.0;
    int detectedNote = -1;
    int targetNote = -1;
    double cents = 0.0;
    double ratio = 1.0;
    bool valid = false;
};

/**
    Pure target-note selection and ratio generation.

    Targets are chosen in absolute MIDI-note space.  Pitch classes use a
    positive modulo so negative MIDI indices behave identically across octaves.
    If the enabled-note mask is empty no correction is applied.
*/
class PitchSelector
{
public:
    void reset() noexcept;
    void setSettings (const PitchSelectionSettings& newSettings) noexcept;

    const PitchSelectionSettings& getSettings() const noexcept { return settings; }
    int getEffectiveMask() const noexcept;

    /** Nearest enabled absolute MIDI note, or -1 when the mask is empty. */
    int nearestAllowedMidi (double midiFloat, int previousTarget) const noexcept;

    /** Selection without smoothing or humanize drift (used by regression tests). */
    PitchSelection selectMidi (double midiFloat, int previousTarget) const noexcept;

    /** Stateful path used by the audio engine. */
    PitchSelection process (double frequencyHz, double deltaSeconds) noexcept;

    double getSmoothedMidi() const noexcept { return smoothedMidi; }
    void seedSmoothedMidi (double midi) noexcept { smoothedMidi = midi; hasSmoothed = true; }

    static double frequencyToMidi (double hz, double a4) noexcept
    {
        return 69.0 + 12.0 * std::log2 (juce::jmax (1.0e-6, hz) / juce::jmax (1.0, a4));
    }

private:
    PitchSelectionSettings settings;
    double smoothedMidi = 60.0;
    bool hasSmoothed = false;
    int previousTarget = -1;
    SmoothRandom drift;
    bool driftPrepared = false;
};

} // namespace vr
