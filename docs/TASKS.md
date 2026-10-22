# Owner tasks

**Last updated:** 23 Oct 2026, session 3 (every A/B pick acted on; nothing waiting for a pick).
**Plugin installed:** none yet (M2 installs the first build with `tools/install_plugin.sh`).

## To do

### Decide
- [ ] **BLOOM on percussion** (`renders/swell_smear/index.html`, picks; the DSP audit's S1): a hit through a long BLOOM comes back as a rising train of ghost hits (measured: the swell flickers 3–7.5 dB). B smears each of the swell's taps across the gap to the next. A today or B?
- [ ] **The PLATE's stereo image** (`renders/plate_stereo/index.html`, picks; the audit's P1): play hard left and the plate first answers only on the right. B answers on the side you played. A (a ping-pong) or B?
- [ ] **Repeated hits in the PLATE** (`renders/diffuser_motion/index.html`, picks; Mutable Instruments idea): identical hits enter the plate identically. B lets the first input allpass wander very slightly. Subtle; A or B?
- [ ] **Ducking at infinite DECAY** (`renders/ducking/index.html`, picks, not level-matched; your M1 notes): A today (darkens and dips), B none, C/D/E 4:1 compression with no darkening (quick 10 ms / 0.15 s, smooth 30 ms / 0.4 s, slow 50 ms / 1 s), in HALL, MASSIVE and PLATE on the new material. Measured: any ducking makes the tail rise when you stop; E pumps least (2.1–3.5 dB) with the smallest step.
- [ ] **The small HALL's build-up** (`renders/small_hall/index.html`, picks; your M1 notes): at long and infinite DECAY, A today, B the smallest SIZE raised to 35 ms, C/D in-loop diffusion (a little / more), E both. Measured on noise: B rings least at long DECAY, the diffusion least at infinite. Your M1 TONE note ("sounds good") is logged.
- [ ] **The held-tail makeup** (`renders/makeup/index.html`, picks, not level-matched): with it, an infinite tail holds where it settled; without it, with tape wobble and the cassette, MASSIVE sags about 3 dB over 10 minutes and HALL about 2.4 dB (frozen pads hold either way). It's the one place the loop may go a hair over 1. Each card is 20 s at minute 1 and at minute 10. Keep it (A) or drop it (B)?
- [ ] **MASSIVE's SIZE glide** (`renders/size_glide/index.html`, picks): SIZE turned quickly up, then down. A today (a semitone of bend, a full sweep ~40 s), B about 3 semitones (~13 s), C about 6 (~6 s), MASSIVE only.




### Listen (after your notes)
- [ ] **M1: the Hall** (`renders/m1_hall/index.html`): SIZE now goes from a small room (shorter lines, and shorter tails, as a small room has) to a huge hall. Is small small enough, and has the buzz on long small rooms gone? TONE's ends go further (dark: more lows, fewer highs; bright: the opposite). Too much, too little?
- [ ] **M4: MASSIVE and ATTACK** (`renders/m4_massive/index.html`): MASSIVE now starts with what you play and swells from there (it was silent for 1.2 s), and SIZE fully CCW is a wash instead of rhythmic echoes. Does MASSIVE feel connected to the source now? Is the smallest SIZE still big enough?
- [ ] **M5: PLATE, freeze and paint** (`renders/m5_plate_freeze/index.html`): also, do you still hear an upper harmonic building in a frozen MASSIVE, or two repeats on the skank into a frozen PLATE? Neither measures in the current build (both notes came from the page before the twin), so tell me where if you do. does the PLATE sound like ours, not a stock Dattorro? Is your playing over a frozen pad a subtle layer that fades back out (the twin)? Does paint now add just a little?

### Listen (first time)
- [ ] **M6: ERA, DRIFT** (`renders/m6_character/index.html`): does ERA feel like going back in time, stage by stage? Are DRIFT's tape side and Cathedra side what you hear in your head? All numbers are starting guesses for you to tune.
- [ ] **M7: DARKEN** (`renders/m7_duck/index.html`): at infinite DECAY the tail darkens and dips while you play and brightens back when you stop (it now lets go after 0.15 s). Too much, too little, too slow?

### At the rack
- [ ] **M0 hardware check** (`docs/m0-hardware-check.md`): flash `firmware/build/immensio_versio_m0test.bin`, check the boot pattern, switches, button (USB session), then knobs, gate and a quick passthrough listen (rack session). Tell Claude the results.
- [ ] **M3 CPU check** (after M0): flash `firmware/build/immensio_versio_profile.bin` and send Claude the serial CPU report, so the CPU budget is measured, not estimated. Include a frozen MASSIVE: the twin and MASSIVE's early taps are the heaviest case.

### When convenient
- [ ] **Reference recordings** (when convenient): the step-by-step is in `docs/reference-recording-guide.md`. Play the kit (`test_audio/reference_kit/`: calibrate, measure, music) through each unit, 100 % wet, at matched decays (about 2 / 5 / 15 s, plus infinite). Knob-only units get one 2-minute calibration take first; I'll tell you the positions. Start with the **H9 Reverse** (the yardstick for REVERSE), then Supermassive, Mercury7 Cathedra, Valhalla Plate and VintageVerb.

## Waiting on Claude

- **Resilio to Ombra Audio** (its own repo): same maker name and code. Its plugin is already in Ableton under Rslo, so changing the code makes Ableton see a new plugin; saved sets that use it need a check first.
- **Make the grainy reading steadier:** MASSIVE's density sits right at the 0.9 line from the start, so its "smooth after" jumps between 0.06 and 1.67 s on a one-sample change in the click; the PLATE and HALL readings move by 0.1–0.2 s. Research how others report it (a lower bar, a time average, or a mixing-time estimate) before trusting the known-misses list.
- **REVERSE tuning**, only if you have notes as you play it (shape, stop, grain); its page was deleted once confirmed (`tools/render_review_pages.sh reverse` brings it back).
- **M8 tuning** from your listening notes on the pages above, including MASSIVE medium and large sounding a little uneven on Hits (grainy_start).
- **M8 findings to confirm by ear first** (`docs/sweetspot.md`, `docs/research/reverb-testing-practices.md`):
  - ERA's last stretch (the Midiverb stage) jumps about 20× harder than the rest of the knob, and its first 20 % barely changes anything measurable.
  - PLATE: TONE's first step from fully CCW jumps, and so does SIZE's last.
  - MASSIVE: DECAY's first step jumps; DRIFT is nearly flat from 20–50 %; TIME in NORMAL barely moves anything for its first 30 %.
- **The rest of the Mutable Instruments ideas and the DSP audit's findings:** MI #2 is on a page; #3 and #4 aren't worth one (`docs/research/mutable-instruments-reverbs.md`); #5 and the audit's remaining findings are still to try.

## Done

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
