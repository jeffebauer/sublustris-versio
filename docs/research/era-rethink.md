# ERA rethought as texture: research, three layouts, their cost

26 Oct 2026, prototype on `claude/era-rethink-9507f2` (from `claude/massive-air`, v1.0.40). The owner asked to keep
the grit they like (the Midiverb's and the BBD's grit, the tape's saturation, the radio's "phasing and the band-pass
like tone"), drop what TONE and DRIFT already do, and fit the module. Background: `docs/research/era-character.md`
(main checkout). Full source notes: the session's research file (side-session notes, `era-rethink-research.md`);
the conclusions and sources are here.

## What the research says, and what the prototype took

| Question | Finding (source) | Taken |
|---|---|---|
| Tape saturation without oversampling | First-order antiderivative anti-aliasing (ADAA) of a cubic is cheap (Parker, Zavalishin & Le Bivic, DAFx-16; Bilbao et al., IEEE SPL 2017; Chowdhury's CCRMA notes). A cubic makes only a 3rd harmonic, so on a wet with little above 8 kHz ADAA is insurance, not a need. Tape's feel is pre-/de-emphasis around the clip, so the highs squash first and nothing darkens (Strymon Deco, Sound On Sound 2015; Chowdhury, DAFx-19, GPL code: ideas only). VintageVerb's Chaotic modes put tape saturation with pre-/de-emphasis in the loop. | Emphasis (+6 dB above 2 kHz) around a clamped cubic, 15 dB of drive across its intensity, a little bias for even harmonics, at 48 kHz with **no ADAA**: ADAA cost twice the instructions (one divide a side, branches), and the wet of a 24 kHz tank has little to fold. On the wet, not in the loop (CPU, see below). |
| BBD grit without its low-pass | Raffel & Smith (DAFx-10): sample-and-hold at the clock between two filters, an NE570 2:1 / 1:2 compander, mostly 2nd-harmonic nonlinearity. Holters & Parker (DAFx-18) for modulated delays (chowdsp, GPL). Without the filters, the compander's breathing and the 2nd harmonic survive; unfiltered clock images turn metallic below ~16 kHz; it reads less "BBD" and more "breathing, grainy analogue" (inference). | The compander with noise between its halves and the 2nd harmonic, its gains following its envelopes a block late (no per-sample divide); the clock's sample-and-hold with no filters, 32 → 14 kHz (not lower), only in the BBD's upper part in A. |
| Bit/rate grit that stays musical on a fade | In a loop, magnitude truncation makes a tail end sooner, rounding can hang at a buzz (limit-cycle literature; Mitra & Kaiser). On the output, a fixed step buzzes then gates as the tail falls through the last bits. A level-relative ("floating", μ-law-like) step keeps the grain the same share all the way down (companded 8-bit samplers; adabarx/float_crush; ToneBoosters BitJuggler). thement/midiverb_emulator is GPL: reference only, nothing used. No primary source found for the Midiverb's 13 bits (ADR 0004 keeps the project's reasoning). | A keeps the Midiverb's 13 → 11 bits truncated **in the loop** (its honest sound), without its dark loop or halfbands. B and C get a **level-relative grain on the wet** (9 → 5 bits under the level, a 16-bit floor so silence stays silent). |
| The radio's band and dip | ITU-R F.1487 / Watterson: two paths 0.5-2 ms apart fading at 0.1-1.5 Hz; selective fading sweeps a null through the audio (a comb). One notch random-walking 300 Hz-3 kHz is the cheap version; a 4-stage allpass phaser is closer to "phaser-like". A real band-pass cuts the highs, which is TONE's job: keep the top gentle. | Today's wandering dip (one notch), today's over-modulation, and one gentle two-pole band-pass ~1-1.2 kHz in place of today's 2-pole high-pass and 4-pole low-pass (half the filters, a gentle top). No station fades, detune, static or hum. |
| How products lay these out | Picking a character is almost always a switch (Swells' LO-FI, VintageVerb's Color, Decapitator's Style, Generation Loss's Model). Knobs made for CV are "wear" macros deepening several stages together (Volante's Wear, iZotope Vinyl, Tape MELLO-FI, RC-20's Magnitude). Deco's single knob runs compression → harmonics → dirt. No product found that sweeps continuously across unrelated media on one knob. | C has the strongest precedent; B is Deco-like; A is today's idea, cleaned. |
| In the loop or on the output | In the loop, grit compounds into the tail (a reverb made of grit), costs per line, and risks energy; on the output it's added once, constant cost, DECAY keeps its meaning. Gentle saturation in the loop tames ringing (VintageVerb). | On the wet for everything but A's Midiverb bits and the tape's compression (today's in-loop extra loss above -18 dBFS, which costs nothing per sample). The tape's in-loop drive (today's 1.4×, a divide per line-sample) is dropped. |

## The three layouts (core/params/Mappings.h `eraLayoutMix`)

All three start as today at ERA 0 (the Neve's warmth and ×0.7 highs in the loop, the base, never darker with ERA),
keep no cassette wow or flutter and no radio detune, and run every wet stage only while it's in.

- **A, grit path:** quarters, each fading in over a twentieth of the knob as the one before goes, sweet → gritty
  across its quarter. Tape (saturation, compression, faint hiss) 0-0.25 → Midiverb (13 → 11 bits in the loop,
  crackle) 0.25-0.5 → BBD (breathing, then the clock's grit from its middle) 0.5-0.75 → radio (band, dip,
  over-modulation) 0.75-1. The Midiverb before the BBD: its grain is the finest grit, the BBD's breathing noise and
  aliasing rougher.
- **B, saturation, then texture on top:** tape clean → driven 0-0.5, then kept; BBD breathing and clock 0.5-0.65;
  the grain 0.65-0.8; the band and dip 0.8-1. Each stage stays once in.
- **C, one wear amount:** every stage in from 0.05 and deepening with the knob; the band's blend as v^1.5.

Loudness: a slow level hold on the chain (~4 s, measured above 200 Hz, -6 / +12 dB) and two small trims keep every
layout within ±1 dB of ERA 0 on music (LUFS; HALL and MASSIVE; the skank, keys and the full mix's send), as
`test_era` asks of today's.

## CPU on the module (estimates)

Method: each stage compiled for the Cortex-M7 (`arm-none-eabi-g++ -O3 -ffp-contract=off`, channels unrolled), its
instructions counted, and scaled by today's BBD stage (286 instructions, 3.8 points measured in round 15: 1.35
cycles an instruction; today's tape, with its oversampler's memory, runs 1.7). The wet stages touch no SDRAM, so
round 15's memory-bound caveat doesn't apply to them. Points = % of the 10,000-cycle sample budget, over the Neve.

| Stage (stereo) | Instructions | Points |
|---|---|---|
| level hold + blend overhead (whenever any stage is in) | 47 | 0.6 |
| tape | 94 | 1.3 |
| breath (compander) | 94 | 1.3 |
| clock | 36 | 0.5 |
| grain | 33 | 0.4 |
| band + dip | 119 | 1.6 |
| all of them | 494 | 6.7 |
| control (per block, while in / moving) | – | ≤ 0.3 |

| Where on the knob | Today (round 15) | A | B | C |
|---|---|---|---|---|
| low (0.05-0.25) | Neve 0 / Midiverb +4.5 in the loop (0 frozen) | tape 1.9 | tape 1.9 | 6.7 |
| middle | BBD 3.8, cassette 8.4 wet + ~1 in the loop | Midiverb ~3-4 in the loop (truncation only, no dark loop; frozen: the twin's lines only), BBD 2.4 | tape + BBD 3.7, + grain 4.1 | 6.7 |
| top | radio 4.9 + 0.5 | radio 2.2 | 6.7 | 6.7 |
| crossfades | two stages at once | up to 4.0 (BBD → radio) | – | – |
| **frozen MASSIVE** (Neve 93.5 %, cassette 104.8 % measured) | up to 104.8 | **≤ ~97.5** (95.4-96 most of the knob) | **95.4 → 100.2** | **~100.2 everywhere** |

Every layout's worst case is below today's frozen cassette (104.8 %), but only A stays near the < 95 % target, and
none is under it without the frozen-twin savings (`frozen-twin` session: −4.5 to −17.5 points). B's top fifth and
all of C sit at ~6.7 points: the cheapest further step would be running the chain inside the tank's own 24 kHz
domain (half the samples, ~3.4 points), or the band in mono (an AM radio is; −0.8).

## Flash (release, as it would ship: `make -C firmware ERA_LAYOUT=1|2|3`)

| Build | Bytes | Spare to 112,640 (110 KB) |
|---|---|---|
| today (v1.0.40, `e570741`) | 112,548 | 92 |
| A, B or C in place of today's eras | 108,540 each | 4,100 |

Today's radio (`process` 2.6 KB, `set` 1.5 KB), tape (1.3 KB) and BBD go; the texture chain (`set` 1.4 KB,
`process` inlined) comes in. Each build still carries all three layouts' mapping; one alone would be a little smaller.

## The page and its checks (renders/era_rethink, main checkout; `tools/pages/era_rethink.py`)

12 cards: HALL and MASSIVE at ERA 0.2, 0.4, 0.6, 0.8, 1.0, PLATE at 0.7, MASSIVE swept 0 → 1 over 16 s; the dub
mix (reverb on its send) and a solo chord with two snare hits; 100 % wet with a MIX fader; pick buttons; 769 MB.

- **Labels** (`_verify.txt`): every render's hook line names its layout; no two versions identical on any card.
- **Level:** every layout within ±1 dB of its ERA 0 on music across the knob (skank, keys, the mix's send; HALL and
  MASSIVE). Against today at the same knob point, -1.1 to +1.7 LU: today itself dips ~1 dB at its cassette.
- **No darkening:** energy above 5 kHz against ERA 0 never falls outside the radio's stretch (B +4 to +8 dB at
  0.6-0.8: the clock's and grain's fizz); the radio's band takes 2.4-6 dB off the top (C ~0: its band is in only
  part-way). Today: -8 to -30 dB.
- **Clicks** (1 ms frames above 5 kHz across ERA sweeps of a held pad and the mix, HALL and MASSIVE): every layout
  at or under today's (8-14 dB against today's 9.6-15.5; A's HALL pad 14.2 against today's 13.8, both at the radio).
- **Energy:** 10-minute infinite holds at DECAY 1 for A (0.2, 0.4, 0.5, every SPACE) within 0.7 dB. The level grid
  against today's same knob flags A louder late in tails (+3 to +6 dB; +125 where today's Midiverb has already
  truncated to silence): today's eras darken in the loop and lose level, these don't. Rerun against the Neve base
  (`--ref era=0`): see below.
