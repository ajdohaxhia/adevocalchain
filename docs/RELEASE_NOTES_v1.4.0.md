# ademusic Vocal Rack 1.4.0 — clean signal path

A DSP-quality release focused on the reported muffled/flanged character.

## Clean-path fixes

- **Phase-vocoder bypass.** When pitch correction is off, the shifter is no longer
  in the audio path at all. A dedicated latency-aligned dry path feeds the rest
  of the chain; previously the phase vocoder ran even when Tune was bypassed.
  This removes its low-frequency smearing and the hollow/flanged character.
- **Unity correction is transparent.** When the target note equals the detected
  note, the shifter now uses a time-domain windowed OLA instead of the phase
  vocoder. 110 Hz and 220 Hz unity tests are level-exact in both Track and Mix.
- **Identity phase locking.** Active shifts now lock the bins around each
  spectral peak, preserving the analysis-phase relationship instead of moving
  every bin independently. The A440 level is within 0.2 dB and the hollow
  phasiness is substantially reduced.
- **Saturation dry path verified.** Zero-blend oversampled saturation is
  phase-exact after the shifter fix.
- **Zero-amount spectral cleanup verified transparent.**
- Existing DSP topology, parameters, presets, automation IDs and session state
  are unchanged.

## Quality regression suite

The headless suite now has **410 checks**, including:

- bypassed tuning/saturation dry path phase correlation > 0.9999;
- zero-amount spectral cleanup effectively transparent;
- unity correction preserves level at 110 Hz and 220 Hz;
- phase-locked A440 correction keeps level.

## Contents

- VST3 + AUv2, macOS arm64.
- 96 factory presets / 16 banks, redrawn 1.3 editor.
- 82 parameters; unchanged preset/session formats.
- Track 720 samples / 15.00 ms; Mix 3716 / 77.42 ms at 48 kHz.

## Install

```bash
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
unzip -q ademusic-vocal-rack-1.4.0-macOS-arm64.zip
cp -R "ademusic-vocal-rack-1.4.0-macOS-arm64/VST3/ademusic Vocal Rack.vst3" \
      ~/Library/Audio/Plug-Ins/VST3/
cp -R "ademusic-vocal-rack-1.4.0-macOS-arm64/Components/ademusic Vocal Rack.component" \
      ~/Library/Audio/Plug-Ins/Components/
```

Clear quarantine if downloaded; restart the DAW and rescan.

## Signing and license

Ad-hoc signed, not notarized. Source AGPL-3.0-or-later; JUCE 8.0.15 dual
AGPL/commercial; MIT phase-vocoder notice retained.
