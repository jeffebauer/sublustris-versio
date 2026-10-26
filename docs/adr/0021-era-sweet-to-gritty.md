# 0021 — ERA runs from sweet to gritty: five eras, each on its own, over the Neve

**Status:** Accepted, 25 Oct 2026 (owner's picks on `renders/era_discrete`, `era_v2`, `era_v2b`, `era_v2c`). SPEC v1.0.30. Supersedes ADR 0003 (stacking, travel weighting) and the μ-law parts of ADR 0004 and ADR 0019.

**Context:**
- **Stacking hid the eras.** ERA stacked Clean → Neve → Cassette → μ-law → Midiverb (ADR 0003): each era kept the ones before it. Heard on their own (`renders/era_discrete`), the owner chose them that way "hands down" ("MidiVerb sounds beautiful").
- **Two eras overlapped.** Measured alone (`tools/era_reference.py`), cassette and μ-law add the same kind of grit: a 3rd harmonic at a fixed distance under the note (25 and 30 dB). They sounded alike.
- **The order didn't match the feel.** The owner felt grit build until the Midiverb, then sweeten. They asked for the knob to run from sweeter tones to grittier ones, and for the Neve's warmth to carry under everything ("adds sweetness overall … the underlying signature").
- **Candidates were researched first** (`docs/research/new-eras.md`, `docs/research/radio-artifacts.md`) and heard over three rounds. An early sampler was dropped ("aliasing too intense") once the bucket-brigade grew its own grit.

**Decision:**
- **Five eras in equal stretches** (`map::eraMix`, `kEraCount`): Neve, Midiverb, bucket-brigade, cassette, AM radio.
  - Each comes in over the first third of its stretch while the one before goes, so only neighbours blend.
  - The BBD and the radio then run from their sweet end to their gritty end across their stretch.
- **The Neve is the base under every era.** Its even harmonics go into the loop's soft clip and its share darkens the T60, at every ERA position. ERA fully CCW is the Neve, not clean (A on every card of `era_v2`, "a clear winner").
- **Midiverb:** as before (ADR 0004 amendments): the in-loop 13-bit box with its dark loop, now on its own.
- **Bucket-brigade** (new; `dsp/EraStages.h` `BbdStage`, on the wet after the loop; Raffel & Smith, DAFx-10). It holds:
  - a low-pass at about a third of the clock;
  - the clock's own sample-and-hold behind a weak anti-alias filter, i.e. its aliasing;
  - the NE570 compander, 2:1 / 1:2, with noise between (its breathing);
  - the chip's soft 2nd harmonic.

  Its grit reaches round 2's "more" two-thirds into its stretch and half as much again by the end (`kBbdGritAt`; owner, `era_v2b`). There's no clock whine, which is tonal.
- **Cassette:** as before (in-loop drive and darkening, hiss into the tank, wow and flutter, tape out with its 20 ms fade, v1.0.29), now on its own.
- **AM radio** (new; `RadioStage`, on the wet after the loop; owner's round 3, `era_v2c`).
  - **The station fades on its own path:** a Rician gain (Watterson, ITU-R F.1487), plus round 1's wandering dip.
  - **The receiver's noise stays put,** and an AGC that follows the carrier gives the fade back, so the static swells as the station dips.
  - **The noise:** soft coloured hiss, plus static in three layers (storm activity, Pareto crackle, and lightning flashes of 1–6 strokes, each flash with its own colour).
  - **Tuning:** a slow detune (duller, rougher, slope flutter). At the gritty end, rare drift-offs and a faint mains hum.
  - **A slow level hold** (~8 s, ±12 dB) keeps a held tail, which darkens towards its floor, from draining below the receiver's band.
  - **Whistles and polar flutter were removed** (owner: they "read as slow rhythmic delays rather than dropouts").
- **Removed:** the μ-law output layer, vintage reads (they belonged to the μ-law stretch), stacking, the travel weighting and every prototype hook.

**Consequences:**
- **Loudness** on music (LUFS, the dub skank) stays within ±1 dB across the knob in HALL and MASSIVE (`test_era`).
- **Safety:**
  - The BBD and radio are after the loop, so they can't build up.
  - Infinite tails hold 10 minutes at every era. The radio is judged over 25 s stretches, which average its designed fades.
  - Every era's noise is gone in silence.
  - Turning ERA through the Midiverb's, BBD's and cassette's starts doesn't click.
  - MIX fully CCW stays a clean passthrough.
- **ERA 0 is no longer clean:** the approved-sound reference files move.
- **Flash and CPU:** μ-law and vintage reads out, the BBD and radio in. Sizes are in the handoff.
- **Turned down:**
  - stacking (ADR 0003);
  - ERA on the dry too (ADR 0004 amendment, 24 Oct);
  - stronger stacked stages (superseded; branch `claude/era-strength` deleted, last commit 3608c89);
  - the early sampler, the round-1 and round-2 radios, whistles and polar flutter.

## Amendment, 26 Oct 2026 (v1.0.40): a narrower overlap

The owner heard the tape → radio hand-over as abrupt even with the overlap, and picked between a third (as built), a sixth and a timed 0.75 s crossfade on `renders/era_overlap` after hearing `era_switch` (no overlap, a 50 ms swap): keep a position-based blend (it follows ERA's CV continuously), but narrower: **each era comes in over the first fifth of its stretch** (`kEraFadeShare` 0.2). Two eras' stages now run together over 16 % of the knob instead of 27 %, which also takes CPU off the dearest positions (the cassette with the radio, measured on the module only as single eras so far). The tape's sparkle still fades with the cassette's drive, early in the overlap; if that hand-over still sounds abrupt, the drive's own curve is the next thing to look at.
