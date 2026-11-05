#!/usr/bin/env python3
"""Generate ademusic Vocal Rack's 36 factory presets.

Source of truth for IDs/ranges is Tools/params.json, extracted from
Source/Parameters.cpp.  The same JSON schema is used by user presets, so the
factory bank exercises the public loader.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPECS = json.loads((ROOT / "Tools" / "params.json").read_text())
DEFAULT = {s["id"]: s["default"] for s in SPECS}
RANGES = {s["id"]: (s["min"], s["max"], s["type"], s["numChoices"]) for s in SPECS}


def mask_for_scale(scale, tonic):
    if scale == 0:
        return 0x0FFF
    if scale == 3:
        return None
    intervals = [0, 2, 4, 5, 7, 9, 11] if scale == 1 else [0, 2, 3, 5, 7, 8, 10]
    mask = 0
    for interval in intervals:
        mask |= 1 << ((tonic + interval) % 12)
    return mask


def preset(bank, name, description, use_case, first_adjustment, **overrides):
    params = dict(DEFAULT)
    params.update(overrides)

    scale = int(params["pitch.scale"])
    tonic = int(params["pitch.tonic"])
    if scale != 3:
        params["pitch.mask"] = mask_for_scale(scale, tonic)

    # Keep booleans and choices integral so the JSON is unambiguous.
    for spec in SPECS:
        pid = spec["id"]
        typ = spec["type"]
        if typ in ("boolean", "choice", "intValue"):
            params[pid] = int(round(float(params[pid])))
        else:
            params[pid] = float(params[pid])

    return {
        "bank": bank,
        "name": name,
        "description": description,
        "useCase": use_case,
        "firstAdjustment": first_adjustment,
        "parameters": params,
    }


BANK_TRACKING = "Tracking and Cleanup"
BANK_RAP = "Rap and Trap"
BANK_POP = "Pop and Dance"
BANK_RNB = "R&B and Melodic"
BANK_ACOUSTIC = "Natural and Acoustic"
BANK_FX = "Effects and Throws"

PRESETS = [
    # ------------------------------------------------------------------ tracking
    preset(
        BANK_TRACKING, "Clean Start",
        "Unity starting point with every processor disengaged and the dry routing verified.",
        "Checking latency, bus routing and session recall before any processing.",
        "For a louder take, raise global.output in 1 dB steps; leave the bypasses alone.",
        **{
            "global.bypass": 0, "global.input": 0.0, "global.output": 0.0, "global.mix": 100.0,
            "exp.bypass": 1, "dn.bypass": 1, "pitch.bypass": 1, "ceq.bypass": 1,
            "comp.bypass": 1, "ds.bypass": 1, "teq.bypass": 1, "sat.bypass": 1,
            "dly.bypass": 1, "rev.bypass": 1, "lim.bypass": 1,
        },
    ),
    preset(
        BANK_TRACKING, "Record Ready",
        "Gentle peak control, a fast low-latency expander, little ambience and a practical limiter.",
        "Tracking a new vocal with predictable monitoring and quick setup.",
        "For a quieter take, lower comp.threshold 3 dB and add 2 dB makeup.",
        **{
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 10.0, "exp.attack": 1.0,
            "exp.hold": 60.0, "exp.release": 150.0,
            "comp.threshold": -20.0, "comp.ratio": 2.5, "comp.attack": 8.0, "comp.release": 80.0,
            "comp.knee": 10.0, "comp.makeup": 1.5,
            "ceq.hpf": 90.0, "ceq.cut1Gain": -1.5, "ceq.cut1Freq": 300.0,
            "teq.lowGain": -1.0, "teq.highGain": -1.0,
            "lim.bypass": 0, "lim.ceiling": -0.5, "lim.release": 50.0,
            "global.output": -1.0,
        },
    ),
    preset(
        BANK_TRACKING, "Home Booth",
        "Moderate spectral reduction for steady room noise with careful low-mid cleanup.",
        "Recording in a small untreated room with a steady noise floor.",
        "For a brighter take, raise teq.presGain 1 dB and move teq.highFreq to 10 kHz.",
        **{
            "dn.bypass": 0, "dn.amount": 45.0, "dn.maxReduction": 14.0,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 14.0, "exp.hold": 80.0,
            "exp.release": 220.0,
            "ceq.hpf": 85.0, "ceq.cut1Freq": 220.0, "ceq.cut1Gain": -3.0, "ceq.cut1Q": 1.1,
            "ceq.cut2Freq": 500.0, "ceq.cut2Gain": -1.5, "ceq.cut2Q": 0.9,
            "comp.threshold": -24.0, "comp.ratio": 3.0, "comp.attack": 10.0,
            "comp.release": 120.0, "comp.knee": 8.0, "comp.makeup": 2.0,
            "teq.lowFreq": 120.0, "teq.lowGain": -1.0, "teq.presGain": 0.5,
            "teq.highGain": -1.5, "lim.ceiling": -0.4,
        },
    ),
    preset(
        BANK_TRACKING, "Laptop Fan",
        "Focused, bounded steady-noise suppression for computer fan and room hiss.",
        "Laptop or fan noise under a close vocal where the voice must stay intact.",
        "For a quieter take, lower dn.amount to 45% and exp.threshold to -55 dB.",
        **{
            "dn.bypass": 0, "dn.amount": 65.0, "dn.maxReduction": 18.0,
            "exp.bypass": 0, "exp.threshold": -45.0, "exp.range": 12.0, "exp.hold": 40.0,
            "exp.release": 180.0,
            "ceq.hpf": 100.0, "ceq.cut1Freq": 420.0, "ceq.cut1Gain": -2.5,
            "ceq.cut2Freq": 1200.0, "ceq.cut2Gain": -1.5,
            "comp.threshold": -26.0, "comp.ratio": 2.5, "comp.attack": 8.0,
            "comp.release": 100.0, "comp.knee": 10.0, "comp.makeup": 2.0,
            "comp.detHPF": 150.0,
            "teq.lowGain": -1.0, "teq.presGain": 0.5, "teq.highGain": -1.0,
            "lim.ceiling": -0.4,
        },
    ),
    preset(
        BANK_TRACKING, "Plosive Rescue",
        "Firm high-pass, low-mid cleanup and a bass-filtered compressor detector.",
        "Repairing close-miked plosives and proximity buildup on a take.",
        "For a darker take, lower ceq.cut1Freq to 150 Hz and add 1 dB teq.lowGain.",
        **{
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 12.0, "exp.hold": 60.0,
            "exp.release": 150.0,
            "ceq.hpf": 120.0, "ceq.cut1Freq": 180.0, "ceq.cut1Gain": -4.0, "ceq.cut1Q": 1.2,
            "ceq.cut2Freq": 350.0, "ceq.cut2Gain": -3.0, "ceq.cut2Q": 1.0,
            "comp.threshold": -22.0, "comp.ratio": 3.0, "comp.attack": 6.0,
            "comp.release": 80.0, "comp.knee": 6.0, "comp.makeup": 2.0,
            "comp.detHPF": 180.0,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -32.0, "ds.range": 6.0,
            "ds.release": 60.0,
            "teq.lowFreq": 100.0, "teq.lowGain": -1.5,
            "lim.ceiling": -0.5,
        },
    ),
    preset(
        BANK_TRACKING, "Bright Mic Tamer",
        "Stronger split-band de-essing with a softened top end for a bright condenser.",
        "A bright microphone or a sibilant close take that needs taming.",
        "For a brighter take, raise teq.highGain to 0 dB and lower ds.range to 8 dB.",
        **{
            "ceq.hpf": 95.0, "ceq.cut1Freq": 500.0, "ceq.cut1Gain": -2.0,
            "ceq.cut2Freq": 3000.0, "ceq.cut2Gain": -2.0, "ceq.cut2Q": 1.2,
            "ds.bypass": 0, "ds.freq": 7800.0, "ds.threshold": -36.0, "ds.range": 12.0,
            "ds.release": 55.0,
            "teq.highFreq": 9000.0, "teq.highGain": -2.5, "teq.presGain": 0.5,
            "teq.lowGain": -0.5,
            "comp.threshold": -24.0, "comp.ratio": 2.5, "comp.attack": 10.0,
            "comp.release": 100.0, "comp.makeup": 1.5,
            "lim.ceiling": -0.5,
        },
    ),

    # ------------------------------------------------------------------ rap
    preset(
        BANK_RAP, "Midnight Rap Lead",
        "Close, darker rap lead with even dynamics and a short stereo slap.",
        "A conversational drake-style close rap lead in a dark, late-night pocket.",
        "For a quieter take, raise comp.threshold 4 dB and lower delay level 2 dB.",
        **{
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 10.0, "exp.hold": 60.0,
            "exp.release": 180.0,
            "ceq.hpf": 85.0, "ceq.cut1Freq": 350.0, "ceq.cut1Gain": -2.5,
            "ceq.cut2Freq": 2500.0, "ceq.cut2Gain": -1.5,
            "comp.threshold": -26.0, "comp.ratio": 3.2, "comp.attack": 10.0,
            "comp.release": 120.0, "comp.knee": 8.0, "comp.makeup": 3.5,
            "comp.detHPF": 140.0,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -32.0, "ds.range": 8.0,
            "ds.release": 70.0,
            "teq.lowFreq": 150.0, "teq.lowGain": 1.5, "teq.presFreq": 2800.0,
            "teq.presGain": 0.5, "teq.highFreq": 10000.0, "teq.highGain": -1.5,
            "sat.bypass": 0, "sat.mode": 0, "sat.drive": 3.5, "sat.blend": 60.0,
            "sat.output": -0.5,
            "dly.bypass": 0, "dly.sync": 0, "dly.ms": 95.0, "dly.feedback": 12.0,
            "dly.level": -20.0, "dly.lowCut": 300.0, "dly.highCut": 5500.0,
            "dly.pingpong": 1, "dly.duck": 35.0,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 18.0, "rev.decay": 0.9,
            "rev.damping": 60.0, "rev.lowCut": 180.0, "rev.highCut": 6000.0,
            "rev.level": -22.0, "rev.duck": 45.0,
            "lim.ceiling": -0.3,
        },
    ),
    preset(
        BANK_RAP, "808 Club Lead",
        "Fast tune, forward midrange, dense compression and short ambience.",
        "A club-ready rap lead that has to stay forward over an 808.",
        "For a darker take, lower teq.presGain 2 dB and raise rev.highCut to 5 kHz.",
        **{
            "pitch.bypass": 0, "pitch.amount": 90.0, "pitch.retune": 8.0,
            "pitch.humanize": 5.0, "pitch.scale": 0, "pitch.range": 1,
            "exp.bypass": 0, "exp.threshold": -50.0, "exp.range": 12.0, "exp.hold": 40.0,
            "exp.release": 120.0,
            "ceq.hpf": 95.0, "ceq.cut1Freq": 400.0, "ceq.cut1Gain": -2.0,
            "ceq.cut2Freq": 3000.0, "ceq.cut2Gain": -1.0,
            "comp.threshold": -28.0, "comp.ratio": 4.0, "comp.attack": 5.0,
            "comp.release": 80.0, "comp.knee": 6.0, "comp.makeup": 4.5,
            "comp.detHPF": 150.0,
            "ds.bypass": 0, "ds.freq": 7500.0, "ds.threshold": -34.0, "ds.range": 8.0,
            "ds.release": 50.0,
            "teq.lowGain": 1.0, "teq.presFreq": 3500.0, "teq.presGain": 2.0,
            "teq.highGain": -0.5,
            "sat.bypass": 0, "sat.mode": 1, "sat.drive": 6.0, "sat.blend": 45.0,
            "sat.output": -1.0,
            "dly.bypass": 0, "dly.sync": 1, "dly.division": 1, "dly.feel": 1,
            "dly.ms": 180.0, "dly.feedback": 18.0, "dly.level": -20.0,
            "dly.lowCut": 300.0, "dly.highCut": 6000.0, "dly.duck": 50.0,
            "rev.bypass": 0, "rev.type": 1, "rev.predelay": 10.0, "rev.decay": 0.8,
            "rev.damping": 30.0, "rev.lowCut": 200.0, "rev.highCut": 7000.0,
            "rev.level": -22.0, "rev.duck": 55.0,
            "lim.ceiling": -0.4,
        },
    ),
    preset(
        BANK_RAP, "Punch In",
        "Dry and close, with crisp consonants and quick recovery between bars.",
        "Punching in rap verses and doubling lines without building a wet sound.",
        "For a louder take, raise comp.makeup 2 dB and lower lim.ceiling to -0.8 dBFS.",
        **{
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 10.0, "exp.hold": 40.0,
            "exp.release": 100.0,
            "ceq.hpf": 90.0, "ceq.cut1Freq": 300.0, "ceq.cut1Gain": -1.5,
            "comp.threshold": -22.0, "comp.ratio": 3.5, "comp.attack": 4.0,
            "comp.release": 60.0, "comp.knee": 6.0, "comp.makeup": 2.5,
            "comp.detHPF": 160.0,
            "ds.bypass": 0, "ds.freq": 8000.0, "ds.threshold": -34.0, "ds.range": 6.0,
            "ds.release": 40.0,
            "teq.lowGain": -0.5, "teq.presFreq": 4000.0, "teq.presGain": 1.5,
            "teq.highGain": 0.5,
            "lim.ceiling": -0.5,
        },
    ),
    preset(
        BANK_RAP, "Dry Pocket",
        "Tight, centered rap with minimal space and controlled low mids.",
        "A dry pocket vocal for dense arrangements that need space elsewhere.",
        "For a brighter take, raise teq.presGain 1.5 dB; for a darker take, lower teq.highGain 1 dB.",
        **{
            "exp.bypass": 0, "exp.threshold": -50.0, "exp.range": 8.0, "exp.hold": 50.0,
            "exp.release": 120.0,
            "ceq.hpf": 80.0, "ceq.cut1Freq": 250.0, "ceq.cut1Gain": -2.0,
            "ceq.cut2Freq": 500.0, "ceq.cut2Gain": -1.0,
            "comp.threshold": -24.0, "comp.ratio": 3.0, "comp.attack": 8.0,
            "comp.release": 90.0, "comp.knee": 8.0, "comp.makeup": 2.0,
            "comp.detHPF": 130.0,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -33.0, "ds.range": 6.0,
            "ds.release": 50.0,
            "teq.lowFreq": 110.0, "teq.lowGain": 0.5, "teq.presGain": 0.5,
            "teq.highGain": -1.0,
            "lim.ceiling": -0.4,
        },
    ),
    preset(
        BANK_RAP, "Dark Melodic Trap",
        "Firm tune, darker tone and a ducked triplet echo.",
        "Melodic trap verses that need pitch lock without a bright, glossy top.",
        "For a quieter take, raise comp.threshold 3 dB; for a brighter take, raise teq.highGain 2 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 80.0, "pitch.retune": 25.0,
            "pitch.humanize": 15.0, "pitch.scale": 2, "pitch.tonic": 9,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 10.0, "exp.hold": 50.0,
            "exp.release": 150.0,
            "ceq.hpf": 90.0, "ceq.cut1Freq": 300.0, "ceq.cut1Gain": -2.0,
            "ceq.cut2Freq": 3000.0, "ceq.cut2Gain": -1.0,
            "comp.threshold": -26.0, "comp.ratio": 3.0, "comp.attack": 12.0,
            "comp.release": 140.0, "comp.knee": 10.0, "comp.makeup": 2.5,
            "comp.detHPF": 150.0,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -32.0, "ds.range": 6.0,
            "ds.release": 70.0,
            "teq.lowFreq": 150.0, "teq.lowGain": 1.0, "teq.presGain": 0.0,
            "teq.highFreq": 7000.0, "teq.highGain": -2.0,
            "sat.bypass": 0, "sat.mode": 0, "sat.drive": 4.0, "sat.blend": 50.0,
            "sat.output": -0.5,
            "dly.bypass": 0, "dly.sync": 1, "dly.division": 2, "dly.feel": 2,
            "dly.feedback": 32.0, "dly.level": -18.0, "dly.lowCut": 300.0,
            "dly.highCut": 5000.0, "dly.duck": 50.0,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 25.0, "rev.decay": 1.2,
            "rev.damping": 65.0, "rev.lowCut": 200.0, "rev.highCut": 5000.0,
            "rev.level": -24.0, "rev.duck": 40.0,
            "lim.ceiling": -0.4,
        },
    ),
    preset(
        BANK_RAP, "Wide Ad-libs",
        "Strong pitch lock with filtered ping-pong and a wider wet return.",
        "Stacked ad-libs that should sit around the lead, not on top of it.",
        "For a drier take, lower rev.level 4 dB and delay level 3 dB; for a louder take, raise input 2 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 95.0, "pitch.retune": 5.0,
            "pitch.humanize": 5.0, "pitch.scale": 1, "pitch.tonic": 0,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 10.0, "exp.hold": 40.0,
            "exp.release": 120.0,
            "ceq.hpf": 100.0, "ceq.cut1Freq": 400.0, "ceq.cut1Gain": -1.5,
            "comp.threshold": -28.0, "comp.ratio": 4.0, "comp.attack": 6.0,
            "comp.release": 90.0, "comp.knee": 6.0, "comp.makeup": 3.0,
            "comp.detHPF": 160.0,
            "ds.bypass": 0, "ds.freq": 7500.0, "ds.threshold": -34.0, "ds.range": 8.0,
            "ds.release": 50.0,
            "teq.lowGain": 0.0, "teq.presFreq": 3500.0, "teq.presGain": 1.5,
            "teq.highGain": 1.0,
            "sat.bypass": 0, "sat.mode": 1, "sat.drive": 5.0, "sat.blend": 40.0,
            "sat.output": -0.5,
            "dly.bypass": 0, "dly.sync": 1, "dly.division": 1, "dly.feel": 0,
            "dly.feedback": 28.0, "dly.level": -15.0, "dly.lowCut": 400.0,
            "dly.highCut": 7000.0, "dly.pingpong": 1, "dly.duck": 40.0,
            "rev.bypass": 0, "rev.type": 1, "rev.predelay": 30.0, "rev.decay": 1.8,
            "rev.damping": 25.0, "rev.lowCut": 200.0, "rev.highCut": 8000.0,
            "rev.level": -18.0, "rev.duck": 35.0,
            "lim.ceiling": -0.4,
        },
    ),

    # ------------------------------------------------------------------ pop
    preset(
        BANK_POP, "Neon Pop Lead",
        "Focused dance lead with clean air, a short ducked plate and a 90–120 Hz high-pass.",
        "A bright, focused pop topline over a dense dance production.",
        "For a brighter take, raise teq.highGain 1.5 dB and ds.range 2 dB; for a darker take, lower teq.highFreq to 8 kHz.",
        **{
            "pitch.bypass": 0, "pitch.amount": 70.0, "pitch.retune": 45.0,
            "pitch.humanize": 25.0, "pitch.scale": 2, "pitch.tonic": 9,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 10.0, "exp.hold": 60.0,
            "exp.release": 150.0,
            "ceq.hpf": 105.0, "ceq.cut1Freq": 300.0, "ceq.cut1Gain": -1.5,
            "ceq.cut2Freq": 3500.0, "ceq.cut2Gain": -1.0,
            "comp.threshold": -24.0, "comp.ratio": 3.0, "comp.attack": 8.0,
            "comp.release": 90.0, "comp.knee": 8.0, "comp.makeup": 2.0,
            "comp.detHPF": 150.0,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -34.0, "ds.range": 10.0,
            "ds.release": 60.0,
            "teq.lowGain": -0.5, "teq.presFreq": 4000.0, "teq.presGain": 1.5,
            "teq.highFreq": 10000.0, "teq.highGain": 1.5,
            "dly.bypass": 0, "dly.sync": 1, "dly.division": 2, "dly.feel": 1,
            "dly.feedback": 22.0, "dly.level": -20.0, "dly.lowCut": 300.0,
            "dly.highCut": 7000.0, "dly.duck": 45.0,
            "rev.bypass": 0, "rev.type": 1, "rev.predelay": 35.0, "rev.decay": 1.15,
            "rev.damping": 25.0, "rev.lowCut": 180.0, "rev.highCut": 8500.0,
            "rev.level": -18.0, "rev.duck": 45.0,
            "lim.ceiling": -0.3,
        },
    ),
    preset(
        BANK_POP, "Nu-Disco Lead",
        "Smooth presence, a short plate and a quiet tempo echo with enough de-essing for bright hats.",
        "A smooth nu-disco topline over a bright, percussion-heavy track.",
        "For a brighter take, raise teq.highGain 1 dB; for a quieter take, lower comp.threshold 2 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 55.0, "pitch.retune": 90.0,
            "pitch.humanize": 45.0, "pitch.scale": 1, "pitch.tonic": 0,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 8.0, "exp.hold": 60.0,
            "exp.release": 160.0,
            "ceq.hpf": 100.0, "ceq.cut1Freq": 350.0, "ceq.cut1Gain": -1.5,
            "comp.threshold": -26.0, "comp.ratio": 2.5, "comp.attack": 12.0,
            "comp.release": 120.0, "comp.knee": 10.0, "comp.makeup": 2.0,
            "ds.bypass": 0, "ds.freq": 7500.0, "ds.threshold": -35.0, "ds.range": 12.0,
            "ds.release": 55.0,
            "teq.lowGain": 0.0, "teq.presFreq": 3000.0, "teq.presGain": 1.0,
            "teq.highFreq": 10000.0, "teq.highGain": 2.0,
            "sat.bypass": 0, "sat.mode": 0, "sat.drive": 3.0, "sat.blend": 35.0,
            "sat.output": -0.5,
            "dly.bypass": 0, "dly.sync": 1, "dly.division": 2, "dly.feel": 0,
            "dly.feedback": 20.0, "dly.level": -23.0, "dly.lowCut": 300.0,
            "dly.highCut": 6500.0, "dly.duck": 55.0,
            "rev.bypass": 0, "rev.type": 1, "rev.predelay": 15.0, "rev.decay": 0.85,
            "rev.damping": 35.0, "rev.lowCut": 200.0, "rev.highCut": 8000.0,
            "rev.level": -20.0, "rev.duck": 45.0,
            "lim.ceiling": -0.4,
        },
    ),
    preset(
        BANK_POP, "House Topline",
        "Clear center, low-end room for the kick and tightly timed eighth-note throws.",
        "A house topline that must leave the low end to the kick and bass.",
        "For a darker take, lower teq.presGain 1.5 dB; for a louder take, raise comp.makeup 2 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 60.0, "pitch.retune": 70.0,
            "pitch.humanize": 35.0, "pitch.scale": 1, "pitch.tonic": 5,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 8.0, "exp.hold": 50.0,
            "exp.release": 140.0,
            "ceq.hpf": 110.0, "ceq.cut1Freq": 300.0, "ceq.cut1Gain": -2.0,
            "ceq.cut2Freq": 4000.0, "ceq.cut2Gain": -1.0,
            "comp.threshold": -25.0, "comp.ratio": 3.0, "comp.attack": 8.0,
            "comp.release": 90.0, "comp.knee": 8.0, "comp.makeup": 2.0,
            "comp.detHPF": 160.0,
            "ds.bypass": 0, "ds.freq": 7200.0, "ds.threshold": -33.0, "ds.range": 8.0,
            "ds.release": 60.0,
            "teq.lowGain": -1.0, "teq.presFreq": 3500.0, "teq.presGain": 1.0,
            "teq.highGain": 0.5,
            "dly.bypass": 0, "dly.sync": 1, "dly.division": 2, "dly.feel": 0,
            "dly.feedback": 18.0, "dly.level": -19.0, "dly.lowCut": 400.0,
            "dly.highCut": 7000.0, "dly.duck": 45.0,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 20.0, "rev.decay": 0.8,
            "rev.damping": 50.0, "rev.lowCut": 200.0, "rev.highCut": 6500.0,
            "rev.level": -24.0, "rev.duck": 55.0,
            "lim.ceiling": -0.4,
        },
    ),
    preset(
        BANK_POP, "Radio Hook",
        "Smooth leveling, glossy shelves and a compact stereo field around the lead.",
        "A radio-ready chorus hook that needs to translate on small speakers.",
        "For a quieter take, raise comp.threshold 3 dB and lower input 1 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 65.0, "pitch.retune": 55.0,
            "pitch.humanize": 30.0, "pitch.scale": 1, "pitch.tonic": 2,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 8.0, "exp.hold": 50.0,
            "exp.release": 140.0,
            "ceq.hpf": 100.0, "ceq.cut1Freq": 300.0, "ceq.cut1Gain": -1.5,
            "ceq.cut2Freq": 3500.0, "ceq.cut2Gain": -1.0,
            "comp.threshold": -24.0, "comp.ratio": 3.5, "comp.attack": 6.0,
            "comp.release": 80.0, "comp.knee": 8.0, "comp.makeup": 2.5,
            "comp.detHPF": 150.0,
            "ds.bypass": 0, "ds.freq": 7500.0, "ds.threshold": -35.0, "ds.range": 10.0,
            "ds.release": 50.0,
            "teq.lowFreq": 150.0, "teq.lowGain": 0.5, "teq.presFreq": 3500.0,
            "teq.presGain": 2.0, "teq.highFreq": 10000.0, "teq.highGain": 2.0,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 12.0, "rev.decay": 0.6,
            "rev.damping": 40.0, "rev.lowCut": 200.0, "rev.highCut": 7000.0,
            "rev.level": -26.0, "rev.duck": 50.0,
            "lim.ceiling": -0.3,
        },
    ),
    preset(
        BANK_POP, "Club Chorus",
        "Denser leveling and a brighter rhythmic return for choruses.",
        "A chorus lift that stays loud and bright without losing the lead.",
        "For a quieter take, lower comp.makeup 2 dB and sat.drive 3 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 75.0, "pitch.retune": 40.0,
            "pitch.humanize": 25.0, "pitch.scale": 2, "pitch.tonic": 9,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 10.0, "exp.hold": 50.0,
            "exp.release": 140.0,
            "ceq.hpf": 105.0, "ceq.cut1Freq": 350.0, "ceq.cut1Gain": -2.0,
            "comp.threshold": -28.0, "comp.ratio": 4.0, "comp.attack": 5.0,
            "comp.release": 80.0, "comp.knee": 6.0, "comp.makeup": 3.5,
            "comp.detHPF": 160.0,
            "ds.bypass": 0, "ds.freq": 7500.0, "ds.threshold": -35.0, "ds.range": 10.0,
            "ds.release": 50.0,
            "teq.lowGain": -0.5, "teq.presFreq": 4000.0, "teq.presGain": 1.5,
            "teq.highGain": 1.5,
            "sat.bypass": 0, "sat.mode": 1, "sat.drive": 3.0, "sat.blend": 30.0,
            "sat.output": -0.5,
            "dly.bypass": 0, "dly.sync": 1, "dly.division": 2, "dly.feel": 1,
            "dly.feedback": 30.0, "dly.level": -17.0, "dly.lowCut": 400.0,
            "dly.highCut": 8000.0, "dly.pingpong": 1, "dly.duck": 40.0,
            "rev.bypass": 0, "rev.type": 1, "rev.predelay": 20.0, "rev.decay": 1.3,
            "rev.damping": 25.0, "rev.lowCut": 200.0, "rev.highCut": 9000.0,
            "rev.level": -19.0, "rev.duck": 40.0,
            "lim.ceiling": -0.3,
        },
    ),
    preset(
        BANK_POP, "Disco Doubles",
        "Lighter dynamics, softened sibilance and subtle width for separately recorded doubles.",
        "Layering doubles behind a disco lead without making the stack muddy.",
        "For a brighter take, raise teq.highGain 1 dB; for a quieter take, lower rev.level 2 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 50.0, "pitch.retune": 100.0,
            "pitch.humanize": 55.0, "pitch.scale": 1, "pitch.tonic": 0,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 6.0, "exp.hold": 60.0,
            "exp.release": 160.0,
            "ceq.hpf": 95.0, "ceq.cut1Freq": 300.0, "ceq.cut1Gain": -1.0,
            "comp.threshold": -22.0, "comp.ratio": 2.0, "comp.attack": 15.0,
            "comp.release": 140.0, "comp.knee": 12.0, "comp.makeup": 1.5,
            "ds.bypass": 0, "ds.freq": 7500.0, "ds.threshold": -36.0, "ds.range": 8.0,
            "ds.release": 60.0,
            "teq.lowGain": 0.0, "teq.presFreq": 3000.0, "teq.presGain": 1.0,
            "teq.highGain": 1.0,
            "dly.bypass": 0, "dly.sync": 1, "dly.division": 2, "dly.feel": 0,
            "dly.feedback": 12.0, "dly.level": -24.0, "dly.lowCut": 400.0,
            "dly.highCut": 7000.0, "dly.duck": 50.0,
            "rev.bypass": 0, "rev.type": 1, "rev.predelay": 18.0, "rev.decay": 0.9,
            "rev.damping": 30.0, "rev.lowCut": 200.0, "rev.highCut": 8000.0,
            "rev.level": -22.0, "rev.duck": 40.0,
            "lim.ceiling": -0.4,
        },
    ),

    # ------------------------------------------------------------------ rnb
    preset(
        BANK_RNB, "Late Night Falsetto",
        "Airy falsetto lead with gentle tuning, a filtered dotted delay and a dark plate.",
        "A Weeknd-style airy falsetto that needs space and a long, dark tail.",
        "For a brighter take, raise teq.highGain 2 dB and rev.highCut to 8 kHz; for a quieter take, lower rev.level 3 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 70.0, "pitch.retune": 90.0,
            "pitch.humanize": 55.0, "pitch.scale": 2, "pitch.tonic": 9,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 8.0, "exp.hold": 60.0,
            "exp.release": 160.0,
            "ceq.hpf": 95.0, "ceq.cut1Freq": 350.0, "ceq.cut1Gain": -1.5,
            "comp.threshold": -24.0, "comp.ratio": 2.5, "comp.attack": 12.0,
            "comp.release": 120.0, "comp.knee": 10.0, "comp.makeup": 2.0,
            "comp.detHPF": 150.0,
            "ds.bypass": 0, "ds.freq": 7500.0, "ds.threshold": -34.0, "ds.range": 8.0,
            "ds.release": 60.0,
            "teq.lowGain": -0.5, "teq.presFreq": 3000.0, "teq.presGain": 1.0,
            "teq.highFreq": 11000.0, "teq.highGain": 2.0,
            "sat.bypass": 0, "sat.mode": 0, "sat.drive": 3.0, "sat.blend": 30.0,
            "sat.output": -0.5,
            "dly.bypass": 0, "dly.sync": 1, "dly.division": 2, "dly.feel": 1,
            "dly.feedback": 26.0, "dly.level": -19.0, "dly.lowCut": 300.0,
            "dly.highCut": 6000.0, "dly.duck": 55.0,
            "rev.bypass": 0, "rev.type": 1, "rev.predelay": 30.0, "rev.decay": 2.05,
            "rev.damping": 50.0, "rev.lowCut": 200.0, "rev.highCut": 6500.0,
            "rev.level": -17.0, "rev.duck": 45.0,
            "lim.ceiling": -0.4,
        },
    ),
    preset(
        BANK_RNB, "Warm Low Lead",
        "Low-register weight with controlled proximity and dark, short ambience.",
        "A low male lead or feature verse that needs weight without boom.",
        "For a brighter take, lower teq.lowGain 1.5 dB and raise teq.presGain 1 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 60.0, "pitch.retune": 110.0,
            "pitch.humanize": 50.0, "pitch.scale": 1, "pitch.tonic": 5,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 8.0, "exp.hold": 60.0,
            "exp.release": 180.0,
            "ceq.hpf": 70.0, "ceq.cut1Freq": 220.0, "ceq.cut1Gain": -3.0,
            "ceq.cut1Q": 1.2, "ceq.cut2Freq": 500.0, "ceq.cut2Gain": -1.5,
            "comp.threshold": -26.0, "comp.ratio": 2.5, "comp.attack": 15.0,
            "comp.release": 140.0, "comp.knee": 10.0, "comp.makeup": 2.0,
            "comp.detHPF": 100.0,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -34.0, "ds.range": 6.0,
            "ds.release": 70.0,
            "teq.lowFreq": 180.0, "teq.lowGain": 2.0, "teq.presFreq": 2500.0,
            "teq.presGain": 0.5, "teq.highFreq": 8000.0, "teq.highGain": -1.0,
            "sat.bypass": 0, "sat.mode": 0, "sat.drive": 3.5, "sat.blend": 40.0,
            "sat.output": -0.5,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 15.0, "rev.decay": 0.75,
            "rev.damping": 60.0, "rev.lowCut": 150.0, "rev.highCut": 5500.0,
            "rev.level": -24.0, "rev.duck": 50.0,
            "lim.ceiling": -0.4,
        },
    ),
    preset(
        BANK_RNB, "Velvet Runs",
        "Slower tracking behavior that leaves runs and vibrato room to move.",
        "R&B runs and melisma where the pitch should guide, not flatten.",
        "For more tuning, lower pitch.retune to 100 ms; for a drier take, lower rev.level 3 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 45.0, "pitch.retune": 180.0,
            "pitch.humanize": 75.0, "pitch.scale": 2, "pitch.tonic": 2,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 6.0, "exp.hold": 60.0,
            "exp.release": 180.0,
            "ceq.hpf": 90.0, "ceq.cut1Freq": 300.0, "ceq.cut1Gain": -1.5,
            "comp.threshold": -22.0, "comp.ratio": 2.0, "comp.attack": 18.0,
            "comp.release": 160.0, "comp.knee": 12.0, "comp.makeup": 1.5,
            "ds.bypass": 0, "ds.freq": 7500.0, "ds.threshold": -36.0, "ds.range": 8.0,
            "ds.release": 60.0,
            "teq.lowGain": 0.0, "teq.presFreq": 3000.0, "teq.presGain": 0.5,
            "teq.highGain": 1.0,
            "sat.bypass": 0, "sat.mode": 0, "sat.drive": 2.5, "sat.blend": 25.0,
            "sat.output": -0.5,
            "dly.bypass": 0, "dly.sync": 1, "dly.division": 3, "dly.feel": 0,
            "dly.feedback": 15.0, "dly.level": -25.0, "dly.lowCut": 300.0,
            "dly.highCut": 6000.0, "dly.duck": 50.0,
            "rev.bypass": 0, "rev.type": 1, "rev.predelay": 25.0, "rev.decay": 1.5,
            "rev.damping": 40.0, "rev.lowCut": 200.0, "rev.highCut": 7000.0,
            "rev.level": -21.0, "rev.duck": 45.0,
            "lim.ceiling": -0.4,
        },
    ),
    preset(
        BANK_RNB, "Whisper Close",
        "Intimate quiet lead with restrained cleanup, soft leveling and barely audible room.",
        "A whispered or very quiet lead that needs to stay close and intimate.",
        "For a louder take, raise comp.makeup 3 dB and lower comp.threshold 2 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 40.0, "pitch.retune": 120.0,
            "pitch.humanize": 60.0,
            "exp.bypass": 0, "exp.threshold": -60.0, "exp.range": 8.0, "exp.hold": 80.0,
            "exp.release": 220.0,
            "dn.bypass": 0, "dn.amount": 25.0, "dn.maxReduction": 8.0,
            "ceq.hpf": 90.0, "ceq.cut1Freq": 300.0, "ceq.cut1Gain": -1.0,
            "comp.threshold": -30.0, "comp.ratio": 2.0, "comp.attack": 20.0,
            "comp.release": 180.0, "comp.knee": 12.0, "comp.makeup": 4.0,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -36.0, "ds.range": 6.0,
            "ds.release": 60.0,
            "teq.lowFreq": 100.0, "teq.lowGain": 0.0, "teq.presGain": 0.5,
            "teq.highGain": 1.0,
            "sat.bypass": 0, "sat.mode": 0, "sat.drive": 2.0, "sat.blend": 20.0,
            "sat.output": -0.5,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 20.0, "rev.decay": 0.6,
            "rev.damping": 40.0, "rev.lowCut": 200.0, "rev.highCut": 7000.0,
            "rev.level": -30.0, "rev.duck": 30.0,
            "lim.ceiling": -0.5,
        },
    ),
    preset(
        BANK_RNB, "Air Without Bite",
        "Open top with stricter split-band sibilance control.",
        "A breathy R&B lead where air is wanted but sibilance is not.",
        "For a darker take, lower teq.highGain 2 dB; for a brighter take, raise ds.threshold 3 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 50.0, "pitch.retune": 100.0,
            "pitch.humanize": 50.0,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 8.0, "exp.hold": 60.0,
            "exp.release": 160.0,
            "ceq.hpf": 95.0, "ceq.cut1Freq": 350.0, "ceq.cut1Gain": -1.0,
            "comp.threshold": -24.0, "comp.ratio": 2.5, "comp.attack": 12.0,
            "comp.release": 120.0, "comp.knee": 10.0, "comp.makeup": 2.0,
            "ds.bypass": 0, "ds.freq": 8000.0, "ds.threshold": -40.0, "ds.range": 14.0,
            "ds.release": 50.0,
            "teq.lowGain": 0.0, "teq.presFreq": 3000.0, "teq.presGain": 0.5,
            "teq.highFreq": 12000.0, "teq.highGain": 3.0,
            "dly.bypass": 0, "dly.sync": 1, "dly.division": 2, "dly.feel": 0,
            "dly.feedback": 14.0, "dly.level": -26.0, "dly.lowCut": 500.0,
            "dly.highCut": 10000.0, "dly.duck": 40.0,
            "rev.bypass": 0, "rev.type": 1, "rev.predelay": 20.0, "rev.decay": 1.2,
            "rev.damping": 20.0, "rev.lowCut": 200.0, "rev.highCut": 12000.0,
            "rev.level": -22.0, "rev.duck": 40.0,
            "lim.ceiling": -0.4,
        },
    ),
    preset(
        BANK_RNB, "Blue Hour",
        "Warm saturation, a ducked dotted echo and a long dark return.",
        "A late-night R&B lead with saturated warmth and a long, dark tail.",
        "For a quieter take, lower sat.drive 3 dB; for a brighter take, raise rev.highCut to 7 kHz.",
        **{
            "pitch.bypass": 0, "pitch.amount": 60.0, "pitch.retune": 100.0,
            "pitch.humanize": 50.0, "pitch.scale": 2, "pitch.tonic": 9,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 8.0, "exp.hold": 60.0,
            "exp.release": 180.0,
            "ceq.hpf": 90.0, "ceq.cut1Freq": 300.0, "ceq.cut1Gain": -2.0,
            "comp.threshold": -26.0, "comp.ratio": 2.5, "comp.attack": 14.0,
            "comp.release": 140.0, "comp.knee": 10.0, "comp.makeup": 2.0,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -34.0, "ds.range": 8.0,
            "ds.release": 70.0,
            "teq.lowFreq": 150.0, "teq.lowGain": 1.0, "teq.presFreq": 2500.0,
            "teq.presGain": 0.5, "teq.highGain": -1.5,
            "sat.bypass": 0, "sat.mode": 0, "sat.drive": 5.0, "sat.blend": 55.0,
            "sat.output": -1.0,
            "dly.bypass": 0, "dly.sync": 1, "dly.division": 2, "dly.feel": 1,
            "dly.feedback": 30.0, "dly.level": -18.0, "dly.lowCut": 300.0,
            "dly.highCut": 5000.0, "dly.duck": 55.0,
            "rev.bypass": 0, "rev.type": 1, "rev.predelay": 40.0, "rev.decay": 2.6,
            "rev.damping": 65.0, "rev.lowCut": 180.0, "rev.highCut": 5000.0,
            "rev.level": -18.0, "rev.duck": 40.0,
            "lim.ceiling": -0.4,
        },
    ),

    # ------------------------------------------------------------------ acoustic
    preset(
        BANK_ACOUSTIC, "Intimate Dry",
        "Little or no correction, soft compression and almost dry placement.",
        "A natural, intimate performance that should stay nearly dry.",
        "For a brighter take, raise teq.presGain 1 dB; for a louder take, raise comp.makeup 2 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 25.0, "pitch.retune": 160.0,
            "pitch.humanize": 70.0,
            "exp.bypass": 0, "exp.threshold": -60.0, "exp.range": 6.0, "exp.hold": 60.0,
            "exp.release": 180.0,
            "ceq.hpf": 80.0, "ceq.cut1Freq": 250.0, "ceq.cut1Gain": -1.0,
            "comp.threshold": -22.0, "comp.ratio": 2.0, "comp.attack": 15.0,
            "comp.release": 140.0, "comp.knee": 12.0, "comp.makeup": 1.5,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -36.0, "ds.range": 6.0,
            "ds.release": 60.0,
            "teq.lowFreq": 100.0, "teq.lowGain": 0.5, "teq.presGain": 0.0,
            "teq.highGain": 0.0,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 10.0, "rev.decay": 0.5,
            "rev.damping": 40.0, "rev.lowCut": 200.0, "rev.highCut": 7000.0,
            "rev.level": -32.0, "rev.duck": 30.0,
            "lim.ceiling": -0.5,
        },
    ),
    preset(
        BANK_ACOUSTIC, "Breath and Space",
        "Natural transients with a gentle room and conservative breath treatment.",
        "A singer-songwriter take where breaths are part of the performance.",
        "For a drier take, lower rev.level 4 dB; for a darker take, lower teq.highGain 1.5 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 30.0, "pitch.retune": 140.0,
            "pitch.humanize": 70.0,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 10.0, "exp.hold": 100.0,
            "exp.release": 220.0,
            "ceq.hpf": 90.0, "ceq.cut1Freq": 300.0, "ceq.cut1Gain": -1.0,
            "comp.threshold": -24.0, "comp.ratio": 2.0, "comp.attack": 15.0,
            "comp.release": 140.0, "comp.knee": 10.0, "comp.makeup": 1.5,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -36.0, "ds.range": 8.0,
            "ds.release": 60.0,
            "teq.lowGain": 0.0, "teq.presGain": 0.5, "teq.highGain": 0.5,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 25.0, "rev.decay": 1.1,
            "rev.damping": 35.0, "rev.lowCut": 200.0, "rev.highCut": 8000.0,
            "rev.level": -24.0, "rev.duck": 40.0,
            "lim.ceiling": -0.5,
        },
    ),
    preset(
        BANK_ACOUSTIC, "Acoustic Lead",
        "Light leveling, low-mid cleanup and a short believable room.",
        "An acoustic or folk lead that should sound like one believable space.",
        "For a brighter take, raise teq.highGain 1 dB; for a quieter take, lower comp.threshold 3 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 35.0, "pitch.retune": 120.0,
            "pitch.humanize": 60.0,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 8.0, "exp.hold": 60.0,
            "exp.release": 180.0,
            "ceq.hpf": 85.0, "ceq.cut1Freq": 220.0, "ceq.cut1Gain": -2.0,
            "ceq.cut1Q": 1.1, "ceq.cut2Freq": 500.0, "ceq.cut2Gain": -1.0,
            "comp.threshold": -24.0, "comp.ratio": 2.5, "comp.attack": 12.0,
            "comp.release": 120.0, "comp.knee": 10.0, "comp.makeup": 2.0,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -36.0, "ds.range": 6.0,
            "ds.release": 60.0,
            "teq.lowGain": 0.5, "teq.presGain": 0.5, "teq.highGain": 0.5,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 18.0, "rev.decay": 0.7,
            "rev.damping": 45.0, "rev.lowCut": 200.0, "rev.highCut": 7000.0,
            "rev.level": -22.0, "rev.duck": 40.0,
            "lim.ceiling": -0.5,
        },
    ),
    preset(
        BANK_ACOUSTIC, "Storyteller",
        "Speech-like clarity with filtered compression and minimal wash.",
        "Spoken word, narration or an intimate story vocal.",
        "For a warmer take, raise teq.lowGain 1 dB; for a brighter take, raise teq.presGain 1.5 dB.",
        **{
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 8.0, "exp.hold": 80.0,
            "exp.release": 200.0,
            "ceq.hpf": 95.0, "ceq.cut1Freq": 300.0, "ceq.cut1Gain": -1.0,
            "ceq.cut2Freq": 3000.0, "ceq.cut2Gain": -1.0,
            "comp.threshold": -26.0, "comp.ratio": 3.0, "comp.attack": 10.0,
            "comp.release": 120.0, "comp.knee": 8.0, "comp.makeup": 2.0,
            "comp.detHPF": 180.0,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -34.0, "ds.range": 8.0,
            "ds.release": 60.0,
            "teq.lowGain": -0.5, "teq.presFreq": 3500.0, "teq.presGain": 1.0,
            "teq.highGain": 0.0,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 15.0, "rev.decay": 0.6,
            "rev.damping": 50.0, "rev.lowCut": 200.0, "rev.highCut": 6000.0,
            "rev.level": -28.0, "rev.duck": 40.0,
            "lim.ceiling": -0.5,
        },
    ),
    preset(
        BANK_ACOUSTIC, "Live Room Control",
        "Gentle expansion, reduced low-mid buildup and dry placement for live rooms.",
        "A live-room or one-take performance where the room needs controlling, not hiding.",
        "For a quieter take, raise exp.threshold 5 dB; for a brighter take, raise teq.highGain 1 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 20.0, "pitch.retune": 150.0,
            "pitch.humanize": 70.0,
            "exp.bypass": 0, "exp.threshold": -45.0, "exp.range": 16.0, "exp.attack": 2.0,
            "exp.hold": 100.0, "exp.release": 300.0,
            "ceq.hpf": 90.0, "ceq.cut1Freq": 250.0, "ceq.cut1Gain": -2.0,
            "ceq.cut2Freq": 600.0, "ceq.cut2Gain": -1.0,
            "comp.threshold": -24.0, "comp.ratio": 2.5, "comp.attack": 15.0,
            "comp.release": 140.0, "comp.knee": 10.0, "comp.makeup": 1.5,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -36.0, "ds.range": 6.0,
            "ds.release": 60.0,
            "teq.lowFreq": 150.0, "teq.lowGain": -1.0, "teq.presGain": 0.0,
            "teq.highGain": -0.5,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 10.0, "rev.decay": 0.5,
            "rev.damping": 50.0, "rev.lowCut": 200.0, "rev.highCut": 6000.0,
            "rev.level": -30.0, "rev.duck": 30.0,
            "lim.ceiling": -0.5,
        },
    ),
    preset(
        BANK_ACOUSTIC, "Harmony Stack",
        "Reduced low mids and sibilance for stacked voices, with light or bypassed tuning.",
        "Layering several harmony parts without low-mid and sibilance buildup.",
        "For a brighter stack, raise teq.presGain 1 dB; for a drier stack, lower rev.level 3 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 30.0, "pitch.retune": 130.0,
            "pitch.humanize": 65.0,
            "exp.bypass": 0, "exp.threshold": -50.0, "exp.range": 10.0, "exp.hold": 60.0,
            "exp.release": 180.0,
            "ceq.hpf": 110.0, "ceq.cut1Freq": 300.0, "ceq.cut1Gain": -2.0,
            "ceq.cut2Freq": 3000.0, "ceq.cut2Gain": -1.5,
            "comp.threshold": -24.0, "comp.ratio": 2.5, "comp.attack": 12.0,
            "comp.release": 120.0, "comp.knee": 10.0, "comp.makeup": 2.0,
            "ds.bypass": 0, "ds.freq": 7500.0, "ds.threshold": -38.0, "ds.range": 10.0,
            "ds.release": 60.0,
            "teq.lowFreq": 150.0, "teq.lowGain": -2.0, "teq.presGain": 0.5,
            "teq.highGain": -0.5,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 15.0, "rev.decay": 0.8,
            "rev.damping": 45.0, "rev.lowCut": 250.0, "rev.highCut": 7000.0,
            "rev.level": -26.0, "rev.duck": 40.0,
            "lim.ceiling": -0.5,
        },
    ),

    # ------------------------------------------------------------------ fx
    preset(
        BANK_FX, "Hard Tune",
        "Near-instant snap and strong pitch lock with a dry, centered lead.",
        "A deliberate hard-tune effect for a hook or a featured verse.",
        "For a less severe take, raise pitch.retune to 15 ms and lower pitch.amount to 85%.",
        **{
            "pitch.bypass": 0, "pitch.amount": 100.0, "pitch.retune": 1.0,
            "pitch.humanize": 0.0, "pitch.scale": 1, "pitch.tonic": 0, "pitch.range": 1,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 8.0, "exp.hold": 40.0,
            "exp.release": 100.0,
            "ceq.hpf": 100.0, "ceq.cut1Freq": 300.0, "ceq.cut1Gain": -1.5,
            "comp.threshold": -26.0, "comp.ratio": 4.0, "comp.attack": 3.0,
            "comp.release": 60.0, "comp.knee": 4.0, "comp.makeup": 2.0,
            "comp.detHPF": 160.0,
            "ds.bypass": 0, "ds.freq": 7500.0, "ds.threshold": -34.0, "ds.range": 8.0,
            "ds.release": 50.0,
            "teq.lowGain": 0.0, "teq.presFreq": 3500.0, "teq.presGain": 1.0,
            "teq.highGain": 0.5,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 8.0, "rev.decay": 0.35,
            "rev.damping": 50.0, "rev.lowCut": 250.0, "rev.highCut": 6000.0,
            "rev.level": -32.0, "rev.duck": 40.0,
            "lim.ceiling": -0.4,
        },
    ),
    preset(
        BANK_FX, "Telephone Verse",
        "Focused band-limited EQ, firm compression and a dry close signal.",
        "A lo-fi telephone throw for a verse, skit or transition.",
        "For a brighter take, raise teq.presFreq to 2500 Hz; for a darker take, lower ceq.cut1Gain to -3 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 50.0, "pitch.retune": 80.0,
            "pitch.humanize": 40.0,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 6.0, "exp.hold": 60.0,
            "exp.release": 160.0,
            "ceq.hpf": 300.0, "ceq.cut1Freq": 600.0, "ceq.cut1Gain": -6.0,
            "ceq.cut1Q": 1.5, "ceq.cut2Freq": 3000.0, "ceq.cut2Gain": -4.0,
            "ceq.cut2Q": 1.2,
            "comp.threshold": -30.0, "comp.ratio": 6.0, "comp.attack": 4.0,
            "comp.release": 60.0, "comp.knee": 4.0, "comp.makeup": 6.0,
            "comp.detHPF": 300.0,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -32.0, "ds.range": 6.0,
            "ds.release": 60.0,
            "teq.lowFreq": 300.0, "teq.lowGain": -6.0, "teq.presFreq": 2000.0,
            "teq.presGain": 3.0, "teq.highFreq": 5000.0, "teq.highGain": -6.0,
            "sat.bypass": 0, "sat.mode": 1, "sat.drive": 8.0, "sat.blend": 70.0,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 5.0, "rev.decay": 0.3,
            "rev.damping": 60.0, "rev.lowCut": 500.0, "rev.highCut": 4000.0,
            "rev.level": -36.0, "rev.duck": 30.0,
            "lim.ceiling": -0.5,
        },
    ),
    preset(
        BANK_FX, "Distorted Hook",
        "Clear saturation and midrange edge with output kept under control.",
        "A saturated, aggressive hook that still needs to sit in a mix.",
        "For a cleaner take, lower sat.drive to 8 dB and sat.blend to 45%; for more edge, raise drive to 18 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 80.0, "pitch.retune": 20.0,
            "pitch.humanize": 15.0,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 8.0, "exp.hold": 40.0,
            "exp.release": 120.0,
            "ceq.hpf": 110.0, "ceq.cut1Freq": 400.0, "ceq.cut1Gain": -2.0,
            "comp.threshold": -28.0, "comp.ratio": 4.0, "comp.attack": 6.0,
            "comp.release": 80.0, "comp.knee": 6.0, "comp.makeup": 3.0,
            "comp.detHPF": 160.0,
            "ds.bypass": 0, "ds.freq": 7500.0, "ds.threshold": -34.0, "ds.range": 10.0,
            "ds.release": 50.0,
            "teq.lowFreq": 150.0, "teq.lowGain": 1.0, "teq.presFreq": 3500.0,
            "teq.presGain": 2.0, "teq.highGain": -1.0,
            "sat.bypass": 0, "sat.mode": 1, "sat.drive": 14.0, "sat.blend": 75.0,
            "sat.output": -4.0,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 10.0, "rev.decay": 0.5,
            "rev.damping": 40.0, "rev.lowCut": 250.0, "rev.highCut": 6000.0,
            "rev.level": -30.0, "rev.duck": 40.0,
            "lim.ceiling": -0.6,
        },
    ),
    preset(
        BANK_FX, "Slap Room",
        "Audible single-repeat slap and short room reflections.",
        "A vintage slapback throw for a verse or a rockabilly-style lead.",
        "For a drier take, lower delay level 4 dB; for a brighter take, raise teq.highGain 1.5 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 40.0, "pitch.retune": 100.0,
            "pitch.humanize": 50.0,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 8.0, "exp.hold": 60.0,
            "exp.release": 160.0,
            "ceq.hpf": 95.0, "ceq.cut1Freq": 300.0, "ceq.cut1Gain": -1.5,
            "comp.threshold": -24.0, "comp.ratio": 3.0, "comp.attack": 8.0,
            "comp.release": 100.0, "comp.knee": 8.0, "comp.makeup": 2.0,
            "comp.detHPF": 150.0,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -34.0, "ds.range": 8.0,
            "ds.release": 60.0,
            "teq.lowGain": 0.0, "teq.presGain": 0.5, "teq.highGain": 0.0,
            "dly.bypass": 0, "dly.sync": 0, "dly.ms": 95.0, "dly.feedback": 0.0,
            "dly.level": -12.0, "dly.lowCut": 300.0, "dly.highCut": 6000.0,
            "dly.pingpong": 1, "dly.duck": 20.0,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 12.0, "rev.decay": 0.6,
            "rev.damping": 40.0, "rev.lowCut": 200.0, "rev.highCut": 7000.0,
            "rev.level": -22.0, "rev.duck": 25.0,
            "lim.ceiling": -0.4,
        },
    ),
    preset(
        BANK_FX, "Dotted Throw",
        "Upfront vocal with a clearly ducked dotted-eighth repeat.",
        "A modern dotted-eighth throw under a lead line.",
        "For a shorter throw, change division to 1/16; for a quieter take, lower delay level 3 dB.",
        **{
            "pitch.bypass": 0, "pitch.amount": 55.0, "pitch.retune": 90.0,
            "pitch.humanize": 45.0,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 8.0, "exp.hold": 60.0,
            "exp.release": 160.0,
            "ceq.hpf": 100.0, "ceq.cut1Freq": 300.0, "ceq.cut1Gain": -1.5,
            "comp.threshold": -24.0, "comp.ratio": 3.0, "comp.attack": 8.0,
            "comp.release": 100.0, "comp.knee": 8.0, "comp.makeup": 2.0,
            "comp.detHPF": 150.0,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -34.0, "ds.range": 8.0,
            "ds.release": 60.0,
            "teq.lowGain": 0.0, "teq.presGain": 0.5, "teq.highGain": 0.0,
            "dly.bypass": 0, "dly.sync": 1, "dly.division": 2, "dly.feel": 1,
            "dly.feedback": 32.0, "dly.level": -13.0, "dly.lowCut": 400.0,
            "dly.highCut": 6500.0, "dly.duck": 55.0,
            "rev.bypass": 0, "rev.type": 0, "rev.predelay": 20.0, "rev.decay": 0.9,
            "rev.damping": 45.0, "rev.lowCut": 200.0, "rev.highCut": 7000.0,
            "rev.level": -26.0, "rev.duck": 45.0,
            "lim.ceiling": -0.4,
        },
    ),
    preset(
        BANK_FX, "End of the Night",
        "Long, dark plate with a filtered slow echo and controlled tails.",
        "A closing ballad or outro where the last note should hang in the room.",
        "For a shorter tail, lower rev.decay to 2.5 s; for a brighter take, raise rev.highCut to 6 kHz.",
        **{
            "pitch.bypass": 0, "pitch.amount": 50.0, "pitch.retune": 120.0,
            "pitch.humanize": 60.0, "pitch.scale": 2, "pitch.tonic": 9,
            "exp.bypass": 0, "exp.threshold": -55.0, "exp.range": 8.0, "exp.hold": 60.0,
            "exp.release": 180.0,
            "ceq.hpf": 90.0, "ceq.cut1Freq": 300.0, "ceq.cut1Gain": -2.0,
            "comp.threshold": -26.0, "comp.ratio": 2.5, "comp.attack": 15.0,
            "comp.release": 140.0, "comp.knee": 10.0, "comp.makeup": 2.0,
            "ds.bypass": 0, "ds.freq": 7000.0, "ds.threshold": -34.0, "ds.range": 8.0,
            "ds.release": 70.0,
            "teq.lowFreq": 150.0, "teq.lowGain": 1.0, "teq.presFreq": 2500.0,
            "teq.presGain": 0.0, "teq.highFreq": 7000.0, "teq.highGain": -2.0,
            "sat.bypass": 0, "sat.mode": 0, "sat.drive": 5.0, "sat.blend": 50.0,
            "sat.output": -1.0,
            "dly.bypass": 0, "dly.sync": 1, "dly.division": 3, "dly.feel": 1,
            "dly.feedback": 45.0, "dly.level": -17.0, "dly.lowCut": 250.0,
            "dly.highCut": 4500.0, "dly.duck": 60.0,
            "rev.bypass": 0, "rev.type": 1, "rev.predelay": 50.0, "rev.decay": 4.5,
            "rev.damping": 75.0, "rev.lowCut": 180.0, "rev.highCut": 4500.0,
            "rev.level": -15.0, "rev.duck": 40.0,
            "lim.ceiling": -0.4,
        },
    ),
]


def validate():
    errors = []
    if len(PRESETS) != 36:
        errors.append(f"expected 36 presets, got {len(PRESETS)}")

    banks = {}
    for p in PRESETS:
        banks.setdefault(p["bank"], 0)
        banks[p["bank"]] += 1

    expected_banks = {BANK_TRACKING, BANK_RAP, BANK_POP, BANK_RNB, BANK_ACOUSTIC, BANK_FX}
    if set(banks) != expected_banks:
        errors.append(f"bank mismatch: {banks}")

    for bank, count in banks.items():
        if count != 6:
            errors.append(f"{bank} has {count} presets")

    names = [p["name"] for p in PRESETS]
    if len(set(names)) != len(names):
        errors.append("duplicate preset names")

    serialised = set()
    for p in PRESETS:
        params = p["parameters"]
        if set(params) != set(DEFAULT):
            missing = set(DEFAULT) - set(params)
            extra = set(params) - set(DEFAULT)
            errors.append(f"{p['name']}: missing {missing}, extra {extra}")
        for pid, value in params.items():
            low, high, typ, nchoices = RANGES[pid]
            if not isinstance(value, (int, float)):
                errors.append(f"{p['name']}: {pid} is not numeric")
                continue
            if value < low - 1e-9 or value > high + 1e-9:
                errors.append(f"{p['name']}: {pid}={value} outside [{low},{high}]")
            if typ in ("boolean", "intValue") and abs(value - round(value)) > 1e-9:
                errors.append(f"{p['name']}: {pid} must be integral")
            if typ == "choice" and (abs(value - round(value)) > 1e-9 or not 0 <= value < nchoices):
                errors.append(f"{p['name']}: {pid} choice out of range")
        key = tuple(sorted((k, round(float(v), 6)) for k, v in params.items()))
        if key in serialised:
            errors.append(f"{p['name']}: duplicate full parameter state")
        serialised.add(key)

    for p in PRESETS:
        if not p["description"] or not p["useCase"] or not p["firstAdjustment"]:
            errors.append(f"{p['name']}: missing metadata")
        if len(p["description"]) > 180:
            errors.append(f"{p['name']}: description too long")

    if errors:
        raise SystemExit("Preset validation failed:\n" + "\n".join(errors))


def main():
    validate()
    payload = []
    for index, p in enumerate(PRESETS):
        bank_slug = {
            BANK_TRACKING: "tracking",
            BANK_RAP: "rap",
            BANK_POP: "pop",
            BANK_RNB: "rnb",
            BANK_ACOUSTIC: "acoustic",
            BANK_FX: "fx",
        }[p["bank"]]
        name_slug = "".join(c.lower() if c.isalnum() else "-" for c in p["name"])
        name_slug = "-".join(filter(None, name_slug.split("-")))
        payload.append({
            "format": "ademusic-vocal-rack-preset",
            "version": 1,
            "id": f"factory.{bank_slug}.{name_slug}",
            "name": p["name"],
            "bank": p["bank"],
            "description": p["description"],
            "useCase": p["useCase"],
            "firstAdjustment": p["firstAdjustment"],
            "parameters": p["parameters"],
        })

    json_text = json.dumps(payload, indent=2)
    (ROOT / "presets" / "ademusic-vocal-rack-factory.json").write_text(json_text + "\n")

    header = (
        "// Generated by Tools/generate_presets.py. Do not hand-edit.\n"
        "#pragma once\n\n"
        "namespace vr {\n\n"
        "inline constexpr const char* kFactoryPresetJson = R\"VRJSON("
        + json_text +
        ")VRJSON\";\n\n"
        "} // namespace vr\n"
    )
    (ROOT / "Source" / "presets" / "FactoryPresetsData.h").write_text(header)

    print(f"Wrote {len(payload)} presets across {len(set(p['bank'] for p in payload))} banks")
    for bank in [BANK_TRACKING, BANK_RAP, BANK_POP, BANK_RNB, BANK_ACOUSTIC, BANK_FX]:
        print(f"  {bank}: {sum(1 for p in payload if p['bank'] == bank)}")


if __name__ == "__main__":
    main()
