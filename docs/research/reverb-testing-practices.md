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
- **Not metallic with modulation off** (items 1 and 2): `test_plate_freeze` runs noise bursts through HALL and MASSIVE at both SIZE ends, and through the PLATE at SIZE 0, with the tanks' own modulation off (`Engine::setModulationScale`, a test hook; `IV_MOD_SCALE` in the Renderer). **Finding:** the PLATE at full SIZE rings without modulation (15 dB at 9.8 kHz, against 4 dB with it), so its modulation is hiding a resonance. It's on the M8 list.
- **Sweet-spot check restored:** the old `tools/sweetspot.py` pointed at Resilio's renderer and couldn't run. The new one sweeps every knob in every SPACE and writes `docs/sweetspot.md`. Its findings are on the M8 list.
- **No separate Dry version where the MIX fader is**; page audio is stored as FLAC.

## Planned (ranked; each to agree before doing)

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
