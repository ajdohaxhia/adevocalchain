# ademusic Vocal Rack 2.3.0 — complete metering pass

## Added

- **Input peak** meter (IN is now peak, not RMS) and **stereo L/R split bars**
  for input and output.
- **Output RMS** readout in the footer.
- **True Peak hold** and **OVER** indication in the footer while True Peak
  mode is enabled.
- True Peak CPU matrix across 44.1/48/88.2/96 kHz and 64/128/256/512 blocks.
- True Peak latency reporting checked across the matrix.

## Metering

- 4× true-peak output meter, active with True Peak mode.
- LUFS-S (3 s K-weighted short-term) with footer display.
- Compressor, limiter and dynamic EQ gain-reduction rows.
- Input/output L/R peak activity.

## True Peak performance

16/16 combinations passed. Average processing was 2.1–4.7 % of the block
budget; reported Mix latency was 3727 samples at 44.1 kHz and 3737 at 48 kHz
with True Peak on.

## Limitations

- True-peak meter hold/overload is shown in the footer; no dedicated
  large-format TP meter.
- LUFS is short-term, not gated integrated LUFS or LRA.
- Real-voice/DAW listening remains BLOCKED in this environment.
- Ad-hoc signing only; not notarized.
