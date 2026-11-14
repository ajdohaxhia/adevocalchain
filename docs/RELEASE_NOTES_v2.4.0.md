# ademusic Vocal Rack 2.4.0 — true-peak precision and channel-strip UI

## Added / Changed

- **8× true-peak limiter oversampling** with a bit-accurate bypass path. When
  True Peak mode is off the oversampled stage runs in unity-gain bypass, so it
  only contributes its fixed delay and never colours the signal.
- **Constant per-mode latency.** The Mix-quality true-peak stage now runs
  whenever Mix is active, so toggling True Peak no longer changes reported
  latency or forces the host to re-sync plugin delay compensation.
- **Block-size-independent A/B smoothing.** The loudness-matched A/B gain now
  smooths on a time constant rather than per block, so the match sounds
  identical at 64, 128, 256 and 512-sample buffers.
- **Channel-strip UI overhaul.** The LookAndFeel palette moves to a neutral
  graphite/blue channel-strip scheme for a professional, mix-console feel.
- **Header layout fix.** The logo and product labels are now sized to their
  measured text width, so "VOCAL RACK" can no longer overlap the preset block
  on narrow windows.

## DSP

- True-peak detection at 8× oversampling across 44.1/48/88.2/96 kHz.
- Unity-gain bypass verified bit-accurate against the dry path.
- Lookahead limiter, de-esser and dynamic EQ unchanged in behaviour.

## Presets

- 96 factory presets across 16 banks of 6, each validated through the loader's
  `validatePreset` path and rendered to finite audio in the test suite.

## Limitations

- True-peak meter hold/overload is shown in the footer; no dedicated
  large-format TP meter.
- LUFS is short-term, not gated integrated LUFS or LRA.
- Real-voice/DAW listening remains BLOCKED in this environment.
- Ad-hoc signing only; not notarized.
