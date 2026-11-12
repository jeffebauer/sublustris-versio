# 0055 — FREEZE just after a SPACE flip holds the old space

**Status:** Accepted, 10 Nov 2026 (owner: "Old space"; heard on the rack with round 2's release build, "it no longer crackles when changing SPACE and freeze in quick succession"). SPEC v1.0.79. Amends ADR 0054's "a freeze during a morph".

**Context:**
- **The owner's rack note on v1.0.78** (10 Nov): flipping SPACE and pressing FREEZE a moment later gives one dropout, a staticky crackle. Every PLATE change, and HALL -> CANYON. FREEZE first, then SPACE, is clean.
- **The desktop hears no click** in about 9,000 renders. The module overloads: the measuring lap (`docs/research/m3-freeze-at-flip-lap.log`) ran 104-120.7 % on all 9 lines with FREEZE 100 ms after a flip, 30-843 late blocks. During a morph, a freeze ran both tanks, the twin room and the old send-back handover at once.

**Decision:**
- **A freeze while the old tank is still the louder one keeps the old space** (`morphHoldOld`): it becomes the pad, the new tank fades out, and the change waits for the unfreeze, the way a change while frozen always has. Once the new tank is under 0.05 of its gain it stops at once.
- **A freeze later in a morph** (the new tank already louder) keeps the new tank as the pad and drops the old one in 0.3 s, as in ADR 0054.
- **The twin room waits** until only one tank is left (the old one gone, or a HALL <-> CANYON reshape finished), then starts. Notes played over the pad reach it up to about 0.3 s late in a PLATE morph, and up to about 2.3 s late in a reshape.
- **HALL <-> CANYON can't go back part-way,** so the pad lands in the new space, as before.
- The old "revert" path in `morphFlip` and the twin's restart at the handover are gone.

**Rejected:**
- **The new space as the pad** (the other answer to the owner's question): the owner chose the old one.
- **Round 1, the twin starting at the freeze:** a freeze 1.5 s into a morph ran 115-118.6 % (`docs/research/m3-freeze-on-change-lap.log`).

**Consequences:**
- **Module CPU, round 2's lap (`docs/research/m3-freeze-on-change-2-lap.log`, on `main`):** 31 of 32 lines with no late blocks, worst 99.0 %; the usual 19 lines worst 97.6 %; a freeze late in a morph 89.9-90.0 %; FREEZE held 2 s after a flip 84.6-90.5 %. **Left:** CANYON -> PLATE with FREEZE pressed and let go within about 220 ms of the flip runs 101.5 %, 96 late blocks, cause unknown (the same change at the owner's knobs is clean at 90.9 %, and a 2 s hold at 90.5 %).
- **The port's release build is byte-identical to round 2's** (`dist/v1.0.78-freeze-on-change/sublustris_versio-freeze-on-change-2-RELEASE.bin`, the one the owner heard). The lap's lines in `firmware/main.cpp` stay on `proto/unfreeze-reshape`.
- **Every other case renders the same:** `same_render` SAME 60/60 against v1.0.78; goldens unchanged. Click check on round 2 (7,680 renders): peak 0.814 against 0.805.
- **Not timed:** a freeze about 2.5 s after the flip, when the new tank becomes the pad.
- **Still open, older:** turning SIZE during the second after an unfreeze crackles in HALL and CANYON (the owner, 10 Nov; `docs/research/m3-review2-audit-212b68b.log`, up to 107.5 %). It's in every build since before v1.0.68 and is the next fix.
- Flash: release 111,236 B (1,404 B under budget), profile 130,628 B (444 B under the chip).
