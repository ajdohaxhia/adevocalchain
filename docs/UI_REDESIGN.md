# Editor redesign — 1.3.0

The editor follows the supplied graphite/lime reference while retaining the
real repository preset names and data.

## Design system

- Background `#101210`; shell `#171A17`; cards `#1B1F1B`; lines `#303630`.
- Off-white text `#EEF1E8`; muted text `#939A91`; lime accent `#C8F45A`;
  danger `#FF7A66`.
- 8–12 px rounded cards, 1 px borders, 34–42 px control rows, 12–13 px labels,
  20 px panel titles.
- Knobs: 5 px lime value arc, dark recessed body, white pointer with lime tip.
- Header controls right-aligned; module strip evenly distributed with chevrons.
- Consistent card titles, labels, values and short descriptions across all
  Detail panels.

## Screens

| Screen | File |
|---|---|
| Quick | `docs/vocal-rack-editor.png` |
| Detail — Pitch | `docs/vocal-rack-editor-detail.png` |
| Detail — Cleanup | `docs/vocal-rack-detail-cleanup.png` |
| Detail — Comp | `docs/vocal-rack-detail-comp.png` |
| Detail — Space | `docs/vocal-rack-detail-space.png` |
| Preset browser | `docs/vocal-rack-preset-browser.png` |
| Minimum width | `docs/vocal-rack-editor-960.png` |
| Long preset name | `docs/vocal-rack-long-name.png` |

## Compatibility

Only `VocalRackLookAndFeel`, `PluginEditor` layout, `ControlGrid`, the preset
browser presentation and component paint code changed. DSP, parameters,
automation attachments and state serialization are untouched.
