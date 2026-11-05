# ademusic Vocal Rack 1.3.0 — redesigned editor

A UI release. The DSP, parameter map, automation, preset format and session
state are unchanged; the editor has been rebuilt around the graphite/lime
reference: larger two-row Quick controls, a nine-module Detail bar, full-width
cards for every module, and a three-column preset browser.

## New editor

- **Graphite shell with lime accents**, consistent 8–12 px radii, one-pixel
  borders, and a tighter spacing/hierarchy system.
- **Header**: identity, preset navigation, Browse, A/B, Copy and global power;
  second row for Quick/Detail, Quality, Key, Scale and Keep Song Key.
- **Status rail**: detected pitch + Hz + cents/confidence, editable note mask,
  and input/output/GR/LIM meters.
- **Quick**: a nine-module strip (Cleanup, Noise, Pitch, Comp, De-ess, Color,
  Delay, Space, Limit) with arrows, plus two rows of large labelled knobs.
- **Detail**: horizontal text tabs with chevrons and lime underline, spanning
  Cleanup, Pitch, Clean EQ, Comp, De-ess, Tone EQ, Color, Space, and Limit.
  Every panel uses the same card, title, large knob, value and description
  treatment; header controls are right-aligned rather than stretched.
- **Preset browser**: full-window three-column layout with bank list,
  Factory/User toggles, searchable preset table, right-hand description/use
  case/first-move details, and a consolidated action area.
- Preset names and data are the real repository library: 96 presets across 16
  banks, including the artist/era studies.

All existing APVTS attachments, parameter automation, preset recall, A/B,
quality switching and state serialization are preserved.

## Contents

- VST3 + AUv2, macOS arm64.
- 96 factory presets / 16 banks.
- 82 parameters; unchanged automation IDs.
- Bundle ID, AU subtype/manufacturer and VST3 CID unchanged.
- Existing user presets and sessions remain compatible.

## Install

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components

unzip -q ademusic-vocal-rack-1.3.0-macOS-arm64.zip
cp -R "ademusic-vocal-rack-1.3.0-macOS-arm64/VST3/ademusic Vocal Rack.vst3" \
      ~/Library/Audio/Plug-Ins/VST3/
cp -R "ademusic-vocal-rack-1.3.0-macOS-arm64/Components/ademusic Vocal Rack.component" \
      ~/Library/Audio/Plug-Ins/Components/
```

If downloaded, clear quarantine:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"ademusic Vocal Rack.vst3" 2>/dev/null || true
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"ademusic Vocal Rack.component" 2>/dev/null || true
```

Restart Logic or the VST3 host, rescan, and insert
**ademusic → ademusic Vocal Rack**.

## Validation

- 400 headless checks, all 96 presets rendered finite.
- `pluginval --strictness-level 7` — VST3 and AU.
- `auval -v aufx Vrck Adms`.
- Host harness: 720 samples latency, 82 parameters, state recall.
- Latency and CPU unchanged: Track 720 / 15.00 ms, Mix 3716 / 77.42 ms at
  48 kHz.

## Signing

Bundles are **ad-hoc signed** and **not notarized**. Developer ID/notarization
requires separate Apple credentials.

## License

Original source: AGPL-3.0-or-later. JUCE 8.0.15 is dual AGPLv3/commercial; this
binary uses the AGPLv3 path. The adapted phase vocoder retains its MIT notice.
See `LICENSE`, `LICENSES/`, and `THIRD_PARTY_NOTICES.md`.
