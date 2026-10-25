# 0012 — MIX, SIZE, TONE and DRIFT

**Status:** Accepted, 21 Oct 2026 (owner). Spec-session decisions 14, 15, 18, 20. Resolves the brief's [OPEN] kill-dry and SIZE zipper items.

**Context:** Four knob behaviours from the spec session. Ranges and curves: SPEC §3.

**Decision:**
- **MIX:** equal-power dry/wet. Fully left = a clean passthrough (the μ-law layer is wet-only); fully right = 100 % wet for a send/return. **No separate kill-dry setting.**
- **SIZE moves bend the pitch, tape-like** (Cathedra/Supermassive): the delays glide, **slew-limited** to about ±1 semitone (≤ 6 % delay change per second), so fast knob or CV moves become a smooth swoop over up to ~1.5 s, never zipper. While frozen, SIZE plays the companion, so the pad never bends.
- **TONE:** a tilt in the loop; **noon is flat**. Left darkens the tail each pass, right brightens it; loop gain is renormalised so bright never exceeds 1.
- **DRIFT is bipolar:** noon is still (only the anti-metallic modulation floor). Left: random tape wow/flutter. Right: smooth Cathedra-style pitch drift and bend. ERA's Cassette wow/flutter adds on top of the left side.

**Rejected:**
- MIX "dub curve" (dry at full up to noon).
- SIZE crossfading with no pitch change (smoother, less playable).
- TONE noon gently dark.
- One-way DRIFT (tape wobble left to ERA only).

**Amendment (21 Oct 2026, owner's M1 listening notes; SPEC v1.0.8):**
- "Difference between small, medium, and large halls seem quite subtle. Small doesn't read as small." HALL's SIZE is now 20 → 260 ms of mean line, and it scales HALL's finite T60s (× 0.4 at SIZE fully CCW, × 1 at CW; a small room can't ring for 16 s), with the level made up. DECAY still sets the tail; infinite stays infinite.
- "Size small, vibes: long has a buzzy resonance build up": the same change (a small room's long, sparse tail let pitched notes build up; vibes ringing score 43 → 26 dB, noise unchanged).
- "Dark and bright don't feel like extreme ends... lower lows, higher highs": TONE also tilts the wet round 600 Hz (CCW +4 / −9 dB, CW −4 / +6 dB, flat at noon), and its loop damping range widens (high-end T60 ratio 0.08 → 1.5). Noon stays flat.

**Amendment (23 Oct 2026, owner's pick on `renders/tone_shelf`; SPEC v1.0.15):**
- Every tank's loss is now a **first-order shelf crossing at 2.5 kHz** (`kLossShelfHz`, Governor.h `absorbent`) instead of a one-pole low-pass, as Jot and Zita-Rev1 do. TONE still sets the low end's and the high end's T60 the same way; the difference is in between: the upper mids (2–6 kHz: vocal edge, hi-hat, snare crack) now fade with the highs instead of ringing almost as long as the body, so TONE noon is warmer (HALL at noon: 3 kHz rings 2.4 s instead of 3.7 s, 6 kHz 2.0 s instead of 3.5 s, lows unchanged). The DSP audit's top finding (A1). B on all 36 cards (HALL, PLATE, MASSIVE at noon, HALL fully CW), the repeated row agreeing.
- The shelf's K is computed once per prepare with a three-term series instead of `std::tan` (within 3e-7 at 44.1–96 kHz; ~5.8 KB of flash saved against the prototype).
- With modulation off, the PLATE at SIZE ½ rings about 1.5 dB more than before (median of bursts ~12.4 dB, under the 15 dB line; 5 dB with its modulation on). `test_plate_freeze` now judges the unmodulated check on the median of three bursts.

**Amendment (23 Oct 2026, owner's picks; SPEC v1.0.11, v1.0.17):** the PLATE's largest scale is 1.59, not 1.6 (at exactly 1.6 one 9.8 kHz mode lined up with its modulation off). HALL's SIZE starts at 35 ms of mean line (was 20) with the small end's tails × 0.49 (was 0.4), the version the owner picked on `renders/small_hall` (E on all 10: "slightly metallic yet dampened resonances" at 20 ms), plus in-loop diffusion at the small end that fades out by noon. Raising the minimum squeezes the range: noon is now where 0.61 was (95 ms, tails ~10 % longer). MASSIVE's SIZE glide stays at a semitone of bend (`renders/size_glide`, A on all 4: 3 semitones made "odd stepped harmonies", 6 "staticky brick walls").

**Amendment (23 Oct 2026, owner's pick on `renders/still_lows` round 2; SPEC v1.0.18):**
- **DRIFT moves everything above ~300 Hz; the bass stays still.** HALL and MASSIVE ring their lows in a second, unmodulated tank (the input split at 300 Hz, Butterworth), so a dub bassline in the tail never wavers in pitch, whatever DRIFT or the cassette does. B on all 25 cards (HALL tape and Cathedra, MASSIVE tape, noon and Cathedra; bass, skank, pad, keys, full mix), the repeated cards agreeing.
- Why two tanks, not a split inside one loop (the first prototype, which the owner also liked, 23 Oct): two reads of one line recombined in the loop either gain up to +3 dB a pass (a one-pole split, Schlecht's thesis: +83 dB in a small HALL) or, with a split whose bands sum to one (Linkwitz-Riley), cancel near the crossover every pass so that band dies early and infinite holds drain. Measured in `docs/research/still-lows.md`.
- Not covered: the freeze twin and REVERSE's short reverb still move their lows (a second low tank each; a listen first).

**Amendment (23 Oct 2026, owner's pick on `renders/drift_cw`, two rounds; SPEC v1.0.20):**
- **DRIFT's right side is Wander:** quick, small random movement, up to ±20 cents at 1 → 2.25 Hz, each line's rate fanned out ±50 % (Lexicon's Random Hall and Valhalla's Random Space randomise delays the same way). The slow Cathedra sine was "too extreme… too seasick": it bent the whole tail out of tune and back together.
- **Rejected after listening:**
  - Halo (vibrato on the input, the Mercury7's way).
  - Orbit, a rotating feedback mix after Schlecht & Habets: "quite subtle" at first, then "too intense and creates pitch-y swirling" when stronger.
  - Three phasers: Jet on the wet, Dweller on the input, Tide in the loop.
- **Tape (left) is unchanged.** Wander uses the same random generator, but quicker, with no flutter and the lines spread wider, so the two sides stay distinct.

**Amendment (24 Oct 2026, owner's picks on `renders/valhalla_round2`; SPEC v1.0.25):** HALL's modulation floor is three times deeper, which smooths a long HALL's resonances toward Valhalla Room's with almost no audible pitch movement. HALL's and PLATE's highs now fade by "air", a steady extra loss of a few dB per second at every finite DECAY, instead of a fixed share of the decay. Short tails stay bright, as in a real room, and long PLATEs lose some top end. TONE still scales it. MASSIVE keeps the share.

**Amendment (25 Oct 2026, owner's pick on `renders/knob_moves`; SPEC v1.0.34, ADR 0025):** HALL's and MASSIVE's SIZE no longer glide. Once the knob rests, the new lengths take over and the old ones fade out over 0.3 s, so a held note keeps its pitch. PLATE's SIZE still glides at a semitone of bend.
