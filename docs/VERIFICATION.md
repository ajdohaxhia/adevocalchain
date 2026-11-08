# ademusic Vocal Rack — verification report

Date: 2026-11-05 (Europe/Rome)  
Build: **2.2.0**, native **arm64** Release  
Project: `ademusic-vocal-rack/`  
Status: **all locally runnable checks passed**; DAW and human listening checks
are blocked by this environment and listed with exact next steps.

## 1. Environment

| Item | Observed |
|---|---|
| Mac | MacBook Air M4, arm64, 16 GB RAM |
| macOS | 27.2 (build 26B123) |
| Toolchain | Apple clang 21.0.0, Command Line Tools, `MacOSX27.2.sdk` |
| CMake | 4.4.5, Unix Makefiles |
| JUCE | 8.0.15, commit `91ad83ae34a81e0833b1a2b0866f54846370ae53` (local offline checkout for this build; CMake default fetches the same pin) |
| pluginval | 1.0.4 (Tracktion) |
| auval | 1.10.0 |
| DAWs | **none installed** (Logic, REAPER, Ableton all absent) |
| Authorized dry vocals | **none provided** |

The first `auval` run failed to find the freshly copied AU because
`AudioComponentRegistrar` had not rescanned. After
`killall -9 AudioComponentRegistrar` and a two-second wait, the exact command
passed. This is an environment cache issue, not a plugin failure.

## 2. Artifacts and identities

| Artifact | Path | Size |
|---|---|---|
| VST3 | `build-release/VocalRack_artefacts/Release/VST3/ademusic Vocal Rack.vst3` | 11 MB |
| AUv2 | `build-release/VocalRack_artefacts/Release/AU/ademusic Vocal Rack.component` | 10 MB |
| Local ZIP | `dist/ademusic-vocal-rack-2.2.0-macOS-arm64.zip` | see `dist/*.sha256` |
| Factory bank | `presets/ademusic-vocal-rack-factory.json` | 96 presets |
| Headless tests | `build-release/VocalRackTests_artefacts/Release/VocalRackTests` | 469 checks |

Identity facts read from the built bundles:

- Bundle ID: `com.ademusic.vocalrack`
- AU: `aufx` / `Vrck` / `Adms` (effect, ademusic Vocal Rack)
- VST3 moduleinfo CID: `ABCDEF019182FAEB41646D735672636B`
- Both Mach-O executables: `arm64` only
- Signature: **ad-hoc**, not Developer ID, **not notarized**
- `codesign --verify --deep --strict` passes on both final bundles

## 3. Automated checks

Command:

```bash
./build-release/VocalRackTests_artefacts/Release/VocalRackTests | tee docs/verification/tests.log
```

Result: **469 PASS, 0 FAIL**. Log: `docs/verification/tests.log`.

Coverage exercised by the suite:

| Area | Evidence |
|---|---|
| Parameters / schema | 103 stable IDs, unique, defaults in range, 96 factory presets parse, legacy migration |
| Latency | Track impulse lands exactly at 720 samples at 48 kHz; Mix at 3716 |
| Dry alignment | Global bypass preserves impulse amplitude and both channels |
| All-stage bypass | Processed path unity ratio 1.000 for Track and Mix |
| Sample rates | 44.1, 48, 88.2, 96 kHz; representative and varying blocks (37/127/512/1024); zero-length and oversized blocks |
| Bus layouts | mono→mono, mono→stereo, stereo→stereo; mono insert retains returns |
| Tuning selector | MIDI 57/A regression, B excluded, negative MIDI, B→C octave boundary, empty-mask unity, major mask |
| Detector | 80, 130, 220, 440, 880 Hz; silence unvoiced; ≤8.5 cents error Track, ≤3.7 Mix |
| Rendered tuning | Hard Tune median **0.36 cents** (target ≤10); Natural retains **53.7 cents** vibrato movement; unvoiced tail survives |
| Noise cleanup | **8.19 dB** steady-noise reduction with **-0.81 dB** voiced-level change; Track does not secretly denoise; expander closes **19.92 dB** |
| Compressor | loud-signal reduction, quiet-signal transparency, detector HPF in signal path |
| EQ / de-esser | high-pass isolation; split-band high attenuation with low-band preservation |
| Saturation | Tape/Edge curves, drive/blend/output; Oversampled Mix path reduces measured high-note aliasing **-24.88 dB → -44.41 dB** |
| Delay | 120 BPM: 500 ms quarter, 250 ms eighth, 375 ms dotted-eighth; invalid tempo falls back to 100 BPM manual and lands at 0.600 s |
| Reverb | finite decaying tail; Room vs Plate different tails (0.00667 vs 0.00948) |
| Limiter | sample-peak ceiling enforced on stress signal; release/GR reported; lookahead 128 Mix / 16 Track |
| Presets | all 96 render finite; sixteen banks × six; full parameter state |
| Key recall | Keep Song Key preserves tonic/scale/mask; off recalls preset key |
| User presets | Save As, export, import, duplicate, rename, delete, atomic files, corrupt-file scan |
| Session state | edited params, preset identity, A/B slots, quality; recall after deleting preset file |
| A/B | independent output trim restores correctly |
| Clean path | bypassed tuning/saturation phase correlation > 0.9999; zero-amount cleanup transparent; unity 110/220 Hz level-exact; A440 phase-locked |
| Formant | free 1.5× centroid 1806.6 Hz; preserved 1375.2 Hz; legacy preset fields default to off/0 |
| Dynamic EQ | 300 Hz threshold reduction PASS; +6 dB static gain PASS; finite PASS |
| True Peak | 8× reference: −1.00 dBTP ceiling; steady −1.91 dB, 11025 Hz −1.07 dB, 12000 Hz −1.30 dB; latency 21 samples |
| A/B match | +12 dB slot boost > 1.2×; bounded ≤ 4.0×; return to unity < 0.05; LUFS-S based |
| LUFS-S | 1 kHz reference measured −24.77 LUFS; finite |
| Quality | preset load does not change Track/Mix; latency remains stable |
| Automation | rapid sweep finite, max sample step 0.138 |
| Local vocal | optional `--vocal` rendered all 96 presets to 48 kHz WAV |

### Measured latency

From `docs/verification/latency.txt`:

| Rate | Track samples / ms | Mix samples / ms |
|---|---:|---:|
| 44.1 kHz | 655 / 14.853 | 3706 / 84.036 |
| 48 kHz | **720 / 15.000** | **3716 / 77.417** |
| 88.2 kHz | 1309 / 14.841 | 7407 / 83.980 |
| 96 kHz | 1440 / 15.000 | 7428 / 77.375 |

Track composition at 48 kHz: shifter 704 + limiter lookahead 16 = 720.
Mix: denoiser 1024 + shifter 2560 + saturation 4 + limiter 128 = 3716.
The host harness independently reads 720 samples after `prepareToPlay` from both
the VST3 and AU. pluginval prints “Reported latency: 0” during its pre-prepare
scan for both formats; this is a pluginval observation, not the post-prepare
host value.

### Measured CPU (warmed Release, M4, 48 kHz stereo)

From `docs/verification/benchmark.txt`; full active vocal chain with tuning,
denoiser (Mix), dynamics, color, delay and reverb:

| Quality | Block | Avg callback | p99 | Avg block budget | p99 budget |
|---|---:|---:|---:|---:|---:|
| Track | 128 | 0.060 ms | 0.093 ms | 2.24 % | 3.49 % |
| Track | 256 | 0.119 ms | 0.161 ms | 2.24 % | 3.01 % |
| Mix | 128 | 0.073 ms | 0.221 ms | 2.72 % | 8.28 % |
| Mix | 256 | 0.144 ms | 0.279 ms | 2.70 % | 5.24 % |

No callback miss measured. Latency values above are the reported plugin latency.

## 4. Validators

### pluginval — VST3

Command:

```bash
tools/pluginval/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 7 \
  --validate "build-release/VocalRack_artefacts/Release/VST3/ademusic Vocal Rack.vst3"
```

Result: **SUCCESS**, exit 0, 25 test sections including editor automation,
automatable parameters, parameter thread safety, background thread state, bus
layouts, and fuzz parameters. Log: `docs/verification/pluginval-vst3.log`.

The JUCE VST3 SDK validator subtest is skipped because pluginval has no
`validator` path configured; the bundle was still loaded and exercised as a VST3.
If an external VST3 validator is available, add `--vst3-validator` to pluginval
or run the SDK validator separately.

### pluginval — AU

Command:

```bash
tools/pluginval/pluginval.app/Contents/MacOS/pluginval \
  --strictness-level 7 \
  --validate "$HOME/Library/Audio/Plug-Ins/Components/ademusic Vocal Rack.component"
```

Result: **SUCCESS**, exit 0. Log: `docs/verification/pluginval-au.log`.

### Apple auval

Command:

```bash
auval -v aufx Vrck Adms
```

Result: **AU VALIDATION SUCCEEDED**, exit 0, including render tests at 22.05,
44.1, 48, 96 and 192 kHz, mono, mono-to-stereo, parameter setting/ramping,
connection semantics, bad-max-frames and MIDI checks. Log:
`docs/verification/auval.log`.

### Host harness (headless, real bundle load)

```bash
./build-release/VocalRackTests_artefacts/Release/VocalRackTests \
  --host "build-release/.../ademusic Vocal Rack.vst3" vst3
./build-release/VocalRackTests_artefacts/Release/VocalRackTests \
  --host "$HOME/Library/Audio/Plug-Ins/Components/ademusic Vocal Rack.component" au
```

Both: scan PASS, instantiate PASS, prepare/process PASS, latency non-zero
(**720 samples**), 84 parameters exposed, state read/write PASS. Logs:
`docs/verification/host-vst3.log`, `docs/verification/host-au.log`.

## 5. Real DAWs — blocked by environment

No Logic Pro or VST3 host is installed on this machine. The exact manual
checklist is in `docs/MANUAL_CHECKLIST.md`. Next actions:

1. Install the two bundles to the user-level folders.
2. In Logic, rescan AU via Plug-in Manager; insert on a vocal track; verify
   controls, automation, preset browse, Save As, quality switch, project
   save/reopen, bounce with tail.
3. In REAPER/Ableton, rescan VST3 and repeat, including mono-to-stereo and a
   96 kHz project.

These are the only checks not exercised. pluginval’s editor automation and the
host harness materially reduce host risk but do not replace the DAW pass.

## 6. Listening — blocked by environment

No authorized dry vocal take was supplied. Generated fixtures were used for
measurements, and the optional renderer was exercised with a synthetic local
WAV:

```bash
./build-release/VocalRackTests_artefacts/Release/VocalRackTests \
  --vocal /tmp/vr-vocal-fixture.wav /tmp/vr-renders
```

96 WAV files were produced successfully. Human listening for consonants,
sibilance, sustained-note artifacts and preset character remains manual; see
`docs/MANUAL_CHECKLIST.md`.

## 7. MegaTune / PitchLab decision

PitchLab/MegaTune was found at `/Users/ade/Downloads/megatune` (MIT,
Copyright 2026 PitchLab, JUCE 8.0.15). Its built DSP suite was run:

```
81 checks, 0 failure(s)
```

Its documented low-latency mode is **24 ms at 48 kHz**, above the 15 ms Track
target, and its 1024/2048 frame vocoder plus YIN window would not fit the
shipped budget. Decision:

- **Reused**: the phase-vocoder shifter idea and code, adapted into
  `Source/dsp/PitchShifter.cpp`, with a 512-sample Track frame and arbitrary
  reported delay. MIT notice retained; upstream untouched.
- **Not reused**: the detector (custom decimated YIN with 512/768 windows and
  mid/side fallback) and the rest of the chain.

No MegaTune project files, presets or sessions were modified.

## 8. Licensing, signing and distribution

- Project source: **AGPL-3.0-or-later** (`LICENSE`, `LICENSES/AGPL-3.0.txt`).
- JUCE 8.0.15: dual AGPLv3/commercial. This binary is the AGPLv3 path;
  closed-source distribution requires a JUCE commercial licence.
- Adapted phase vocoder: MIT (Copyright 2026 PitchLab), notice retained.
- No RNNoise, model weights, fonts, samples or recorded audio are bundled.
- The local bundles are **ad-hoc signed**, **not notarized**, and not
  Developer ID signed. Public distribution and credential-dependent signing
  remain separate. A downloaded ZIP may need
  `xattr -dr com.apple.quarantine` (see `INSTALL.txt`).
- pluginval is a development tool and is excluded from the ZIP.

## 9. Packaging

`scripts/package.sh` stages:

```
dist/ademusic-vocal-rack-2.2.0-macOS-arm64/
  VST3/ademusic Vocal Rack.vst3
  Components/ademusic Vocal Rack.component
  presets/ademusic-vocal-rack-factory.json
  docs/VERIFICATION.md
  docs/MANUAL_CHECKLIST.md
  docs/PRESET_FORMAT.md
  docs/PRESET_RESEARCH.md
  docs/RELEASE_NOTES_v2.2.0.md
  docs/UI_REDESIGN.md
  docs/AUDIT_2.0.md
  docs/PLAN_2.0.md
  docs/AUDIT_2.1.md
  docs/verification/
  docs/vocal-rack-*.png
  docs/vocal-rack-long-name.png
  LICENSE, LICENSES/, THIRD_PARTY_NOTICES.md
  README.md, INSTALL.txt
```

The ZIP is extracted and compared recursively; bundle contents and signatures
are verified after extraction. The SHA-256 of the final ZIP is written to
`dist/ademusic-vocal-rack-2.2.0-macOS-arm64.zip.sha256`.

## 10. Final pass/fail/blocked matrix

| Class | Status | Evidence / next step |
|---|---|---|
| Build both formats arm64 | **PASSED** | Bundles + `lipo`/`file` |
| Bundle metadata and AU/VST3 IDs | **PASSED** | `plutil`, `moduleinfo.json` |
| Headless DSP/OO tests | **PASSED** | 469 checks |
| Latency and alignment | **PASSED** | impulse ≤15 ms Track |
| CPU benchmark | **PASSED** | 2.1–2.6 % average |
| pluginval VST3 strictness 7 | **PASSED** | log |
| pluginval AU strictness 7 | **PASSED** | log |
| auval AU | **PASSED** | log |
| Host scan/insert/state smoke | **PASSED** | harness logs |
| Logic + VST3 DAW workflow | **BLOCKED** | install bundles, run `docs/MANUAL_CHECKLIST.md` |
| Human listening | **BLOCKED** | supply dry takes; render via `--vocal` |
| Notarization / Developer ID | **BLOCKED** | requires Apple credentials; ad-hoc only |
| Universal x86_64 | **NOT BUILT** | target machine is arm64; build with `CMAKE_OSX_ARCHITECTURES=arm64;x86_64` on a machine with Rosetta/x86_64 SDK support |
| True Peak limiter | **NOT CLAIMED** | sample-peak limiter only; no independent oversampled True Peak meter |

## 11. Reproduce

```bash
# Offline JUCE 8.0.15 checkout:
VR_JUCE_PATH=/path/to/JUCE ./scripts/build.sh

# or let CMake fetch the 8.0.15 pin:
./scripts/build.sh

./scripts/test.sh
PLUGINVAL_BIN=/path/to/pluginval ./scripts/validate.sh
./scripts/package.sh
```

The tarball hash is written to `dist/ademusic-vocal-rack-2.2.0-macOS-arm64.zip.sha256`.
