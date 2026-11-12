# 0058 — The flip waits for SIZE

**Status:** Accepted, 11 Nov 2026 (owner: A, "the flip waits for SIZE", on all 14 cards of `renders/flip_waits_size`). SPEC v1.0.80. Amends ADR 0054 ("finish, then go" now covers SIZE's crossfade too). Its CPU part, SIZE's fast crossfade paths on lines filled deep enough, is sound-neutral and needed no page. Research: `docs/research/flipflick-landing-2026-11-11.md`.

**Context:**
- **What was left after ADR 0057** (`docs/research/m3-flipflick-B-lap.log`, `m3-flipflick-B-OFF-lap.log`): two overloads, both heard as crackles.
  - **Flipping SPACE while SIZE is still settling** (SIZE moved, then SPACE within about a second): the old tank's SIZE crossfade ran on under the morph or the reshape, beside the new tank's start-up. 109.7-125.5 %, up to 337 late blocks, with or without ADR 0057.
  - **The landing of ADR 0057's held SIZE** (about 3.4 s after a PLATE -> CANYON flip): the new tank's lines aren't full yet (5.46 s), so the crossfade took its slow paths, about 5 points dearer. 100.8-102.5 %.

**Decision:**
- **The flip waits for SIZE (2a).** While the tank in use crossfades its SIZE, or waits for the knob's rest to start one, the SPACE switch isn't read, as during a morph; when it ends, the switch's latest position starts a normal change. The wait is **capped at 1.3 s from the switch** (`Engine::kFlipWaitsSizeCapSeconds`), so a SIZE that never rests (CV) can't hold SPACE for ever; at the cap the flip goes as before, over the crossfade. Not while frozen. A SIZE moved inside the flip's own 0.05 s fade-out waits for the handover, so the tank about to leave never starts a crossfade under the morph or the reshape.
- **SIZE's fast crossfade paths on lines filled deep enough (1a).** `Fdn::lineXf` and the old early taps' fast stream also run on a line still filling after a reset, once its writes reach every read in the block (old and new reads and taps, plus the block, plus 4). Each read then gives what the slow path gives, so the output is the same to the bit.

**Rejected:**
- **C, today (ADR 0057 alone):** the flip goes at once, over the crossfade (the version that crackles).
- From the research, not put on the page: snapping the crossfade to its end at the flip (a click in the old tail), stopping the old tank's reshaping (doesn't remove the peak), resting SHIMMER's heads into PLATE too (a few points against 17-27 needed). For the landing: landing later, once the lines are full (SIZE dead for 5.5 s), in smaller steps (no help), or in 8 groups.

**Consequences:**
- **SPACE answers up to ~1.2 s late after a SIZE move,** from HALL or CANYON (up to ~0.5 s from PLATE), never more than 1.3 s from the switch, and only when SIZE moved in the second before. With SIZE still, a flip goes as before.
- **Module CPU, the final build** (`docs/research/m3-flipflick-landing-lap.log`, with ADR 0056 and 0057): the 4 flip-into-SIZE-crossfade lines 93.3-98.9 %, the 15 flip-flick lines (the landing) 85.1-98.9 %, all 47 lines 0 late, worst 98.9 %. Both crackles left after ADR 0057 are gone.
- **Everything else renders the same:** `same_render` SAME 60/60 against v1.0.79; goldens unchanged. The fast paths were checked to the bit against a build forced onto the slow paths on the prototype branch. `host/tests/test_flip_waits_size.cpp` checks that no SIZE crossfade runs once a change has begun, the wait, the cap, a still SIZE, a SIZE that never rests, and a SIZE moved in the flip's fade-out.
- Flash: with ADR 0056 and 0057, release 112,236 B (404 B under budget).
