# Review 2: MASSIVE's CPU margin (review2/cpu, 28 Oct 2026)

The owner's question: "With SHIMMER + WEAR 0.5 + MOTION's tape side while SIZE moves, the profile lap peaks at
93-95 % in MASSIVE (target <= 90 %; release runs a point or two heavier). Find a safe further saving."

Branch `review2/cpu` (off `review/overnight-2`, v1.0.55). Nothing here was run on the module: every CPU figure for
the module below is an **estimate** from the disassembly and the module's own MICRO numbers (round 11:
`exp 112.7`, `pow 208.8`, `log10 157.1`, `sin 85.1`, `vdivDep 18`, `vdivThr 19`, `vdivIndep 22` cycles), except
the ABSEXP lines, which are **measured** (docs/research/m3-cpusave2-7ee389e-full.log). One block = 48 samples =
480,000 cycles at 480 MHz; 1 point = 1 % of that = 4,800 cycles a block (= 100 cycles a sample).

## 0. Summary

| # | What | Bits | Points (est.) where | Flash (release) | Owner / module |
|---|------|------|---------------------|-----------------|----------------|
| A | `ABSORB_EXP=1`: the loss filters on `std::exp` | not the same (desktop ≤ 7e-7, 1 ulp-ish; up to 5e-4 single samples where WEAR's grain rounds the other way, residual 78-83 dB under) | **−1.3 measured** in `tkabs` (3.4 → 2.1); the ABSEXP lines' worst blocks −1.5 to −4.1 | +16 B | no ears needed in my view (inaudible by 60+ dB); module lap to confirm |
| S | `SHIMMER_POLY=1`: the shimmer heads' window with no divide | not the same: residual 62-69 dB under the wet at TIME full | **~−2** (−1.9 to −2.7) whenever SHIMMER plays | +16 B | a sound change (tiny): owner's OK by the rules; module lap |
| B1 | a low tank skips its unread governor levels | same (SAME 60/60) | −0.2 in HALL / MASSIVE, always | 0 B | committed |
| B2 | SIZE's trip correction skips its divide once done | same (SAME 60/60) | −0.15 (creep) to −0.47 (sweep) while SIZE moves | 0 B (with B1) | committed |
| B3 | a done line's taps skip their correction | same (SAME 60/60) | −0.3 (sweep), ≤ −0.06 other hands | +32 B | committed, optional |
| B4 | the fade gain skipped for lines waiting (staggered) | same (SAME 60/60) | ~−0.2 in every block a SIZE fade runs | +24 B | committed, optional |

Flash: the branch's release with everything is **112,620 B** (B1-B4; 20 B under the 112,640 budget);
B1-B4 + A + S **112,652 B, 12 B over**. Fits: B1 + B2 + A + S = **112,596 B** (44 B left), and one of B4 (+24) or
B3 (+32). My pick: B1 + B2 + B4 + A + S = **112,620 B** (measured, 20 B left; B1 + B2 + B4 alone 112,588) for about **−4.5 points** on the sweep's worst
block (est.: 94.8 → ~90), ~−4 on the twists and creep. That is about the target in the profile, not in release
if release really runs 1-2 points heavier: the measurement on the module decides.

## 1. Why `make 'TRIAL_DEFS= -DIV_ABSORB_EXP'` gave the same .bin (verified)

- `BUILD_DIR` (firmware/Makefile, `override BUILD_DIR = build/obj-$(MODE)-$(PROFILE_BLOCK)...`) is named from
  `RELEASE_TAG` / `PROFILE_TAG`, which hold `TRIAL_TAG`, not `TRIAL_DEFS`. A command-line `TRIAL_DEFS` changes
  the compile flags but not the directory, and make tracks files, not flags: every object in `obj-release-48`
  was newer than its source, so nothing compiled. `make -n` with the override prints **0 compile commands**,
  only the three `cp build/obj-release-48/... build/...` lines: the old release, copied up again.
- A command-line variable replaces the whole computed `TRIAL_DEFS`, so with `SIZESPIKE=4` the
  `-DIV_PROFILE_SIZESPIKE=4` define vanished (`make -n MODE=profile SIZESPIKE=4 'TRIAL_DEFS= -DIV_ABSORB_EXP'`: 0
  occurrences) while the directory and name still said `-cpusave` (`PROFILE_TAG` reads `SIZESPIKE` itself). A
  fresh tree would have built the corner lap under the cpusave name.
- The hazard the other way: after a clean, the override builds `exp` objects into `obj-release-48` and names the
  result `sublustris_versio.bin`; every later default build reuses them. Never override `TRIAL_DEFS`.

## 2. The switches

`make -C firmware ABSORB_EXP=1` / `SHIMMER_POLY=1` (release or profile, together or apart) add `-DIV_ABSORB_EXP` /
`-DIV_SHIMMER_POLY` to `C_DEFS` through `TRIAL_DEFS`, and `-absexp` / `-shpoly` to `TRIAL_TAG`, so each gets its own
object directory and .bin (`obj-release-48-absexp`, `sublustris_versio-absexp.bin`,
`sublustris_versio_profile-absexp-shpoly-cpusave.bin`, ...). `all-variants`' size table names the tagged files (the
same names when no switch is set). The Renderer: `IV_ABSORB_EXP=1`, `IV_SHIMMER_POLY=1` (host/render/main.cpp, set
before `prepare()`; globals `prof::absExp` / `prof::shimmerPoly` in core/dsp/ProfileHook.h, desktop builds only; the
module's builds are compile-time only, so the profile's head loop isn't touched).

Default builds byte-identical, each step: a clean `make all-variants` before and after the Makefile change, `cmp`
release 112,564 B, m0test 83,556 B, profile 127,808 B: identical; again after the Renderer hooks (Governor.h,
ProfileHook.h: `#ifndef IV_FIXED_VOICINGS` only); the shimmer prototype keeps the firmware's text of the loop
exactly (`cmp` against c79dcd7's release and profile: identical). A first version that wrote `w = s * s` for every
build gave the same size but different bytes (GCC scheduled the loop differently), so the default keeps the
original expression.

The ABSEXP profile lines: `make -C firmware MODE=profile SIZESPIKE=4 ABSORB_EXP=1 SHIMMER_POLY=1` builds the CPU-save
lap with both trials in every line (its `ABSEXP` lines then repeat the `SHIMMER` ones: `IV_ABSORB_EXP` ignores
`prof::absExp`); flash `SIZESPIKE=4` alone for the A side. Sizes at e488c61: 120,804 B and 120,724 B.

## 3. A: ABSORB_EXP

**Flash** (measured): release 112,564 → 112,580 B (+16): `Fdn::control` +8 B, `absorbent` +4, alignment. `powf`
stays linked (Output.h TONE, EraTexture.h, Drift.h, Transport.h, Engine.cpp all call `std::pow`), so the swap frees
nothing; `expf` (0xe4 B) was already linked. Dropping `powf` everywhere would free 0x318 + 0x128 (`__powf_log2_data`)
= 1,088 B, but every other caller would have to change (all control-rate, all sound changes): not tried.

**CPU** (measured on the module, CPU save 2 lap): `tkabs` 3.4 → 2.1 % on the worst MASSIVE lines (64 `powf` a
block at 209 against 64 `expf` at 113: 6,100 cycles = 1.3 points, as measured). The lines' worst blocks: sweep
94.8 → 90.7 / 90.6, twists 94.0 → 92.4 / 92.5, creep 93.6 → 91.8 / 92.5 (two laps). Averages −1.4 points. Why every
chunk: at DECAY 0.9 (past `kDecayInfiniteAt` 0.85: governor weight 1) the floor governor's `extraRate` (and WEAR's
tape compression, `compress`) move every chunk, so `absChanged` re-works all 16 lines of both tanks, twice a block.

**Sound** (measured, desktop, `tools/review2/null_test.py`, 12 s of 04_skank, IV_ABSORB_EXP 0 against 1):

| case | max abs diff | residual RMS under the render |
|------|--------------|-------------------------------|
| MASSIVE SHIMMER W.5 M0 D.9 creep / still / sweep wet, HALL creep, MASSIVE D1 wet | 0 (bit-identical: slow loss, tiny exponents round the same) | — |
| PLATE D.9 W.5 | 7.2e-7 (−122.8 dBFS) | 123 dB |
| MASSIVE D.5 wet | 1.7e-7 (−135.5 dBFS) | 130 dB |
| MASSIVE D.7 sweep wet | 3.9e-7 (−128.2 dBFS) | 123 dB |
| MASSIVE D.2 WEAR 1 wet | 2.9e-4 (−70.7 dBFS), single samples | 83 dB |
| HALL D.5 WEAR 1 wet | 5.3e-4 (−65.5 dBFS), single samples | 78 dB |

With WEAR at 1 the grain's quantiser rounds a sample the other way now and then; the residual stays 75-80 dB under
the render second by second over 12 s (it doesn't grow). Inaudible. On the module newlib's `powf` and `expf` differ
from Apple's, so these are desktop evidence; the module's own difference should be of the same order.

Not done, an add-on under the same switch: `absorbLine`'s two divides by `onePlusK` as one shared reciprocal
(`AbsorbShared` would carry `1 / (1 + K)`): 2 divides a line, 64 a block, ~1,200 cycles = **−0.25 points**, ~1 ulp
apart; +~8 B.

## 4. S: the shimmer heads' window without a divide (SHIMMER_POLY=1)

Each of the 16 heads (8 a channel) works out sin(πp) as Bhaskara's 16q / (5 − 4q), q = p(1 − p), then squares it:
one `vdiv` a head and sample. The module measured a divide at 18-19 cycles and showed it holds the FPU up
(`vdivIndep` 22: four independent multiplies behind it hid only one cycle). The trial uses a least-squares fit
w = q²(9.91796 + q(18.84379 + 21.92742q)) (Shimmer.h `shimmerWindowPoly`): **within 4e-5 of sin²** (Bhaskara's square
is within 2.1e-3), and the eight heads still sum to 4 within 1e-5 (Bhaskara's: 3.9949-3.9952, so the octave comes
out 0.011 dB louder). Disassembly (release vs `-shpoly`, `Engine::process`): the head loop loses its `vdiv` and
`vmul s, s`, gains three multiplies, two adds and three literal loads (the loads pair with FP ops); the dependent
chain of a head drops from ~39 cycles (q, 4q, 5 − 4q, the 18-cycle divide, s², ×, +) to ~27. **Estimate −12 to −17
cycles a head, −190 to −270 cycles a sample: −1.9 to −2.7 points whenever SHIMMER plays** (the lap's `shim` 14.8 %).

**Sound** (measured, desktop, IV_SHIMMER_POLY 0 against 1): MASSIVE SHIMMER creep (the lap's settings, BLEND .5)
max 9.3e-5 (−80.6 dBFS), residual −99.5 dBFS, 69 dB under the render; MASSIVE SHIMMER wet TIME 1: max 1.5e-4
(−76.4 dBFS), residual 62 dB under; PLATE SHIMMER wet TIME 1: 1.2e-4, 62 dB under. A slightly different (truer)
grain window: inaudible in my judgement, but it is a sound change, so it needs the owner's OK by the rules.
Flash +16 B (112,580). Renderer bit-identical with the hook off (SAME 60/60).

## 5. B1-B4: the same output to the bit

Evidence: `tools/same_render.py` against the pinned ref-main renderer, SAME 60/60 after B1+B2 (twice: before and
after B2's final form), after B3 (both forms), after B4; ctest 21/21 at the end (see 7). Counts of how often each
path runs come from a desktop renderer with counters in `Fdn::control` (scratch build, not committed), 12 s of
music at the lap's settings (MASSIVE, SHIMMER, WEAR .5, MOTION 0, DECAY .9), the last 6 s turned by
`tools/review2/make_size_hands.py` (the lap's four hands through release's 0.003 deadband), per 48-sample block:

| hand | trip-correction calls | in their hold | done | ramp (`expf`) | taps B3 skips |
|------|------|------|------|------|------|
| creep | 157 | 94 | 34 | 28 | 5 |
| sweep | 159 | 28 | 112 | 19 | 77 |
| twists | 155 | 73 | 52 | 30 | 17 |
| nudge | 152 | 70 | 51 | 31 | 13 |

- **B1** (Fdn.cpp `control`, Governor.h `skip`): the still lows' tank (`lowFdn_`) follows its leader's governor
  (`gov = leader_->governor_`), and nothing reads its own levels (Engine reads `fdn_`'s only: Engine.h:252-253,
  Engine.cpp:560/816), but its `governor_.control()` still ran its three `log10f` every chunk. Now it drops the
  chunk's power. 2 chunks a block: 2 × (3 × 157 + ~30) ≈ 1,000 cycles = **−0.2 points**, in HALL and MASSIVE always.
- **B2** (the trip correction's `corr` lambda): once a line's or tap's trip is done, `(s − hold) / xfLen_ >= 1` cost a
  divide each call. `s − hold >= xfLen_` gives the same answer (a correctly rounded divide is monotonic and
  xfLen_ / xfLen_ = 1), and below it the quotient is at most 1, where `exp(lnGain · 0)` is exactly the 1 the old
  `r >= 1 ? 1 :` gave, so that compare went too (it paid for B2's bytes). 34-112 divides a block at ~20 cycles:
  **−0.15 (creep) to −0.47 (sweep) points** while SIZE moves.
- **B3**: a tap's hold is a share of its line's, so its trip ends no later. Once the line's own has been done a whole
  control block (s moves at most `kControlBlock` between evaluations), its taps were evaluated done at the last one
  and hold 1: skipped. 77 calls a block in the sweep at ~18 cycles: **−0.3 points**, ≤ −0.06 for the other hands.
  +32 B (the lambda returns a bool; three other forms cost 32-40 B).
- **B4**: the crossfade's agreement gain (`xfK_`: a divide and a square root a line) was worked out for all 8 lines
  each control call while a fade ran; staggered, a line waiting for its fade never reads it (`qi` 0: lineFast) and a
  fade's start sets it to 1. Skipped for those (6 of 8 with 4 groups): ~6 × 46 cycles × 4 calls ≈ 1,100 cycles =
  **−0.23 points** in every block a fade runs (5,000-5,200 of the lap's 6,000 in the creep / twists / nudge lines).
  The `staggered() &&` stays (+8 B over `!xfLine_[i]` alone) because the profile changes the stagger between lines.

## 6. What else I looked at (not done), estimates

- **The trip correction's ramp on a straight line in gain** (`val = g0 + (1 − g0) r` instead of `exp(lnGain (1 − r))`,
  g0 kept per line and tap): 19-31 `expf` a block while SIZE moves, **−0.45 to −0.75 points**. Not the same bits (a
  difference of order lnGain²/8: ~1e-7 for the creep's steps, a percent mid-ramp for a 0.2 → 1 jump): a tiny sound
  change; +224 B RAM, est. +30-50 B flash.
- **The tank's per-line divides by the chunk length** (`readStep_`, `tapStep_`: 16 a call): on the module the chunks
  are 16 / 8 / 4 samples, powers of two, where `x / n` and `x * (1 / n)` agree to the bit; elsewhere (the Plugin's odd
  blocks) they don't, so it needs a power-of-two test. ~−0.2 points, est. +20 B.
- **The predelay crossfade** (`Engine::predelayCrossfade`): MASSIVE's predelay follows SIZE (Mappings.h:80), so
  every SIZE move also runs a 0.3 s predelay crossfade: the lap's `in` 6.9 → 9.2 % while SIZE moves. It calls the
  out-of-line `DelayLine::readCubic` twice a sample (two 18-cycle Horner chains that can't overlap across the calls,
  and the index maths twice for two lines in lockstep). One reader for both channels: est. **−0.35 points** during
  the fade, the same bits, but +100-150 B. Only with flash found elsewhere.
- **The governor's fast level** is `log10f`'d by `Fdn::control` and again by `Engine::controlTick` (Engine.cpp:560):
  keep it: −0.07 points. Not worth bytes.
- **`tkctl` rises 5.3 → 8-11 % while SIZE moves** (the lap's SPLIT). The counts above account for ~1.6 points of it
  (the trip corrections ~1.3, the fade gains ~0.3); the rest isn't explained by call counts. A profile mark around
  the trip-correction loop and the fade-gain loop inside `Fdn::control` would say where the remaining 2-3 points go.

## 7. Could not check

- Anything on the module: no flashing tonight. Points for S and B1-B4 are estimates (assumptions above); A's are
  the measured ABSEXP lines.
- Whether the desktop's bit-identity of B1-B4 holds on the module: they change no float operation that runs (they
  skip work whose result is unread or provably 1), so they hold there by construction; `cmp` can't show it since the
  code changes.
- The release-vs-profile gap ("release runs a point or two heavier"): not looked at.
- ctest: see the branch's last commit message.
