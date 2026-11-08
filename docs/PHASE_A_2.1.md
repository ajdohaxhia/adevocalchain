# 2.1 Phase A — dynamic EQ (partial milestone)

Branch: `feature/vocal-rack-2.1`, starting from 2.0.0 `5b67c2e`.
Status: **implemented and tested, not released as 2.1.0**.

## Delivered

- Two bands, each with enable, frequency, Q, static peaking gain, threshold,
  ratio, maximum dynamic range, attack and release.
- 18 appended parameters (`deq1.*`, `deq2.*`); all 84 existing IDs preserved.
- Stereo-linked band-pass detector, exponential attack/release envelope and
  smooth dynamic cut; static peaking filter plus dynamic band gain. No
  coefficient modulation or audio-thread allocation.
- Inserted after tone EQ and before saturation; zero latency.
- Old presets missing `deq*` fields receive neutral defaults (off, 0 dB).
- Dynamic EQ panel with a real-coefficient frequency response graph.
- Draggable band handles for frequency/gain; mouse wheel adjusts Q; double-click resets gain.
- Live per-band gain-reduction markers and a live response curve derived from the
  actual static peaking + dynamic bandpass transfer functions.
- Dynamic EQ gain reduction row added to the level meter.

## Verification

- 457 headless checks PASS.
- Dynamic EQ reduces a 300 Hz tone above threshold.
- Static +6 dB peaking gain measured.
- Filter/envelope output finite.
- Latency unchanged: Track 720 / 15.00 ms; Mix 3716 / 77.42 ms (48 kHz).
- CPU (dynamic EQ off): Track 2.2 %, Mix 2.8 % average.

## Not delivered / not ready for 2.1.0

- True Peak limiting remains absent.
- Loudness-matched A/B.
- Dynamic EQ/LUFS/true-peak metering UI.
- Real-voice and DAW listening.

2.1.0 is therefore **not tagged or released**. The working 2.0.0 release
remains the latest public build.
