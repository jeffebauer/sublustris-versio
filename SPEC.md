# IMMENSIO VERSIO — Firmware Spec

**Name:** Immensio Versio (Latin *immensus*, "boundless"). Firmware target name `immensio_versio`. Sibling to Resilio Versio.
**Target:** Noise Engineering Versio platform (Electro-Smith Daisy Seed inside).
**Goal:** a huge, warm, lush, characterful reverb for dub techno, ambient and dark ambient. Supermassive / Cathedra scale and drift, played like an instrument. Never metallic.
**Status:** Spec **v1.0 (frozen)**, 21 Oct 2026. Input: `IMMENSIO_BRIEF.md`. Vocabulary: `CONTEXT.md`. Decisions: `docs/adr/` (0001–0017; index in `docs/adr/README.md`). Session record: `docs/spec-session.md`. Changes after freeze: new ADR + changelog entry. Numbers marked *(start)* are starting guesses; tuned numbers replace them as milestones confirm them.

### Changelog
- v1.0.28 — owner's picks (24 Oct 2026; `renders/decay_level` C "all made up" on every card, the repeated cards agreeing, and "levels should ideally be consistent across reverb types"; new ADR 0020): **DECAY keeps the loudness.** A shorter tail is no longer quieter: the wet (and the twin) is made up to the level of each SPACE's longest finite tail (`map::decayLevelMakeup`), and REVERSE's short stops come up to the forward wet (`map::reverseLevelMakeup`); the PLATE is 0.4 dB louder to stay matched. Every finite setting in every SPACE now measures within about 2 dB on music (13 dB before; MASSIVE was 5 dB under HALL and PLATE at the same tail length). The finite top, the crossover, infinite and a frozen pad are unchanged.
- v1.0.27 — owner's pick (24 Oct 2026; `renders/tape_out` round 2 on the settled tape, E "a little stronger, light hysteresis" on all 24 cards, the repeated cards agreeing; ADR 0019 amendment; `docs/research/tape-out.md`): **tape out is stronger and has light hysteresis** (`dsp/Tape.h`). Across the cassette layer:
  - a record pre-emphasis shelf at 2 kHz, up to +6 dB;
  - drive up to +12 dB, taken off after the curve;
  - a play-operator hysteresis whose width follows the level;
  - the asymmetric curve (a third of v1.0.22's bias) at 4×;
  - the exact de-emphasis and a 5 Hz DC blocker.

  It is unity for small signals; the peaks and the highs change. At the cassette layer's full drive the tape soft-limits the wet's peaks to about a quarter of full scale (ERA's loudness on music still within ±1 dB; `test_output`'s limiter check moved to ERA 0.3). The steady-level tape was turned down (`renders/tape_level`, A on 12 of 15 cards). The full Jiles–Atherton hysteresis wasn't picked and is gone.
- v1.0.26 — owner's picks (24 Oct 2026; `renders/tape_bias` B "a third" on 14 cards, one split; ADR 0019 amendment):
  - **Tape out's asymmetry is a third of v1.0.22's** (`kTapeBias` 0.05 × the cassette layer). The full bias bent even quiet held notes: a 2nd harmonic 14–22 dB under a held vibes note. Now it sits 24–32 dB under, and the tape's even warmth stays on peaks.
  - **The full mix's send is 12 dB hotter** in the page material (`tools/make_material.py`; "both" on `renders/fullmix_send`), as a pushed dub send.
  - Not yet decided: the steady-level tape that page also preferred. On a quiet held note it drives the tape harder (3rd harmonic up to −36 dB), so it gets a held-note page first. With the tape bypassed (`IV_TAPE_OUT=0`), the held vibes' harmonics are −56 to −78 dB, the same as ERA clean: what's left is tape out's own odd saturation.
- v1.0.25 — owner's picks (24 Oct 2026; `renders/valhalla_round2`, the vibes cards set aside for the tape distortion; ADR 0007, 0012, 0013 amendments; `docs/research/valhalla-notes.md`): **five fixes from the Valhalla comparison.**
  - **HALL starts right behind the note:** it gets MASSIVE's early taps. The first sound comes at 10 ms, not 80 ("a slight slap-back"). The taps now glide with SIZE sample by sample, so small-HALL SIZE jumps don't click.
  - **HALL's floor is 3× deeper** (`kHallFloorScale`): a long HALL's resonances smooth toward Room's (drums 11 → 8.9 dB over their surroundings). A held 1 kHz wavers 6.5 → 7.3 cents.
  - **Air** (`kAirHallDbPerSecond` 5.5, `kAirPlateDbPerSecond` 8): HALL's and PLATE's highs lose a steady extra few dB per second at every finite DECAY, as air does, instead of a fixed share of the decay. A 2 s HALL's 8 kHz rings 1.8 s (was 1.1; Room 1.8). A 15 s PLATE's 8 kHz rings 5.6 s (was 7.8).
  - **The PLATE's lows are centred:** the cross-branch output taps are added, not subtracted. With true-stereo input, Dattorro's signs made left and right lows near mirror images (vibes −0.86 below 250 Hz); now +0.14. The highs stay wide.
  - **MASSIVE's built-in swell is halved** (`kMassiveSwellScale`): it peaks ~0.3 s sooner.
  - **Level:** HALL measured 2.3 LU louder, so `kHallOutputTrim` takes it off and SPACEs stay matched.
  - **Side effect:** a big PLATE's start is 0.01–0.03 s grainier (`test_density`'s known list updated).
- v1.0.24 — owner's OK after `renders/infinite_stop` and the held-Midiverb listen (24 Oct 2026; ADR 0008 amendment; `docs/research/soak-audit.md`): **an infinite tail holds where you stopped.**
  - **The problem:** the floor governor (and ERA's cassette compression) judged the tail on a 1.5 s level that lagged behind it, so a loud input that stopped or faded made an infinite tail plunge and creep back over minutes. Fed 12 dB hot, PLATE fell 27 dB; at ERA's Midiverb end, held keys fell ~7 dB when they stopped.
  - **The fix:** both now take the lower of the 1.5 s and a 50 ms level, and a held tail's hold waits for the tank's own input to go quiet (BLOOM still feeding it). Every dip is now within ~2 dB. Steady fed levels, finite tails, pads and the goldens are unchanged.
  - **Same merge:** the overnight soak's ten bug fixes (sound unchanged in steady state). Among them: freeze taps mid-block becoming paints, clicks on SPACE flips, small-HALL SIZE jumps and REVERSE switching, a frozen pad swelling 20–55 dB, and a phantom level after a flip.
- v1.0.23 — owner's pick (24 Oct 2026; `renders/midiverb_hold` round 2, B "brighter fading" on all 16 cards, the repeated cards agreeing; ADR 0004 amendment): **a held Midiverb tail stays brighter and fades slowly.** Across DECAY's infinite crossover the Midiverb box's dark loop fades to a quarter (`kMidiDarkAtInfinite`). With all of it, a held tail became a low drone (a held 1 kHz tone was gone within a minute). With none of it (round 1's "bright"), ERA's per-pass grain piled up in the highs until the level control pulled it back (a rise and drop, +18 dB of hiss above 5 kHz in a minute). With a quarter, the hiss settles at a steady low level and a held 1 kHz tone fades about 0.25 dB/s. Finite tails are unchanged. 10-minute holds at ERA 1 pass in every space.
- v1.0.22 — owner's picks (24 Oct 2026; `renders/dsp_audit`, B on every card, the repeated cards agreeing; ADR 0019 new, ADR 0009 amendment; `docs/research/dsp-audit.md` A3, E4, E5):
  - **A frozen pad holds still:** the floor and the PLATE's chorus glide out with the freeze (a frozen HALL chord's purity over 5 min: 0.66 → 0.93).
  - **Vintage reads:** across ERA's μ-law stretch, HALL and MASSIVE read their lines the Lexicon 224 / PCM70 way (32-step quantised linear interpolation plus a small noisy jitter; Costello).
  - **Tape out:** the cassette stage saturates the wet on its way out (an asymmetric tanh at 4×, outside the loop). Its in-loop drive never reached its knee on music.
  - Energy-checked (sweeps, 10-minute holds, 5-minute freeze holds). `test_era`'s aliasing check now compares 48 against 96 kHz, because tape's asymmetry makes real difference tones.
  - The rest of the audit was measured and closed (P2–P5, E2, E3, E6, R1, R2, MI #5).
- v1.0.21 — owner's picks (24 Oct 2026; `renders/knob_feel`, B on 22 of 23 cards, the repeated Hits card agreeing; the one A was ERA's 95 % position on the skank; ADR 0003, 0007, 0015 amendments; `docs/research/knob-feel-audit.md`): **five knobs made even.**
  - **ERA:** Neve grows in straight steps from 3 % (Clean was 0–10 %, then a smoothstep: the first 20 % changed nothing measurable), and the Midiverb box fades in across its whole stretch as (share)² (it switched on within 2 % at 85 % and cut a tail ~5× in one notch, then the last 13 % did nothing).
  - **MASSIVE DECAY:** the shortest tail rings at least 3 passes of the mean line (2 s at SIZE 0 as before, ~2.5 s at noon, ~6 s at full SIZE; the first notch jumped 19–30× a typical step), with the level made up so each setting keeps its loudness.
  - **MASSIVE TIME in NORMAL:** predelay 0 → 250 ms in straight steps (it was x², dead for the first 25–40 %).
  - **BLOOM:** starts at 0.12 s (was 0.1, where the swell stage still passes the dry attack, so the first notch jumped 3–6×).
  - **MASSIVE's early taps:** 6 per line instead of 3, the lines' taps interleaved, at the same total energy (the swell dipped up to 5.8 dB before its peak: the owner's "uneven on Hits").
  - ERA's B was energy-checked (no level above today over 405 settings; holds as today). The PLATE-at-Midiverb hold creep found by the audit predates this and is unchanged.
- v1.0.20 — owner's pick (23 Oct 2026; `renders/drift_cw`, two rounds; ADR 0012 amendment; `docs/research/drift-cathedra-side.md`): **DRIFT's clockwise side is Wander.** The slow Cathedra sine (0.05–0.3 Hz, ±35 cents) was "too seasick": measured, it bent a HALL tail 64 cents p95 with the whole chord swaying ±25 cents together at 0.3 Hz, three times the tape side. Round 1 compared Halo (input vibrato), Orbit (a rotating feedback mix) and Wander; round 2 stronger Orbit and Wander and three phasers (on the wet, on the input, in the loop). "Wander strongest is the winner… maybe a half-way point between stronger and strongest": the wow generator, 1 → 2.25 Hz, up to ±20 cents, line rates fanned out ±50 % (`DriftAmounts::rateSpread`), no flutter. A held 1 kHz at full wavers ~20 cents p95 in HALL, 17 in MASSIVE, 26 in PLATE (the tape side: 21 / 11 / 20), with no shared sway. Energy-checked: every 10-minute hold at DRIFT ¾, ⅞ and full passes in all three spaces, no build-up. The prototypes are removed.
- v1.0.19 — owner's M6 note ("Midiverb can sound louder than the others with certain material", 23 Oct 2026; ADR 0004 amendment): **ERA's loudness make-up is set on music**. Measured on the M6 page (LUFS, twelve materials), music got 4–6 LU louder across ERA (HALL +4.7, MASSIVE +5.4 median at ERA fully CW), because the make-up had been tuned on noise bursts' tails, where ERA's darkening costs level that music doesn't show. The stages themselves barely lose level on music, so the make-up is now Neve 0.2, cassette 0.2, μ-law 0, Midiverb 0.4 dB (was 0.6 / 1.4 / 2.4 / 1.3): ERA fully CW is −0.2 (HALL) / +0.5 (MASSIVE) LU from clean, every stage within ±0.5 dB median. `test_era` now checks ±1 dB in LUFS on the dub skank (BS.1770 K-weighting) instead of ±2 dB on noise-burst tails.
- v1.0.18 — owner's pick (23 Oct 2026): **still lows** (`renders/still_lows` round 2, B on all 25 cards, the repeated skank cards agreeing; ADR 0012 amendment; the DSP audit's A4, `docs/research/still-lows.md`): HALL and MASSIVE split the tank's input at 300 Hz (2nd-order Butterworth, power complementary); below it a second FDN at 12 kHz with no modulation (no floor, DRIFT or cassette wow) rings the lows, following the main tank's SIZE, DECAY, TONE, ERA and governor; above it the main tank moves as before. A held 55 Hz with DRIFT fully CW wavers ~7 cents instead of ~190 (HALL), ~14 instead of ~51 (MASSIVE), the same at either end of DRIFT. Energy-checked: no build-up over SPACE × SIZE × DECAY × DRIFT, every 10-minute hold passes, band T60 within 7 %. A split inside one loop was rejected: every crossover either builds up or drains (measured). ERA's Midiverb make-up 1.0 → 1.3 dB (the highs-only tank loses a little more to the 13-bit truncation: its finite tails end ~10–20 % sooner at short DECAYs). The freeze twin and REVERSE's short reverb still move their lows. Release flash 97,692 → 99,644 B (`std::tan` avoided: `bilinearK`).
- v1.0.17 — owner's picks (23 Oct 2026): **ducking is 4:1 compression of the wet** (`renders/ducking`, D on all 18 cards; ADR 0014 amendment; from the M1 notes: DARKEN was "too extreme", held too long and made the held tail jump when you stopped): above −24 dBFS of input, attack 30 ms, release 0.4 s (a freeze lets go in 50 ms), about 3 dB of reduction while playing, no darkening; still only from DECAY 0.85 up and never while frozen. DARKEN is gone. **The small HALL:** the smallest SIZE is 35 ms of mean line (was 20) with the small end's tails × 0.49 (was 0.4), matching the version picked, and HALL gains in-loop diffusion (0.5, allpasses scaled with the room) at the small end, fading out by SIZE noon (`renders/small_hall`, E on all 10; the audit's F1/F2): ringing at SIZE fully CCW 12.7 / 11.0 / 10.2 → 7.3 / 8.6 / 0 dB at DECAY 0.45 / 0.7 / infinite. **The held-tail makeup is approved** (`renders/makeup`, A on all 4; ADR 0008 amendment; without it an infinite MASSIVE sagged ~3 dB in 10 minutes).
- v1.0.16 — owner's picks (23 Oct 2026): **ERA's Midiverb is rebuilt as the real unit** (`renders/midiverb`, B on all 15 cards; ADR 0004 amendment): the loop box keeps 13 bits (was 12 → 8, a bitcrusher) and adds the unit's ½ one-pole low-pass every pass, so the stage darkens instead of turning to grit (a HALL tail's share above 4 kHz −6.6 → −35.7 dB); its make-up 1.2 → 1.0 dB. **BLOOM smears its taps** (`renders/swell_smear`, B on all 15; the audit's S1): three allpasses per side sized to the gap between taps, so a hit through a long BLOOM swells instead of returning as ghost hits (flicker 3–7.5 → 2.7–6.1 dB); the taps close up by 5 % so the swell still peaks on time (the owner heard "the attack is a little slower"; its first 10 % still comes 0.06–0.4 s later); BLOOM only, not MASSIVE's mass or the twin. **In PLATE the first input allpass wanders** (`renders/diffuser_motion`, B on all 4; Mutable Instruments' Clouds/Beads): 0.18 ms at 0.4 Hz, L and R opposite, so repeated hits don't enter the plate identically. Kept as they were: the PLATE's stereo image (`renders/plate_stereo`, A on all 8: "more cohesive"; answering on the same side "creates a sonic gap"), and the still lows (liked, but not energy-safe; Claude's list).
- v1.0.15 — owner's TONE pick (23 Oct 2026, `renders/tone_shelf`; ADR 0012 amendment): **every tank's loss is a shelf crossing at 2.5 kHz**, so at TONE noon the upper mids fade with the highs instead of ringing as long as the body (a warmer noon; the DSP audit's A1). B on all 36 cards.
- v1.0.14 — owner's pick (23 Oct 2026, `renders/twin_level`): **the twin sits 6 dB under the pad** (level 0.5, was 0.3 / −10.5 dB), so playing over a frozen pad is no longer a 6–7 dB drop at the tap. C (+6 dB) on all 24 cards, the repeated row agreeing; "the sweet spot is between plus3 and plus6", so +4.4 dB.
- v1.0.13 — owner's picks (22–23 Oct 2026, `renders/swell_curve`, B on all 18 cards): **MASSIVE's twin swells in on an S-curve** (raised cosine: eases in, rises through the middle, settles into place) instead of an even ramp; picked over a crescendo and an analog-style arrive-and-settle. The tank's own swell (BLOOM, MASSIVE's mass) stays linear (`renders/tank_scurve`: A on 21 of 27 cards). +544 B flash.
- v1.0.12 — owner's PLATE pick (23 Oct 2026, `renders/plate_modulation`): **the PLATE's long delays wander too** (§2.3 rule 1: every delay in the tank moves), about 2 cents each at its own rate, as Beads moves both of its long delays. B on all 9 small cards and 7 of 9 large; the frozen cards were a coin flip (4–4, the repeated card disagreed), so **a frozen PLATE keeps today's stillness** (the long wander fades out with the freeze). The twin wanders like an unfrozen PLATE.
- v1.0.11 — PLATE full SIZE (22 Oct 2026, owner's OK): the tank's largest scale is **1.59**, not 1.6. At exactly 1.6 one 9.8 kHz mode lined up and outlasted its neighbours with the modulation off (15 dB; 8 dB at 99 % SIZE; nothing with the modulation on or on a frozen pad). 0.6 % shorter lines, far below hearing; `test_plate_freeze` now checks the unmodulated PLATE at SIZE 0, ½ and 1.
- v1.0.10 — owner's MASSIVE pick (22 Oct 2026; ADR 0007 note): MASSIVE keeps its built-in swell, which also smears its first reflections (without it, plucks get two less-diffuse repeats). **MASSIVE's twin gets its own swell** (§4.2): measured on a click, its spikes at 0.12 / 0.3 / 0.8 s are gone; it arrives 0.2–0.33 s after the input, ~1 dB quieter. One more swell stage in SDRAM (7.6 s × 2), 24 more reads a sample per side while a MASSIVE is frozen.
- v1.0.9 — owner's REVERSE notes, two rounds (22 Oct 2026; ADR 0015 amendment): **REVERSE is the hardware reverse reverb**, rising taps into a short reverb of its own (§4.11), replacing the reversed segments of the tank's output ("a delay, then a break after the first repeat, then audible chunks"). SIZE sets the short reverb's size, DECAY how sudden the stop is.
- v1.0.8 — owner's M1 listening notes (21 Oct 2026; ADR 0012 amendment): **HALL's SIZE reads small to huge:** mean line 20 → 260 ms (was 40 → 180), and SIZE scales a HALL's finite T60s (× 0.4 fully CCW → × 1 CW; infinite stays infinite), its level made up so it doesn't drop; the long, sparse tail of a small room let pitched notes build into a buzz ("Long has a buzzy resonance build up"; vibes ringing score 43 → 26 dB). **TONE's ends go further** ("lower lows, higher highs"): besides the loop damping (high-end T60 ratio 0.08 → noon → 1.5, was 0.12 → 1.3), the wet is tilted round 600 Hz, CCW +4 dB lows / −9 dB highs, CW −4 dB / +6 dB, flat at noon (vibes: highs move 8.6 dB across the knob, was 2.3; lows 6.6 dB, was 0). **Fixes found on the way:** freezing at infinite DECAY stepped the wet in one sample (the duck dropped to 0 at once; now it fades with the freeze glide, and the floor and governor weight glide too); the limiter could rise a hair past its ceiling while releasing (now never past what each sample allows).
- v1.0.7 — owner's freeze picks (21 Oct 2026; ADR 0009, 0010 amendments): **the twin replaces the companion.** Playing over a frozen pad goes into a twin of the big tank (the same space, SIZE and DECAY playing it, no predelay or swell), heard at −10.5 dB as "a more subtle layer on top of the frozen pad, not the main focus", always fading within 8 s so the pad returns to what it was. **Painting** copies a little of the twin into the pad (never the raw input) and eases off as the pad nears 1.5 dB over its frozen level (it grew 7 dB before: "once it's painted into the pad, it becomes dominant"); a painted pad then settles without a ceiling, like a fresh one (held to the paint headroom, the makeup kept lifting it). Unfreeze no longer drops (the governor weight doesn't glide with the freeze). The companion plate, its DTCM (84 KB) and the freeze-voicing prototypes are gone.
- v1.0.6 — owner's M4 listening notes (21 Oct 2026; ADR 0007 and 0015 amendments): **MASSIVE starts with the source:** besides each line's end, its output reads three **early taps** partway along every line (louder the later they sit; output only, the loop is unchanged), and its SIZE predelay is 0 → 100 ms (was 500 ms): after a hit the wet starts in 0.13–0.35 s (was 1.17 s at SIZE 0.7) and swells over up to ~1 s; BLOOM can lengthen that swell but not make it shorter than MASSIVE's build. **No rhythmic echoes at SIZE 0** ("too rhythmic and distracting"): spread 0.4 → 1, diffusion 0.45 → 1 (was 0.08 and 0.15); every SIZE is a wash, SIZE sets how long and how big (spikiness 0.3–0.7 dB, HALL's level). MASSIVE's trim is −4.4 dB (the taps add level). **Pending the owner's pick** (`renders/reverse_options`): REVERSE as today's pieces that rise to a cut, or two overlapping faded backwards readers with no cuts.
- v1.0.5 — owner's M5 listening notes (21 Oct 2026): **bug fixed:** the PLATE's left output read one tap past the end of its line, so at full SIZE it clicked once a lap ("rhythmic flickering static in the left channel"); the taps now follow Dattorro's table (each output starts from the other branch) and a compile-time check keeps every tap inside its line. A frozen PLATE's chorus drops to 15 % depth (it piled up into a seasick wobble); a frozen MASSIVE goes to full in-loop diffusion (painted material came back as audible repeats); DARKEN's release is 150 ms (0.4 s dwelt between skanks). **Pending the owner's pick** (`renders/freeze_options`): how playing over a frozen pad works: the companion plate on top (today, ADR 0009/0010), the input feeding the pad itself, or both.
- v1.0.4 — M7 build (21 Oct 2026): DARKEN is a two-pole low-pass on the **wet output**, not in the loop: a loop filter takes the highs for good, and §4.9 asks for a tail that "brightens back when you stop" (measured: the wet's > 4 kHz share 4 % while playing, 31 % a second after); no ducking while frozen (playing over a pad mustn't darken it). The soft limiter holds the wet at −1 dBFS and reports its gain to the LEDs. In-loop allpass-interpolated reads keep their fraction in [0.5, 1.5) (Dattorro): with the fraction near 0 the interpolator's pole sat by Nyquist and a moving read leaked energy into the top octave (a frozen MASSIVE pad's share above 12 kHz doubled in 5 min; now steady).
- v1.0.3 — M6 build (21 Oct 2026; ADR 0008 amendment, **for the owner to approve**): ERA and DRIFT in the loop, built to the §4.7 table with these changes, each measured: ERA's darkening is a shorter high-end T60 (Neve × 0.7, cassette × 0.4) through the Jot loss, not a low-pass in the loop (a filter takes its loss every pass, so short lines darkened and drained far faster than long ones and infinite tails died); the always-on soft clip is exactly linear below 0.84 of full scale (a cubic curve shaved ~0.1 % a pass); Neve's even harmonics carry no DC (their own mean is subtracted) and fade out at infinite DECAY (they add energy in proportion to the level: a runaway in a lossless loop); the cassette drive is × 1.4 (× 1.8 pushed held tails' peaks into the knee); the head bump is left out (a boost in the loop breaks rule 2); the Midiverb box truncates toward zero on finite tails and rounds at infinite (truncation ate infinite tails), with no dither in the loop (it would pile up as noise); per-layer makeup keeps ERA's loudness within ±0.5 dB; TONE fully CW lets the highs outlast the lows (high-end T60 × 1.3). **Rule 2 exception:** a *held* tail (a frozen pad, or an infinite tail once the input is quiet) that sags below its own settled level is raised back toward it, never above, at most ~0.3 dB/s, because moving allpass reads (fast DRIFT, the cassette's wow and flutter) lose a little energy and nothing else could keep it.
- v1.0.2 — M5 build (21 Oct 2026): every in-loop read is **allpass-interpolated** (Dattorro), so a frozen pad loses nothing pass after pass (a cubic read cost ~5 dB in 5 min); the floor governor also **holds** a still pad at its own settled level (taken 20 s after the freeze, once a MASSIVE pad has spread out) and an infinite tail where it settled once the input goes quiet, judged on an 8 s level, so neither creeps up; the PLATE's loss is calibrated (+9 %) to DECAY's mapping, and its shortest tails are set by its size (one pass round the figure 8 is ~0.65 s at SIZE noon); SPACE flips wait while frozen; the M5 pad criterion is measured from 25 s after the freeze.
- v1.0.1 — M1/M4 build (21 Oct 2026, ADR 0007 amendment): §3 SIZE in MASSIVE made consistent with ADR 0007's "long echoes at low SIZE" (mean line 0.35 → 2 s, was 60 ms → 2 s, which can't give long echoes); MASSIVE gets in-loop diffusion (two allpasses per line, scaled by SIZE) so its long lines blur into a wash; the FDN's output is level-compensated for line length (√(mean / 0.1 s)) so SIZE doesn't turn the tail down, and MASSIVE is trimmed −3.3 dB to match HALL; the swell stage is a 24-tap rising feedforward (§4.5); the infinite floor stops darkening once the tail is at its floor (§4.4). No change for the owner to re-approve except the MASSIVE SIZE range, flagged for listening in `docs/TASKS.md`.
- v1.0 — frozen after owner approval (21 Oct 2026); decisions split into ADRs 0001–0017. TIME = predelay in NORMAL and the P1–P7 layout approved (ADR 0015, 0017).
- v0.2 — after the P7 / SW1 demos (owner, 21 Oct 2026): DESK removed; P7 = TIME; SW1 = ATTACK (NORMAL / BLOOM / REVERSE); ducking fixed to DARKEN; the switch governs the attack in every space, Massive included (ADR 0014, 0015).
- v0.1 — first draft from the spec session (21 Oct 2026).

---

## 1. Hardware facts

| Item | Detail | Source |
|---|---|---|
| MCU | STM32H750 Cortex-M7, 480 MHz (boost) | docs.daisy.audio/hardware/Seed; Resilio SPEC §1 |
| Memory | 64 MB SDRAM; 8 MB QSPI flash; **128 KB internal flash** (code); internal SRAM: 128 KB DTCM (zero-wait), 512 KB AXI SRAM (behind a 16 KB D-cache), 288 KB D2 SRAM | ST RM0433; Resilio `firmware/main.cpp` |
| Knobs | 7 (`KNOB_0`…`KNOB_6`), each with a CV jack, summed in hardware → one 0–1 value | libDaisy `daisy_versio.h` |
| Switches | 2 × 3-position (`SW_0`, `SW_1`) | same |
| Button | 1 momentary | same |
| Gate in | 1, digital (~+2 V), no jack detection: unpatched reads low | same; NE manuals |
| LEDs | 4 × RGB | same |
| Audio | stereo in / out. **In L is normalled to In R in hardware**, so mono on L needs no firmware logic | NE Versio manuals; Resilio SPEC §3 |

Flashing: NE Firmware Swap web app, "Select Custom File" (Resilio ADR 0011). That means a standard libDaisy app in internal flash, no bootloader.

---

## 2. Sound target

### 2.1 Core characteristics
- **Warm and lush, never metallic.** Reference dislikes: Make Noise Erbe-Verb, Plexophone.
- **Sustains near-infinitely without running away.** At infinite, the tail evolves to a warm floor and holds there (ADR 0008).
- **Massive mode has mass:** a gentle built-in swell in NORMAL (or a timed BLOOM) that starts with the source, and SIZE from a long wash to a boundless one (§4.5, §4.11; v1.0.6).
- **Era is a journey back in time**, from clean to Neve, cassette, μ-law and Midiverb. The layers stack (§4.7).
- **Its own instrument:** not a Dattorro plate with distortion (Plateau Versio), and no Desmodus-style "scream".

### 2.2 Inspirations → what we take
Valhalla Supermassive (Sagittarius, Andromeda, Large Magellanic Cloud: swell, mass, echoes → wash), Meris Mercury7 Cathedra (in-tank pitch drift), Eventide Blackhole (shaping the envelope), Vongon Ultrasheer (vintage bits, warble), Lexicon 224 / Griesinger (modulated delays against metallic ringing), Alesis Midiverb (12-bit, ~10 kHz, ~23.4 kHz rate), the dub mixing desk send/return (EQ and drive in the loop, played like an instrument: Tone and the saturation), Neve (even-harmonic warmth), Portastudio (compression, head bump, hiss, wobble). Details and sources: `IMMENSIO_BRIEF.md` §2 and `docs/spec-session.md` "[VERIFY] results".

### 2.3 Design principles
1. Every tank delay is modulated (the anti-metallic floor), delay lengths are mutually prime, and damping sits in the loop.
2. In-loop gain is ≤ 1 at every frequency for every setting. Infinite means exactly 1 at the floor, held there by the soft saturation. No setting adds energy.
3. Bit loss truncates toward zero, so a fading tail always reaches silence or its floor and never holds a stuck buzz (a limit cycle). This follows Resilio's `EchoBits`.
4. Every control move is smooth: no zipper, no clicks. Size moves bend in pitch on purpose (ADR 0012); nothing else does.
5. Sound work happens on the desktop first (Renderer, then Plugin). Hardware is for profiling and final feel.
6. Deterministic: every random source is seeded, so the same input and params give bit-identical output.

---

## 3. Panel map

### Knobs (all CV-able, 0–5 V + pot offset)

Pots are named by panel position P1–P7, in reading order, as in Resilio ADR 0028. Layout (ADR 0017): it keeps Resilio's muscle memory under the same printed panel. P1 MIX (= BLEND), P2 DECAY, P3 TONE, P4 ERA (where SPLASH was), P5 SIZE (where TENSION, "which tank", was), P6 DRIFT (where the bipolar WOBBLE was), P7 TIME (where DRIVE was).

| Pot | Name | Function | Range and curve *(start)* | Smoothing |
|---|---|---|---|---|
| P1 | **MIX** | Dry/wet, equal-power (ADR 0012) | θ = x·π/2; dry = cos θ, wet = sin θ. Fully CCW = clean passthrough (dry untouched, even with Era up: the μ-law box is wet-only); fully CW = 100 % wet | snappy 5 ms |
| P2 | **DECAY** | Tail length; past ~3 o'clock, infinite plus ducking (ADR 0008, 0014); keeps the loudness (ADR 0020); in Massive NORMAL it also shortens the built-in mass (ADR 0007) | 0–0.80: T60 exponential. Plate 0.4 → 12 s, Hall 0.8 → 25 s, Massive 2 → 120 s. 0.80–0.85: crossover to infinite. 0.85–1: infinite (loop gain 1 at the floor), duck depth grows 0 → full (§4.9) | gliding 80 ms |
| P3 | **TONE** | Tilt EQ in the loop: the tail darkens or brightens with each pass, plus a matching tilt on the wet (v1.0.8: CCW +4 dB lows / −9 dB highs, CW −4 / +6 dB round 600 Hz). Noon is flat (ADR 0012) | Pivot ~800 Hz. Per pass, gain at 10 kHz relative to 100 Hz: −6 dB (CCW) → 0 (noon) → +2 dB (CW). The loop gain is renormalised so that "bright" never exceeds 1 at any frequency. Noon dead zone ±2 % | snappy 5 ms |
| P4 | **ERA** | Clean → Neve → Cassette → μ-law → Midiverb, stacking (ADR 0003, 0004, 0005) | Travel weighted to the middle: 0–10 / 10–35 / 35–65 / 65–85 / 85–100 %. Each layer fades in across its stretch, then stays. §4.7 | gliding 60 ms (rate changes crossfade) |
| P5 | **SIZE** | The space. Plate / Hall: scale. Massive: long wash → boundless wash, predelay grows a little (ADR 0007, v1.0.6). Moves bend pitch like tape (ADR 0012). While frozen it plays the companion | Plate: tank scale 0.5 → 1.59 (v1.0.11). Hall: mean line delay 35 → 260 ms, finite T60 × 0.49 → 1 (v1.0.8, v1.0.17). Massive: mean line delay 0.35 → 2 s, exponential; delay-ratio spread (Warp) 0.4 → 1; in-loop and input diffusion 0.45 → 1 (×); predelay 0 → 100 ms (∝ x²) (v1.0.6). Slew-limited to ≤ ±1 semitone of bend (≤ 6 % delay change per second); a fast move becomes a swoop over up to ~1.5 s | slew-limited (§4.3) |
| P6 | **DRIFT** | Bipolar pitch movement in the tank (ADR 0012) | Dead zone 0.47–0.53, which leaves only the anti-metallic floor (±1–2 cents per line, slow random). Left: tape wow 0.2–1 Hz up to ±15 cents, flutter 5–9 Hz up to ±3 cents, random and never repeating. Right: Wander (v1.0.20): quick, small random movement, 1 → 2.25 Hz up to ±20 cents at full, each line's rate fanned out ±50 % so they never sway together (Lexicon Random Hall, Valhalla Random Space). Below 300 Hz the tail stays still (v1.0.18) | gliding 80 ms |
| P7 | **TIME** | The attack's timing, per ATTACK (ADR 0015). NORMAL: predelay. BLOOM: swell time. REVERSE: reverse length | NORMAL: predelay 0 → 250 ms (∝ x²) *(start)*. BLOOM: swell time 0.1 → 4 s, exponential (shorter keeps a swell from clashing with the harmony coming in). REVERSE: segment length 0.2 → 2 s, exponential | gliding 80 ms (REVERSE length changes at the next segment) |

### Switches

| Switch | Left | Centre | Right |
|---|---|---|---|
| SW0 **SPACE** | **PLATE**: Dattorro-family plate, dense, quick attack | **HALL**: the FDN voiced as a big room, normal attack | **MASSIVE**: the FDN voiced with swell, mass and long delays |
| SW1 **ATTACK** | **NORMAL**: the sound enters the tank at once (Massive keeps a gentle built-in mass) | **BLOOM**: the tail swells in over TIME | **REVERSE**: classic reverse swell: each stretch of reverb plays backwards, rising to a cut; TIME sets the length |

Which toggle is SW0 and which is SW1 follows the printed panel (confirmed at M0).
Switch changes: SPACE crossfades ~100 ms; the old tail fades out while the new space takes the input, and only one big tank ever runs (ADR 0013). ATTACK crossfades ~50 ms on the wet; into or out of REVERSE, the forward and reversed wet crossfade over 50 ms.
Ducking has no switch: it is 4:1 compression of the wet at infinite DECAY (ADR 0014, amended v1.0.17).

### Audio I/O
True stereo in (ADR 0013): L and R enter the tank at different points. Mono on L reaches both sides via the hardware normal. Outputs are not normalled.

### Button + Gate (one gesture reader, ADR 0009–0011)
- **Tap** (press < 0.3 s) **latches the freeze.** The big tank becomes a still pad (ADR 0009); the input moves to the **twin**, a second tank of the same space whose sound is a quieter layer over the pad and fades back out (Size and Decay play it; v1.0.7). Tap again to unfreeze: the pad decays naturally, and the big tank glides to the current Size and Decay over ~1 s (ADR 0009).
- **Timing:** freeze engages **on the press** (no latency, so you catch the chord). While frozen, a press opens a 0.3 s window. Released inside it: unfreeze on release. Still held at 0.3 s: **paint**.
- **Hold to paint** (while frozen): a little of the twin (what is playing) blends into the pad over ~3 s *(start)*, at most ~1.5 dB, so the pad stays itself; on release the pad keeps the new layers. With no hold, the pad is untouched (v1.0.7).
- **Gate = the same gestures:** a rising edge is a press, a falling edge is a release. A short pulse toggles; a gate held over 0.3 s while frozen paints. Unpatched, the gate reads low and does nothing.
- Ramps *(start)*: input to the big tank closes over 20 ms on freeze; the in-loop colour glides out over 0.2 s; the twin's input opens over 20 ms.

### LEDs (ADR 0016)
Level meters as Resilio (ADR 0031): LED 1–2 In L / In R, LED 3–4 Out L / Out R, green → amber with level, red on clip or limiter. When frozen, the output pair turns cool blue under the meter; painting pulses it slowly (~1 Hz).

---

## 4. DSP architecture

### 4.1 Signal flow

```
 In L/R ─┬──────────────────────────────────────────────────────────────┐ dry
         │                                                              │
         ├─► input gain ─► envelope follower (duck, control rate) ──┐    │
         │                                                         │    │
         ├─(+ hiss, Era ≥ Cassette, ducked by input)               │    │
         ▼                                                         │    │
   [freeze router] ──normal──► predelay (NORMAL: TIME; Massive: + Size)   │
         │                       ─► input diffusion                 │    │
         │                       ─► swell stage (BLOOM: TIME; Massive NORMAL: built-in mass, §4.5)
         │                       ─► BIG TANK: Plate | Hall/Massive FDN    │
         │                            loop contains, per line / pass:     │
         │                             modulated delay (floor + DRIFT;    │
         │                               below 300 Hz a still twin tank,  │
         │                               v1.0.18)                         │
         │                             TONE tilt + damping                │
         │                             loop soft saturation (always on)   │
         │                             ERA in-loop layers (Neve, Cassette,│
         │                               Midiverb 24 kHz box + bits)      │
         │                             (ducking: 4:1 on the wet, §4.9) ◄──┘
         │                                   │
         │    diffused input ─► REVERSE: rising taps → short FDN (replaces the tank's wet, §4.11)
         │                                   │
         └─frozen──► TWIN tank (same space) ─(paint, a little)─┘ (into the frozen pad)
                          │
   tank out + twin out (−6 dB, v1.0.14) ─► ERA output layer (μ-law 24 kHz / 12→10-bit)
                          ─► output limiter ─► MIX (equal-power) ◄── dry
                          ─► Out L/R
```

### 4.2 Plate engine
- Dattorro-family figure-8 tank: input diffusers, 2 cross-coupled branches of modulated allpasses and delays, damping, multi-tap stereo output. Written from the 1997 paper; **no Plateau Versio code** (it is GPL-3.0).
- What makes it ours: every delay is modulated (floor + DRIFT), and the loop carries the full character chain (Tone, saturation, Era).
- **Twin (v1.0.7, replaces the companion):** while frozen, a second Plate (or FDN, per SPACE) of the same code takes the input; Size and Decay play it, its T60 at most 8 s; it has the knobs' ERA and DRIFT. SDRAM, like the big tank. In MASSIVE it has its own swell stage, set like the tank's NORMAL mass, so playing over a frozen MASSIVE arrives as a wash, not distinct repeats (v1.0.10).

### 4.3 FDN engine (Hall and Massive)
- **8 lines** *(start; 16 is a stretch goal, decided at M3)*. Mixed feedforward + feedback as Costello describes. Householder or Hadamard feedback matrix (fast 8-point Walsh transform, 24 add/sub).
- Delay lengths are mutually prime, spread by a Warp-style ratio control (Size in Massive). Each line has a modulated read: allpass interpolation (v1.0.2), floor modulation plus DRIFT.
- **Still lows (v1.0.18, ADR 0012 amendment):** the tank's input splits at 300 Hz (Butterworth, power complementary: the two tanks' outputs are uncorrelated and add in power). The lows go to a second 8-line FDN at 12 kHz (÷4, anti-aliased by the split and a 2 kHz low-pass) with **no modulation**, the same SIZE, DECAY, TONE, ERA (the Midiverb box keeps only its bits) and in-loop diffusion, and the main tank's governor (which hears both). So the bass never wavers while everything above moves (Schlecht: a modulated low end sounds detuned; Keith Barr's dual-band reverb, Eventide's H90 Hall: one reverb per band).
- Per-line loop gain is set from T60 and the line length (Jot), so Decay doesn't retune.
- **Size moves:** each line's length target is slew-limited (≤ 6 %/s, about ±1 semitone of bend), so the tail swoops like tape. The read interpolates; no crossfade.
- **Hall voicing:** mean delay 40–180 ms, moderate input diffusion, no built-in mass (BLOOM still applies), predelay 0–60 ms (TIME's predelay adds in NORMAL).
- Stereo: L and R inject into different lines (true stereo in); the outputs are taken from opposite-signed line sums.

### 4.4 Infinite and the floor (ADR 0008)
- Decay 0.85–1: the loop gain reaches 1 at the floor level. Tone, Era and the saturation stay in the loop, so the tail keeps darkening and graining. The always-on soft saturator plus a slow RMS loop-level governor (control rate) hold the tail at a warm floor *(start: −24 dBFS RMS)* so it never turns to mud or falls silent.
- **Freeze is different (ADR 0009):** the pad holds still. Gain is exactly 1, input is closed, the in-loop colour is bypassed (glided over 0.2 s), and only Drift remains (the modulation floor glides out too, v1.0.22).

### 4.5 Massive mode: mass, echoes → wash (ADR 0007)
- **Swell stage:** a feedforward stage between the input diffusion and the tank: 24 taps of one long delay per side, spread over 1.25 × A with rising gains (total energy 1), jittered and sign-scrambled per side (v1.0.1; was "4 long modulated delays and allpasses", which can't be steered to a chosen attack time). The energy reaches the loop gradually, so the attack is a slab, not a hit; the 10 → 90 % rise lands at A. Below A ≈ 40 ms it hands over to the direct path. **Shared by every space** (it is also BLOOM, §4.11).
- **ATTACK governs the attack in every space (ADR 0015).** In Massive, NORMAL keeps a gentle built-in mass: A = A₀(Decay) · m(Size), A₀ 0.5 → 0.2 s across Decay (log), m 1 → 1.5 across Size *(start)*. BLOOM replaces it with TIME's swell, still × m(Size), so Size adds a little mass. Longer swells are BLOOM's, where TIME sets them.
- **Size (v1.0.6; was echoes → wash):** low Size gives shorter lines (mean 0.35 s, spread 0.4) with diffusion 0.45: already a wash, no rhythmic echoes (the owner, M4). Turning Size up lengthens and fans out the lines and raises the diffusion, input and in-loop (two allpasses per line, 24–91 ms, coefficient 0.62 × diffusion), into a boundless wash, with predelay up to 100 ms. Three **early taps** per line feed the output so the wet starts within ~0.35 s of the source at any Size and swells from there.
- **Andromeda-scale tails stay tame** through §4.4: in-loop damping, soft saturation and the floor governor.

### 4.6 Loop character chain (per pass)
Order inside each line or branch *(start)*: modulated delay read → TONE tilt (one-pole shelf pair) + fixed damping → soft saturation (cheap polynomial or rational soft clip, no tanh, no oversampling in the loop) → ERA in-loop layers → DARKEN duck. The dub-desk inspiration lives on through Tone, the saturation and Era; there is no resonant Desk EQ (ADR 0015).

### 4.7 Era (ADR 0003, 0004, 0005)
Stacking: each stage's layer fades in across its stretch of the knob and then stays.

| Stage (layer added) | Bandwidth | Bits / companding | Saturation | Noise | Wow / flutter | Where |
|---|---|---|---|---|---|---|
| Clean (0–10 %) | full | float | always-on loop soft-clip only | none | none (Drift only) | – |
| + Neve (10–35 %) | loop LP 20 → 16 kHz; +1.5 dB shelf at 80 Hz | float | even-harmonic, asymmetric; 2nd harmonic ~−30 dB at nominal | none | none | in loop |
| + Cassette (35–65 %) | 16 → 9 kHz; head bump +2 dB at ~70 Hz | float; slow 2:1 soft compression above −18 dBFS (gain at control rate) | tape curve on top | hiss into the tank input, up to ~−66 dBFS, ducked by input level (ADR 0005) | wow 0.3–0.8 Hz up to ±4 cents, flutter 6–8 Hz up to ±1 cent, added to DRIFT | in loop (hiss at tank input) |
| + μ-law (65–85 %) | unchanged | 24 kHz / 12-bit μ-law → 10-bit at 85 % | – | – | – | wet output (ADR 0004) |
| + Midiverb (85–100 %) | ~10 kHz ceiling via a 48 → 24 → 48 kHz box | 13-bit (the unit's; was 12 → 8 until v1.0.16) with its ½ one-pole every pass, truncation toward zero (rounding at infinite; no dither since v1.0.3) | – | sparse crackle, growing to the end | – | in loop (ADR 0004) |

- The Midiverb box uses polyphase IIR halfbands (Resilio `Oversampler`). Its images are deliberately not fully removed, for a little aliasing grit *(start: stopband −40 dB)*. It **saves no CPU**: the tank stays at 48 kHz, because a live tail's tank rate can't crossfade cleanly (ADR 0004).
- Midiverb character only (rate, bits, halving accumulator maths); no ROM microcode. emeb/MIDIVerb_RE is MIT and may inform it; thement/midiverb_emulator is GPL-3.0, reference only.

### 4.8 (removed) Desk
Removed before the freeze, after the owner's demo listening: the knob became TIME (ADR 0015).

### 4.9 Ducking: 4:1 compression of the wet (ADR 0014, amended v1.0.17)
- The input's ~10 ms power keys a 4:1 compressor on the wet (after ERA's μ-law layer and TONE's tilt, before the limiter): above −24 dBFS, attack 30 ms, release 0.4 s, at control rate with the gain ramped per sample. Its depth is 0 below Decay 0.85, then grows to full at Decay 1. Not while frozen (a freeze lets it go in 50 ms). No darkening. While you play the tail steps back about 3 dB, and it comes back over about a second when you stop. (Until v1.0.17 it was DARKEN, a low-pass closing to ~1.5 kHz plus a 3 dB dip.)

### 4.10 Output stage
Era's μ-law layer (wet only) → soft limiter (ceiling −1 dBFS, Resilio-style) → MIX. The dry path is untouched.

### 4.11 ATTACK and TIME (ADR 0015)
- **NORMAL:** TIME is predelay, 0 → 250 ms *(start)*. Massive adds its gentle built-in mass (§4.5) and Size's predelay.
- **BLOOM:** the swell stage (§4.5) is set so the tail's envelope rises over TIME, 0.1 → 4 s, in every space (× m(Size) in Massive). A short TIME keeps a swell from clashing with the harmony coming in (owner's demo note).
- **REVERSE (the hardware reverse reverb, v1.0.9):** each note blooms into a swell that starts with it, rises over TIME (0.2 → 2 s) and stops; the reverb's envelope is reversed, nothing plays backwards (AMS RMX16 / Lexicon / Midiverb reverse programs). The diffused input feeds a 2 s delay with 32 taps per side spread over TIME (jittered, signs per side), rising 30 dB first to last *(start)*, into a short FDN of its own (8 lines, mean 20 → 90 ms with SIZE, T60 0.15 → 1 s with DECAY: how sudden the stop is) *(start)*. Its output replaces the tank's wet (+15.5 dB, loudness-matched); the big tank keeps running underneath, so leaving REVERSE crossfades back to a live tail. The same in every SPACE (MASSIVE adds its predelay). While frozen, REVERSE is bypassed with a 50 ms crossfade.

---

## 5. Performance budget

48 kHz, block 48 *(start)*. 480 MHz ÷ 48 kHz ≈ **10,000 cycles/sample**. Target **≤ 75 % peak** worst case, **80 % ceiling** for a release that passes the click check by ear (Resilio's numbers, ADR 0030).

### 5.1 CPU per block *(desktop estimates, ×2 safety for M7 reality; verified at M3)*

| Block | Cycles/sample (est.) | Notes |
|---|---|---|
| Input gain, follower, hiss | ~40 | follower at control rate |
| Predelay + input diffusion (8 allpasses, stereo) | ~120 | diffusers in DTCM |
| Swell stage (BLOOM / Massive, 4 stages × 2 ch) | ~250 | SDRAM reads, sequential per line |
| REVERSE segmenter (stereo write + reversed read + edge fades) | ~40 | SDRAM |
| FDN 8 lines: modulated allpass-interpolated reads | ~350 | SDRAM; cost dominated by cache misses (to measure) |
| FDN matrix (8-pt Walsh) | ~30 | |
| Loop chain × 8 (tilt, damping, saturation) | ~250 | cheap soft clip, `Select.h` VSEL |
| ERA in loop (Neve/Cassette cheap; Midiverb box × 8) | ~350 | worst case, Era at full |
| Twin tank (when frozen) | ~ the big tank's | SDRAM; frozen pad + twin is the worst case (M3 measures it) |
| Output μ-law box (stereo) + limiter + mix | ~200 | Resilio `OutputBits` scale |
| Control-rate work (amortised) | ~150 | LFOs, coefficient redesigns, governor |
| **Total (Massive, Era full, BLOOM, frozen + painting)** | **~2,080 → ×2 ≈ 4,200 (≈ 42 %)** | leaves room; 16 lines decided at M3 |

Plate mode ~1,500 × 2 ≈ 30 %. Mitigations, if short: stagger coefficient redesigns across ticks (Resilio ADR 0030); `-ffp-contract=off`; Midiverb box on a subset of lines; block 96.

### 5.2 Memory placement

| Buffer | Size | Where | Why |
|---|---|---|---|
| FDN lines (8 × 2.05 s mono, incl. modulation headroom) | ~3.2 MB | SDRAM | too big for internal RAM; reads are sequential per line, so the D-cache helps |
| Swell stage (4 × 2 ch × 1 s) | ~1.5 MB | SDRAM | |
| Predelay (stereo, 0.75 s: Size + TIME) | ~0.3 MB | SDRAM | |
| REVERSE segments (2 × 2 s, stereo) | ~0.75 MB | SDRAM | |
| Plate engine (scale up to 1.59, stereo taps) | ~150 KB | AXI SRAM | random-ish taps; internal is faster than SDRAM |
| Twin tank (an FDN and a Plate, as the big tank) | ~3.5 MB | SDRAM | runs only while frozen (v1.0.7) |
| Input diffusers | ~20 KB | DTCM | hottest loop |
| Filter states, Era boxes, μ-law, LFOs | < 8 KB | DTCM | |
| Stack, HAL/USB buffers | as libDaisy | AXI SRAM | (Resilio moved its pool to DTCM and kept the stack in AXI) |

DTCM total ~85 KB of 128 KB. SDRAM total ~6 MB of 64 MB. **Unverified:** SDRAM random-read cost under modulation. Measured first at M3 with an on-chip microbenchmark, before features are stacked.

### 5.3 Flash budget (128 KB internal, the tightest limit)
Resilio ended at 97.7 %. Immensio tracks flash per milestone: **release ≤ 110 KB at v1.0** (≥ 18 KB free). From M0 it uses Resilio's techniques: no QSPI chip setup, no USB device stack in release, `-Os` on control-rate code (`IV_SIZE_OPT`), only shipped voicings compiled in, a size gate in every build. Two engines, not three (ADR 0006). The fallback if the budget can't be met, BOOT_SRAM with a custom linker script, loses the Firmware Swap install, so it is the **owner's call via a new ADR** (ADR 0002).

---

## 6. Architecture: one DSP core, three hosts
As Resilio §6: `core/` (params + dsp, no platform includes), `host/render` (CLI: WAV + JSON → WAV, automation, sweeps, metrics), `plugin/` (JUCE AU + VST3 test bench, params generated from `ParamSpec`; MIDI notes = the gate, so a note-on is a press and a note-off a release; the button is a momentary plugin control), `firmware/` (libDaisy `DaisyVersio`). Namespace `iv::`. Every host passes normalised 0–1 values; mappings live only in `core/params/`.

### 6.1 Seeded from Resilio Versio (copied, provenance in `NOTICE`; ADR 0001: new repo, no shared lib yet)
- **As-is:** `ParamSpec` pattern, `host/common` (Wav, Json, Fft, Spectrogram, Metrics, Automation, Sweep), the test runner and ctest wiring, the plugin shell + `plugin_host_test`, firmware `PotEndStops`, `LedMeter`, LED PWM, build variants (release / m0test / profile), `ProfileHook`, `Filters`, `Oversampler`, `Select`, `Seed`, `SizeOpt`, and the tools (stimulus, review pages, sweetspot, install and release scripts).
- **Refactored (maths kept, Resilio voicings dropped):** μ-law (`OutputBits` → the output layer, `EchoBits` → the Midiverb box), `Drive` / `EchoWear` (saturation, tape), `Wobble` (→ DRIFT).

### 6.2 Repo layout
```
immensio-versio/
  CMakeLists.txt
  core/params/   ParamSpec.h, Mappings.h, EraVoicing.h, SpaceVoicing.h, GestureReader.h
  core/dsp/      Plate.h/.cpp, Fdn.h/.cpp, Swell.h, Reverse.h, LoopChain.h, Era.h, Duck.h,
                 OutputBits.h, Oversampler.h, Filters.h, Select.h, Seed.h, SizeOpt.h, ProfileHook.h
  host/render, host/common, host/tests
  plugin/
  firmware/      Makefile (size gate), main.cpp, LedMeter.h, PotEndStops.h
  tools/  presets/sweeps/  test_audio/  docs/ (adr, handoff, TASKS.md)
```

---

## 7. Build milestones and test plan

**[A]** automated (Renderer / tests), **[L]** owner listening check, **[H]** on hardware. Shared definitions are as in Resilio: the stimulus set (`tools/make_stimulus.py`, plus held chords, pads and dub stabs), review pages, and the click detector (*start: 20 dB above the local HF level*).

### M0 — Toolchains and seed
- [A] The seeded repo builds: Core + Renderer + tests + Plugin (AU/VST3) + firmware. Passthrough is bit-identical. Firmware ≤ 128 KB, with the size reported.
- [H] Test firmware via Firmware Swap: passthrough within 0.5 dB, every control read correctly, the P1–P7 → libDaisy table and switch orientation confirmed.

### M1 — FDN Hall, clean
- [A] Stable at every Decay × Size corner: no NaN/Inf, no growth, and loop gain ≤ 1 at every frequency (magnitude-response test).
- [A] T60 within 10 % of the mapping across Decay (Plate/Hall/Massive tables, §3).
- [A] **Metallic check:** no spectral peak in the tail more than 6 dB *(start)* over its 1/3-octave neighbourhood (Resilio's resonance-ratio metric).
- [A] Deterministic (bit-identical reruns). Size sweep: no click-detector hits, bend ≤ ±1 semitone.
- [L] "Warm, lush, not metallic" on the stimulus set, compared with Supermassive / Cathedra recordings (never committed).

### M2 — Plugin shell
- [A] Params generated from `ParamSpec`. Plugin output equals Renderer output (≤ −120 dBFS). Works at 44.1/48/96 kHz (T60 within 5 %). Latency reported. MIDI note = gate, sample-accurate.

### M3 — Hardware profiling (early, before features stack)
- [H] FDN + diffusers on the chip: CPU average/peak per corner; an SDRAM read microbenchmark (sequential vs modulated). ADR: 8 or 16 lines, block size, memory placement confirmed.

### M4 — Massive mode + ATTACK
- [A] Attack: the 10 → 90 % rise of the envelope on a burst matches the target within 20 %: NORMAL in Massive across Decay × Size (A₀·m); BLOOM across TIME in every space.
- [A] REVERSE: each segment's envelope rises to its end, then cuts; no click-detector hits at segment edges; wet latency = TIME (±1 block), dry not delayed; frozen bypass is click-free.
- [L] BLOOM at short TIME doesn't smear the harmony of a changing chord; REVERSE sounds like a classic reverse reverb.
- [A] Echoes → wash: at Size 0, the autocorrelation shows distinct repeats; at Size 1, echo density exceeds a threshold *(start)* within 300 ms.
- [A] Infinite: a 10-minute render at Decay 1 holds the floor (RMS within ±3 dB of the target, no growth, no NaN), in every Era stage.
- [L] CygnusX1 / DarkMatter / BensonArizona / Ice Floe "family": a slab with mass that swells in (NORMAL's gentle mass, and BLOOM for the slow Sagittarius / Andromeda swells).

### M5 — Plate + twin + freeze/paint
- [A] Gesture reader (button and gate share tests): tap < 0.3 s toggles, freeze engages on press, a hold ≥ 0.3 s while frozen paints, pulses from a sequencer, bounces.
- [A] Freeze: the pad's level stays within ±0.5 dB over 5 min (from 25 s after the freeze, v1.0.2) with in-loop colour bypassed, and its pitch is stable (pitch tracker). Unfreeze glide has no clicks.
- [A] SPACE flips mid-tail: no clicks, and the loudness of Plate/Hall/Massive is within ±1.5 dB.
- [L] Playing over the pad feels like playing into a small room, not dry.

### M6 — Character chain: Tone, Drift, Era
- [A] Era: each stage's measured bandwidth, bit depth (noise floor vs signal) and THD match §4.7 within tolerance. Grain grows as the tail fades (SNR falls with level). No limit cycle (a tail reaches silence or its floor). Loudness on music across Era within ±1 dB (LUFS on the dub skank, v1.0.19).
- [A] Aliasing check (HF sine sweeps) passes at Era's top and with the saturation hit hard.
- [A] Drift dead zone exact (noon = floor only). Hiss is ducked to below −90 dBFS with no input, in a 2-minute silent render.
- [L] Era travel feels like going back in time. Blind pick of each stage at its centre.

### M7 — Ducking + integration
- [A] Duck depth is 0 below Decay 0.85, then rises monotonically; the wet steps back while playing without darkening and recovers after the input stops (v1.0.17).
- [A] Every knob at every corner: clicks, NaN, clip, CPU (on hardware).
- [H] Full CPU corner run ≤ 75 % peak; flash ≤ 110 KB release; click check by ear.

### M8 — Tuning pass
- [L] `sweetspot.py` per knob (dead zones, cliffs); owner tuning sessions against the references; starting guesses replaced by tuned numbers in the spec.

### M9 — Polish and release
- Manual, panel SVG, release notes, `make_release.sh`, licences (`NOTICE`: libDaisy, STM32 HAL, CMSIS, JUCE, VST3 SDK; our code MIT).

---

## 8. Flashing / distribution
As Resilio §8: NE Firmware Swap → Select Custom File → `immensio_versio.bin`. Never connect USB and rack power at once. Stock firmware is restorable through the same app.

---

## 9. Risks

| Risk | Mitigation |
|---|---|
| **Flash over 128 KB** (Resilio hit 97.7 %) | budget per milestone; two engines; Resilio's size techniques from M0; BOOT_SRAM only by owner ADR |
| SDRAM read cost under modulation | microbenchmark at M3, before features stack; swell stage and lines can trade length for speed |
| Metallic ringing | modulation floor on every delay, prime lengths, metallic metric from M1 |
| Infinite runaway or mud | gain ≤ 1 rule, soft saturation, floor governor, 10-minute renders |
| Era stages sound like separate boxes | stacking, plus the travel weighting and blind tests at M6 |
| Midiverb box aliasing ugly rather than charming | stopband is a tuning number; owner listening at M6 |
| Size bend feels seasick | slew limit is a tuning number; noon-ish default |
| Hiss builds up in infinite tails | hiss ducked by the input level; silent-render test |
| Plugin ≠ hardware | shared `ParamSpec`, validate at 48 kHz, Renderer = Plugin test |
| Licence contamination | no Plateau (GPL) or unlicensed code; Midiverb character only |

---

## 10. Open questions
- None at the freeze. Every *(start)* number is tuned at its milestone; SW0/SW1 orientation is confirmed at M0 (ADR 0017). Demos behind ADR 0014/0015: `renders/p7_sw1_demos/` (sketch in `docs/prototypes/p7-sw1-demos/`).

---

## 11. References
See `IMMENSIO_BRIEF.md` §8, plus:
- Valhalla, "ValhallaSupermassive updated to 1.1.1" (Large Magellanic Cloud, Triangulum), 29 Sep 2020.
- Valhalla, "Tips and Tricks: Infinite Supermassive", 19 May 2021.
- emeb/MIDIVerb_RE (MIT); thement/midiverb_emulator (GPL-3.0, reference only).
- Resilio Versio repo: SPEC.md, ADRs 0011, 0028, 0030, 0031, 0039, 0042.
