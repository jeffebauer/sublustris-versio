# Owner tasks

**Last updated:** 21 Oct 2026 (M1, M4–M7 built; five listening pages ready, all rendered from the same build: `tools/render_review_pages.sh` remakes them).
**Plugin installed:** none yet (M2 installs the first build with `tools/install_plugin.sh`).

## To do

- [ ] **M0 hardware check** (`docs/m0-hardware-check.md`): flash `firmware/build/immensio_versio_m0test.bin`, check the boot pattern, switches, button (USB session), then knobs, gate and a quick passthrough listen (rack session). Tell Claude the results.
- [ ] **M1 listening: the Hall** (`renders/m1_hall/index.html`, open in a browser): warm and smooth, never metallic? Does SIZE go from big room to huge, and DECAY from short to infinite, the way you want? TONE dark / noon / bright. Notes or picks via "Copy results for Claude". It's the bare tank: no ERA colour, DRIFT or saturation yet.
- [ ] **M4 listening: MASSIVE and ATTACK** (`renders/m4_massive/index.html`): does MASSIVE's SIZE go from long repeating echoes to a wash the way you hoped (I set its range, ADR 0007 amendment)? Is BLOOM's timing control what you wanted, and is REVERSE the classic reverse you pictured? Infinite DECAY in both spaces.
- [ ] **M5 listening: PLATE, freeze and paint** (`renders/m5_plate_freeze/index.html`): does the PLATE sound like ours, not a stock Dattorro? Is playing over a frozen pad (through the companion) what you pictured, and does paint blend the way you want?
- [ ] **M6 listening: ERA, DRIFT, TONE** (`renders/m6_character/index.html`): does ERA feel like going back in time, stage by stage? Is DRIFT's tape side and Cathedra side what you hear in your head? TONE's bright end now lets the highs outlast the lows. All numbers are starting guesses for you to tune.
- [ ] **M7 listening: DARKEN** (`renders/m7_duck/index.html`): at infinite DECAY the tail darkens and dips while you play and brightens back when you stop. Too much, too little, too slow?
- [ ] **Approve or reject (ADR 0008 amendment):** infinite tails and frozen pads get a small, capped "makeup" that lifts a sagging held tail back to its own level (never above, ≤ 0.3 dB/s). Without it, infinite tails with tape wow fade out over ~10 minutes. It's the one place the loop gain may go a hair over 1.
- [ ] **One decision from M4:** SIZE moves bend at most a semitone (ADR 0012), so a full SIZE sweep in MASSIVE glides for ~40 s. Keep that gentle bend, or allow a faster swoop with more bend in MASSIVE only?
- [ ] **Plugin identity** (one-line answers): keep manufacturer code `Rslo` and company name / bundle-ID prefix as Resilio's, with plugin code `ImVs`? (Same developer, so it groups both plugins together in Ableton.)
- [ ] **Reference recordings** (when convenient): your Mercury7 Cathedra on a few stimulus files (`test_audio/stimulus/`), for M1/M4 listening comparisons. Never committed.

## Waiting on Claude

- Nothing blocking. Next for Claude once you've listened: M8 tuning passes from your notes. Next for you at the rack: the M0 check, then M3 (the profile build's CPU report) so the CPU budget is measured, not estimated.

## Done

- 21 Oct 2026: M7 DARKEN + limiter built (tests: ducking only from DECAY 0.85, not while frozen, darkens and brightens back, limiter holds −1 dBFS, MIX 0 still bit-exact, 40 random all-control runs with freezes and flips finite, bounded and click-free).
- 21 Oct 2026: M6 character built (tests: ERA darkens stage by stage, loudness within ±0.5 dB, μ-law grain on the wet only, Midiverb tails reach true silence, hiss gone in silence, infinite holds 10 min at every ERA, DRIFT dead zone exact and both sides click-free, no bad aliasing).
- 21 Oct 2026: M5 PLATE, companion, freeze and paint built (tests: gestures from button and gate, PLATE T60 / not metallic / stable, all three SPACEs loudness-matched, pads hold 5 min within 0.5 dB, the companion answers DECAY, paint grows the pad boundedly, click-free freeze / paint / unfreeze, REVERSE rests while frozen).
- 21 Oct 2026: M4 MASSIVE + ATTACK built (tests: BLOOM rise within 20 % of TIME, MASSIVE's mass, infinite holds 10 min without growing, echoes → wash, REVERSE rises to its cut, click-free SPACE flips, HALL/MASSIVE loudness matched).
- 21 Oct 2026: M1 Hall built (FDN, 8 lines; tests: stable at 45 corners, T60 within 2 %, not metallic, SIZE glides click-free, same at 44.1/48/96 kHz).
- 21 Oct 2026: spec session; P7 / SW1 demos (`renders/p7_sw1_demos/`); spec v1.0 approved and frozen; ADRs 0001–0017.
