# Handoff

**Written:** 22 Oct 2026, end of session 2. Start the next session with `/immensio-start`.

**Nothing running:** no agents, clean tree, `main` pushed to `origin` (https://github.com/jeffebauer/immensio-versio).

**One worktree left:** `.claude/worktrees/serene-snyder-30d3a2` (branch `claude/serene-snyder-30d3a2`). It belongs to the app-managed DSP audit session. Its single commit is **merged** into main, so the worktree can go whenever the app cleans it up. Don't do work there.

## State

| Milestone | State |
|---|---|
| Spec | v1.0 frozen, plus build amendments **v1.0.1–v1.0.10** (SPEC changelog). This session: v1.0.9 (REVERSE rebuilt) and v1.0.10 (MASSIVE's twin swell) |
| M0 desktop / firmware | Built |
| M0 hardware | **Owner check pending** (`docs/m0-hardware-check.md`) |
| M1 Hall | Built. Owner re-listen pending (M1 page, now with nine materials) |
| M4 MASSIVE + ATTACK | Built. **REVERSE settled** (the hardware way, confirmed). **MASSIVE keeps its built-in swell** (owner pick, ADR 0007 note) |
| M5 PLATE + freeze | Built. The owner's two freeze notes (an upper harmonic in a frozen MASSIVE; PLATE repeats on the skank) **don't reproduce** in the current build: measured, re-listen asked. **MASSIVE's twin now swells**, awaiting the owner's confirmation on `twin_swell` |
| M6 character, M7 duck | Built. Owner hasn't listened |
| M2 | Criteria pass. **Not installed** in Ableton |
| M3 | Needs the owner at the rack |
| M8 | Inputs gathering: the sweet-spot findings, the audit, the A/B picks below |
| M9 | Not started |

- **`main` HEAD:** `435468a`; this handoff's commit follows.
- **Plugin in Ableton:** none installed.
- **Gates** (at `435468a`, nothing changed since): ctest **"100% tests passed out of 12"**.
- **Firmware:**

  | Variant | Bytes | % of 128 KB |
  |---|---|---|
  | release | 96,836 | 74 % |
  | profile | 97,824 | 75 % |
  | m0test | 82,356 | 63 % |

  That leaves ~13 KB to the 110 KB release budget. About 5.8 KB of it is the TONE shelf prototype's `std::tan` (below), so reclaim that whatever the owner picks.
- **Listening pages** (`renders/`, 8.6 GB, gitignored): m1_hall, m4_massive, m5_plate_freeze, m6_character, m7_duck, plate_modulation, tone_shelf, twin_swell. All play nine materials and come from current code, except m1/m4–m7, which were rendered before the twin swell (v1.0.10) and the PLATE/TONE prototype hooks (off by default, so their sound is unchanged apart from the twin in M5's MASSIVE cards). Audio sidecars are FLAC.

## In flight (prototypes behind desktop-only hooks, waiting on owner picks)

- **`IV_TWIN_SWELL`** (`Engine::setTwinSwell`, default **on**, the new behaviour). Page `renders/twin_swell`.
  - **If B (smooth) is confirmed:** remove the setter, `twinSwellOn_` and the hook; delete the page.
  - **If A:** the twin swell goes.
- **`IV_PLATE_LONGMOD`** (`Plate::setLongDelayMod`, default 0). The Plate's d1/d2 wander about 2 cents (`kPlateLongModSeconds`, `kPlateLongModHz`). Page `renders/plate_modulation`.
  - **If B:** make it the default (amount 1), drop the setter, and re-check `test_plate_freeze`'s pad-steadiness tests across seeds.
  - **If A:** remove it, including the extra memory in `Plate::memoryFloats` and `prepare`.
- **`IV_TONE_SHELF`** (`Engine::setLossShelfHz`, default 0): every tank's loss as a first-order shelf at 2.5 kHz (`absorbent(…, shelfHz)` in `Governor.h`; `b1_`/`absorbX_` in Fdn, `x1`/`x2` in Plate). Page `renders/tone_shelf`.
  - **If B:** make it the default. **Replace `std::tan`** with a precomputed K, since it's a per-control-tick call and the ~5.8 KB flash cost. Re-run T60 / TONE tests (`test_fdn` lowBandT60 measures below 500 Hz, so it should hold). Write an ADR 0012 amendment, as TONE's noon changes.
  - **If A:** remove it all.
- **Test hook `Engine::setModulationScale` / `IV_MOD_SCALE`** is permanent (used by `notMetallicUnmodulated` in `test_plate_freeze`).

## Next steps

1. **Act on the three A/B picks** above when the owner sends them (memory [[measure-owner-notes-first]] for any notes).
2. **M8 findings, confirm by ear before changing anything** (`docs/sweetspot.md`, `docs/TASKS.md` "Waiting on Claude"):
   - **ERA:** the Midiverb stage (80–100 %) jumps about 20× harder than the rest of the knob, and 0–20 % barely changes anything measurable.
   - **PLATE:** TONE's first step from fully CCW and SIZE's last step jump.
   - **MASSIVE:** DECAY's first step jumps; DRIFT is flat 20–50 % (pitch not measured); TIME in NORMAL is weak 0–30 %.
   - **The PLATE at full SIZE rings with modulation off** (15 dB at 9.8 kHz): `notMetallicUnmodulated` checks only SIZE 0 until this is fixed. Look at the Plate's scaled lengths at `kPlateMaxScale` 1.6: Dattorro's ratios at a larger scale lower the mode density.
3. **The DSP audit's other findings** (`docs/research/dsp-audit.md`): HALL has no in-loop diffusion; freeze keeps the modulation floor; ERA and the governor are our own inventions. Also the MI ideas still untried (`docs/research/mutable-instruments-reverbs.md` #2–#5: modulated input diffusers, a modulated MASSIVE in-loop allpass, independent R diffuser lengths, a decay-dependent input trim). Each one gets an A/B page.
4. **The testing plan:** the owner picks which and in what order (`docs/research/reverb-testing-practices.md`, "Planned": echo density, golden renders, per-band decay on pages, a hidden reference).
5. **The owner's reference recordings**, when they arrive in `test_audio/reference/incoming/`. Read them with `.venv/bin/python tools/ingest_reference.py <wav> calibrate|measure|music <unit_algo[_target]>`.
   - For calibration takes, tell the owner which knob positions give 2 / 5 / 15 s.
   - Music goes to pages with `make_review.py --reference test_audio/reference/<name>`.
   - Never commit them.
6. **Owner at the rack:** M0, then M3. A frozen MASSIVE is now the pad, the twin FDN and the twin's swell (24 more SDRAM reads a sample per side), plus REVERSE's 32 taps and short FDN when not frozen. Profile both.

## Waiting on the owner (all in `docs/TASKS.md`)

- **Picks:** twin_swell, plate_modulation, tone_shelf. Also choose the testing plan.
- **Decisions still open:** the ADR 0008 makeup, MASSIVE's SIZE glide, the plugin identity.
- **Listening:** M1, M4, M5 again (plus re-checking the two freeze notes); M6 and M7 for the first time.
- **At the rack:** M0, then M3.
- **Recordings:** per `docs/reference-recording-guide.md`, starting with the H9 Reverse.

## Will bite

- **Python tools need `.venv`** (numpy and soundfile: `python3 -m venv .venv && .venv/bin/pip install numpy soundfile`): `make_material.py`, `make_reference_kit.py`, `ingest_reference.py`, `sweetspot.py`. `render_review_pages.sh` builds the material automatically only if `.venv` exists.
- **Rendering takes a while now.** Every page renders nine materials: a full `tools/render_review_pages.sh` is ~11 GB before FLAC and takes a long time. Render single pages (`tools/render_review_pages.sh <page>`), and delete a page once its question is decided.
- **`make_review.py` now shells out to ffmpeg twice per file:** FLAC sidecars, and `ebur128` loudness for level-matching. Silent files (−70 LUFS) fall back to RMS. Re-running it on a page whose `_audio` is current only regenerates the HTML.
- **There's no Dry version on pages with the MIX fader** (`SHOW_DRY` in `listen_template.html`), by the owner's request.
- **REVERSE has its own reverb.** It is rising taps (`Reverse.h`, 32 per side) into `reverseFdn_`, a short FDN with its own memory sizing (`Fdn::prepare(…, maxLineSeconds)`; `Engine::largeFloats` counts it).
  - It replaces the tank's wet. It doesn't change with SPACE except for MASSIVE's predelay.
  - SIZE sets its line length and DECAY its stop.
  - `kReverseLevel` 6.0 (+15.5 dB) is a measured loudness match.
- **The twin's swell is set at freeze time** (`onGesture` Freeze) and each control tick while active. Anything that changes the swell's attack must also reach `twinSwell_`.
- **Earlier traps still apply:** freeze glides, painted pads, HALL's SIZE-scaled T60, allpass-read fractions, and chaotic tanks (judge across seeds).
- **`ingest_reference.py`'s octave bands are zero-phase FFT windows padded with a second of silence.** Without the padding, the filter's pre-ringing wraps round and floors the decay curve (it read 63 s for a 5 s reverb).
- **iv_render's sidecar doesn't record the `IV_*` environment hooks.** When a page's versions differ only by a hook, check by md5 that they really differ (memory [[verify-voicing-labels]]).
- **The reference-device research has guesses** for knob-only units: calibrate, don't trust the guessed positions.

## Where to look

- `docs/TASKS.md`
- `docs/research/dsp-audit.md`
- `docs/sweetspot.md`
- `core/dsp/Engine.cpp` (applySpace, the twin, REVERSE)
- `core/dsp/Plate.cpp` and `core/dsp/Governor.h` (the two prototypes)
- `tools/render_review_pages.sh`
