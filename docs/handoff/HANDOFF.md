# Handoff

**Written:** 23 Oct 2026, end of session 3. Start the next session with `/immensio-start`.

**Nothing running:** no agents, clean tree, `main` pushed to `origin` (https://github.com/jeffebauer/immensio-versio) with this handoff.

**Worktrees left (not ours, both merged):** `.claude/worktrees/serene-snyder-30d3a2` (the DSP audit session) and `.claude/worktrees/swell-curve` (the session that built MASSIVE's twin S-curve swell, merged as v1.0.13). The app cleans them up; don't work there.

## State

| Milestone | State |
|---|---|
| Spec | v1.0 frozen, plus build amendments **v1.0.1–v1.0.17** (SPEC changelog). This session: v1.0.11–v1.0.17 (below) |
| M0 desktop / firmware | Built |
| M0 hardware | **Owner check pending** (`docs/m0-hardware-check.md`) |
| M1 Hall | Built; owner re-listened: "sounding much better", TONE "sounds good". Small HALL reworked (v1.0.17) |
| M2 | Criteria pass. **Not installed** in Ableton. Plugin now branded Ombra Audio (ADR 0018) |
| M3 | Needs the owner at the rack |
| M4 MASSIVE + ATTACK | Built; owner: "much better". BLOOM smear (v1.0.16) |
| M5 PLATE + freeze | Built; owner: "excellent". Twin 6 dB under the pad (v1.0.14) |
| M6 character | Built; **first listen pending** on `renders/m6_character` (re-rendered from today's code, 12 materials) |
| M7 duck | **Redesigned**: 4:1 compression replaces DARKEN (v1.0.17, ADR 0014 amendment) |
| M8 | Inputs gathering (TASKS "Waiting on Claude") |
| M9 | Not started |

Spec changes this session (each with a SPEC changelog line; ADRs 0004, 0008, 0009, 0012, 0014, 0015 amended, 0018 new):
- v1.0.11 PLATE max scale 1.59 (1.6 lined up a 9.8 kHz mode, modulation off).
- v1.0.12 PLATE long delays wander (frozen: still).
- v1.0.13 MASSIVE's twin swells on an S-curve (merged from the swell-curve session).
- v1.0.14 the twin 6 dB under the pad (level 0.5).
- v1.0.15 every tank's loss is a 2.5 kHz shelf (warmer TONE noon); K by a series, not `std::tan` (−4.4 KB flash).
- v1.0.16 ERA's Midiverb as the real unit (13 bits + ½ one-pole every pass; make-up 1.0 dB); BLOOM smear (BLOOM only); the PLATE's first input allpass wanders (0.18 ms, 0.4 Hz).
- v1.0.17 ducking = 4:1 compression of the wet (−24 dBFS, 30 ms / 0.4 s, 50 ms on a freeze, no darkening; DARKEN removed); HALL SIZE from 35 ms, small-end tails × 0.49, in-loop diffusion 0.5 at the small end fading out by noon; the held-tail makeup approved.

- **`main` HEAD:** this handoff's commit (code last changed at `a1c86dd`'s parent chain; gates run on it).
- **Plugin in Ableton:** none installed.
- **Gates:** ctest **"100% tests passed out of 14"** (`test_golden` and `test_density` are new this session).
- **Firmware:**

  | Variant | Bytes | % of 128 KB |
  |---|---|---|
  | release | 97,692 | 74 % |
  | profile | 98,648 | 75 % |
  | m0test | 82,356 | 62 % |

  ~12.3 KB left to the 110 KB release budget. It includes the remaining prototype hooks below (small).
- **Listening pages** (`renders/`, 2.5 GB): only `m6_character` and `_material`. Every decided page was deleted.

## New this session (testing; owner's plan in `docs/research/reverb-testing-practices.md`)

- **Material:** low voice (macOS Ralph), synthesized dry piano, full dub-techno mix fed to the reverb only through `fullmix_send.wav` (`tools/make_material.py`). Pages render 12 materials.
- **Pages:** a Tail button (T) with "Tails +12 dB"; a **Numbers** panel per card (decay per band, early decay, grain, clarity, width) measured on one click per version (`renders/<page>/_measure/`, written by `render_review_pages.sh`'s `r()`); a "grainy start" flag above 0.1 s; pick pages with ≥ 3 rows repeat one row (the hidden twin) and Copy-results reports disagreeing picks as "no clear preference".
- **Metrics** (`host/common/Metrics.*`): echo-density **grain** (`echo_density_grain_s`, an area below NED 0.9 over 4 s; steady to ~0.01 s), late density, band T60/EDT (exactly `tools/ingest_reference.py`'s method), C80, IACC, tail flatness. `echoDensityTimeS` (the old crossing) is deprecated, kept only for `test_golden`'s stored `smooth_after_s`.
- **Gates:** `test_density` (grainy gate, known misses: PLATE full SIZE, MASSIVE SIZE 0) and `test_golden` (10 corners' fingerprints in `host/tests/golden/fingerprints.json`; update with `IV_GOLDEN_UPDATE="why" build/iv_test_golden`, which refuses without a newer SPEC changelog version).

## In flight

- **Prototype hooks still in the Core, off by default** (desktop only, via `host/render/main.cpp` env vars):
  - `IV_STILL_LOWS` (`Fdn::setStillLows`, `Engine::setStillLows`): the owner liked it (B on MASSIVE) but it **isn't energy-safe** (HALL builds up to +12.7 dB; as MASSIVE's default the 10-min infinite hold failed). Needs a stable design, then a page. Remove the hook if abandoned.
  - `IV_DIFF_INDEPENDENT` (`Engine::setDiffuserIndependent`): MI idea 4, no measurable gain. Remove when convenient.
  - `IV_HALL_DIFFUSION` (`Engine::setHallInLoopDiffusion`): an override of the now-default HALL diffusion, for demos.
  - `IV_MOD_SCALE`: permanent test hook.
- **Resilio → Ombra Audio** (its own repo): not started; its plugin is installed in Ableton under `Rslo`, so check the owner's sets first.

## Next steps

1. **M6 notes** when the owner sends them (memory [[measure-owner-notes-first]]).
2. **Still lows, done safely:** research a stable way to keep MASSIVE's bass steady under DRIFT (Schlecht & Habets: time-varying unitary feedback matrices are stable where delay modulation isn't; or a steep, delay-matched split). Energy-check it (memory [[energy-check-prototypes]]) before a page.
3. **M8 findings** (TASKS "Waiting on Claude"): re-run `.venv/bin/python tools/sweetspot.py` (ERA's Midiverb stretch changed), then the DECAY/TONE/SIZE/TIME jumps; the MASSIVE "uneven on Hits" note (measure the swell/taps level, not grain).
4. **Remaining research ideas:** MI #5 (input trim with DECAY); the audit's A3 (freeze keeps modulating), P2–P5, E2–E6, R1–R2. Each measured first, then an A/B page.
5. **Owner at the rack:** M0, then M3 (profile a frozen MASSIVE: the twin and its swell; and BLOOM, whose smear adds 6 allpasses).

## Waiting on the owner (all in `docs/TASKS.md`)

- **Listen:** M6 (first time).
- **At the rack:** M0, then M3.
- **Recordings:** per `docs/reference-recording-guide.md`, starting with the H9 Reverse.
- No open picks or decisions.

## Will bite

- **Don't rebuild `build/` while a page renders in the background:** `render_review_pages.sh` calls `build/iv_render` per file, so later files come from the new binary. Default sound must be unchanged (check with `iv_test_golden`) or re-render.
- **`render_review_pages.sh` recipes of decided pages were removed** (their hooks are gone). `reverse`, `grainy_start`, `m5`, `m6`, `m7_duck` remain; `m7_duck` now describes compression.
- **The safety net (`test_golden`) fails on any deliberate sound change** until updated, and the update needs a new SPEC changelog version first. Chord bands > 40 dB under the total aren't compared.
- **HALL's SIZE range was squeezed (v1.0.17):** noon is now old 0.61 (95 ms). The owner hasn't heard a medium HALL since; say so on the next HALL page.
- **BLOOM smear:** the swell's first 10 % arrives 0.06–0.4 s later than unsmeared (the owner noticed "a little slower"); the span is × 0.95 to keep the peak on time (0.9 broke `test_massive`'s BLOOM-rise check).
- **Ducking** releases in 50 ms on a freeze (`freezeMix_ > 0`); `test_output` checks no duck 0.4 s after a freeze.
- **Diffuser wander depth is capped at 8 samples** (the buffers' nextPow2 spare room); raising it needs bigger diffuser buffers (fast memory).
- **The local page server** (`.claude/launch.json` "review-pages", port 8765) stops between sessions; restart it with the browser pane's preview_start.
- **Picks relayed by other sessions** need the owner's confirmation (memory [[relayed-picks-need-owner]]).
- Earlier traps still apply: freeze glides, painted pads, allpass-read fractions, chaotic tanks (judge across seeds; `notMetallicUnmodulated` uses the median of three bursts), `ingest_reference.py`'s padding, iv_render sidecars don't record `IV_*` hooks (check md5s).

## Where to look

- `docs/TASKS.md`
- `docs/research/dsp-audit.md` and `docs/research/mutable-instruments-reverbs.md` (what's tried, what's left)
- `docs/research/reverb-testing-practices.md` (the testing tools)
- `core/dsp/Engine.cpp` (ducking in `controlTick`, HALL diffusion in `applySpace`)
- `core/dsp/Fdn.cpp` (still-lows prototype) and `core/dsp/Swell.h` (BLOOM smear)
- `tools/render_review_pages.sh`
