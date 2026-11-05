# Preset format

Version 1. The factory bank and every user preset use the same schema and the
same validation/loading path.

```json
{
  "format": "ademusic-vocal-rack-preset",
  "version": 1,
  "id": "factory.tracking.clean-start",
  "name": "Clean Start",
  "bank": "Tracking and Cleanup",
  "description": "Unity starting point with every processor disengaged...",
  "useCase": "Checking latency, bus routing and session recall...",
  "firstAdjustment": "For a louder take, raise global.output in 1 dB steps...",
  "parameters": {
    "global.input": 0.0,
    "exp.bypass": 1,
    "pitch.bypass": 1,
    "pitch.mask": 4095,
    "...": "<all 82 stable parameter ids>"
  }
}
```

## Rules

- `format` must be exactly `ademusic-vocal-rack-preset`.
- `version` must be `1`. Unknown future versions are rejected without applying
  anything.
- `id`, `name` and `bank` are non-empty strings. Factory ids begin with
  `factory.`; user ids begin with `user.`.
- `parameters` must contain **every one of the 82 parameter ids**. Unknown ids
  are rejected. Values are denormalised (the same values a host automates):
  floats in engineering units, booleans `0` or `1`, choices their index,
  `pitch.mask` an integer bit mask.
- File size is limited to 256 KiB and must be valid JSON before parsing.
- All values are range-checked against `Source/Parameters.cpp`. Out-of-range or
  non-finite values reject the whole preset; nothing is partially applied.
- Factory presets are immutable. User presets are written to a temporary file in
  the user folder and atomically moved into place.

## Files and locations

- Factory bank: `presets/ademusic-vocal-rack-factory.json` (also embedded).
- User presets: `~/Library/Application Support/ademusic/Vocal Rack/Presets/*.vocalrack.json`.
- Import accepts a file with the same schema and adds a user preset with a new
  stable user id.
- Export writes the selected preset's full metadata and parameters.

## State vs preset

A DAW session stores the full effective instance state: all 82 parameters, the
quality configuration, current preset identity, edited flag, Keep Song Key,
A/B slots, and A/B active slot. Session recall and explicit full-state import do
not depend on the preset file still existing; they restore the edited values
themselves.

Factory browsing applies a preset through one state transaction. With
**Keep Song Key** on (default), the current `pitch.tonic`, `pitch.scale` and
`pitch.mask` are preserved. Turn it off to load the preset's stored tuning.
User-preset loads and imports restore their saved tuning values.

## A/B

A and B each hold a complete effective state, including output trim, so a
manual level match is preserved. Switch with A/B; COPY copies the active slot to
the other one. Loading a factory or user preset resets A to the newly loaded
state and clears B.
