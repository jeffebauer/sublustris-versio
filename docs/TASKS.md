# Owner tasks

**Last updated:** 22 Oct 2026 (REVERSE rebuilt the hardware way, on `renders/reverse`. Every page is rendered from the same build by `tools/render_review_pages.sh`).
**Plugin installed:** none yet (M2 installs the first build with `tools/install_plugin.sh`).

## To do

### Decide
- [ ] **Listen: REVERSE, the hardware way** (`renders/reverse/index.html`): rebuilt the established way (RMX16/Lexicon reverse programs), as you picked: each note blooms into a swell over TIME, then stops. Is this the sound? Is the swell the right shape, and is the stop sudden enough (DECAY) and the grain right (SIZE)?
- [ ] **Approve or reject (ADR 0008 amendment):** infinite tails and frozen pads get a small, capped "makeup" that lifts a sagging held tail back to its own level (never above, ≤ 0.3 dB/s). Without it, infinite tails with tape wow fade out over ~10 minutes. It's the one place the loop gain may go a hair over 1.
- [ ] **MASSIVE's SIZE glide:** SIZE moves bend at most a semitone (ADR 0012), so a full SIZE sweep in MASSIVE glides for ~40 s. Keep that gentle bend, or allow a faster swoop with more bend in MASSIVE only?
- [ ] **Plugin identity** (one-line answers): keep manufacturer code `Rslo` and company name / bundle-ID prefix as Resilio's, with plugin code `ImVs`? (Same developer, so it groups both plugins together in Ableton.)

### Listen (after your notes)
- [ ] **M1: the Hall** (`renders/m1_hall/index.html`): SIZE now goes from a small room (shorter lines, and shorter tails, as a small room has) to a huge hall. Is small small enough, and has the buzz on long small rooms gone? TONE's ends go further (dark: more lows, fewer highs; bright: the opposite). Too much, too little?
- [ ] **M4: MASSIVE and ATTACK** (`renders/m4_massive/index.html`): MASSIVE now starts with what you play and swells from there (it was silent for 1.2 s), and SIZE fully CCW is a wash instead of rhythmic echoes. Does MASSIVE feel connected to the source now? Is the smallest SIZE still big enough?
- [ ] **M5: PLATE, freeze and paint** (`renders/m5_plate_freeze/index.html`): does the PLATE sound like ours, not a stock Dattorro? Is your playing over a frozen pad a subtle layer that fades back out (the twin)? Does paint now add just a little?

### Listen (first time)
- [ ] **M6: ERA, DRIFT** (`renders/m6_character/index.html`): does ERA feel like going back in time, stage by stage? Are DRIFT's tape side and Cathedra side what you hear in your head? All numbers are starting guesses for you to tune.
- [ ] **M7: DARKEN** (`renders/m7_duck/index.html`): at infinite DECAY the tail darkens and dips while you play and brightens back when you stop (it now lets go after 0.15 s). Too much, too little, too slow?

### At the rack
- [ ] **M0 hardware check** (`docs/m0-hardware-check.md`): flash `firmware/build/immensio_versio_m0test.bin`, check the boot pattern, switches, button (USB session), then knobs, gate and a quick passthrough listen (rack session). Tell Claude the results.
- [ ] **M3 CPU check** (after M0): flash `firmware/build/immensio_versio_profile.bin` and send Claude the serial CPU report, so the CPU budget is measured, not estimated. Include a frozen MASSIVE: the twin and MASSIVE's early taps are the heaviest case.

### When convenient
- [ ] **Reference recordings:** your Mercury7 Cathedra on a few stimulus files (`test_audio/stimulus/`), for M1/M4 listening comparisons. Never committed.

## Waiting on Claude

- **Frozen MASSIVE resonance and the PLATE's early repeats** (your freeze_options notes: an upper harmonic building in a frozen MASSIVE; two audible repeats on skank in PLATE before it diffuses): to measure and fix next.
- **REVERSE tuning** from your notes on `renders/reverse`.
- **M8 tuning** from your listening notes on the pages above.

## Done

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
