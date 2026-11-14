# Audit — ademusic Vocal Rack 2.4.0

## Verified in this release

- **Build:** VST3 + AU targets configure and link clean at project version
  2.4.0 (`CMAKE_PROJECT_VERSION=2.4.0`, build exit 0, no errors/undefined
  symbols).
- **Tests:** `VocalRackTests` passes (1/1, ~1.5 s) on the Release build.
- **True-peak limiter:** 8× oversampling with a unity-gain bypass path when
  True Peak mode is off; the Mix-quality stage runs whenever Mix is active so
  reported latency is constant per mode and toggling True Peak no longer forces
  host PDC re-sync.
- **A/B match:** loudness-matched A/B gain smooths on a time constant, so the
  match is block-size independent (verified identical behaviour across 64/128/
  256/512-sample buffers in the suite).
- **Presets:** 96 factory presets across 16 banks of 6, each passing the
  loader's `validatePreset` path and rendering to finite audio in tests.
- **UI:** channel-strip LookAndFeel palette (graphite/blue); header identity
  labels sized to measured text width so "VOCAL RACK" cannot overlap the preset
  block on narrow windows.

## Not done (out of scope for this environment)

- Code signing uses ad-hoc identity only; no Developer ID signing or
  notarization (needs Apple Developer credentials).
- Real-voice / in-DAW listening remains unverified here; correctness is
  established through the automated render and validation suite.
