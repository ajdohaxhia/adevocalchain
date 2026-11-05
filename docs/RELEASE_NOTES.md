# ademusic Vocal Rack 1.0.0

First public release of the native Apple Silicon vocal-chain plugin.

## Formats

- **VST3** — `ademusic Vocal Rack.vst3`
- **AUv2** — `ademusic Vocal Rack.component`
- Bundle ID: `com.ademusic.vocalrack`
- AU: `aufx` / `Vrck` / `Adms`
- VST3 CID: `ABCDEF019182FAEB41646D735672636B`
- Architecture: **arm64** (MacBook Air M4 target)
- 82 host-automatable parameters; 36 factory presets in six banks.

## Install (macOS)

Quit the DAW, then:

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components

# Combined ZIP:
unzip -q ademusic-vocal-rack-1.0.0-macOS-arm64.zip
cp -R "ademusic-vocal-rack-1.0.0-macOS-arm64/VST3/ademusic Vocal Rack.vst3" \
      ~/Library/Audio/Plug-Ins/VST3/
cp -R "ademusic-vocal-rack-1.0.0-macOS-arm64/Components/ademusic Vocal Rack.component" \
      ~/Library/Audio/Plug-Ins/Components/
```

The standalone assets contain one bundle each, at the archive root. If the ZIP
was downloaded, clear quarantine:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"ademusic Vocal Rack.vst3" 2>/dev/null || true
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"ademusic Vocal Rack.component" 2>/dev/null || true
```

Restart the DAW. In Logic, rescan AU under **Settings → Plug-in Manager** and
insert **Audio FX → Audio Units → ademusic → ademusic Vocal Rack**. In a VST3
host, rescan the VST3 folder and insert **ademusic → ademusic Vocal Rack**.

## Signal path

Input trim → expander → spectral denoiser → pitch correction → cleanup EQ →
compressor → de-esser → tone EQ → saturation → parallel delay + reverb →
global mix → output trim → lookahead limiter → output meter.

- Track: **720 samples / 15.00 ms** at 48 kHz. Low-latency expander and tuning;
  spectral cleanup is not available.
- Mix: **3716 samples / 77.42 ms** at 48 kHz. Full spectral cleanup and 2x
  saturation oversampling.
- Presets never change Track/Mix. Stop transport before switching if the host
  cannot re-align live; the plugin re-prepares and updates host latency.

## Validation in this release

- 330 headless DSP/session checks, 0 failures.
- `pluginval --strictness-level 7` — **VST3 SUCCESS**, **AU SUCCESS**.
- `auval -v aufx Vrck Adms` — **AU VALIDATION SUCCEEDED**.
- Host harness: VST3 and AU scan, instantiate, process, 720-sample latency,
  82 parameters, state read/write pass.
- Hard Tune median error 0.36 cents; spectral cleanup 8.19 dB noise reduction
  with −0.81 dB voiced-level change; measured aliasing −24.88 → −44.41 dB with
  2x oversampling.
- Full evidence: `docs/VERIFICATION.md` in the combined ZIP and in the source
  repository.

## Signing

Bundles are **ad-hoc signed** and **not notarized**. They run locally after
clearing quarantine; public distribution with Developer ID/notarization requires
separate Apple credentials. See `INSTALL.txt` and `docs/MANUAL_CHECKLIST.md`.

## Known limitations

- arm64 only; no universal binary.
- Limiter enforces sample peak, not True Peak; no True Peak mode is claimed.
- Real DAW workflow and human listening were environment-blocked during the
  build; the manual checklist is included in the combined ZIP.
- Spectral cleanup is Mix-only by design.

## License

Original source: AGPL-3.0-or-later. JUCE 8.0.15 is dual AGPLv3/commercial; this
binary uses the AGPLv3 path. The adapted phase vocoder retains its MIT notice.
See `LICENSE`, `LICENSES/`, and `THIRD_PARTY_NOTICES.md`.

Combined ZIP SHA-256:
`cbb9bb9b90021676345d1cf297bd05302c110301aec6c5d79c2e9ae80b6882c1`
