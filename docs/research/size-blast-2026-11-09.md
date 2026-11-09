# The SIZE blast (issue 2), 9 Nov 2026

A player (chriisu, [issue 2](https://github.com/jeffebauer/sublustris-versio/issues/2)) on v1.0.68, on the module: a fast SIZE move from big to small "blasts the speaker with very loud audio", HALL or CANYON, probably BLOOM, violin, gain turned up.

Desktop only so far: about 45,000 renders through the release knob path (1 ms blocks, 10 ms smoothing, end stops, 0.003 send threshold). The harness is local, in `dist/size-blast-harness/sb.cpp`. Nothing ran on the module.

## What was measured (50 ms level, peak after the move against the steady level before it)

| Scenario | v1.0.68 | Beta `e4a9738` |
|---|---|---|
| Closest to the report: CANYON, BLOOM, DECAY 0.85, TONE and WEAR 0.5, violin-like tone peaking at 1.0, SIZE 1.0 to 0.05 in 40 ms | +9.5 dB (-11.5 to -2.0 dBFS; limiter down 6 dB; about 1.5 s) | +4.2 dB |
| The same, SIZE held still | -1.7 dB | -0.5 dB |
| The same move over 16 s or more | +0.1 dB | +0.2 dB |
| The same in 12 rested steps | +1.3 dB | +0.9 dB |
| Worst anywhere: CANYON, NATURAL, DECAY 0.10, TONE 0.8, WEAR 0, SIZE 1.0 to 0.2, 20-300 ms, 70 Hz at 0.3 | +19.6 dB over the louder steady state | +14.6 dB |
| PLATE (the control), wide grid | at most +4.7 dB | at most +2.5 dB |

- The limiter held the output at -1 dBFS: the "blast" is the wet jumping up to that ceiling from a quiet level.
- Move speed barely matters from 15 ms to 6 s, because SIZE jumps once the knob rests 50 ms.
- Not reproduced: "only from midway" at mid or high DECAY. Starts of 0.5-0.85 at DECAY 0.5-0.85 with BLOOM and a violin-like tone rose about 2 dB at most. The report's real DECAY and input level are unknown.
- The beta's DECAY knob curve changed (ADR 0048), so the same knob number isn't the same tail between the two columns.

## A second problem: the wet can die

197 of 12,960 moves on v1.0.68 and 172 on the beta turned the output to NaN, and it stayed NaN for the whole 25 s tested; none of 10,080 held controls did. Nearly all were CANYON, input level 1.0, WEAR 1.0, with bass. `core/` and `firmware/` have no NaN guard (checked at v1.0.68). On the module this should be silence until a restart (inferred).

## The cause (checked in the code at v1.0.68 and on the beta)

`core/dsp/Fdn.cpp`, the SIZE crossfade's start (line 459 at v1.0.68, 460 on the beta): `xfLnGain_[i] = 6.9077553f * xfDelta_[i] / sampleRate_ * rate;`, applied as `std::exp(lnGain)` with no limit. After a shrink the new read is nearer, so it's boosted to make up for decay it supposedly skipped ("nearer (quieter) when shorter", the code's comment). That holds for a dying tail. With sustained playing the nearer sound isn't quieter, so the boost is real gain inside the loop. `rate` includes the governor's extra loss, which rises with hot input.

Sizes measured: x3.2 at DECAY 0.6 with a moderate tone, x344 at DECAY 0.8 hot, about 1e34 at DECAY 0 (the NaNs). In a scratch copy with the correction at zero: CANYON DECAY 0.1 violin went from +7.3 dB to 0.0 dB, the limiter stayed idle, and NaN rows went from 197 to 0.

Confidence: high for the desktop surge, medium that it's what the player heard. Running out of CPU is unlikely: module laps show no late blocks while moving SIZE and playing, and lateness tears audio rather than raising its level.

## Fix ideas (none built)

1. Cap the boost at a few dB, and add a safety that restarts a tank that goes invalid. Held settings stay bit-identical; only extreme shrinks change.
2. Base the boost on DECAY's own loss, leaving out the governor. Fixes the hot high-DECAY case, not low DECAY, so it would pair with 1.
3. Break very large jumps into a few steps (the rested stairs measured +1.3 dB). Changes the feel: a big SIZE move takes longer to land.

The correction exists because of the owner's note, "some jumps in amplitude with the crossfades": a fix has to keep normal SIZE moves as they are.

## The module test (low volume, rack power only)

BLEND max, CANYON, BLOOM, DECAY about 3 o'clock, TONE and WEAR noon, SIZE fully clockwise. Feed a steady hot tone, wait 10 s, flick SIZE fully counter-clockwise, listen for 2 s; three times. Repeat from SIZE noon, then in HALL, then with DECAY at 9 o'clock. A smooth swell lasting about 1.5 s matches this cause. A broken buzz points to timing. Note whether the reverb goes silent until a restart.

## Not checked

ARM float maths and the firmware's fixed voicings and memory layout; real knob noise, hand shapes and CV; a real violin (synthetic, mono into both inputs); anything on the module.
