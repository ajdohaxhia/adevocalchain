# ademusic Vocal Rack 1.1.0 — 60 presets

Second release. Same native arm64 VST3 + AU processor and DSP; the factory
library grows from 36 to **60 presets across 10 banks**.

## New in 1.1.0

- **24 new factory presets** in four new banks:
  - **Artist-Inspired Leads** — After Hours Air, Rodeo Warble, Low Light Warble,
    Toronto Close, Whisper Cinematic, Cinematic Diva.
  - **Modern Rap Chains** — Auto-Tune Melody, UK Drill Dark, Emo Rap Wide,
    Rage Unison, Latin Trap Heat, Afrobeats Bounce.
  - **Pop, Soul & Rock** — Maximal Pop Stack, Neo Soul Warmth, Nashville Story,
    Arena Rock Edge, Gospel Lift, Bedroom Pop Dry.
  - **Broadcast & Live** — Podcast Broadcast, Voiceover Master, Live Stage,
    Worship Ambient, Interview Clean, Phone Memo Rescue.
- Original preset names; artist references in descriptions are descriptive
  production directions only. No affiliation, endorsement, or exact recreation
  is implied.
- Research notes and source links: `docs/PRESET_RESEARCH.md`.
- Preset format unchanged (version 1). Existing user presets and session state
  remain compatible.
- Plugin version updated to 1.1.0; bundle ID, AU subtype/manufacturer and VST3
  CID are unchanged.

## Formats

- **VST3** — `ademusic Vocal Rack.vst3`
- **AUv2** — `ademusic Vocal Rack.component`
- Bundle ID: `com.ademusic.vocalrack`
- AU: `aufx` / `Vrck` / `Adms`
- VST3 CID: `ABCDEF019182FAEB41646D735672636B`
- Architecture: **arm64** (MacBook Air M4 target)
- 82 host-automatable parameters; 60 factory presets in 10 banks.

## Install (macOS)

Quit the DAW, then:

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components

unzip -q ademusic-vocal-rack-1.1.0-macOS-arm64.zip
cp -R "ademusic-vocal-rack-1.1.0-macOS-arm64/VST3/ademusic Vocal Rack.vst3" \
      ~/Library/Audio/Plug-Ins/VST3/
cp -R "ademusic-vocal-rack-1.1.0-macOS-arm64/Components/ademusic Vocal Rack.component" \
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

Unchanged from 1.0.0:

| Configuration | 44.1 kHz | 48 kHz | 88.2 kHz | 96 kHz |
|---|---:|---:|---:|---:|
| Track | 655 / 14.85 ms | **720 / 15.00 ms** | 1309 / 14.84 ms | 1440 / 15.00 ms |
| Mix | 3706 / 84.04 ms | **3716 / 77.42 ms** | 7407 / 83.98 ms | 7428 / 77.38 ms |

Warmed Release CPU on this M4: Track ~2.4 %, Mix ~3.0 % average block budget.
Presets never change Track/Mix; stop transport before switching if the host
cannot re-align live.

## Validation in this release

- Full headless suite re-run on the 60-preset bank (all presets rendered finite,
  ten banks × six).
- `pluginval --strictness-level 7` — **VST3 SUCCESS**, **AU SUCCESS**.
- `auval -v aufx Vrck Adms` — **AU VALIDATION SUCCEEDED**.
- Host harness: VST3 and AU scan, instantiate, process, 720-sample latency,
  82 parameters, state read/write pass.
- Full evidence and hashes in the combined ZIP and in the source repository.

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
