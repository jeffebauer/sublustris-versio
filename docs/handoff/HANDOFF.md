# Handoff

**Written:** 24 Oct 2026, end of session 4. Start the next session with `/immensio-start`.

**Nothing running:** no agents, a clean tree, `main` pushed to `origin` with this handoff.

## State

| Milestone | State |
|---|---|
| Spec | v1.0 frozen, plus build amendments **v1.0.1–v1.0.27** (SPEC changelog). This session: v1.0.18–v1.0.27, below |
| M0 desktop / firmware | Built |
| M0 hardware | **Owner check pending** (`docs/m0-hardware-check.md`) |
| M1 Hall, M4 MASSIVE, M5 PLATE | Built and listened. This session's Valhalla fixes reshape HALL and PLATE (v1.0.25) |
| M2 | Criteria pass. **Not installed** in Ableton |
| M3 | Needs the owner at the rack (CPU). The heaviest desktop case is below |
| M6 character | Listened (DRIFT CW → Wander; ERA level fixed). `renders/m6_character` **predates** the ERA changes since v1.0.21 |
| M7 duck | Done (4:1 compression, v1.0.17) |
| M8 | Knob feel done (v1.0.21) |
| M9 | Not started |

**This session's spec changes** (each with a SPEC changelog line and an ADR amendment where it applies):
- **v1.0.18 still lows** (ADR 0012): HALL and MASSIVE split the tank's input at 300 Hz. The lows go to a second, unmodulated FDN at 12 kHz that follows the main tank's governor (`Engine::processTwoTanks`). Research: `docs/research/still-lows.md`.
- **v1.0.19 ERA make-up set on music** (ADR 0004). `test_era` now checks loudness in LUFS on the dub skank.
- **v1.0.20 DRIFT's clockwise side is Wander** (ADR 0012): the wow generator at 1 → 2.25 Hz, ±20 cents, line rates spread ±50 % (`DriftAmounts::rateSpread`). Research: `docs/research/drift-cathedra-side.md`.
- **v1.0.21 knob feel** (ADR 0003 / 0007 / 0015), with `massiveDecayLevelTrim` keeping each setting's loudness:
  - ERA: Neve in straight steps, the Midiverb faded in across its stretch;
  - MASSIVE's shortest DECAY lasts 3 passes;
  - MASSIVE's NORMAL predelay moves in straight steps;
  - BLOOM starts at 0.12 s;
  - MASSIVE has 6 interleaved early taps.
- **v1.0.22 frozen pads hold still; vintage reads; tape out** (new ADR 0019; ADR 0009).
- **v1.0.23 a held Midiverb tail keeps a quarter of its dark loop** at infinite DECAY (`kMidiDarkAtInfinite`; ADR 0004).
- **v1.0.24 the soak merge** (ADR 0008):
  - infinite tails no longer plunge when a loud input stops (the governor and the cassette compression take the lower of a 1.5 s and a 50 ms level);
  - ten bug fixes;
  - `test_soak_regressions` and `tools/soak/` (the 341-hour harness, `iv_soak`).
- **v1.0.25 Valhalla fixes** (ADR 0007 / 0012 / 0013):
  - HALL early taps, gliding per sample;
  - HALL floor ×3;
  - air (HALL 5.5, PLATE 8 dB/s, in `absorbent()`);
  - PLATE cross-branch taps added (centred lows);
  - MASSIVE swell ×0.5;
  - `kHallOutputTrim` −2.3 dB.
  - Research: `docs/research/valhalla-notes.md`.
- **v1.0.26 tape bias a third; the full-mix send 12 dB hotter** (page material, written as float).
- **v1.0.27 tape out "a little stronger" with light hysteresis** (`core/dsp/Tape.h`; ADR 0019). Research: `docs/research/tape-out.md`.

**Turned down after listening** (in the ADRs): the steady-level tape, the Jiles–Atherton hysteresis, Orbit, Halo, the phasers, and the split-in-the-loop still lows.

- **`main` HEAD:** this handoff's commit.
- **Plugin in Ableton:** none installed.
- **Gates:** ctest **"100% tests passed out of 15"** (`test_soak_regressions` is new).
- **Firmware** (110 KB budget = 112,640 B; **6.9 KB left**):

  | Variant | Bytes | % of 128 KB |
  |---|---|---|
  | release | 105,756 | 80 % |
  | profile | 106,744 | 81 % |
  | m0test | 82,356 | 62 % |

- **Listening pages** (`renders/`, 2.7 GB): `m6_character` (stale, see above), plus the `_material*` folders. Every decided page was deleted.
- **Owner's reference recordings:** the Valhalla Room / Plate / Supermassive takes, read straight from their Ableton set (memory [[reference-takes-from-ableton]]), ingested into `test_audio/reference/` (never committed). `tools/compare_reference.py` regenerates the comparison tables (`docs/research/valhalla-comparison.md`).

## In flight

- **Nothing running.**
- **Branches kept on purpose:**
  - **`claude/listen`** (unmerged, no worktree): superseded prototypes with their page scripts. They are the Jiles–Atherton and stronger tape modes, `IV_TAPE_EVEN` / `IV_TAPE_BIAS`, `IV_MIDI_BRIGHT`, and the `ValhallaProto` hooks. Everything picked from it has shipped. Keep it for the record, or delete it when convenient.
  - `.claude/worktrees/serene-snyder-30d3a2` and `swell-curve`: older sessions' worktrees, already merged. The app cleans them.
- **Prototype hooks still in the Core, off by default** (desktop only, via `host/render/main.cpp`): `IV_STILL_LOWS=0` (one tank, for demos), `IV_TAPE_OUT=0` (bypass tape out, for measuring), `IV_DIFF_INDEPENDENT`, `IV_HALL_DIFFUSION`, `IV_MOD_SCALE`.

## Next steps

1. **The rest of the Valhalla comparison** (TASKS "Waiting on Claude"; `docs/research/valhalla-comparison.md` suggestions 3, 4 and 6). Each is a page beside the Valhalla takes:
   - our wet gets ~10 dB quieter from 15 s to 2 s (HALL), while Room keeps its level;
   - MASSIVE's 8 kHz dies early (8 s against 15 s);
   - Supermassive moves more in pitch at long settings.

   Measure first; use the energy check for anything in a loop (`tools/pages/energy_check.py --hook NAME=VALUE`).
2. **Re-render M6** if the owner wants ERA as it is now: `tools/render_review_pages.sh m6_character`.
3. **Owner at the rack:** M0, then M3. Profile at least these corners:
   - a frozen MASSIVE with ERA full, DRIFT CW and BLOOM: 4.5× a plain HALL on the desktop (`docs/research/soak-audit.md`'s CPU table);
   - HALL now runs early taps too;
   - still lows add 16–28 %;
   - tape out runs at 4× oversampling;
   - a long silent tail, for subnormals: the firmware doesn't set flush-to-zero; the plugin does.
4. **Flash:** 6.9 KB left. Before adding Core code, look for savings (e.g. `std::pow` / `std::exp` calls), and check `make -C firmware all-variants` after every Core change.
5. Resilio → Ombra Audio (its own repo; check the owner's Ableton sets first).

## Waiting on the owner (all in `docs/TASKS.md`)

- No picks or pages waiting.
- **At the rack:** M0, then M3.
- **Recordings:**
  - re-export three clipped Valhalla takes 6 dB lower (Plate Chrome 15 s, Lithium 15 s, Andromeda 100 %);
  - re-record Supermassive's short settings at a feedback that sounds like a reverb (about 5 % for 2 s), or record longer settings;
  - check Supermassive's DELAY between takes;
  - H9 Reverse, Mercury7, VintageVerb when convenient.
- **M2:** install the plugin when they want it in Ableton.

## Will bite

- **Flash budget:** 6.9 KB left at v1.0.27. Each shipped pick this session cost 0.5–3 KB.
- **The tape soft-limits the wet's peaks** to about ¼ of full scale from the cassette stage up, by design: the drive is taken off after the curve. `test_output`'s limiter check therefore runs at ERA 0.3. A hot input into a long, bright reverb with ERA ≥ 0.55 gets squashed.
- **The plugin test's direct render runs under `juce::ScopedNoDenormals`,** as the plugin does. The engine's output depends on subnormal handling at about −180 dB (MASSIVE's early taps read subnormal tank memory). The firmware doesn't flush.
- **`test_density`'s known list** was re-baselined at v1.0.25. A big PLATE's start is 0.01–0.03 s grainier, and SIZE 0.5 / DECAY 0.7 sits at 0.100 s.
- **`tools/make_material.py` can't run from a worktree:** its CC0 samples are looked up next to the main checkout. Run it from `main`. `fullmix_send.wav` is now float (it peaks over 0 dBFS).
- **Level-matched pages hide loudness.** Measure LUFS whenever a change could alter level or the wet/dry balance (HALL needed a −2.3 dB trim; MASSIVE's DECAY curve a decay trim).
- **Earlier traps still apply:**
  - don't rebuild `build/` while a page renders from it;
  - the goldens need a newer SPEC version to update;
  - iv_render sidecars don't record `IV_*` hooks (check md5s);
  - the local page server (`.claude/launch.json` "review-pages", port 8765) stops between sessions;
  - picks relayed by other sessions need the owner's confirmation.
- **Remote Control sessions can't start new sessions.** Background agents (the Agent tool, worktree isolation) worked instead for the overnight audits.

## Where to look

- `docs/TASKS.md`
- `docs/research/valhalla-comparison.md` and `docs/research/valhalla-notes.md` (the next sound work)
- `docs/research/soak-audit.md` (CPU table, subnormals, what's left)
- `core/dsp/Engine.cpp` (`applySpace`, `processTwoTanks`) and `core/dsp/Tape.h`
- `SPEC.md` changelog v1.0.18–v1.0.27
