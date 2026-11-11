# 0056 — SIZE after an unfreeze: the room makes way, and keeps its size

**Status:** Accepted, 10 Nov 2026 (owner: A, "the room makes way", on all 24 cards of `renders/unfreeze_size`; then their own idea on the rack, "fix the twin so size doesn't affect it"). SPEC v1.0.80. Research: `docs/research/unfreeze-size-2026-11-10.md`.

**Context:**
- **The owner, on round 2:** "A crackle when I turn SIZE during the second the pad fades out after I let go of FREEZE, in HALL and CANYON." Players heard it too. It's a module overload, not a click, and it's older than v1.0.68: the 29 Oct audit's UNFRZ lines ran up to 107.5 % in CANYON (`docs/research/m3-review2-audit-212b68b.log`).
- **What runs at that moment:** the small room (the twin, the layer you play over the pad) still rings out after the unfreeze, at 10-13 % of the block, while SIZE's crossfade makes a quarter of the tank's lines read twice. Either fits on its own; the two together don't.
- **The desktop never crackles,** so the page judged only how each fix feels, and module laps judged the CPU.

**Decision:**
- **The room makes way (A).** Unfrozen in HALL and CANYON, while the room still rings after an unfreeze, a SIZE off the one the pad froze with waits: the tank keeps its lengths, the room fades out in 0.3 s (a morph's fast drop) and stops, then the latest SIZE lands once, as any SIZE move does. A SIZE moved while frozen starts it at the unfreeze. A new freeze during the wait ends it: the pad keeps the lengths the tank has, and a room fading out comes back up. PLATE is unchanged (its twin is a second Plate, and its SIZE glides).
- **The room keeps its size** (the owner's idea, after hearing A on the rack). In HALL and CANYON the room takes the SIZE the pad froze with when it starts, from silence, and keeps it, frozen and ringing out. SIZE never bends its lines, so the room's glide (per-sample work in the second after an unfreeze) is gone from the code. Everything the room's SIZE sets (its T60 scale, its level trim) follows the frozen SIZE.

**Rejected:**
- **B, the room rings, SIZE waits** (to 30 dB down or 1.5 s, then makes way): more of the echo, but a longer, rubberier lag. The owner picked A on every card.
- **C, today:** SIZE lands at once while the room rings on (the version that crackles).
- From the research, not put on the page: gliding SIZE after an unfreeze (the pitch bend the owner turned down for HALL and CANYON, ADR 0025), waiting until the room stops on its own (SIZE dead for seconds), 8 crossfade groups ("some slight staggering"), and trimming the room's idle work (a few points at most).

**Consequences:**
- **SIZE answers up to ~0.3 s late after an unfreeze** in HALL and CANYON, and only while the room still rings. The quiet echo of what was played over the pad is cut short when SIZE moves then; it's already about 8 dB under the pad and fading.
- **SIZE no longer plays the small room while frozen** in HALL and CANYON (it still plays PLATE's twin). The room's lines stay where the freeze put them.
- **Module CPU:** today's build overloaded on the owner's gesture with SHIMMER on (to 110.7 %, `docs/research/m3-unfreeze-size-shimmer-control-lap.log`). A alone: 34 lines 0 late, worst 97.8 % (`m3-unfreeze-size-A-lap.log`); with the room keeping its size, 0 late in two laps, the owner's gesture HALL at most 92.9 %, CANYON at most 95.0 % (`m3-unfreeze-size-shimmer-fixedroom-lap.log`). In the final build's lap (`m3-flipflick-landing-lap.log`, with ADR 0057 and 0058) the unfreeze lines are 83.9-95.2 %, 0 late.
- **Everything else renders the same:** `same_render` SAME 60/60 against v1.0.79; goldens unchanged. `host/tests/test_unfreeze_size.cpp` checks the wait, the room's stop, a re-freeze, PLATE, changes with no freeze, and the room's fixed lines.
- Flash: with ADR 0057 and 0058, release 112,236 B (404 B under budget). The profile build leaves out its boot micro-tests to fit (`firmware/main.cpp` `IV_PROFILE_MICRO`).
