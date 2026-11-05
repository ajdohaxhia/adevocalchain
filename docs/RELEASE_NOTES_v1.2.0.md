# ademusic Vocal Rack 1.2.0 — 96 presets

Third release. The DSP, formats and 82-parameter surface are unchanged; the
factory library grows from **60 to 96 presets across 16 banks**, with detailed
artist/era studies.

## New in 1.2.0

**36 granular studies** added in six banks:

- **Weeknd Studies** — After Hours, Blinding Lights, Starboy, Save Your Tears,
  Die For You, Dawn FM.
- **Dua Lipa Studies** — Future Nostalgia, Levitating, Don't Start Now, Houdini,
  Training Season, Radical Optimism.
- **Drake Studies** — Take Care, Nothing Was The Same, Views, Scorpion,
  Honestly Nevermind, For All The Dogs.
- **Kanye Studies** — Graduation, My Beautiful Dark Twisted Fantasy, Yeezus,
  The Life of Pablo, Ye, Vultures.
- **Modern Pop Studies** — Billie Eilish, Ariana Grande, Taylor Swift,
  Olivia Rodrigo, Sabrina Carpenter, Chappell Roan.
- **R&B & Hip-Hop Studies** — SZA, Frank Ocean, Beyonce, Rihanna, Travis Scott,
  Post Malone.

The chains were informed by the detailed artist notes at
[adé Vocal Chain Explorer](https://adelajdo.com/tools/vocal-chain-explorer),
plus genre references from Logic Studio Training, Antares, Waves, Sound on
Sound, Mix with the Masters, Equipboard and bchillmix. Full source notes and the
mapping are in `docs/PRESET_RESEARCH.md`.

Artist and song references are **descriptive production directions only**. No
affiliation, endorsement, or exact recreation of any artist's record is implied.
All parameter values are original designs, not copied presets.

## Formats and compatibility

- **VST3** — `ademusic Vocal Rack.vst3`
- **AUv2** — `ademusic Vocal Rack.component`
- Bundle ID: `com.ademusic.vocalrack`
- AU: `aufx` / `Vrck` / `Adms`
- VST3 CID: `ABCDEF019182FAEB41646D735672636B`
- Architecture: **arm64**
- 82 host-automatable parameters; 96 factory presets in 16 banks.
- Preset format and session state unchanged; existing user presets remain
  compatible.

## Install (macOS)

Quit the DAW, then:

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components

unzip -q ademusic-vocal-rack-1.2.0-macOS-arm64.zip
cp -R "ademusic-vocal-rack-1.2.0-macOS-arm64/VST3/ademusic Vocal Rack.vst3" \
      ~/Library/Audio/Plug-Ins/VST3/
cp -R "ademusic-vocal-rack-1.2.0-macOS-arm64/Components/ademusic Vocal Rack.component" \
      ~/Library/Audio/Plug-Ins/Components/
```

The standalone VST3 and AU archives contain one bundle each at the archive
root. If the ZIP was downloaded, clear quarantine:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"ademusic Vocal Rack.vst3" 2>/dev/null || true
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"ademusic Vocal Rack.component" 2>/dev/null || true
```

Restart Logic or the VST3 host, rescan, and insert
**ademusic → ademusic Vocal Rack**.

## Latency and CPU

Unchanged from 1.1.0:

| Configuration | 44.1 kHz | 48 kHz | 88.2 kHz | 96 kHz |
|---|---:|---:|---:|---:|
| Track | 655 / 14.85 ms | **720 / 15.00 ms** | 1309 / 14.84 ms | 1440 / 15.00 ms |
| Mix | 3706 / 84.04 ms | **3716 / 77.42 ms** | 7407 / 83.98 ms | 7428 / 77.38 ms |

Warmed Release CPU: Track ~2.2 %, Mix ~2.6 % average block budget.

## Validation in this release

- Full headless suite on 96 presets / 16 banks, including all-preset finite
  render checks.
- `pluginval --strictness-level 7` — **VST3 SUCCESS**, **AU SUCCESS**.
- `auval -v aufx Vrck Adms` — **AU VALIDATION SUCCEEDED**.
- Host harness: VST3 and AU scan, instantiate, process, 720-sample latency,
  82 parameters, state read/write pass.
- Combined ZIP includes full evidence and `SHA256SUMS.txt`.

## Signing

Bundles are **ad-hoc signed** and **not notarized**. They run locally after
clearing quarantine; Developer ID/notarization requires separate Apple
credentials.

## Known limitations

- arm64 only.
- Limiter enforces sample peak, not True Peak.
- Real DAW workflow and human listening were environment-blocked during the
  build; the manual checklist is in the combined ZIP.
- Spectral cleanup is Mix-only by design.

## License

Original source: AGPL-3.0-or-later. JUCE 8.0.15 is dual AGPLv3/commercial; this
binary uses the AGPLv3 path. The adapted phase vocoder retains its MIT notice.
See `LICENSE`, `LICENSES/`, and `THIRD_PARTY_NOTICES.md`.
