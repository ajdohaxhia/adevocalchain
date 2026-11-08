# 2.2 audit — metering and A/B

Starting point 2.1.0, commit `419330b`.

## Delivered

- K-weighted LUFS-S meter with 3 s short-term averaging.
- 4× true-peak output meter active with True Peak mode.
- Footer LUFS/TP display sourced from audio-thread atomics.
- A/B comparison uses LUFS-S instead of block RMS.
- New LUFS test and updated A/B tests.

## Status

| Item | Status |
|---|---|
| LUFS-S meter | VERIFIED |
| True Peak meter | VERIFIED at 48 kHz |
| True Peak meter hold/overload | NOT IMPLEMENTED |
| Gated integrated LUFS / LRA | NOT IMPLEMENTED |
| True Peak CPU matrix | NOT TESTED |
| Real voice / DAW | BLOCKED |
| Notarization | BLOCKED |
