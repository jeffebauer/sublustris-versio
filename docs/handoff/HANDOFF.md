# Handoff

**Written:** 26 Oct 2026, end of session 8 (M3 hardware rounds 11-14, the crackle hunt, v1.0.37-40). Start the next session with `/sublustris-start`, which now has a **"Working as the orchestrator"** section: this session ran out of context doing everything in one thread; hand prototypes, pages, debug and flash hunts and hardware-log reads to nested sessions or subagents.

**Nothing running:** no agents, no nested sessions. `main` holds docs and skills only; **all the code is on branch `claude/massive-air`** (worktree `.claude/worktrees/massive-air`, HEAD `48d46a6`), not merged into main (the owner's Decide item).

## State

| Milestone | State |
|---|---|
| Spec | v1.0 frozen plus build amendments to **v1.0.40** (v1.0.34-40 on the branch only; main's SPEC stops at v1.0.33) |
| M0 | Passed (25 Oct) |
| M1/M4/M5/M6/M7/M8 | Built and listened |
| M2 | Plugin installed `b39d451` (v1.0.34). v1.0.35-40 not installed |
| M3 | **Nearly there.** Everyday sounds fit (playing MASSIVE 61-71 %, HALL 57-73 %, PLATE 44-60 %; no drop-outs up to ~99 % load). Open: **the SIZE crossfade's peak** (106 % before the stagger; round 15 measures v1.0.39's stagger) and **a frozen MASSIVE with the cassette** (104.5 % in real time) |
| M9 | Not started |

**This session (all on `claude/massive-air`):**
- **Hardware rounds:** 11 (`docs/research/m3-round11.log`), 12 (+ITCM, `m3-round12.log`, `m3-round12-itcm.log`), 13 (real-time knob sends, `m3-round13.log`), the playing build's own report (`m3-release-diag.log`), 14 (`m3-round14.log`). `firmware/README.md` has each round's reading.
- **v1.0.37:** release at SDRAM TRP/TRCD 3/3 (`SDRAMCHK` 0/0/0), the Core's hot code in ITCM (−2.5-3.5 points), SIZE's 0.1 % hysteresis and the twin stopping on its 50 ms level as Core defaults, flash freed by `IV_SIZE_OPT` on `queueEdge` / `onGesture` / `Drift::control`.
- **v1.0.38:** ERA's tape at 2× (owner's pick, `tape_os`).
- **The crackle hunt:** the owner's MASSIVE drop-outs were **audio leaking into the knob readings** (the ADC), moving SIZE by itself and starting SIZE crossfades (~110 % CPU). Fixed in the release's knob reading: a 10 ms one-pole + a deadband of 0.001 (0.003 for SIZE and TIME). Ruled out on the way: the gate, the SDRAM timing (libDaisy timing still crackled), input level, the LEDs, fast notes (desktop and the module's `RELEASE_DIAG` build both flat).
- **v1.0.39:** SIZE's crossfade in two staggered groups of lines (owner's pick B on `size_stagger`; ADR 0025 amendment). Three clicks in the prototype found and fixed before the page (waiting lines need their own in-loop amount, loss-filter blend, trip correction; HALL fades together where its diffusion joins or leaves).
- **v1.0.40:** ERA's overlap a fifth of each stretch (owner: "B but split the difference ... 20%"; ADR 0021 amendment), after `era_switch` (owner kept the overlaps for CV) and `era_overlap`.
- **Firmware tooling:** `RELEASE_DIAG=1` (the playing build on USB power, knobs held at noon, own input, a serial load report), `RELEASE_LATE_LIGHT=1` (rightmost LED red/amber after a late/near block), `RELEASE_SDRAM_TIGHT=0` (libDaisy timing), `PROFILE_ITCM=0`; m0test has a knob-jitter readout on serial and an LED jitter mode for rack power (SPACE left, ATTACK right). Profile: frozen corner last, `max at block N`, `REALTIME` lines.
- **Desktop tooling:** `IV_BLOCK_PROFILE=<csv>` per-block section timing in a `-DIV_PROFILE_HOOKS` renderer (relative only: see Will bite); prototype hooks `IV_ERA_SWITCH`, `IV_ERA_FADE_SHARE`, `IV_SIZE_STAGGER=1` (the old fade); page scripts `tools/pages/era_switch.py`, `era_overlap.py`, `size_stagger.py`.

**Gates** (branch HEAD `48d46a6`, `build-scratch` in the worktree): ctest **"100% tests passed out of 17"**. `tools/same_render.py` against v1.0.39: only the three ERA sweeps differ (the deliberate v1.0.40 change). Firmware (budget 112,640 B):

| Variant | Bytes | % of 128 KB |
|---|---|---|
| release | 112,548 | 85 % (**92 B under budget**) |
| release `-latelight` | 112,812 | test build, over budget on purpose |
| profile | 124,832 | 95 % (profile-only; the warning is expected) |
| m0test | 83,556 | 63 % |
| release `-diag` | ~129,700 | USB-power diagnostic only |

`main` HEAD: this handoff's commit. Plugin installed: `b39d451`.

## In flight

- **Nothing running.** Branch `claude/massive-air` kept until the owner OKs the merge into main (TASKS "Decide"); its `docs/TASKS.md` / `HANDOFF.md` are stale copies: at the merge take main's.
- **The owner's next flashes** (TASKS): round 15 profile on USB power, then `immensio_versio-latelight.bin` at the rack (SIZE moves in MASSIVE; ERA at 20 %). The `.bin`s sit in the worktree's `firmware/build/`.
- `screenlog.0` (untracked, main checkout root): the owner's screen log; rounds read so far are filed in `docs/research/`. It keeps growing while logging is on.
- Trash holds `immensio-cleanup-2026-10-26` (old temp, 15 GB) and `immensio-judged-pages-2026-10-26` (`era_switch`, `size_stagger`, `era_overlap`): the owner empties it.

## Next steps

1. **Read round 15** when the owner sends it (delegate the read to a subagent: compare with `m3-round14.log`): `MASSIVE neve SIZE .3<>.8`'s `max` and `MASSIVE S1 neve`'s max (the SIZE 0 → 1 jump) with the stagger; the frozen corner; `REALTIME` lines. Target: under ~95 %.
2. **The rack test of v1.0.40** (owner): drop-outs after SIZE moves, ERA's feel at 20 %. If drop-outs remain, the SIZE crossfade needs more (old early taps faded faster, or a page for three groups).
3. **A frozen MASSIVE with the cassette (104.5 %):** options, each a page or an owner call: a lighter twin while frozen (review item N, −10 to −35 % while frozen), the block size (`PROFILE_BLOCK=96`, −7.5 points model, +1 ms latency), the still lows at 6 kHz (`still_lows_rate` page still exists, −7-10 % in MASSIVE). Also time the ERA overlap positions on the module (cassette + radio at ERA ~0.82), never measured. A good nested-session task: prototype + page + gates.
4. **ERA's tape → radio hand-over** if the owner still hears it as abrupt: the tape drive's curve within the overlap (its sparkle needs near-full drive).
5. **Merge `claude/massive-air` into main** on the owner's OK; install the plugin (v1.0.40) when they want it (`tools/install_plugin.sh <commit>`, Ableton closed).
6. Housekeeping from the review (report section 8, item 7), each small.

## Waiting on the owner (`docs/TASKS.md`)

- Flash round 15 (USB) and play v1.0.40 with the late light (rack).
- Decide: merge the branch into main.
- Listen: `swell_seed` (session 6), `still_lows_rate` (only if more CPU is needed).
- Empty the Trash; reference recordings as before.

## Will bite

- **Flash: 92 B left.** Anything new in the release needs bytes found first. Cheap known sources: none left that are sound-neutral and measured; the constructors (`Engine()` / `Fdn()` member initialisers over already-zeroed storage, ~1-2 KB, unmeasured, risky), newlib `sinf`/`cosf` (~2.2 KB, not bit-identical). `-fno-math-errno` / `-fmove-loop-invariants` made the release *bigger* at -O3.
- **Desktop timing is not the module** (memory): the desktop is compute-bound, the M7 memory-bound. Desktop per-block timing finds which block spikes, never how much a memory saving is worth.
- **Knob readings on the module are noisy and the audio gets into them.** Any new knob-driven behaviour that restarts on change (crossfades, rest timers, memos) needs the deadband in mind. On USB power the knobs aren't powered at all (memory).
- **The release's knob path differs from the Plugin's** (smoothing + deadband, firmware only): SIZE/TIME move in 0.3 % steps on the module.
- **The stagger's per-line state** (`xfLine_`, `xfOff_`, `apAmtNow_/Old_` in `Fdn`) has no initialisers; `Fdn::reset()` sets them. Anything that reads them before `prepare()`/`reset()` reads garbage.
- **Two tank paths** (`lineFast` / `lineSlow`) plus now per-line fade state: any change to the crossfade goes into both paths and the waiting-line logic; `tools/same_render.py` and a 1 ms-frame click check at every move are the gates.
- **The profile build is at 95 %**; the all-variants warning is expected. The `-diag` build is near 128 KB.
- **Firmware objects in the worktree once pointed at a deleted worktree** (stale `.d` files): if a build complains about a missing `review-overnight/...` path, remove `firmware/build/obj-*` (literal paths) and rebuild.
- **`screenlog.0` holds several rounds**: when filing a round, cut it from its own BENCH/first CORNER to its last line (or let a subagent do it).

## Where to look

- `.claude/skills/sublustris-start/SKILL.md` ("Working as the orchestrator") and `docs/TASKS.md`
- `firmware/README.md` (rounds 11-14, the crackle hunt, v1.0.37-40, the build switches)
- `docs/research/m3-round14.log`, `m3-round13.log` (the baselines for round 15)
- `core/dsp/Fdn.cpp` (`control`'s crossfade start, `runBlock`, `lineSlow`), `firmware/main.cpp` (release `AudioCallback`'s knob reading)
- `docs/adr/0025-size-and-time-crossfade.md`, `0026-fitting-the-cpu.md`, `0021-era-sweet-to-gritty.md` (amendments)
