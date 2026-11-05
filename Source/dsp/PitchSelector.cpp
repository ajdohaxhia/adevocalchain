#include "PitchSelector.h"

#include <algorithm>
#include <cmath>

namespace vr
{

void PitchSelector::reset() noexcept
{
    smoothedMidi = 60.0;
    hasSmoothed = false;
    previousTarget = -1;
    drift.reset();
    driftPrepared = false;
}

void PitchSelector::setSettings (const PitchSelectionSettings& newSettings) noexcept
{
    if (newSettings.scaleIndex != settings.scaleIndex
        || newSettings.tonic != settings.tonic
        || newSettings.mask != settings.mask)
    {
        previousTarget = -1;
    }

    const bool rangeChanged = newSettings.rangeIndex != settings.rangeIndex;
    settings = newSettings;
    settings.mask &= 0x0FFF;
    settings.amount = juce::jlimit (0.0, 1.0, settings.amount);
    settings.humanize = juce::jlimit (0.0, 1.0, settings.humanize);
    settings.retuneMs = juce::jlimit (0.5, 1000.0, settings.retuneMs);

    if (rangeChanged)
        previousTarget = -1;
}

int PitchSelector::getEffectiveMask() const noexcept
{
    if (settings.scaleIndex == 3)
        return settings.mask & 0x0FFF;

    return maskForScale (settings.scaleIndex, settings.tonic);
}

int PitchSelector::nearestAllowedMidi (double midiFloat, int previous) const noexcept
{
    const int mask = getEffectiveMask();

    if ((mask & 0x0FFF) == 0)
        return -1;

    const int centre = (int) std::lround (midiFloat);
    const int low = centre - 24;
    const int high = centre + 24;

    int bestNote = -1;
    int previousBest = -1;
    double bestDistance = 1.0e9;

    for (int note = low; note <= high; ++note)
    {
        const int pc = positiveModulo12 (note);

        if ((mask & (1 << pc)) == 0)
            continue;

        const double distance = std::abs (midiFloat - (double) note);

        if (distance < bestDistance - 1.0e-9)
        {
            bestDistance = distance;
            bestNote = note;
            previousBest = note;
        }
        else if (std::abs (distance - bestDistance) <= 1.0e-9)
        {
            // Deterministic tie-break: prefer the note previously selected,
            // otherwise the lower absolute note.
            if (note == previous)
                bestNote = note;
            else if (bestNote != previous && note < bestNote)
                bestNote = note;
        }

        juce::ignoreUnused (previousBest);
    }

    // Hysteresis: keep the current legal target while it is within 30 cents of
    // being equally close.  This suppresses flicker at semitone boundaries
    // without hiding a genuinely wrong note.
    if (previous >= 0
        && (mask & (1 << positiveModulo12 (previous))) != 0
        && bestNote >= 0
        && previous != bestNote)
    {
        const double previousDistance = std::abs (midiFloat - (double) previous);

        if (previousDistance <= 0.55
            && previousDistance <= bestDistance + 0.30)
        {
            return previous;
        }
    }

    return bestNote;
}

PitchSelection PitchSelector::selectMidi (double midiFloat, int previous) const noexcept
{
    PitchSelection result;
    result.detectedMidi = midiFloat;
    result.detectedNote = (int) std::lround (midiFloat);
    result.targetNote = nearestAllowedMidi (midiFloat, previous);

    if (result.targetNote < 0)
        return result;

    result.valid = true;
    result.cents = ((double) result.targetNote - midiFloat) * 100.0 * settings.amount;
    result.ratio = std::exp2 (juce::jlimit (-1200.0, 1200.0, result.cents) / 1200.0);
    return result;
}

PitchSelection PitchSelector::process (double frequencyHz, double deltaSeconds) noexcept
{
    const double midi = frequencyToMidi (frequencyHz, settings.a4);

    if (! hasSmoothed)
    {
        smoothedMidi = midi;
        hasSmoothed = true;
    }
    else
    {
        // Humanize slows the pitch used for target selection so natural
        // vibrato and fast runs stay expressive while the centre is tuned.
        const double tau = juce::jmax (0.002, settings.humanize * 0.22);
        const double dt = juce::jlimit (0.0, 0.20, deltaSeconds);
        const double alpha = 1.0 - std::exp (-dt / tau);
        smoothedMidi += alpha * (midi - smoothedMidi);
    }

    PitchSelection result = selectMidi (smoothedMidi, previousTarget);

    if (result.valid)
    {
        if (! driftPrepared)
        {
            drift.prepare (48000.0, 0.17f);
            drift.reset (0.37f);
            driftPrepared = true;
        }

        // The drift generator is updated once per audio callback-ish interval.
        // Keep it very small: it is a musical loosening, not a detune effect.
        const double driftCents = (double) drift.process() * settings.humanize * 2.5;
        result.cents += driftCents;
        result.ratio = std::exp2 (juce::jlimit (-1200.0, 1200.0, result.cents) / 1200.0);
        previousTarget = result.targetNote;
    }

    return result;
}

} // namespace vr
