# ademusic Vocal Rack 2.2.0 — LUFS and True Peak metering

## Added

- Short-term K-weighted **LUFS-S** meter (3 s time constant, not a gated
  integrated-LUFS implementation).
- 4× true-peak output meter, active when True Peak mode is enabled.
- Footer readout: `LUFS-S ... TP ... dBTP`.
- A/B loudness matching now uses the LUFS-S measurement rather than
  block RMS; comparison gain remains bounded, smoothed and non-destructive.

## Existing 2.1 features

- Two-band dynamic EQ with real-coefficient graph, draggable handles and GR
  metering.
- Optional 8× oversampled True Peak limiter with an independent 8×
  reference test and a conservative 1.7 dB safety margin.
- A/B loudness matching, formant-preserving pitch, all 103 parameters and
  96 factory presets.

## Validation

- 469 headless checks PASS.
- LUFS-S 1 kHz test: −24.77 LUFS, finite and in range.
- True Peak test: −1.00 dBTP ceiling, measured steady −1.91 dB;
  11025 Hz −1.07 dB; 12000 Hz −1.30 dB.
- A/B LUFS boost/bound/unity tests PASS.
- pluginval strictness 7 and auval to be recorded for 2.2.0.

## Limitations

- No gated integrated LUFS, loudness range or true-peak hold/overload.
- True Peak safety margin means steady output is below the requested ceiling.
- True Peak CPU at every sample rate is not yet benchmarked.
- Real-voice and DAW listening remain blocked.
- Ad-hoc signing only; not notarized.
