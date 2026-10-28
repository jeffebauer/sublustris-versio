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

**Mix material rebuilt (27 Oct).** Owner: "the full mix example seems to have mashed multiple samples together, with different tempos and keys making for a very chaotic mix."
- [M] The first mix_keys summed `make_material.py`'s drums (122 bpm), bass (75 bpm, A minor) and keys (84 bpm; Dm9 G7 C7 Fmaj7, read as C major).
- [M] The first mix_dub was `fullmix`, one piece at 120 bpm in C minor. But its send went through a 24-bit trim that clipped 20 samples peaking at 1.26.
- **Now:** both mixes come from one stem set in `tools/pages/one_key_mix.py`: 120 bpm, C natural minor, Cm9 | Abmaj7 | Fm9 | Gm7, 8 bars, written as float.
  - Bass and Wurlitzer samples are retuned to A4 = 440 Hz from their own measured harmonics (they were up to 20 c off).
  - Checks are in the page's `_mix_check.txt`:
    - every onset found is within 1–3 ms (median) of the score, none extra, and each stem's tempo is within 0.02 % of 120 bpm;
    - the 56 measured bass and keys notes are within 1.4 c of equal temperament;
    - bass and keys estimate as C minor.

## Round 2: the left side as tape (27 Oct 2026)

Owner: "the wow and flutter side also needs to be more obvious as the deviations get a bit lost in the diffuse reverb. Given the intent of this side is to emulate tape, I'm wondering if we can add crinkle and dropouts towards the extreme end as well?" Also: round 1's single page offered different versions on different cards with no card descriptions. It is now two pages, `renders/drift_rethink/left_tape` and `right_movement`, each with one set of versions and a line per card.

### Sources

**Wow and flutter**
- [S] ChowTape (GPL-3.0: numbers only, no code) reads its delay at a length set by wow plus flutter ([WowFlutterProcessor.cpp](https://github.com/jatinchowdhury18/AnalogTapeModel/blob/master/Plugin/Source/Processors/Timing_Effects/WowFlutterProcessor.cpp)).
  - Wow runs 0 to 3.5 Hz: a sine plus an Ornstein-Uhlenbeck random part, with its rate drifting.
  - Flutter is three harmonics, from 0.1 to 100 Hz.
  - Depth is cubed on the knob and slewed over 50 ms.
- [S] Generation Loss MKII: wow is "slow, smooth, random pitch modulation"; flutter is "rapid, twitchy, random modulation of both amplitude and pitch" ([quick guide](https://www.chasebliss.com/s/Generation-Loss-MKII_Pedal_Chase-Bliss_Quick-guide.pdf), via a search snippet).
- [S] Goodhertz Wow Control: 0.1 to 20 Hz, default 0.55 Hz ([manual](https://manuals.goodhertz.com/3.7/wow-ctrl/)).
- [S] Weighting standards (IEC 386, DIN 45507) peak at 4 Hz. Wow is roughly 0.5 to 6 Hz, flutter 6 to 100 Hz, and scrape flutter lies above 100 Hz ([Wikipedia](https://en.wikipedia.org/wiki/Wow_and_flutter_measurement), [Lindos](https://lindos.co.uk/articles/wow-and-flutter-measurement)).
- [S] The best cassette decks reach about 0.08 %, which is "still audible under some conditions". [I] 1 % ≈ 17.3 cents, so today's ±15 cents per line is ±0.87 %, and the "seasick" ±35 cents is ±2 %.

**Crinkle and dropouts**
- [S] Strymon Volante files wow, splices and crinkles under "Mechanics", and the high loss of worn tape and heads under "Wear" ([SOS](https://www.soundonsound.com/reviews/strymon-volante)).
- [S] Generation Loss "Failure" covers "snags, drops and wrinkles" ([product page](https://chasebliss.com/generation-loss-mkii)).
- [S] El Capistan's Tape Crinkle covers friction, creases, splices and contaminants ([listing](https://spicersmusic.com/products/el-capistan-v2-dtape-echo)).
- [S] A dropout is the head-to-tape gap opening for a moment. The spacing loss is 54.6·d/λ dB, so the highs go first ([US4451531](https://patents.google.com/patent/US4451531)).
- [S] ChowTape's "Chew" (its dropouts) has random on and off periods, an expansion, and a low-pass sliding from 22 down to 5 kHz, with 5–10 ms ramps ([ChewProcessor.h](https://github.com/jatinchowdhury18/AnalogTapeModel/blob/master/Plugin/Source/Processors/Chew/ChewProcessor.h)).

**Shared versus per-line movement**
- [S] A forum post says modulating each line independently "is more of a diffused effect and less like a chorus" ([KVR](https://www.kvraudio.com/forum/viewtopic.php?p=1748676)).
- [I] One shared delay moves every partial by the same ratio, so it reads as a pitch bend at a fraction of the depth.

### Measured: why it got lost

At full left, today's per-line wobble measures about 20 c on a held tone, but the lines move independently. Two tones played together through HALL bend together with a correlation of only 0.0–0.38. A shared transport of ±10 c laid on top barely showed (+0.5 c of wow, correlation +0.04). The independent movement buries it.

So in C, D and E the transport replaces two thirds of the tank's own wobble (`kTransportLineShare` 0.33). The correlation then rises to 0.6–0.9 (0.3–0.5 on PLATE), and the tail bends as one.

### The prototype (`dsp/Transport.h`, `IV_TAPE_TRANSPORT` = 1, 2, 3)

**Transport.** It sits on the wet after the tank, before ERA's tape out.
- One delay per channel, both read at the same point (8.3 ms centre, cubic read).
- Wow: mostly random (two smoothed one-poles) with a drifting sine under it, 0.5 → 1.2 Hz, ±12 c at full left.
- Flutter: two sines at 6 → 10 Hz, ±3 c, which also move the level by ±0.3 dB.

**Crinkle** (mode 2 and up)
- Starts 60 % of the way along the side and rises in a straight line to 0.6 bursts a second at full.
- Both faults come jittered-regular rather than Poisson: each waits 0.5–1.5 of its mean interval, the first 0.2–0.8. With a fixed seed, a Poisson draw put the first crinkle at ¾ left after 15 s in every render, so the label check caught D identical to C on the 15 s organ card.
- Each burst lasts 50–300 ms on a raised-cosine envelope: ±8 c of jitter at about 30 Hz, the level rough by 1 to 3.5 dB, and the highs down to about 5 kHz.

**Dropouts** (mode 3)
- Start 75 % of the way along the side and ease in to 0.8 a second at full.
- Each lasts 20–150 ms (log-uniform), falls 6–18 dB with the highs down to about 3 kHz, and has raised-cosine ramps of up to 8 ms.

**Smoothing.** Everything moves once per 32-sample chunk, then a 150 Hz one-pole smooths the read point, level and tone per sample.
- [M] Without that smoothing, crinkle's rough level stepped every chunk: a faint hash about 57 dB under a held organ chord, +37 dB on the 1 ms check against today's +12. With it: +13 dB, about 86 dB under the signal.

**Flash and CPU.** A trial release with the transport always on (IV_TRANSPORT_TRIAL, mode 3):
- Flash: +2,680 B (115,228 B against bbd2635's 112,548 B). The per-sample part is out of line (IV_NOINLINE); the per-chunk part and the mapping are built for size (IV_SIZE_OPT).
- RAM: +8.4 KB (two 1024-float buffers; 512 would do).
- CPU [I], estimated from the code: about 100–130 cycles a sample for the per-sample part, plus about 25 for the per-chunk part, so about 1.5 % of the M7. Not measured on the module.

**Where it belongs.** Wow, flutter and crinkle are transport faults: MOTION, as Strymon's "Mechanics" has them. Sources put dropouts with wear (ChowTape's Chew, Strymon's "Wear"); they sit on MOTION here because the owner asked. If they move, they would sit with ERA's cassette (WEAR).

## Round 2, right side: sines with lines on their own phase or together (27 Oct 2026)

Owner: "for the sinewave examples, are they also modulating per delay-line? I'd like to hear those compared to modulating all delay lines together."

Yes. D and E are per line: each line has its own phase (spread over ¾ of a cycle) and the rate is spread ±6 %. Two new ways sit beside them:
- **F and G, all lines together** (`IV_DRIFT_SINE=3`, `4`): every line at one rate and one phase, at the same depth per line (16 c fast, 14 c slow).
- **H, on the output** (`IV_TAPE_TRANSPORT=4`, `IV_TAPE_SINE_CENTS=16`): the fast sine as one vibrato on the wet transport, outside the loop, with Wander off.

[M] Two held tones (700 Hz and 1.3 kHz), p95 cents of the 1.3 kHz, MOTION at ¾ right / fully right:

| | HALL | MASSIVE | PLATE |
|---|---|---|---|
| A today | 11.9 / 20.3 | 8.1 / 13.1 | 14.0 / 17.9 |
| D fast, each line its own phase | 23.1 / 43.6 | 18.9 / 30.1 | 23.4 / 31.0 |
| F fast, all lines together | 16.3 / 33.3 | 17.0 / 28.6 | 25.3 / 29.5 |
| E slow, each line its own phase | 25.1 / 37.8 | 18.8 / 27.3 | 24.4 / 35.2 |
| G slow, all lines together | 15.7 / 25.6 | 15.4 / 25.9 | 24.3 / 38.4 |
| H fast on the output, 12 c | 9.0 / 14.3 | 7.8 / 13.8 | 14.7 / 16.8 |

**In step comes out no deeper.** In this loop the in-step sines measure the same or shallower than the spread ones, so no depth-matched version was needed.
- [I] The tail at any moment holds sound from many passes through the loop, taken at different points of a 0.2–1 s cycle. Those average, while the spread versions' figures also carry the lines' beating.
- [I] What should differ is the character: one vibrato (F, G, H) against a shimmer (D, E).
- The page's own figures, at H's final 16 c, are in `right_movement/_verify.txt`.

**Flash if adopted:**
- F or G: as D and E (the flutter oscillator; the phase and rate spread are constants), about −8 B.
- H: the transport class, the same ~2.7 KB as the left side, or a few bytes more if the left side's transport ships too.

**On the pages** (`right_movement/_verify.txt`, H at 16 c), "together" is the correlation of the two tones' movement below 8 Hz, at full right:
- D and E (each line its own phase): −0.12 to +0.21.
- F and G (all lines together): −0.01 to +0.26.
- H (on the output): 0.82 to 0.93.

[I] So even with every line in step, the loop smears the phase: the tail doesn't bend as one, and only a vibrato after the tank does. That is the same reason the tape side needed its transport.

**Checks**
- Energy (F and G, 135 settings each): G passes (+0.35 dB). F fails the same PLATE, SIZE 1, DECAY 0.75, MOTION 1 cell as D did, at +0.79 dB in the 16–30 s window at about −128 dBFS. With other noise seeds that cell reads −0.32 to +1.48 dB there, and within ±0.15 dB in every window above −50 dBFS: the floor's noise, not growth.
- 10-minute holds at DECAY 1, MOTION ¾ and full, every SPACE: 12 of 12 pass for F and G.
- Clicks: the MOTION sweep reads +7.4 to +9.4 dB for every version against today's +7.9 to +9.0.
- Left page, MOTION fully left held on a pure organ chord:
  - every version's loudest 1 ms frame above 3 kHz is 82–86 dB under the signal;
  - the ratio to the surrounding frames rises on PLATE D and E (+14, +17 against today's +7) only because the faults dull the highs around the frame.

## Round 3: more variance, and crinkle as Ribbons or Magneto (27 Oct 2026)

Owner's picks: on the right side, "B stronger = winner" (stronger Wander; the sines are out, so the right page is deleted). On the left, D, the transport and crinkle. Notes: "Tape transport could introduce a bit more variance at the extreme end — Kinotone Ribbons pedal does a good job of introducing slight pitch fluctuations/wow/flutter — we should use this pedal as a reference". And: "The crinkle seems more like light-weight dropouts rather than a true crinkle — or at least what I'm used to that parameter sounding like on the Kinotone Ribbons pedal and Strymon Magneto."

### Sources

**Kinotone Ribbons** ([manual](https://kinotoneaudio.com/ribbons-manual), [PDF v2.0](https://kinotoneaudio.com/wp-content/uploads/RibbonsPaperManual2.0.pdf))
- [S] Wow is random: the pitch "randomly wobble[s]", more dramatically with depth. Wow Frequency "changes the max speed": a ceiling on the rate, not a fixed rate.
- [S] Flutter is "faster and more jittery", and turns "more chaotic" with its frequency.
- [S] Dropouts are the tape "getting stuck momentarily and then catching up": a speed event.
- [S] Crinkle is "surface-level imperfections on a wrinkly piece of cassette tape": "brief moments of signal loss as the tape separates from the tape head".
- [S] Dropouts and crinkle are processed separately for left and right. No rates or depths are published.

**Strymon Magneto** ([manual](https://www.strymon.net/manuals/Magneto_UserManual_REVB.pdf)) and **El Capistan** ([manual](https://www.strymon.net/manuals/ElCapistan_v2_UserManual_RevC.pdf))
- [S] Crinkle "controls the amount and severity of tape irregularities including friction, creases, splices, and contaminants".
- [S] It runs from fresh tape to tape "mangled and chewed", and tracks tape speed: slower tape, more effect.
- [S] Wow & Flutter is one knob for "mechanically related tape speed fluctuations".

**ChowTape** (GPL: numbers and mechanism only) ([WowProcess.cpp](https://github.com/jatinchowdhury18/AnalogTapeModel/blob/master/Plugin/Source/Processors/Timing_Effects/WowProcess.cpp))
- [S] Wow is a cosine plus an Ornstein-Uhlenbeck random walk.
- [S] "Variance" scales the random part as amt^1.25. "Drift" multiplies the rate by up to 2, re-drawn every block.

**Scrape flutter**
- [S] Above 100 Hz, from stick-slip of the tape over the head; it adds "a roughness" ([Wikipedia](https://en.wikipedia.org/wiki/Wow_and_flutter_measurement)).

### Conclusion

- [I] The references disagree on crinkle. On Ribbons it is many brief contact losses: level and highs, no pitch, each side on its own. On Magneto it is the tape's friction and creases, which [I] read as pitch roughness.
- [I] Round 2's crinkle was sparse 50–300 ms bursts of jitter with a 1–3.5 dB dip. That is close to what Ribbons calls neither: it reads as light dropouts, as the owner heard.
- [I] Variance at the far end follows Ribbons and ChowTape: a rate that wanders under a ceiling, plus a random walk.

### Prototypes (`IV_TAPE_TRANSPORT` 5, 6, 7; the left page `renders/drift_rethink/left_tape`)

**B (mode 2):** round 2's D, unchanged in sound. The transport's centre moved from 8 to 21 ms to make room for C's wider swings, so the wet arrives 13 ms later.

**C (mode 5): more variance**, over the last half of the side, eased in (`variance` 0 → 1). At 9 o'clock it is barely moving.
- The wow's speed is re-picked every 0.5–2 s, from −40 % to +100 % at full, and glided over about 0.4 s.
- A slow random drift (~0.15 Hz, up to ±6 c) runs underneath.
- Three in ten of the wow's new targets are half as big again at full.
- Wow depth goes 12 → about 20 c, flutter 3 → 4.5 c.

**D (mode 6): C, with crinkle as on Ribbons.**
- Crumpled stretches of 0.3–1.5 s, about 0.6 a second at full.
- Within a stretch, each channel loses the head for 5–40 ms, 6–20 dB, with the highs down to about 4.5 kHz. That happens about 8 times a second, with ramps of up to 2.5 ms.
- No change of pitch.

**E (mode 7): C, with crinkle as on Magneto.**
- A bed of scrape roughness: random pitch around 100–200 Hz, added to the read point per sample, growing over the last 40 % of the side.
- Bursts of 0.1–0.6 s with jittery pitch at about 45 Hz, up to ±12 c.
- No level loss.

[M] Two held tones (700 Hz and 1.3 kHz), MOTION fully left, p95 cents of the 1.3 kHz (HALL / MASSIVE / PLATE):

| | p95 | wow (< 4 Hz) | together |
|---|---|---|---|
| A today | 21.4 / 13.2 / 18.5 | 8.6 / 5.0 / 6.4 | 0.38 / 0.30 / 0.38 |
| B round 2's pick | 13.7 / 10.2 / 16.1 | 8.7 / 8.3 / 10.2 | 0.65 / 0.89 / 0.59 |
| C more variance | 16.6 / 12.4 / 17.9 | 11.7 / 10.5 / 11.4 | 0.82 / 0.95 / 0.68 |
| D Ribbons crinkle | 20.9 / 17.4 / 21.8 | 18.8 / 15.4 / 18.3 | 0.83 / 0.97 / 0.85 |
| E Magneto crinkle | 18.5 / 15.1 / 19.9 | 13.1 / 12.0 / 13.8 | 0.85 / 0.96 / 0.80 |

- Every version stays far below the seasick slow sine's 64 c p95.
- D's and E's wow differs from C's only because the faults draw from the same random numbers, which gives the wow a different random path.
- Peaks of 35–72 c show up in A too: tracker glitches where the tail's level dips.

[M] Faults over 60 s in HALL, counted by the transport:
- D at full left: 248 gaps (103 at ¾ left), costing 0.26 dB of level.
- E: scrape sidebands 60–250 Hz around a held 1 kHz are +21.6 dB over A at full left, against C's +11.9. That is about 10 dB of added roughness.

**Checks**
- Labels clean.
- Clicks on the sweeps: +8.0 to +8.6 dB for every version, the same as today.
- On the organ at full left, D's ratio rises (+19 to +32 dB) from its crackly gaps, which stay about 72 dB under the signal.
- Outside the loop, so no energy runs or holds.
- Firmware byte-identical to bbd2635, SAME 55/55, ctest 17/17.

**Flash and CPU**
- Trial with everything compiled in (all crinkle kinds and round 2's dropouts): +4,792 B (117,340 B) and +16.7 KB RAM (two 2048-float buffers).
- One crinkle kind without dropouts [I]: about 3.5–4 KB.
- CPU [I]: about 2 % of the M7, from the code.

### Recordings worth making of the references (analysis only, never committed: CLAUDE.md, ADR 0006)

Play `test_audio/stimulus/08_held_tones.wav` (its first 8 s is a held 1 kHz sine: `tools/pitch_track.py` measures wow and flutter on it) and `renders/_material_drift_rethink/held_organ.wav` through each unit, 100 % wet or effect only, 48 kHz.

- **Ribbons**, mix fully wet, saturation and noise off:
  1. Wow depth at ¼, ½ and full (wow frequency at noon, flutter off). Then wow frequency at minimum and full with depth at ½.
  2. Flutter depth at ½ and full (flutter frequency noon, wow off), and flutter frequency at full.
  3. Crinkle at ¼, ½ and full (everything else off).
  4. Dropouts at ½ depth with frequency at ½ and full.
  5. The owner's usual "tape" setting.
- **Magneto**, delay mix off or the wet-only output, feedback low:
  1. Wow & Flutter at ¼, ½ and full.
  2. Crinkle at ¼, ½ and full (W&F at minimum).
  3. Tape speed slow and fast with Crinkle at ½.
- Each take 20 s, named `<unit>_<control>_<setting>.wav`, into `test_audio/reference/incoming/`.

Those let `pitch_track.py` read wow and flutter (rate, p95 and peak cents) and a dip and sideband count read crinkle. Then the transport's depth, rate wander and crinkle can be matched to measured numbers.

## Round 4: the crinkle's dips (27 Oct 2026)

Owner's pick on round 3 (level-matched, MIX 100 %): E, the Magneto crinkle, on every card, with the repeated cards agreeing. Note: "I like the magneto crinkle, but compared to C there are less dropouts. I think C's dropouts are a little extreme, so let's find a middle ground between C's dropouts in option E."

### What the "dropouts" in C were [M]

C carries round 2's crinkle. Round 2's crinkle is pitch jitter plus a rough level dip (1–3.5 dB) and dulled highs; the owner called it "light-weight dropouts" in round 2.

**Method.** A control (mode 9) is C exactly, the same random path, with only the crinkle's dip and dulling switched off. C against it, sample-aligned, in 10 ms windows, MOTION fully left, 9 cards (HALL, MASSIVE, PLATE × organ, solo chord, mix dub):
- 44 dips a minute of 1 dB or more, against 46 crinkle bursts a minute: one per burst.
- The deepest point has a median of −1.6 dB, the worst −2.1 dB. Each lasts 60 ms (median) at 1 dB or more.
- The highs above 3 kHz dull at 34 moments a minute.
- At ¾ left: 16 dips a minute against 16 bursts.
- Example, HALL solo chord: bursts at 1.90, 3.76, 5.45, 7.77 s; dips at 1.98, 3.80, 5.52, 7.80 s.

**What it isn't.**
- A dip detector on the sound alone counts the music's own dips (25–33 a minute in every version, today included), so it can't tell them apart. Only the paired comparison can.
- C's speed changes and bigger swings don't dip the level. The variance-only control (mode 8) has no more dips than today.
- The transport's delay range was never reached.
- E's random path isn't calmer: its wow measured 13 c p95 against C's 12 (HALL, full left).
- **So:** E has "less dropouts" because its crinkle is pitch roughness only, with no dip.

### The middle ground (`IV_TAPE_TRANSPORT` 10, 11)

E with C's own dip put back on some of its crinkle bursts, chosen by a counter, not by chance, so the share holds in every phrase:
- Each dip is C's: a raised-cosine envelope over 50–300 ms, the level rough by 1–3.5 dB, the highs toward ~5 kHz.
- Its random numbers are its own, so E's wow and bursts stay exactly the picked ones. [M] The burst times are identical to E's.
- E's bursts last longer (0.1–0.6 s against C's 0.05–0.3 s), so there are fewer of them: 34 a minute at full left against C's 40 (60 s held organ, HALL). Shares of 0.65 and 0.43 put C's dip on 22 and 15 of them: 55 % and 38 % of C's 40. At ¾ left it is 8 and 6 against C's 13.
- [M] Paired against E, the dips measure as C's: median −1.6 dB, 60–80 ms.

**Confirm page:** `renders/drift_rethink/left_tape_final`.
- A: E as picked. B: about a third of C's dips. C: about half.
- At 9 o'clock, ¾ left and full left, on the mix dub, solo chord and held organ, in all three spaces.

## Shipped (v1.0.46, 27 Oct 2026)

The owner's picks: on the right side B "stronger"; on the left round 3's E (the transport, C's variance, Magneto crinkle) with the confirm page's B (a third of C's crinkle dips).

- **Right:** `map::driftAmounts`, Wander × (1 + 0.4 x²) deep and × (1 + 0.2 x²) quick, x 0 over the side's first half (`kWanderGrow`, `kWanderRateGrow`).
- **Left:** `dsp/Transport.h` on the wet after the tank (before WEAR's chain), only E's parts: wow, flutter, the variance, the Magneto crinkle (scrape bed and bursts) and the counted dips (share 0.43, `kTransportDipShare`). The tank's lines keep a third of their wobble (`kTransportLineShare`, in `map::driftAmounts`).
- **Gone:** the sines (`DriftAmounts::sine*`), the in-step and output-sine versions, Ribbons crinkle, round 2's crinkle, the dropouts, every `IV_DRIFT_*` and `IV_TAPE_TRANSPORT` hook (the renderer says it ignores them).
- **Off the left side** the transport rests and the wet is bit for bit as before (the dead zone and the right side never touch it). Turned onto the left side, its centre glides from 8 to 1000 samples over 5 s (a smoothstep: the tail runs at most ~11 c slow while it comes up to speed) after a ~5 ms crossfade from the direct wet; turned back, the reverse. **Latency:** on the left side the wet arrives 1000 samples (20.8 ms at 48 kHz) later than at noon.
- **Proof:** renders bit-identical to the prototype at `79c3420` with the pages' hooks (`IV_TAPE_TRANSPORT=11` on the left, `IV_DRIFT_GROW=0.4 IV_DRIFT_RATE_GROW=0.2` on the right) at MOTION 0, 0.1175, 0.125, 0.2, 0.25 (left) and 0.75, 0.875, 1 (right), every SPACE (WEAR 0, TONE noon, not frozen; 12 s and 48 s). `test_motion` pins the page's figures.
- **Flash** +3,992 B (release 106,588 → 110,580 B), no new libm. RAM +16 KB (DTCM, two 2048-float buffers). CPU [I]: about 2 % of the M7 on the left side; off it a buffer write a sample.
