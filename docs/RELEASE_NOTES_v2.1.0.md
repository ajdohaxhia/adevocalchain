# ademusic Vocal Rack 2.1.0 — dynamic EQ, True Peak and A/B matching

## Dynamic EQ

Two genuine vocal dynamic-EQ bands. Each band has enable, frequency, Q, static
peaking gain, threshold, ratio, maximum dynamic range, attack and release.
The detector is stereo-linked and the dynamic cut uses a band-pass sidechain.
A real-coefficient response graph supports draggable frequency/gain handles and
mouse-wheel Q. Live per-band gain reduction is drawn from atomic DSP state and
is also shown in the level meter.

## True Peak limiter

Optional 8× oversampled True Peak mode in Mix quality. The detector, lookahead,
gain envelope and ceiling run on the reconstructed signal. Output is measured
with an independent 8× oversampling reference in the headless suite, including
high-frequency and phase-offset stress cases. A conservative **1.7 dB internal
safety margin** prevents reconstruction-filter ringing from exceeding the
requested ceiling; steady test signals therefore sit slightly below the setting.
True Peak adds **21 samples** of reported latency in Mix (3737 samples / 77.85 ms
at 48 kHz). Track remains sample-peak at 720 / 15.00 ms.

## Loudness-matched A/B

The header **M** toggle enables bounded, non-destructive A/B comparison gain.
RMS is measured before the comparison gain, so the detector cannot feed back
into its own control. Gain is clamped to ±12 dB, smoothed over 50 ms, and never
written into A/B snapshots, presets, automation or output trim. A dedicated
headless test switches between a 0 dB slot and a +12 dB slot and verifies both
boost and return-to-unity.

## Compatibility

- All previous 84 parameter IDs preserved; 19 appended (18 dynamic EQ,
  1 True Peak), total 103.
- 96 factory presets retained; new parameters default to off/neutral.
- Legacy preset migration accepts missing new fields.
- Old sessions start with dynamic EQ and True Peak off.
- Formant preservation, Track/Mix, delay/reverb tails and plugin identity
  unchanged.

## Not implemented in 2.1

- True Peak/LUFS metering display and overload/peak hold for those metrics.
- Independent LUFS-S loudness display (A/B uses RMS).
- Real-voice listening and DAW validation (BLOCKED in this environment).
- Apple notarization.

## Validation

- 467 headless checks.
- Dynamic EQ response, reduction, static gain, stability.
- 8× true-peak reference tests at 997 Hz, 11025 Hz and 12000 Hz.
- A/B loudness match boost and unity return.
- Track 720 / 15.00 ms; Mix 3716 / 77.42 ms; True Peak Mix 3737 / 77.85 ms.
- pluginval strictness 7 and auval to be recorded in `docs/verification/`.
