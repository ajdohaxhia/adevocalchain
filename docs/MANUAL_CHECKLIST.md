# Manual checklist (DAW and listening)

This environment has **no DAW installed** (no Logic Pro, REAPER, Ableton Live)
and no authorized real dry vocal takes. The automated validation is complete in
`docs/VERIFICATION.md`; the following checks are the exact manual steps still
required and are reported as **blocked by the environment**.

## Install

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
cp -R "dist/ademusic-vocal-rack-2.1.0-macOS-arm64/VST3/ademusic Vocal Rack.vst3" \
      ~/Library/Audio/Plug-Ins/VST3/
cp -R "dist/ademusic-vocal-rack-2.1.0-macOS-arm64/Components/ademusic Vocal Rack.component" \
      ~/Library/Audio/Plug-Ins/Components/
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/"ademusic Vocal Rack.vst3" 2>/dev/null || true
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/"ademusic Vocal Rack.component" 2>/dev/null || true
```

Expected identities:
- AU: type `aufx`, subtype `Vrck`, manufacturer `Adms`.
- VST3: bundle `com.ademusic.vocalrack`; CID `ABCDEF019182FAEB41646D735672636B`.
- Architecture: arm64 only.

## Logic Pro (AU)

1. Launch Logic; open **Settings > Plug-in Manager**; confirm **ademusic Vocal
   Rack** appears and validates. If not, quit Logic, run
   `killall -9 AudioComponentRegistrar`, relaunch.
2. Create an audio track, insert **Audio FX > Audio Units > ademusic > ademusic
   Vocal Rack** on a mono vocal track. The editor should open.
3. Confirm the header shows preset **Clean Start** or Init, latency **720 smp /
   15.00 ms** (48 kHz, Track), and the meters move with the vocal.
4. Drag Input, Output, Mix, Tune Amount, Retune, Comp Threshold; confirm audio
   changes and values are readable. Double-click a knob to reset it.
5. Automate `global.output` and `pitch.amount`; play back and confirm smooth
   movement, no clicks or dropouts.
6. Open **BROWSE**. Filter banks, search, load **Neon Pop Lead**, **Hard Tune**,
   **End of the Night**. Confirm the edited asterisk appears after a control
   move, Keep Song Key toggles, and A/B switches restore output trim.
7. Save As a user preset; duplicate, rename, delete it. Export it, import it
   again. Confirm factory entries remain immutable.
8. Switch Quality from TRACK to MIX (stop transport first). Confirm latency
   becomes **3716 smp / 77.42 ms** at 48 kHz and the spectral denoiser is
   audible. Switch back.
9. Save the project, quit Logic, reopen. Confirm all controls, key/mask,
   quality, preset identity, A/B, and the sound are restored. Delete the user
   preset file on disk, reopen the project, and confirm the edited state still
   restores.
10. Bounce a phrase plus 2 s of tail with delay/reverb engaged; confirm the tail
    is present and the ceiling is not exceeded.

## VST3 host (REAPER, Ableton Live, or similar)

Repeat steps 2–10 using the VST3. Additionally:
- Rescan the VST3 folder; confirm the plugin is listed as `ademusic Vocal Rack`
  (vendor ademusic).
- Test mono-to-stereo by inserting on a mono track and confirming stereo
  returns; test mono-in/mono-out and confirm usable delay/reverb tails.
- Test a 96 kHz project; confirm host latency changes to 1440 samples in Track
  and 7428 in Mix.
- Open and close the editor repeatedly during playback; the meters and controls
  must recover and audio must continue.

## Listening (real takes)

Use three dry takes at matched loudness (match by output trim or a meter):
1. spoken/rap, 2. melodic singing, 3. higher/breathier singing.

For each, cycle at least **Clean Start, Record Ready, Home Booth, Laptop Fan,
Plosive Rescue, Bright Mic Tamer, Midnight Rap Lead, 808 Club Lead, Neon Pop
Lead, Late Night Falsetto, Velvet Runs, Hard Tune, Distorted Hook, End of the
Night**.

Check:
- Unvoiced consonants and breaths are not smeared or choked.
- Natural mode keeps vibrato and runs; Hard Tune snaps audibly but does not
  warble on held notes.
- Noise suppression lowers steady fan/hiss without hollowing sustained notes.
- Sibilance is controlled without lisping; low band is intact.
- Delay/reverb tails ring out, duck under the lead, and do not click on preset
  changes.
- A/B level match is usable and switching does not jump in level.
- No zipper noise on fast automation.

No take was auditioned in this environment. Generated-fixture renders can be
produced locally with:

```bash
./build-release/VocalRackTests_artefacts/Release/VocalRackTests \
  --vocal /path/to/dry-vocal.wav ./vocal-renders
```

That writes 96 WAV files (48 kHz, 24-bit stereo, latency-trimmed, 2 s tail).

## Expected measurements

| Setting | 44.1 kHz | 48 kHz | 88.2 kHz | 96 kHz |
|---|---:|---:|---:|---:|
| Track latency | 655 smp / 14.85 ms | 720 / 15.00 | 1309 / 14.84 | 1440 / 15.00 |
| Mix latency | 3706 / 84.04 | 3716 / 77.42 | 7407 / 83.98 | 7428 / 77.38 |

Warmed Release CPU on this M4, 48 kHz stereo, active vocal chain (measured):
Track 2.1 % of block budget average; Mix 2.6 % average; worst measured p99
7.5 %.

## If a host is unavailable

Use the bundled headless harness (no DAW) as a smoke test:

```bash
./build-release/VocalRackTests_artefacts/Release/VocalRackTests \
  --host "build-release/VocalRack_artefacts/Release/VST3/ademusic Vocal Rack.vst3" vst3
./build-release/VocalRackTests_artefacts/Release/VocalRackTests \
  --host "$HOME/Library/Audio/Plug-Ins/Components/ademusic Vocal Rack.component" au
```

This confirms scan, instantiate, prepare, process, latency, parameter discovery,
state read/write. It is **not** a DAW UI automation test.
