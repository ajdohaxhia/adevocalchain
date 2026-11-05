# ademusic Vocal Rack 1.2.1 — text layout fixes

Patch release for the editor. DSP, formats, latency, parameters and the
96-preset factory bank are unchanged.

## Fixed

- **Pitch confidence readout** is no longer clipped. The bar and the
  `conf 100%` label now use separate widths, and the label is rendered with an
  explicit integer percentage.
- **Preset browser search placeholder** now fits: `Search name, bank, notes`.
- **Long preset names** such as `Kanye: My Beautiful Dark Twisted Fantasy` scale
  down instead of clipping at the minimum 960 px width; the full name is also
  available as a tooltip.
- **Long bank metadata** such as `R&B & Hip-Hop Studies · FACTORY · TRACK`
  scales inside the header instead of clipping, with a full tooltip.
- **Long footer notices** scale slightly before clipping.
- Added a long-preset-name screenshot to the combined ZIP evidence.

## Contents

- **96 factory presets / 16 banks**, including the Weeknd, Dua Lipa, Drake,
  Kanye, modern-pop and R&B/hip-hop studies added in 1.2.0.
- **VST3** and **AUv2**, macOS arm64.
- Bundle ID, AU subtype/manufacturer and VST3 CID unchanged.
- Preset format and session state unchanged; existing user presets remain
  compatible.

## Install (macOS)

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components

unzip -q ademusic-vocal-rack-1.2.1-macOS-arm64.zip
cp -R "ademusic-vocal-rack-1.2.1-macOS-arm64/VST3/ademusic Vocal Rack.vst3" \
      ~/Library/Audio/Plug-Ins/VST3/
cp -R "ademusic-vocal-rack-1.2.1-macOS-arm64/Components/ademusic Vocal Rack.component" \
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

- 400 headless checks, 0 failures, all 96 presets rendered finite.
- `pluginval --strictness-level 7` — **VST3 SUCCESS**, **AU SUCCESS**.
- `auval` — **AU VALIDATION SUCCEEDED**, version 1.2.1.
- Host harness: 720 samples latency, 82 parameters, state read/write pass.
- Latency unchanged: Track **720 / 15.00 ms**, Mix **3716 / 77.42 ms** at
  48 kHz.
- CPU: Track ~2.1 %, Mix ~2.6 % average block budget.

## Signing

Bundles are **ad-hoc signed** and **not notarized**. Developer ID/notarization
requires separate Apple credentials.

## License

Original source: AGPL-3.0-or-later. JUCE 8.0.15 is dual AGPLv3/commercial; this
binary uses the AGPLv3 path. The adapted phase vocoder retains its MIT notice.
See `LICENSE`, `LICENSES/`, and `THIRD_PARTY_NOTICES.md`.
