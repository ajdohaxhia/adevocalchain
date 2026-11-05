# Build status

**Phase:** Release validation complete (local). DAW/listening checks blocked by
this environment.

Updated: 2026-11-05 12:14 Europe/Rome

## Last successful commands

```bash
cmake -S . -B build-release -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DVR_JUCE_PATH=/Users/ade/Downloads/megatune/.juce \
  -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build-release --target VocalRack_VST3 VocalRack_AU VocalRackTests -j8
./build-release/VocalRackTests_artefacts/Release/VocalRackTests
# 96 presets / 16 banks; 400 checks

tools/pluginval/pluginval.app/Contents/MacOS/pluginval --strictness-level 7 \
  --validate "build-release/VocalRack_artefacts/Release/VST3/ademusic Vocal Rack.vst3"
# SUCCESS
tools/pluginval/pluginval.app/Contents/MacOS/pluginval --strictness-level 7 \
  --validate "$HOME/Library/Audio/Plug-Ins/Components/ademusic Vocal Rack.component"
# SUCCESS
auval -v aufx Vrck Adms
# AU VALIDATION SUCCEEDED
```

## Artifacts

- `build-release/VocalRack_artefacts/Release/VST3/ademusic Vocal Rack.vst3`
- `build-release/VocalRack_artefacts/Release/AU/ademusic Vocal Rack.component`
- `dist/ademusic-vocal-rack-1.2.1-macOS-arm64.zip`
  SHA-256 `a612bb4dab898d19685a26ad4ae245b33b9b567a2008e9087d389de85e38a4f2`
- `presets/ademusic-vocal-rack-factory.json`
- `docs/VERIFICATION.md`
- `docs/verification/tests.log` (400 checks)
- `docs/verification/pluginval-vst3.log`
- `docs/verification/pluginval-au.log`
- `docs/verification/auval.log`
- `docs/verification/benchmark.txt`
- `docs/verification/latency.txt`
- `docs/vocal-rack-editor.png`, `docs/vocal-rack-editor-detail.png`,
  `docs/vocal-rack-preset-browser.png`

## Measured

- Track latency 720 samples = 15.00 ms @48 kHz; 655 / 1309 / 1440 at
  44.1 / 88.2 / 96 kHz.
- Mix latency 3716 samples = 77.42 ms @48 kHz.
- CPU track 2.1 %, mix 2.6 % average block budget; p99 3.4 % / 7.8 %.
- Hard Tune 0.36 cents median; noise reduction 8.19 dB with -0.81 dB voice
  change; expander 19.92 dB; alias -24.88 → -44.41 dB with 2x oversampling.

## Real blockers

1. **No DAW installed** — Logic/REAPER/Ableton manual workflow is blocked. Run
   `docs/MANUAL_CHECKLIST.md` on a machine with the host.
2. **No authorized dry vocal takes** — human listening is blocked. Use
   `--vocal <take.wav> <outdir>` locally.
3. **No Apple signing credentials** — bundles are ad-hoc signed and not
   notarized; public distribution needs Developer ID + notarization.
4. **arm64 only** — universal/x86_64 was not built; the target machine is M4.

## Next commands on a DAW machine

```bash
./scripts/install-local.sh
# Logic: Plug-in Manager -> rescan AU -> Audio FX -> ademusic
# VST3 host: rescan VST3 folder -> insert
```

Then follow `docs/MANUAL_CHECKLIST.md`.
