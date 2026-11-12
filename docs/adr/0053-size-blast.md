# 0053 — The SIZE blast: a shrink's boost limited, and a tank that goes invalid restarts

**Status:** Accepted, 9 Nov 2026 (owner, pick C on `renders/size_blast`). SPEC v1.0.77. Amends ADR 0025's one-trip correction. Issue 2. Nothing here has run on the module: the rack check is still to do.

**Context:**
- **The report** (chriisu, [issue 2](https://github.com/jeffebauer/sublustris-versio/issues/2), on v1.0.68, on the module): a fast SIZE move from big to small "blasts the speaker with very loud audio", in HALL or CANYON, a violin, probably BLOOM, gain turned up.
- **The cause, measured on the desktop** (`docs/research/size-blast-2026-11-09.md`, on `main`): SIZE's crossfade corrects the new read for one trip by the tail's own decay over the length difference (ADR 0025). On a shrink that's a boost, on the idea that the nearer sound is quieter. True for a dying tail; with the input still coming the nearer sound isn't quieter, so the boost is real gain in the loop. It had no limit: x3.2 at DECAY 0.6 on a moderate tone, x344 at DECAY 0.8 played hot, about 1e34 at DECAY 0.
- **A second fault, the same cause:** some of those moves turned the wet to NaN, and nothing clears a NaN from a loop: silence until a restart (inferred for the module).
- **The correction exists on purpose:** the owner's note "some jumps in amplitude with the crossfades". That's why the correction stays and normal moves are left as they were.

**Decision:**
- **C is the sound.** The boost after a shrink is limited to about 6 dB: `kXfMaxBoostLn` 0.6875 in `Fdn.cpp` (5.97 dB; the page's C was 6.00, and this value loads in one instruction). One-sided: a growth's cut and any boost under the limit are untouched.
- **The restart.** The Engine sums the wet of each chunk before anything with a memory downstream hears it. A NaN or Inf stays in the sum, so one test of its exponent catches it. On a failure: `reset()` (every tank and filter, the knobs kept), this chunk's wet dropped, and the reverb builds again with the next sound. Every build, Firmware and Plugin too. Pure Data's filters check their state once a block the same way.
- **The prototype's hook is gone** (`IV_SIZE_BLAST`, voicings A/B/D, its page script). Only the test accessors stay, behind `#ifndef IV_FIXED_VOICINGS`: no firmware bytes.

**Rejected:**
- **A, as built:** the report.
- **B, 3 dB:** the owner's call on the page; a normal move on a dying tail changed by up to about a dB.
- **D, C with the boost from DECAY's own loss, no governor:** fixes the hot high-DECAY case, not low DECAY, and the limit alone already covers both.
- **Breaking a big jump into steps:** changes the feel, a big move would take longer to land.

**Consequences:**
- **The numbers,** from the investigation's harness (release knob path: 1 ms blocks, 10 ms smoothing, end stops, 0.003 send threshold; 50 ms RMS, the loudest after the move against the loudest of the 4 s before it, dB), rerun on the port against the as-built A (`3905c32` with the hook off, `e4a9738`'s code):

| Scenario | As built | Port |
|---|---|---|
| The report's patch: CANYON, BLOOM, DECAY 0.85, TONE and WEAR 0.5, a violin-like tone peaking at 1.0, SIZE 1.0 to 0.05 in 40 ms | +4.8 | +1.2 (limiter 0.53 → idle) |
| The same, SIZE held still | +0.9 | +0.9, to the bit |
| The same over 16 s | +0.8 | +0.8, to the bit |
| The same in 12 rested steps | 0.0 | 0.0, to the bit |
| Worst of 8,640 hard moves (every SPACE, DECAY 0 to 0.85, SIZE 0.6 or 1.0 down to 0.05 or 0.2, 40 or 300 ms, violin, pad or bass at peak 0.3 or 1.0) | +19.3 | +13.8 |

- **A steady low note into a short CANYON still rises about 10 dB** (the page's figure; this harness reads +11.8 to +12.0 for 70 Hz at 0.3, CANYON, SIZE 1.0 to 0.2, as built +14.5). That isn't this boost: with the limit at 0.01 dB it still rises +10.5 to +11.7. The smaller room rings up on a low note. PLATE has no such correction and does the same on bass: up to +13.7 dB, unchanged here. Neither is in this ADR's scope.
- **Runs ending invalid:** 162 of the 8,640 moves on the as-built, 0 on the port, with 0 restarts: the limit alone prevents every NaN measured. The restart is the safety for anything else that gets there; the suite proves it by poisoning each tank.
- **Normal moves unchanged, to the bit:** 384 of 384 (SIZE moved 0.10 to 0.15 over 300 ms at DECAY 0.5 to 0.85 in every SPACE and BLOOM or NATURAL: tails and playing), 54 of 54 growths, 2,160 of 2,160 held controls. The limit engages on 3,317 of 5,760 HALL and CANYON moves of that hard grid, all shrinks. The cost: a dying tail shrunk far enough to reach the limit gets less make-up than it did, so it can dip through the move instead of gliding. The normal-move set doesn't reach the limit, so it can't show this; the owner heard it on the page's card 4 (C matched A there), and it wasn't measured further.
- **Restart on a poisoned tank:** FDN, still lows and plate each give no NaN out, one restart, the reverb back within 12 dB of its steady level inside 5 s of continued playing. The first chunk after a restart is silent, so a click is possible; heard only if a tank goes invalid, which didn't happen in any real move measured.
- **`same_render` against `e4a9738`:** SAME 60/60 (its sweep's SIZE moves don't reach the limit).
- **Goldens:** unchanged, none updated; they hold settled settings.
- **Tests:** `test_size_blast` is new (three hot shrinks bounded where as built fails each, two moves that went NaN stay finite and sounding, each tank poisoned and recovered, a hot held setting never restarts).
- **Flash:** release 112,228 B (+56, 412 B left of the 112,640 budget), profile 131,068 B (+80, 4 B left of the chip), m0test 83,556 B (unchanged, no reverb engine).
- **Not checked:** anything on the module: the player's patch, the rise, ARM float maths, a real violin (synthetic, mono into both inputs), real knob noise. The owner hasn't heard the port (only the page's C). The research's module test is the rack check.
