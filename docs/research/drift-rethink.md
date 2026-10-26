# DRIFT rethink: stronger ends, a sine right side (26 Oct 2026)

Owner: "In a realistic setting amongst a broader mix, the extreme ends of the modulation knob don't create obvious results ... we should make them stronger to at least be more noticeable in a mix." And of the right side's per-line movement, which at times reads as panning: "I wonder if we should consider a more straight sinewave-based modulation for the right side instead to juxtapose the more random modulation on the left side of the knob, like we did with Resilio Versio."

History: `drift-cathedra-side.md` (the slow, deep, in-step sine was "too seasick"; Wander won).

Tags: **[S]** a source says it; **[M]** we measured it; **[I]** our inference.

## Sources

- **Dattorro (1997), [Effect Design Part 1](https://ccrma.stanford.edu/~dattorro/EffectDesignPart1.pdf).**
  - [S] The plate's two modulated allpasses get "the same rate and depth", but on a quadrature oscillator "to decrease the correlation".
  - [S] Ideally every delay line moves, each at its own rate and depth, so that each needs less depth.
  - [S] About 1 Hz and 8 samples peak (16 at most) at 29.8 kHz: [I] ±3 cents typical, ±6 at most.
  - [S] On piano a "perceived vibrato" can be objectionable.
- **Mutable Instruments Clouds and Rings** ([clouds reverb.h](https://github.com/pichenettes/eurorack/blob/master/clouds/dsp/fx/reverb.h), [rings reverb.h](https://github.com/pichenettes/eurorack/blob/master/rings/dsp/fx/reverb.h)).
  - [S] Clouds: two cosine LFOs (0.5 and 0.3 Hz), on the in-loop allpass and a long delay, "a slow shimmer/chorus".
  - [S] Rings: both long delays move, at different rates.
  - [I] About ±5 cents.
- **Valhalla.**
  - [S] ValhallaRoom: up to a few dozen randomised LFOs; about 0.5 Hz smooths artefacts, above 1 Hz adds chorusing ([ValhallaRoom](https://valhalladsp.com/?p=781)).
  - [S] "Crank both mod depths for that seasick decay." Less obvious detuning comes from lower depth and a higher rate ([tips](https://valhalladsp.com/2011/05/27/valhallaroom-tips-and-tricks-unnatural-hall-reverbs/)).
  - [S] ÜberMod gives each tap its own LFO, or its own phase of a multiphase LFO ([modes](https://valhalladsp.com/2012/01/06/valhallaubermod-the-modes/)).
- **Ensemble chorus.** [S] The string ensemble's three delays run at the same rate with phases 120° apart ([Solina](https://forums.cherryaudio.com/viewtopic.php?p=8675)).
- **Perception.**
  - [S] Pitch wobble is easier to detect at 2 Hz than at 20 Hz, for tones up to 4 kHz ([PMC10349105](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC10349105/)).
  - [S] At slow rates and large extents, listeners hear the cycle's high and low as separate pitches (d'Alessandro & Castellengo 1994, [HAL](https://hal.archives-ouvertes.fr/hal-02008510)).
- **Not found:** Lexicon's 224/480L chorus figures; Schlecht & Habets 2015 beyond its citation.

## Conclusions

- [I] **Same rate, spread phases.** For a regular side, give every line the same rate (±5–10 %) and spread their phases round the cycle. The bends then cancel in the sum: the tail shimmers periodically while the chord's centre pitch stays put. Spread rates instead drift towards random wow, which blurs the contrast with the left side.
- [I] **The plate:** Dattorro's sine and cosine on its two branches.
- [S/I] **Rate before depth.** To be heard in a mix, raise the rate before the depth. 1.5–5 Hz is near the ear's most sensitive band, and a fast cycle completes within a note. A slow, deep cycle is what reads as seasick. Avoid starting a sine below about 1 Hz.

## Prototypes (renderer hooks; `map::driftAmountsPrototype`)

| | Hooks | What |
|---|---|---|
| B | `IV_DRIFT_GROW=0.4 IV_DRIFT_RATE_GROW=0.2` | Both ends: depth × (1 + 0.4 x²), wow/Wander rate × (1 + 0.2 x²); x = 0 over the first half of a side, 1 at its end |
| C | `IV_DRIFT_GROW=0.8 IV_DRIFT_RATE_GROW=0.4` | The same, × 1.8 depth, × 1.4 rate at the end |
| D | `IV_DRIFT_SINE=1 IV_DRIFT_SINE_CENTS=16` | Right side: a sine, 1.5 → 5.5 Hz, up to 16 c a line, rates ±6 %, phases spread over ¾ of a cycle (the plate's branches in quadrature); no Wander |
| E | `IV_DRIFT_SINE=2 IV_DRIFT_SINE_CENTS=14` | The same, 1 → 2 Hz, up to 14 c |

[M] A held 1 kHz, p95 cents in the tail (3–8 s), SIZE 0.6 (MASSIVE 0.7), DECAY 0.7, Neve. One random take per cell, so ±2–3 c is noise:

| DRIFT | Space | A today | B | C | D sine fast | E sine slow |
|---|---|---|---|---|---|---|
| full left | HALL | 17.9 | 23.2 | 23.8 | | |
| | MASSIVE | 8.7 | 16.1 | 19.8 | | |
| | PLATE | 16.7 | 19.8 | 22.2 | | |
| ¾ right | HALL | 15.9 | = A | = A | 23.2 | 22.1 |
| | MASSIVE | 5.9 | = A | = A | 16.4 | 16.6 |
| | PLATE | 11.2 | = A | = A | 20.5 | 22.7 |
| full right | HALL | 17.0 | 27.1 | 33.4 | 38.5 | 31.6 |
| | MASSIVE | 7.9 | 9.1 | 17.6 | 23.4 | 22.0 |
| | PLATE | 14.5 | 23.5 | 32.4 | 29.6 | 30.4 |

(For comparison, the rejected slow sine measured 64 / 37 / 57 c at full: `drift-cathedra-side.md`.)

**Flash** (release, a trial build at e570741; 112,548 B today, 92 B spare):

| Change | Release | Cost |
|---|---|---|
| D or E as built: the right side drives the existing flutter sine, phase span 0.37 → 0.75, rate spread 0.12, Wander off | 112,540 B | −8 B |
| B or C, depth only (one shared x² after the branches) | 112,668 B | +120 B |
| B or C, depth and rate | 112,716 B | +168 B |
| B or C as plain constants (linear, so ¾ grows too) | 112,548 B | 0 B |

Built on the flutter oscillator, D and E differ from the prototype in one way: on the right side, the cassette's flutter would blend into the sine's rate by depth.

Page: `renders/drift_rethink` (DRIFT full left, ¾ right and full right; HALL, MASSIVE and PLATE; two in-mix materials and a solo chord).

**Checks** [M]:
- **Energy** (`tools/pages/energy_check.py`; 135 settings each against today):
  - C passes (+0.35 dB at most) and E passes (+0.32 dB).
  - D fails one cell by 0.08 dB: PLATE, SIZE 1, DECAY 0.75, DRIFT 1, at +0.58 dB in the 16–30 s window, about −128 dBFS (the floor's numerical noise). With another noise seed that cell is within ±0.2 dB, and its decay slope matches today's.
- **10-minute holds** at DECAY 1: C at DRIFT 0 and 1, D and E at DRIFT 0.75 and 1, every SPACE. All 18 pass, within 0.7 dB.
- **Clicks:** DRIFT swept 0 → 1 over 16 s, then flicked 1 → 0.5 in 0.2 s, on a held chord, in 1 ms frames above 5 kHz. Every version reads +7.9 to +9.4 dB against today's +7.9 to +8.6, which is the music's own grain; no clicks.
