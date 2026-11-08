# ademusic Vocal Rack 2.0.0 — formant-preserving pitch

A production-focused pitch release. The existing DSP topology, preset
library and session format are preserved; two new Mix-mode formant parameters
are added with backward-compatible migration.

## New: formant-preserving pitch

- **Formant Pres.** toggle (`pitch.formant`) in the Pitch panel. Available in
  Mix quality; disabled in Track, where the extra envelope processing does not
  fit the 15 ms budget.
- **Formant Shift** (`pitch.formantShift`, −12…+12 semitones) moves the
  preserved vocal-tract envelope independently of pitch.
- Real spectral-envelope processing inside the phase vocoder: a ~1/6-octave
  log-magnitude envelope is estimated with prefix-sum smoothing and applied per
  bin. This is not an EQ macro.
- Active only when pitch correction is doing real work; the transparent bypass
  and unity-ratio OLA paths are unchanged. Formant processing is off by default.

Measured on a synthetic harmonic/formant fixture: free 1.5× shift moved the
spectral centroid from **1807 Hz**; formant preservation kept it at **1375 Hz**
toward the original 1200 Hz envelope.

## Compatibility

- Existing **82 parameter IDs** keep their meaning and automation.
- Two new parameters are appended; total **84**.
- Old user presets that omit `pitch.formant` and `pitch.formantShift` validate
  with documented defaults (off/0 st). Factory presets include the defaults.
- Existing factory presets (96), user preset import/export, A/B snapshots,
  Track/Mix behavior and session recall remain compatible.
- No silent reset of old projects.

## Still not implemented in 2.0

The following were audited but are **not shipped as claimed features**:

- True Peak limiting (sample-peak limiter remains; no dBTP claim).
- Dynamic EQ / multiband dynamics.
- Loudness-matched A/B
- Vocal Assistant
- Quick macro redesign
- Waveform/EQ/GR visualizations

These remain explicit gaps in `docs/AUDIT_2.0.md`, not hidden behind controls.

## Validation

- 418 headless checks.
- New formant regression fixture and legacy-preset migration checks.
- pluginval strictness 7 VST3 + AU; auval; host harness.
- Track 720 samples / 15.00 ms; Mix 3716 / 77.42 ms at 48 kHz.

## Install

```bash
unzip -q ademusic-vocal-rack-2.0.0-macOS-arm64.zip
cp -R "ademusic-vocal-rack-2.0.0-macOS-arm64/VST3/ademusic Vocal Rack.vst3" \
      ~/Library/Audio/Plug-Ins/VST3/
cp -R "ademusic-vocal-rack-2.0.0-macOS-arm64/Components/ademusic Vocal Rack.component" \
      ~/Library/Audio/Plug-Ins/Components/
```

Ad-hoc signed, not notarized. AGPL-3.0-or-later; JUCE 8.0.15 dual
AGPL/commercial; MIT phase-vocoder notice retained.
