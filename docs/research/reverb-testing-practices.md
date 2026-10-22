# How reverb makers test, and what we take from it

**Written:** 22 Oct 2026, at the owner's request ("research how other creators have tested and evaluated their reverbs, then adapt any relevant learnings"). Sources were read where reachable. Griesinger's papers and Abel and Huang's are behind the AES paywall, so only their abstracts and how others cite them were read.

## What they do

1. **Find metallic sound with pulse-like material; male voice is the hardest case (Costello).** A snare hides coloration because it's metallic itself. A voice is a train of pulses that makes short allpasses ring. https://valhalladsp.com/2011/01/21/reverbs-diffusion-allpass-delays-and-metallic-artifacts/
2. **Tune the reverb to sound good with modulation off; modulation is seasoning (Costello).** FutureVerb's modes "have been designed to sound good without any modulation". https://valhalladsp.com/2025/10/03/valhallafutureverb-the-reverb-modes/ · https://valhalladsp.com/2020/05/06/the-philosophy-of-valhallasupermassive/
3. **Test signals first, then varied music, listened to 100 % wet** (KVR / HISE developers). https://forum.hise.audio/post/90042
4. **Judge the space behind the music, by ear** (Airwindows). https://reaper.blog/2017/11/4_airwindows_plugins/
5. **Match levels within about 0.1–0.5 dB**, or the louder version wins as "more detail". https://stereophile.com/content/mark-levinson-no38s-preamplifier-level-matching
6. **MUSHRA (ITU-R BS.1534):** several versions at once, a hidden reference and an anchor that is clearly worse, to show when a judgement is reliable. https://en.wikipedia.org/wiki/MUSHRA
7. **Reverb loudness is its own perception and depends on masking by the music** (Griesinger, Lexicon). https://www.ecousticsystems.com/how-loud-my-reverberation
8. **Normalized echo density (Abel and Huang)** tracks "grainy against smooth"; 1.0 means noise-like. https://github.com/microsoft/echodensity
9. **Mode density.** Schroeder's guide is at least 0.15 modes per Hz per second of T60. https://dsprelated.com/freebooks/pasp/Choice_Delay_Lengths.html
10. **Modal-excitation spread predicts coloration** (Heldmann and Schlecht, DAFx 2021). https://dafx2020.mdw.ac.at/proceedings/papers/DAFx20in21_paper_17.pdf
11. **Energy decay relief:** decay per band over time, which catches one band outliving the rest. https://www.dsprelated.com/freebooks/pasp/Energy_Decay_Relief.html
12. **ISO 3382 measures.** EDT tracks perceived reverberance better than T60; C80 measures clarity; IACC measures spaciousness. https://pyrato.readthedocs.io/en/latest/_modules/pyrato/parameters.html
13. **Tuning against an objective ringing measure** (Strymon, BigSky MX). https://www.newsfilecorp.com/release/206526/Strymon-Announces-New-BigSky-MX-Reverb-Workstation
14. **Golden-file regression:** a trusted render is stored, and every change is compared against it within tolerances. https://infinum.com/blog/snapshot-testing-for-audio-dsp/

**What we already do well:**
- Level-matched, sample-locked A/B, with blind mode on choice pages.
- Ear first, with 100 % wet renders and the dry blended in live.
- A ringing metric that follows each frequency's decay (close to items 11 and 13).
- Analytic signals plus music.
- Stability, click, infinite-hold and sample-rate gates.
- Private reference captures.

## Done (22 Oct 2026)

- **Level-matching by loudness** (item 5): pages match versions on gated BS.1770 loudness (ffmpeg `ebur128`) instead of RMS over the whole file. The silence after a tail no longer makes a long-tailed version read as quiet. (`tools/review/listen.py`)
- **Not metallic with modulation off** (items 1 and 2): `test_plate_freeze` runs noise bursts through HALL and MASSIVE at both SIZE ends, and through the PLATE at SIZE 0, ½ and 1, with the tanks' own modulation off (`Engine::setModulationScale`, a test hook; `IV_MOD_SCALE` in the Renderer). **Finding (fixed, v1.0.11):** the PLATE rang without modulation at exactly full SIZE (15 dB at 9.8 kHz) and nowhere else (8 dB at 99 %; nothing with modulation or frozen): a coincidence of its lengths at a scale of exactly 1.6, so full SIZE is 1.59.
- **Mode density (item 9), measured 22 Oct 2026:** Schroeder's guide (total loop delay ≥ 0.15 s per second of T60) holds for MASSIVE everywhere, for the PLATE at DECAY 0.4 and from SIZE ¾ at 0.6, and for the HALL from SIZE ½ at 0.4. Small PLATEs and HALLs with longer tails have fewer modes than the guide asks, as any small room with a long tail does: like Dattorro's own plate, they rely on their modulation floor, which is always on, and the unmodulated test above shows none of them rings even so. No gate: the guide would fail by design; the unmodulated test is the check.
- **Sweet-spot check restored:** the old `tools/sweetspot.py` pointed at Resilio's renderer and couldn't run. The new one sweeps every knob in every SPACE and writes `docs/sweetspot.md`. Its findings are on the M8 list.
- **No separate Dry version where the MIX fader is**; page audio is stored as FLAC.

## Done (22 Oct 2026, session 3: the owner's plan)

- **Echo density, the grainy check** (item 8): `Metrics` measures Abel and Huang's normalized echo density per side (`echo_density_time_s`, `echo_density_late`). `test_density` requires every SPACE at SIZE 0 / ½ / 1, DECAY 0.4 / 0.7 to be a smooth wash (NED ≥ 0.9) within 0.3 s and to stay one. **Finding:** every late tail passes, but nine settings take longer than 0.3 s to turn smooth at the start: the PLATE from SIZE ½ (0.34–0.68 s), the HALL at full SIZE (0.37–0.51 s), MASSIVE at SIZE 0 / DECAY 0.4 (0.80 s) and at full SIZE (0.38–1.42 s). They're listed as known misses with their numbers (each fails if it gets 25 % + 20 ms slower); the musical question is the owner's (`renders/grainy_start`). **Caveat (23 Oct 2026):** the reading is a threshold crossing, and MASSIVE's density hovers at 0.85–0.95 from the start, so a one-sample change in the click moves its "smooth after" from 1.67 s to 0.06 s; PLATE and HALL readings move 0.1–0.2 s with the click and settings. Make it steadier before acting on the numbers.
- **Decay per band and EDT** (items 11, 12): octave bands 125 Hz–8 kHz, exactly `tools/ingest_reference.py`'s method (its T60 is 3 × the −5 to −25 dB time), so renders and the owner's units compare (C++ and Python agree to 0.003 %). Pages show them in a card's **Numbers**, measured on one click per version (`_measure/`), since music never leaves a clean decay; a band ringing 30 % longer than the mids is bold.
- **C80, IACC, late-tail flatness** (item 12), in the sidecar and the Numbers.
- **Hidden reference** (item 6): pick pages with three or more rows repeat one row away from its original; the results say "no clear preference" where the copies got different picks.
- **Material** (items 1, 7): a low male voice, a synthesized dry close piano, and a full dub-techno mix whose stabs and rims alone go to the reverb (`fullmix_send.wav`, 13 dB under the mix), the page's dry being the whole mix.
- **Tail only** (item 3): a Tail button (key T) loops from where the playing stops; "Tails +12 dB" lifts only a looped tail.
- **Golden renders** (item 14): `test_golden` fingerprints ten corners (each SPACE's SIZE ends, TONE dark and bright, ERA's Midiverb, REVERSE) on a click and a chord: chord loudness and band balance (±0.5 dB), decay per band (±5 %), smooth-after (±30 ms), C80 (±0.5 dB), in `host/tests/golden/fingerprints.json`. To change a sound on purpose, add the SPEC changelog line, then `IV_GOLDEN_UPDATE="why" build/iv_test_golden`; it refuses without a newer changelog version.
- **Later:** pitch wobble on pages (item 5 of the plan), when DRIFT tuning starts.

## Planned (ranked; each to agree before doing)

**Owner's choices (22 Oct 2026, session 3):** yes to 1, 2, 3, 4, 7 and 8 (all of 8); 5 (pitch wobble) later, when DRIFT tuning starts; 6 only the low voice + dry piano and the full-mix bed (no long frozen-pad row). Order: the mode-density check (8), the low voice + dry piano (6) and the tail-only button (7) first, so the PLATE full-SIZE ring fix and its A/B page benefit; then echo density (1), decay per band (3), the hidden reference (4), the full-mix bed (6), the rest of 8; golden renders (2) last, once the PLATE fix has landed.

1. **Echo density** (item 8): a normalized-echo-density measure in `host/common/Metrics`. Every SPACE at every SIZE should reach about 1.0 by mid-tail; pages flag "grainy".
2. **Golden renders** (item 14): metric snapshots of about 10 preset × material corners, so an unintended sound change fails the build. Changing a golden needs a changelog line.
3. **Decay per band and EDT on pages** (items 11 and 12): "one band hangs" becomes visible, and it's the same measure `tools/ingest_reference.py` uses on your units.
4. **Hidden reference on choice pages** (item 6): a silent duplicate of one version. If the two copies get different picks, the page says "no clear preference" instead of me acting on it.
5. **Pitch wobble on pages:** `tools/pitch_track.py` folded in, calibrated on your "seasick" note, for DRIFT and frozen pads.
6. **Material:**
   - a lower male voice and a dry close piano for "metallic?" questions (item 1);
   - a dense full-mix bed at a realistic send level, since masking changes how loud the reverb seems (item 7);
   - a minutes-long frozen-pad row.
7. **A "tail only" listening aid:** jump to the decay after the last note, optionally +12 dB, where ringing shows (item 3).
8. **Mode-density check** (item 9) and late-tail flatness, C80 and IACC (item 12).
