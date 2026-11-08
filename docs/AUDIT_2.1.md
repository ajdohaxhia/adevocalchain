# 2.1 audit and status

Starting point 2.0.0, commit `5b67c2e`.

## Implemented and tested

- Two-band dynamic EQ with 18 parameters and legacy-safe migration.
- Real-coefficient frequency graph, draggable handles and live gain reduction.
- Dynamic EQ gain-reduction meter.
- 8× oversampled True Peak limiter (Mix-only) with 1.7 dB safety margin.
- Independent 8× reference measurements for steady, high-frequency and
  phase-offset stress signals.
- Bounded loudness-matched A/B with pre-gain RMS detection, tested boost and
  return-to-unity.

## Explicit limitations

| Item | Status |
|---|---|
| True Peak/LUFS meter and hold | NOT IMPLEMENTED |
| LUFS-S comparison display | NOT IMPLEMENTED |
| Real-voice/DAW listening | BLOCKED |
| Apple notarization | BLOCKED |
| True Peak latency-change behavior in every host | NOT TESTED |

## Measurements

- Dynamic EQ static/dynamic tests: PASS.
- True Peak: requested −1.00 dBTP; measured steady −1.91 dB, 11025 Hz −1.07 dB,
  12000 Hz −1.30 dB with 8× reference; latency 21 samples.
- A/B gain boost and unity return: PASS.
