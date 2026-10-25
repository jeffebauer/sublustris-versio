# Overnight review: architecture, implementation, technique and performance

Paste everything below the line into a new Claude Code session (Fable 5.1 recommended), started in
`/Users/jesse/Documents/Sites/immensio-versio`. It runs unattended overnight and leaves a report.

---

You're doing an independent, in-depth review of Immensio Versio, a reverb firmware for the Noise Engineering Versio (Daisy Seed, STM32H750, Cortex-M7 at 480 MHz). Nobody will answer questions tonight: work on your own judgment, and leave decisions that are the owner's (anything that changes the sound) as clearly argued options.

## Why this review

The design was built desktop-first and turned out about 2.6× too heavy for the module. Ten rounds of measurement on hardware (M3) got it to about 1.3× over at worst. Some of that was paid for with sound: the HALL / MASSIVE tank at 24 kHz, a 12-point swell, fewer early reflections. The owner wants to know:

1. **Performance first:** where the remaining CPU goes, what established techniques could recover it **without** further cost to the sound, and whether any trade-off already made could be undone by a better implementation.
2. **Architecture and implementation:** is the structure sound, and is the code idiomatic for DSP on a Cortex-M7?
3. **Technique:** how do our algorithms compare with established papers, products and open-source examples?
4. **Sound and musicality:** concrete opportunities, with sources.

## Read first (in this order)

- `CLAUDE.md`: the rules. They bind you.
- `docs/handoff/HANDOFF.md`: the current state. **The code is on branch `claude/massive-air`** (worktree `.claude/worktrees/massive-air`), not on main.
- `SPEC.md` (changelog v1.0.30-v1.0.36 and §4, §5); `CONTEXT.md` (the vocabulary: use its terms).
- `docs/adr/`, especially 0007, 0008, 0012, 0021, 0024, 0025 and 0026.
- `docs/research/`: what has already been studied. Don't redo it; build on it or challenge it.
- `docs/TASKS.md`: the owner's open items.
- The measured CPU data:
  - HANDOFF's M3 row, and TASKS' Done entries for rounds 1-9b;
  - the profile firmware: `firmware/main.cpp` (MeasureCorner, the MICRO tests, the headroom test);
  - `firmware/README.md`.
- The code: `core/dsp/` (Engine, Fdn, Plate, HalfRate, Swell, Shimmer, Era, EraStages, Tape, Memory, Governor, Drift, Oversampler) and `core/params/`.

## Hard limits on what you may do

- **Work only in a new worktree** branched from `claude/massive-air`, e.g. `review/overnight`. Commit there with explicit paths (never `git commit -a`). Don't touch main, don't merge, don't push.
- **Never delete or recreate `build/`.** Use your own build directory.
- **No sound changes outside prototypes.**
  - A prototype goes behind an `IV_*` renderer hook or `#ifndef IV_FIXED_VOICINGS`, and is set **before** `prepare()` when it changes the layout. A count set after `prepare()` once produced a mislabelled page (memory: verify-voicing-labels).
  - A "sound-neutral" claim must be proven: 32-bit float renders compared sample for sample against a reference renderer built from the parent commit, through SIZE moves, SPACE flips and freezes.
- **No hardware:** no flashing, no serial, no `auval`, no Audio Unit or `coreaudiod` commands, nothing in Ableton.
- **Licences (ADR 0006):** no GPL code (Plateau, thement's emulator are reference only), nothing unlicensed. MIT and BSD code may be cited and learned from.
- **Flash is the tightest limit:** 460 B left of the 110 KB release budget. Any change must report its flash cost (`make -C firmware all-variants`).
- **The tests must stay green:** gate on ctest's "100% tests passed" line in a log file.

## What to produce

**1. A report:** `docs/research/review-2026-10-26.md`, committed on your branch. Plain language, for a designer new to DSP. Explain each idea in a sentence before going into detail. Sections:

- **Summary** (one page): the five to ten most valuable findings, ranked.
- **Where the CPU goes:**
  - your own model of each stage's cost on the M7: cycles a sample, from the code, the disassembly (`arm-none-eabi-objdump`) and the round 6-9b measurements;
  - reconciled with the measured shares (tank ≈ 75 % of the worst case; swell ≈ 15 %; ERA's stages up to ~8 %; control 7-10 %).
  - **Explain why the arithmetic costs what it does:** the round 8 test ran a 12 kHz tank in fast memory at ~3,750-4,050 cycles a sample.
- **Performance opportunities.** One table, then a section per item. For each:
  - what it is and why it should work, with a source;
  - the estimated saving in % of the module's budget (state the assumptions);
  - flash cost;
  - risk;
  - **sound impact: none (provable) / tiny / audible.**
  - Cover at least:
    - the FDN's per-sample structure (unrolling across lines, FP latency and dual issue on the M7, the per-line divide in `readAllpass`, branches in the hot loop);
    - block-based processing, and which parts can be vectorised (CMSIS-DSP, M7 has no float SIMD but has load/store pairs);
    - memory placement: AXI SRAM, DTCM, ITCM for hot code (64 KB unused), int16 storage for long lines, what goes where;
    - ERA's cassette (4× oversampled tape) and AM radio (per-sample `sin`, divisions, per-block filter design), and the BBD;
    - control-rate work and libm use;
    - the still-lows second tank (cost against what it adds);
    - the swell and early reflections (structures that give the same density for less);
    - the shimmer and the twin;
    - compiler flags and attributes (`-O2`/`-O3`/`-Os` mixes, `-ffp-contract`, `__restrict`, alignment), denormal flushing (FPSCR FZ).
- **Undoing trade-offs:** for the 24 kHz tank, the 12-point swell and the fewer early reflections, what it would take to afford the original, and whether a cheaper structure can sound like it.
- **Architecture and implementation review:** structure, coupling, the Engine's size, the test strategy, numerical robustness (denormals, NaN, stability under modulation), real-time safety, anything that will bite at scale.
- **Technique against the literature.** Compare each algorithm with established work and say where ours differs and whether that's a weakness:
  - FDN design: Jot & Chaigne 1991; Schlecht & Habets on FDN design, modulation and spectral colouration; Välimäki et al., "Fifty Years of Artificial Reverberation" (2012);
  - the Dattorro plate (1997);
  - Julius O. Smith's PASP (delay-line interpolation, crossfading);
  - Costello's Valhalla DSP blog;
  - Keith Barr / Spin Semiconductor (FV-1) notes;
  - Mutable Instruments' Clouds / Rings / Elements reverbs (MIT);
  - DaisySP (MIT);
  - ST's application notes AN4839 (L1 cache) and AN4891 (STM32H7 system architecture and performance);
  - Arm's Cortex-M7 optimisation guidance.

  Fetch sources and cite them (URL, section). Mark anything from memory as unverified.
- **Sound and musicality opportunities,** separate from performance: what could sound better or play better, with reasons and sources.
- **What I'd do next:** an ordered plan for the next working session.

**2. Evidence for the claims that matter.** Where a performance idea can be tested on the desktop, prototype it on your branch and measure:
- relative timing with a small benchmark, noting that desktop ratios only roughly follow the M7;
- static instruction counts from the ARM disassembly;
- bit-identity for sound-neutral claims.

Put the numbers in the report. If a promising idea needs the module to confirm, add a timing to the profile build's MICRO tests on your branch and say so; the owner flashes in the morning. Don't flash or build anything for the owner to install.

**3. Listening candidates (optional).** If a sound-affecting option looks worth it, render level-matched candidates with `tools/review/make_review.py` into `renders/<name>/`. Leave them unsent; list them in the report. Keep `renders/` under ~2 GB.

## How to work

- Check the existing measurements before trusting a hunch. Three hunches this project chased cost a round of hardware each (memory: measure-owner-notes-first, research-before-designing).
- Commit the report as you go (each section when it's done), so an interrupted night still leaves something useful.
- Be honest about uncertainty: give ranges, say what's measured and what's estimated, and say when an idea is probably not worth it.
- **When you finish:** a final commit on your branch, and a short summary at the top of the report of what you changed (prototypes and tests added), with the branch name and HEAD.
