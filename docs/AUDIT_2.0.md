# 2.0 audit — evidence-based findings

Baseline: v1.4.0 checkout, commit `497dcdd`, 410 headless checks, arm64
Release. Baseline build and tests reproduced before changes.

## Confirmed defects / limitations

| Severity | Finding | Evidence | Status |
|---|---|---|---|
| High | Formant shifting was uncontrolled during pitch correction | No formant parameter; spectral envelope moved with ratio | **FIXED in 2.0** — optional Mix-mode envelope correction |
| High | Phase vocoder previously ran while Pitch was bypassed | v1.4 diagnostic; fixed in 1.4 | **VERIFIED FIXED** — dry path; 1.00000 correlation |
| High | Unity-ratio vocoder dulled in-tune notes | v1.4 diagnostic | **VERIFIED FIXED** — unity OLA; 110/220 Hz level exact |
| Medium | Old presets would reject new parameters if schema stayed all-or-nothing | `Presets.cpp` required every spec | **FIXED** — introduced params default on migration |
| Medium | High Mix CPU with phase locking | 4.3–5.3 % average, p99 up to 19.9 % | **MEASURED / accepted for quality** |
| Medium | True Peak not implemented | Limiter is sample-peak | **NOT IMPLEMENTED** — no dBTP claim |
| Medium | No dynamic EQ / multiband | Only static EQ | **NOT IMPLEMENTED** |
| Medium | No loudness-matched A/B | A/B preserves states but not level | **NOT IMPLEMENTED** |
| Medium | No Quick macro layer | Quick shows direct controls | **NOT IMPLEMENTED** |
| Medium | No waveform/EQ/GR visualization | Metering only | **NOT IMPLEMENTED** |
| High | Real DAW workflow not exercised | No DAW installed | **BLOCKED** |
| High | Human listening not performed | No authorized takes | **BLOCKED** |
| Low | Developer ID/notarization absent | ad-hoc signing | **BLOCKED** |

## Audio-quality measurements

- Bypassed tuning/saturation phase correlation: 1.00000.
- Unity correction 110/220 Hz: 0.10000 (exact, both qualities).
- Phase-locked A440: 0.09831 Track / 0.09811 Mix.
- Zero-amount spectral cleanup: phase-clean.
- Formant fixture 1.5×: free centroid 1806.6 Hz; preserved 1375.2 Hz.
- 2× saturation alias reduction: −24.88 → −44.41 dB (retained).

## Preset/backward compatibility

- 96 factory presets preserved.
- 82 legacy IDs preserved; 2 new IDs appended.
- Pre-2.0 preset validation with missing formant fields passes with 0/0 defaults.
- Factory data regenerated with 84 fields.
