# 0057 — SIZE waits for the morph

**Status:** Accepted, 10 Nov 2026 (owner: B, "SIZE waits for the morph", on all 17 cards of `renders/flipflick_size`). SPEC v1.0.80. Amends ADR 0054 (what SIZE does during a PLATE <-> HALL / CANYON morph). Research: `docs/research/flipflick-size-2026-11-10.md`.

**Context:**
- **The owner, on the fixed-room build:** "switching between PLATE and CANYON (or CANYON to PLATE) and quickly moving the SIZE from one extreme to the other" crackles, with no freeze involved. Players heard it too. It's in v1.0.79 as well: the laps with and without the fixed room agree to within 0.5 points (`docs/research/m3-flipflick-fixedroom-lap.log`, `m3-flipflick-control-lap.log`).
- **What runs at that moment:** for 3 s after a PLATE <-> HALL / CANYON flip two big tanks run (ADR 0054), and a SIZE move started the new tank's own SIZE crossfade on top of both. PLATE -> CANYON with SHIMMER ran 107.8-115.2 %, 1,201 late blocks on every line; CANYON -> PLATE 109.7-110.4 %, 300 late.

**Decision:**
- **During a two-tank morph the new tank keeps the SIZE it started with** (the SIZE at the flip). A SIZE off it waits while the old tank fades over its own 3 s; once the old tank has gone, the latest SIZE lands, once, as any SIZE move does (one crossfade). Everything SIZE sets in the new tank (lengths, T60 scale, trim) waits with it.
- **A freeze during the wait ends it:** the pad keeps the lengths the new tank has, and SIZE lands after the unfreeze (with ADR 0055's rules for a freeze during a morph, and ADR 0056's for after an unfreeze).
- **HALL <-> CANYON reshapes are unchanged:** one tank, so a SIZE move goes through the reshape's own crossfade as before.

**Rejected:**
- **A, the old space makes way** (a SIZE move sends the old tank out in 0.3 s, then SIZE lands): SIZE answers sooner, but the old tail is cut short. The owner picked B on every card.
- **C, today:** SIZE lands at once while both spaces ring (the version that crackles).
- From the research, not put on the page: starting the new tank at the new SIZE (a click or smear, and only for flicks in its first half second), a lighter overlap (not enough points), 8 crossfade groups (not enough, and "some slight staggering").

**Consequences:**
- **SIZE feels held for up to 3 s after a PLATE <-> HALL / CANYON flip,** then the whole move lands at once. Outside those 3 s nothing changes.
- **B's landing was the next overload:** it falls about 3.4 s after the switch, before the new tank's lines have filled (5.46 s), so SIZE's crossfade took its slow paths: 100.8-102.5 %, up to 111 late blocks (`docs/research/m3-flipflick-B-lap.log`). ADR 0058's CPU part (the fast paths on lines filled deep enough) takes those points back.
- **Module CPU, the final build** (`docs/research/m3-flipflick-landing-lap.log`, with ADR 0056 and 0058): the 15 flip-flick lines 85.1-98.9 %, 0 late; all 47 lines 0 late, worst 98.9 %.
- **Everything else renders the same:** `same_render` SAME 60/60 against v1.0.79; goldens unchanged. `host/tests/test_flipflick_size.cpp` checks that no SIZE crossfade runs while the old tank rings, the wait's end at the old tank's stop, the landing, a freeze during the wait, and reshapes that never wait.
