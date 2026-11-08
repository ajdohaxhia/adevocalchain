# Architecture notes

## Threading and lifetime

- `VocalRackAudioProcessor` owns the APVTS, the `ParamCache` of atomic parameter
  pointers, the `Engine`, and the preset library.
- `processBlock` snapshots the parameter atomics into a plain `RackParams` at the
  block boundary. The audio thread reads no strings, files, locks, or APVTS
  objects.
- All buffers, FFTs, delay lines, filters, noise profiles and meter queues are
  allocated in `prepareToPlay` / `Engine::prepare`.
- Meter values and pitch diagnostics are atomics read by the editor timer.
- Preset/state changes happen on the message thread. The processor raises a
  state-fade request; the audio thread ramps from zero over 15 ms at the next
  block, so all parameter changes from one transaction are heard as a coherent
  update. Delay/reverb state is not cleared by preset loads, so tails follow the
  documented ramp policy.
- `setStateInformation` restores the complete state, including quality, key/mask,
  manual BPM, bypasses, preset identity, A/B snapshots and active slot. The
  preset file is not needed for recall.

## Latency

```
Track: shifter(delay 704 @48k) + limiter lookahead 16        = 720 samples
Mix:   denoiser 1024 + shifter 2560 + saturation 4 + limiter 128 = 3716
```

- Every serial module has fixed latency for its configuration. Module bypass
  still runs the latency-bearing path (or an equivalent delayed dry path), so the
  reported latency is stable across bypass and presets.
- Two dry delay lines (raw and input-trimmed) are read at
  `prelimiterLatency`; the limiter adds its lookahead afterwards. Global bypass
  therefore returns the original input at the full reported latency, while 0 %
  mix still passes through output trim and the limiter as specified.
- Track and Mix require different latency and internal resources. Switching
  quality suspends processing, waits for the audio thread, re-prepares and
  updates host latency. The UI says to stop transport if the host cannot re-align
  live. Presets do not change quality.

## Pitch path

- `PitchDetector`: low-pass + decimate to ~15 kHz; YIN with 512-sample Track /
  768-sample Mix windows; confidence, RMS and ZCR gates; range selector
  (65–350, 65–1100, 150–1500 Hz). The window is anchored to the newest input so
  its history is inside the shifter's lookahead; no extra reported latency.
- Detector feed: mid by default; if mid energy is weak or phase-cancelled and
  side energy is present, side is used. Both channels receive the same ratio.
- `PitchSelector`: effective mask from chromatic/major/minor or custom 12-bit
  mask; nearest legal absolute MIDI note across octaves; lower-note deterministic
  tie-break; 30-cent hysteresis; empty mask means no correction.
- `PitchShifter`: adapted MIT phase vocoder, 512/1024 Track frames and
  2048/4096 Mix frames, 75 % overlap, window-sum normalisation, arbitrary
  aligned delay. 2.0 adds an optional Mix-only formant envelope correction and
  formant shift; the unity-ratio and inactive paths bypass it. Ratio is smoothed in log2 space by the retune time constant,
  and unvoiced transitions release to unity rather than cutting the shifter.

## Noise path

- Expander: peak follower, soft depth curve, hold state that is armed only while
  the gate is open, click-free gain smoothing.
- Denoiser: 1024/2048 FFT, 75 % overlap, minimum-statistics noise estimate with a
  bias correction, Wiener-style bounded gain, fast attack/slow release, shared
  stereo gain. It handles steady broadband noise; it is not a click/transient
  remover.
- Track omits the denoiser entirely; Mix always runs it (amount 0 when bypassed)
  to keep latency stable.

## Dynamics and FX

- Compressor: soft-knee gain computer, peak envelope, detector HPF, makeup,
  parallel mix, GR meter.
- De-esser: Linkwitz-Riley split (tunable detection band), high-band attenuation,
  low band untouched, audition monitor.
- Saturation: two curves, auto compensation, manual output, 2x oversampling in
  Mix with a latency-aligned dry path. Measured alias reduction is in the
  verification report.
- Delay: fractional read, smoothed time, bounded feedback, feedback filters,
  ping-pong cross-feed, dry-vocal ducking, manual tempo fallback.
- Reverb: 8-line Hadamard FDN with modulated lines, damping, input diffusion,
  Room early reflections, Plate density, pre-delay, filters, ducking, and a
  25 ms type-change fade.
- Limiter: lookahead delay, required-gain attack, slow release, final safety
  clamp at the ceiling. It is a sample-peak limiter; no True Peak claim.
