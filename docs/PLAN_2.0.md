# 2.0 implementation plan and status

| Milestone | Scope | Result |
|---|---|---|
| A — Audit/baseline | Verify v1.4, build, tests, docs | **VERIFIED** — 410 baseline |
| B — Core reliability | Already-fixed shifter bypass/unity path | **VERIFIED** — regression tests retained |
| C — Pitch/formant | Mix-only formant preserve + shift, preset migration | **IMPLEMENTED & TESTED** |
| D — Dynamics/metering | True Peak, dynamic EQ, loudness A/B | **NOT IMPLEMENTED** — no fake controls |
| E — Interface | Formant controls, disabled-in-Track state | **IMPLEMENTED**; macros/visualizations remain gaps |
| F — Release | Build, tests, pluginval, auval, package, docs | **VERIFIED** |

## Acceptance

- Formant feature must measurably move the spectral envelope toward the source.
- Disabled state must be transparent; Track must remain within latency.
- Existing IDs, sessions and presets must survive.

## Explicit non-goals in 2.0

True Peak, dynamic EQ, macros, waveform/EQ visualizations, Vocal Assistant and
loudness-matched A/B are audited but not shipped. They are listed as gaps rather
than represented by inert controls.
