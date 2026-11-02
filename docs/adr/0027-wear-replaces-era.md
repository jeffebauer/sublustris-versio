# 0027 — WEAR replaces ERA: one wear amount over the Neve

**Status:** Accepted, 26-27 Oct 2026. Owner's picks:
- `renders/era_rethink` (level-matched, MIX 100 %): "My pick is C".
- 27 Oct: "Start Clean. We should rename this knob to reflect its new behavior: Wear".

SPEC v1.0.42. Supersedes ADR 0021 (five eras, each on its own) and what was left of ADR 0004 (the Midiverb box) and ADR 0019 (tape out). ADR 0005's hiss stays, now part of the wear. The panel name is ADR 0029.

**Context:**
- **What the owner kept, and what they didn't.** On the eras (v1.0.30-v1.0.40) the owner liked the grit: the Midiverb's and the BBD's, "the reason I like the tape saturation", and the radio's "phasing and the band-pass like tone". They didn't want ERA darkening the tail (TONE's job) or moving its pitch (DRIFT's, now MOTION's), and it had to fit the module's CPU and flash.
- **Hand-overs.** Even with the overlaps narrowed (v1.0.40), each era had to hand over to the next one somewhere on the knob. The owner heard "a hard transition between tape and radio" on `era_switch`.
- **Cost.** A frozen MASSIVE at the cassette ran ~105 % of the CPU on the module (round 15). The eras' code (the radio, tape out, the BBD, the Midiverb box's halfbands) left 92 B of the flash budget.
- **Researched first** (`docs/research/era-rethink.md`). Products that choose a character almost always use a switch. Knobs made for CV are "wear" macros that deepen several stages together (Volante's Wear, iZotope Vinyl, RC-20's Magnitude); Strymon Deco's one knob runs compression → harmonics → dirt. No product was found that sweeps continuously across unrelated media on one knob.
- **Three layouts were heard** (`era_rethink`, 12 cards, HALL / MASSIVE / PLATE): A a grit path, B saturation then texture, C one wear amount.

**Decision:**
- **One wear amount** (`map::eraMix`, `dsp/EraTexture.h`). Every stage comes in together over the first twentieth of the knob (a smoothstep), then deepens with it. The chain is on the wet, after the loop, in this order:
  - **tape:** record pre-emphasis (+6 dB above 2 kHz) around a soft clip with up to 15 dB of drive taken off again, then the exact de-emphasis, and a little bias for even harmonics. At 48 kHz with no oversampling: the clip makes only a third harmonic below its ceiling, and the wet of a 24 kHz tank has little above 8 kHz to fold;
  - **breath:** the BBD's NE570 compander (2:1 in, 1:2 out) with its noise between and the chip's soft 2nd harmonic, without the line's filters, so the noise and the level breathe with the playing;
  - **clock:** the BBD's sample-and-hold at its clock (32 → 14 kHz, never lower: unfiltered it turns metallic) with no filters, so the reverb's top folds back;
  - **grain:** a step that follows the level (9 → 5 bits under it, a floating step like μ-law's), so the grain stays the same share of a fading tail instead of buzzing and gating; a floor keeps silence silent;
  - **band:** the AM radio's gentle two-pole band-pass (~300 Hz-3 kHz, soft at the top), its over-modulation and its wandering dip (a slow random notch, like a phaser), without the station's fades, detune or static. Its blend grows as WEAR^1.5; at infinite DECAY it keeps half (`kBandAtInfinite`), since a held tail darkens to its floor where the band is ~20 dB down.
- **With them, inside the loop:** the tape's slow 2:1 compression above −18 dBFS, the hiss into the tank (ADR 0005) and the crackle, all growing with the knob.
- **The Neve stays the base** in the loop at every setting: its even harmonics and its ×0.7 highs, never darker with WEAR.
- **No darkening and no pitch movement from WEAR.** The cassette's wow and flutter are gone (MOTION's left side carries tape movement since v1.0.46, ADR 0030).
- **Loudness:** a slow level hold on the chain (its 0.1 s levels above ~200 Hz compared, its gain gliding over ~0.5 s, within −6 / +12 dB) plus a small trim for the band keeps music within ±1 dB of WEAR 0.
- **WEAR starts clean:** the default is 0, the Neve alone (as v1.0.41's default 0.1 sounded). The key stays `era` (ADR 0029).
- **Removed:** the five eras and their stacking, the Midiverb box and its halfbands, the BBD and radio stages, tape out (`dsp/Tape.h`), and the ERA prototype hooks (the renderer says it ignores `IV_ERA_LAYOUT`, `_SWITCH`, `_FADE_SHARE`, `IV_TAPE_OUT`, `_OS`).

**Rejected:**
- **A, a grit path:** the eras' idea cleaned up, in quarters, sweet → gritty: tape → the Midiverb's 13 → 11 bits in the loop → BBD → radio. It was the cheapest on the module, but it keeps hand-overs between unrelated sounds.
- **B, saturation then texture:** tape clean → driven over the first half, then the BBD, the grain and the band added on top, each staying once in (Deco-like). Its top fifth costs as much as C.
- **Keeping the five eras** with narrower overlaps or a switch-like crossfade (`era_overlap`, `era_switch`): there would still be hand-overs, and they would still cost the CPU and the flash.
- **Antiderivative anti-aliasing on the tape's clip** (Parker et al., DAFx-16): twice the instructions for what the wet barely has.
- **The Midiverb's bits in the loop** (A's, and ADR 0004's): grit in the loop compounds into the tail and costs per line. The level-relative grain on the wet keeps the grain without either.

**Consequences:**
- **Sound:** WEAR 0 sounds as v1.0.41's default did; every other setting changed. The goldens are rewritten (`hall_wear30` / `50` / `90` were `hall_midiverb` / `_bbd` / `_radio`).
- **Safety:** the wear is after the loop, so it can't build up. 10-minute infinite holds pass in all three SPACEs at WEAR 0, 0.1, 0.5 and 1. Tails reach silence and silence stays silent. WEAR adds no pitch. Turning it doesn't click: the chain keeps running until its level hold is back at unity, since turned off mid-hold the wet stepped. All of this is in `test_era`.
- **Flash:** release 110,516 → 102,508 B with the wear (−8,008 B: the eras' code out). That paid for the small room (ADR 0028), TONE's bell (ADR 0031) and MOTION's tape transport (ADR 0030).
- **CPU:** about 6.7 points with every stage in (estimate). On the module (the candidate lap, `docs/research/m3-candidate-lap1.log`), C costs ~3-4 points more than A or B. With the small room, a frozen MASSIVE at any WEAR peaks at 81.6 % with no late block (ADR 0026 amendment).
- **Tests:** `test_era` is rewritten for the wear. The T60, paint-burst and limiter tests pin WEAR 0, because the tape's compression and grit are not what they measure.

**Amendment (27 Oct 2026, owner's picks after the v1.0.48 rack test; SPEC v1.0.49):** the grit (the BBD clock and the grain) starts at 0.65 of the knob, not at its start (`renders/wear_grit` D: "Grit from 0.65 wins."); the lowest clock is 28 kHz (`renders/wear_fizz` F); the tape drive and the radio's over-modulation are ×0.75 and the compander's harmonics halved (`renders/wear_saturation` E), so a vibraphone's pure partials no longer sprout loud new tones at 1.5-6 kHz; the level hold's ceiling is +15 dB. Measured: the bitcrushing the owner heard was mostly the clock's fizz, the hold's mirror images of the reverb's body, so a filter before the hold (C) barely helped and wasn't kept; a filter after it would, and is noted for later if flash allows. The compander's gains now ramp across each block (its steps had been hidden by the old early fizz).

**Amendment (1 Nov 2026, owner's pick F on every card of `renders/dip_motion`; SPEC v1.0.60; ADR 0036):** the radio's band (its band-pass, over-modulation and wandering dip) leaves WEAR for MOTION's right side. The owner: "leaving the wear knob more focused on saturation-like effects." WEAR is now tape, breath, clock and grain over the Neve; `map::eraMix` has no band blend and `EraWear` no makeup.

**Amendment (2 Nov 2026, owner's pick C on every card of `renders/wear_no_bbd`; SPEC v1.0.64; ADR 0038):** the BBD's clock (its sample-and-hold with no filters, folding the top back) leaves WEAR. The owner: "the aliasing can feel a bit static". WEAR is tape, the BBD's breathing and the grain; nothing moves to MOTION.
