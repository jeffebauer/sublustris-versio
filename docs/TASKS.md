# Owner tasks

**Last updated:** 23 Oct 2026, session 4 (DRIFT clockwise page ready; ERA's level fixed on music, v1.0.19).
**Plugin installed:** none yet (M2 installs the first build with `tools/install_plugin.sh`).

## To do

### Decide
- [ ] **DRIFT's clockwise side** (`renders/drift_cw/index.html`, pick per card): today's slow drift (A) against the three characters you chose: Halo (a shimmering vibrato on what goes in; the tail stays in tune), Orbit (the tail circles around you, in space, not pitch) and Wander (quicker, small random movement). E is the tape side, only for comparison. All three are checked safe (nothing builds up, 10-minute holds pass). Measured wobble at full, HALL: today 64 cents, Halo 20, Orbit 4, Wander 10 (tape 21). Orbit only works in HALL and MASSIVE; a PLATE version would need its own design.

### Listen (after your notes)
- Nothing open.

### Listen (first time)
- [ ] **Immensio beside Valhalla** (`renders/valhalla_compare/index.html`, notes only): your Room, Plate and Supermassive takes in the same cards as our HALL, PLATE and MASSIVE, each column at a matched decay (2, 5, 15 s). What do they do that we don't, and the other way round? The Compare with menu now plays each card's own piece of music from the take you choose, so it works on any page.
- Nothing open.

### At the rack
- [ ] **M0 hardware check** (`docs/m0-hardware-check.md`): flash `firmware/build/immensio_versio_m0test.bin`, check the boot pattern, switches, button (USB session), then knobs, gate and a quick passthrough listen (rack session). Tell Claude the results.
- [ ] **M3 CPU check** (after M0): flash `firmware/build/immensio_versio_profile.bin` and send Claude the serial CPU report, so the CPU budget is measured, not estimated. Include a frozen MASSIVE: the twin and MASSIVE's early taps are the heaviest case.

### When convenient
- [ ] **Reference recordings** (when convenient; `docs/reference-recording-guide.md`): Valhalla Room, Plate and Supermassive are in (I read them straight from your Ableton set; no need to export). Still to record: the **H9 Reverse** (the yardstick for REVERSE), Mercury7 Cathedra, VintageVerb, then the rest. For Supermassive, Andromeda at 0 % feedback rang only 0.5 s; about 5 % should give the 2 s target. LMC can't go short: even at 20 % it rings 26 s.

## Waiting on Claude

- **Resilio to Ombra Audio** (its own repo): same maker name and code. Its plugin is already in Ableton under Rslo, so changing the code makes Ableton see a new plugin; saved sets that use it need a check first.
- **REVERSE tuning**, only if you have notes as you play it (shape, stop, grain); its page was deleted once confirmed (`tools/render_review_pages.sh reverse` brings it back).
- **M8 tuning** from your listening notes on the pages above, including MASSIVE medium and large sounding a little uneven on Hits (grainy_start; the new grain figure says it's smooth there, so measure the level of the swell and taps).
- **M8 findings to confirm by ear first** (`docs/sweetspot.md`, `docs/research/reverb-testing-practices.md`):
  - ERA: its first 20 % barely changes anything measurable; the Midiverb stretch jumped 20× harder than the rest, measured before its rebuild (v1.0.16), so re-measure with `.venv/bin/python tools/sweetspot.py`.
  - PLATE: TONE's first step from fully CCW jumps, and so does SIZE's last.
  - MASSIVE: DECAY's first step jumps; DRIFT is nearly flat from 20–50 %; TIME in NORMAL barely moves anything for its first 30 %.
- **The rest of the Mutable Instruments ideas and the DSP audit's findings:** MI #1 and #2 are in, #3 and #4 aren't worth a page (`docs/research/mutable-instruments-reverbs.md`); still to try: MI #5 (input trim with DECAY), and the audit's A3 (freeze keeps modulating), P2–P5, E2–E6, R1–R2 (`docs/research/dsp-audit.md`).

## Done

- 23 Oct 2026: **Your Valhalla takes are on a page** (`renders/valhalla_compare`), at matched decays. Our DECAY for 2 / 5 / 15 s: HALL 0.30 / 0.51 / 0.77 (SIZE 0.6), PLATE 0.40 / 0.62 / 0.82 (SIZE 0.6), MASSIVE 0.07 / 0.20 / 0.41 (SIZE 0.7; its shortest is 2.2 s).
- 23 Oct 2026: **Your Valhalla recordings read from the Ableton set** (`Immersio Versio Testing.als`): the 28 named takes (Room: Large Room and Dark Chamber; Plate: Chrome and Lithium; each at 2 / 5 / 15 s; Supermassive Sagittarius and Andromeda, measure and music at four feedbacks) plus your four calibration exports. Supermassive's feedback hit the targets closely: Sagittarius 7 / 30 / 73 / 100 % rang 2 / 5 / 15 / 52 s, Andromeda 12 / 54 / 100 % rang 5 / 15 / 45 s. The six unnamed "ValhallaSuperMassive" clips were left out (they look like earlier tries).
- 23 Oct 2026: **ERA keeps its loudness on music** (your note: the Midiverb could sound louder; SPEC v1.0.19): measured, music got 4–6 LU louder across ERA, because each stage's level boost had been set on a noise-burst test that music doesn't behave like. The boosts are now set on music: ERA fully CW is within 0.5 LU of clean in HALL and MASSIVE, every stage in between too, and the test now listens to the dub skank.
- 23 Oct 2026: **M6 listened** (ERA, DRIFT, TONE): "Tone sounds great"; tape and still "sound great". DRIFT fully CW is too seasick and the Midiverb can sound louder than the other stages (both measured, now on Claude's list).
- 23 Oct 2026: **Still lows shipped** (B on all 25 cards, the repeated cards agreeing; SPEC v1.0.18, ADR 0012 amendment): in HALL and MASSIVE the bass below ~300 Hz rings in its own still reverb, so it never wavers, whatever DRIFT does; everything above moves as before. Two side effects, both small: with ERA at the Midiverb end, short tails end 10–20 % sooner (its make-up went up 0.3 dB to keep the level), and the layer over a frozen pad (the twin) still moves its lows. The page is deleted.
- 23 Oct 2026: **Still lows rebuilt safely, for your re-listen** (`docs/research/still-lows.md`). Splitting each line in two inside the loop can't be made safe: the steadier splits stop the build-up but let infinite holds drain and the band around the split die early. The lows get their own still reverb instead (two tanks), which measures like today in every band, holds 10 minutes everywhere and keeps a 55 Hz bass within ~10 cents (HALL with DRIFT CW: 190 today).
- 23 Oct 2026: **MASSIVE's SIZE glide stays a semitone** (A on all 4: "the most smooth and cohesive"; 3 semitones made "odd stepped harmonies", 6 "staticky brick walls").
- 23 Oct 2026: **M1, M4 and M5 re-listened**: "sounding much better" (HALL; TONE "sounds good"), "much better" (MASSIVE and ATTACK), "excellent" (PLATE, freeze and paint).
- 23 Oct 2026: **Ducking is now 4:1 compression** (D on all 18; ADR 0014 amended): attack 30 ms, release 0.4 s, about 3 dB while you play at infinite DECAY, no darkening; DARKEN is gone. A freeze lets it go in 50 ms, so a fresh pad doesn't swell up.
- 23 Oct 2026: **The small HALL** (E on all 10): smallest SIZE 35 ms with in-loop diffusion at the small end; its ringing at long DECAYs dropped (12.7 → 7.3 dB, infinite 10.2 → 0). Raising the minimum squeezes HALL's whole range, so SIZE noon is now a slightly bigger room (where 0.61 was: 95 ms instead of 72, tails ~10 % longer). Worth a listen on the next HALL page.
- 23 Oct 2026: **The held-tail makeup approved** (A on all 4; ADR 0008).
- 23 Oct 2026: **ERA's Midiverb rebuilt as the real unit** (B on all 15): 13 bits and its dark loop instead of an 8-bit bitcrusher; level across ERA's last stretch kept within about 1.4 dB (make-up 1.2 → 1.0 dB).
- 23 Oct 2026: **BLOOM smears its taps** (B on all 15): no more ghost hits. Your note "the attack is a little slower" measured true (the smear adds delay); the taps now close up so the swell still peaks on time, but its very start (first 10 %) still arrives 0.06–0.4 s later. Worth a re-listen on the next BLOOM page. BLOOM only, not MASSIVE's mass.
- 23 Oct 2026: **PLATE's repeated hits** (B on all 4): its first input allpass wanders very slightly, so identical hits don't enter the plate identically.
- 23 Oct 2026: **PLATE's stereo image stays as it is** (A on all 8: "more cohesive"; answering on the same side "creates a sonic gap").
- 23 Oct 2026: **Bass in a moving tail: you liked B ("the frequency response feels more even and balanced"), but it isn't safe yet, so nothing changed.** Your HALL tape note ("waves of built-up distortion") measured as energy building in the loop: the still and moving reads add up to +3 dB a pass where they cross (~400 Hz), up to +12.7 dB in a small HALL. Made MASSIVE's default, it broke the 10-minute infinite hold and shortened MASSIVE's 1 kHz decay (22 → 17 s), so it's reverted. Part of the "even balance" you heard may have been that side effect. A stable design is on Claude's list.
- 23 Oct 2026: **Steadier grainy reading merged** (your OK): the new "grain" figure moves by at most ~0.01 s where the old one swung 1.6 s. Only the PLATE at full SIZE and MASSIVE at its smallest SIZE start a little grainy (0.12–0.15 s); the MASSIVE Hits unevenness you heard measures as level, not grain. Pages show "grain" in the Numbers and flag it above 0.1 s.
- 23 Oct 2026: **Brand: Ombra Audio.** Immensio's plugin now shows as "Ombra Audio: Immensio Versio" (maker code Ombr, plugin code ImVs, bundle com.OmbraAudio.ImmensioVersio). Fixed on the way: the install and release scripts still used Resilio's plugin code (RsVs) and linked Resilio's GitHub. Resilio moves to Ombra Audio in its own repo.
- 23 Oct 2026: **TONE, a warmer noon: B** (all 36 cards, the repeated row agreeing). Every tank's loss is now a shelf at 2.5 kHz, so the upper mids fade with the highs (ADR 0012 amendment). It also freed 4.4 KB of flash (release 92,948 B).
- 23 Oct 2026: **Big spaces don't start too grainy** (your listen on `grainy_start`): "overall it sounds good", apart from a couple of MASSIVE medium and large cards that sound a little uneven on Hits (kept for M8 tuning).
- 23 Oct 2026: **Playing over a frozen pad is louder** (C on all 24 cards; "the sweet spot is between plus3 and plus6"): the twin now sits 6 dB under the pad (+4.4 dB on before). The switch and its page are gone.
- 23 Oct 2026: **The main reverb's swell stays an even ramp** (A on 21 of 27 cards; MASSIVE 8 of 9). Only the frozen layer gets the S-curve. The page is deleted.
- 22 Oct 2026: **The layer over a frozen MASSIVE swells in on an S-curve** (B on all 18 cards): it eases in and settles into place instead of rising in an even ramp. The page is deleted.
- 23 Oct 2026: **PLATE, more movement: B** (all 9 small cards, 7 of 9 large). The PLATE's long delays now wander too. The frozen cards were a coin flip (4 to 4, and the repeated card disagreed), so a frozen PLATE stays as still as before. Your Skank note (a big drop after the first two repeats) measured as the freeze tap itself: from the tap on, chords go into the quieter twin (−10.5 dB), about 6–7 dB below the two before it, the same in A and B.
- 22 Oct 2026: **Your testing plan is built.** Pages now have a Tail button (T) with a +12 dB switch, a **Numbers** panel per card (decay per band, how soon the tail turns smooth, clarity, width), a grainy flag, and on pick pages a repeated row that catches coin-flip picks. Three new kinds of material: a low voice, a dry piano and a full mix. Underneath: a grainy test and a safety net that fails the build if an approved sound changes without a changelog line. The wobble meter waits for DRIFT tuning.
- 22 Oct 2026: **Playing over a frozen MASSIVE: B confirmed** (smooth, on all 18 cards). The twin swells like MASSIVE's tank; the A/B switch and its page are gone.
- 22 Oct 2026: **The PLATE's full-SIZE ring** (your OK): nothing you could hear. It only showed with the PLATE's movement switched off, at the knob's last notch. Full SIZE is now 0.6 % smaller, which removes it, and the test checks small, medium and full PLATEs.
- 22 Oct 2026: **Testing plan chosen**: yes to the grainy check, the safety net, decay per band, the hidden twin, the tail button and all the lab checks; the wobble meter later; low voice + dry piano and a full mix as new material (no long frozen pad).
- 22 Oct 2026: **MASSIVE keeps its swell** (A on 15 of 18 cards): without it, plucky sounds get two less-diffuse repeats before the wash; the swell smears them in. Recorded in ADR 0007; the prototype and its page are gone.
- 22 Oct 2026: **DSP audit merged** into main (`docs/research/dsp-audit.md`).
- 22 Oct 2026: **Testing practice researched and the quick wins adopted** (`docs/research/reverb-testing-practices.md`): pages level-match by loudness, not RMS; a new test checks HALL, MASSIVE and the small PLATE aren't metallic with their modulation off (Valhalla's rule); the sweet-spot check works again (`docs/sweetspot.md`).
- 22 Oct 2026: **Listening pages tidied**: the separate Dry version is gone where the MIX fader is (MIX fully left is the dry); the p7_sw1_demos and REVERSE pages deleted (decided); page audio stored as lossless FLAC (pages ~40 % smaller).
- 22 Oct 2026: **Reference recording kit and guide** (`docs/reference-recording-guide.md`): settings for all ten of your units, researched from their manuals (`docs/research/reference-devices.md`); the kit and the tool that measures and splits your recordings are tested on our own HALL.
- 22 Oct 2026: **Your freeze notes measured** (an upper harmonic building in a frozen MASSIVE; two repeats on the skank into a frozen PLATE): neither shows in the current build. A frozen pad's spectrum doesn't get peakier over a minute, and the PLATE's twin has no distinct echoes. Both notes predate the twin; re-listen on M5.
- 22 Oct 2026: **SPEC's stale "cubic interpolation" lines fixed** (it's been allpass interpolation since v1.0.2).
- 22 Oct 2026: **REVERSE confirmed**: "Reverse, the hardware way sounds much better."
- 22 Oct 2026: **More material on every page**: dub techno stabs, an ambient pad, a synth lead, a drum groove, a dub bassline, electric piano and a spoken voice, beside the skank and vibes (`tools/make_material.py`).
- 22 Oct 2026: **Mutable Instruments' Clouds and Beads reverbs analysed** (`docs/research/mutable-instruments-reverbs.md`): four ideas worth trying (more modulation in PLATE, the input diffusers and MASSIVE's loop; independent stereo diffusers). One is a gap in our own spec: the PLATE's long delays aren't modulated.
- 22 Oct 2026: **REVERSE rebuilt the established way** (your pick: the hardware kind). Starts with the note, swells over TIME, stops within ~0.25 s; no break, no chunks. The old reversed pieces are gone.
- 22 Oct 2026: **REVERSE notes** measured (the first reversed piece fades out into a 0.2 s hole, then jumps of up to 13 dB) and two new ways rendered for round 2.
- 21 Oct 2026: **M1 notes** acted on: HALL's SIZE now reads small to huge (20 → 260 ms, small rooms have shorter tails), the buzz on long small rooms gone (ringing score 43 → 26 dB), TONE's ends wider (dark +4 dB lows / −9 dB highs, bright the opposite). Fixed on the way: a click when freezing at infinite DECAY, and the limiter edging past −1 dBFS.
- 21 Oct 2026: **Freeze confirmed: the twin** (B on every card). It's the only behaviour now: quieter (−10.5 dB), fading back to the untouched pad, and painting adds ~1 dB (it was 7 dB). The old companion plate is gone (84 KB of fast memory freed).
- 21 Oct 2026: **M4 notes** acted on: MASSIVE starts 0.13–0.35 s after you play (was 1.17 s) and swells from there; SIZE 0's rhythmic echoes gone; REVERSE has a smooth option to pick.
- 21 Oct 2026: **Freeze options picked:** B (feed the pad) on every card; your notes led to the twin.
- 21 Oct 2026: **M5 notes** acted on: the PLATE's left-channel static at large SIZE (a bug), the seasick PLATE freeze, MASSIVE paint repeats, DARKEN dwelling between skanks.
- 21 Oct 2026: GitHub repo created (private): https://github.com/jeffebauer/immensio-versio.
- 21 Oct 2026: M7 DARKEN + limiter built (tests: ducking only from DECAY 0.85, not while frozen, darkens and brightens back, limiter holds −1 dBFS, MIX 0 still bit-exact, 40 random all-control runs with freezes and flips finite, bounded and click-free).
- 21 Oct 2026: M6 character built (tests: ERA darkens stage by stage, loudness within ±0.5 dB, μ-law grain on the wet only, Midiverb tails reach true silence, hiss gone in silence, infinite holds 10 min at every ERA, DRIFT dead zone exact and both sides click-free, no bad aliasing).
- 21 Oct 2026: M5 PLATE, freeze and paint built (tests: gestures from button and gate, PLATE T60 / not metallic / stable, all three SPACEs loudness-matched, pads hold 5 min within 0.5 dB, paint grows the pad boundedly, click-free freeze / paint / unfreeze, REVERSE rests while frozen).
- 21 Oct 2026: M4 MASSIVE + ATTACK built (tests: BLOOM rise within 20 % of TIME, MASSIVE's mass, infinite holds 10 min without growing, REVERSE rises to its cut, click-free SPACE flips, HALL/MASSIVE loudness matched).
- 21 Oct 2026: M1 Hall built (FDN, 8 lines; tests: stable at 45 corners, T60 within 2 %, not metallic, SIZE glides click-free, same at 44.1/48/96 kHz).
- 21 Oct 2026: spec session; P7 / SW1 demos (`renders/p7_sw1_demos/`); spec v1.0 approved and frozen; ADRs 0001–0017.
