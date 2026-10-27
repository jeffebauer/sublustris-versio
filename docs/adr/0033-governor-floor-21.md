# 0033 — The endless tail settles 3 dB higher: the floor at −21 dBFS

**Status:** Accepted, 28 Oct 2026 (owner, pick D on `renders/level_duck`). SPEC v1.0.51. Amends ADR 0008's floor level (the mechanism is unchanged).

**Context:**
- **The owner's note** after the v1.0.49 rack test: "on hall and massive with Long decays, I feel the limiter is keeping the level a bit too low", and sweeping BLEND from 50:50 to fully wet "the reverb gets noticeably louder".
- **Measured** (`docs/research/level-at-infinite.md`): not the limiter (it takes 0.00-0.13 dB on music; nothing reaches it at infinite DECAY). At infinite DECAY the wet sat 7-12 dB under the dry while playing: the floor governor's −24 dBFS floor gave 4.3 dB (HALL) / 7.5 dB (MASSIVE) of it, the 4:1 duck the rest.
- **The page** (`renders/level_duck`) offered the duck at 2:1 and 1.5:1, the floor at −24, −21 and −18, and combinations. The owner picked D: "Duck 2:1 floor21 sounds the best".

**Decision:**
- **The floor governor's floor is −21 dBFS RMS** (`voicing::kFloorDbfs`, was −24): an infinite tail settles about 3 dB higher; the darkening toward the floor stops 3 dB earlier.
- Everything that follows the floor moves with it: a painted pad's room to grow (the floor + `kPaintHeadroomDb`), and the floor's glide out with a freeze.
- The duck's 2:1 and its following the playing are ADR 0014's amendment of the same day.

**Checked before building** (`tools/pages/level_duck.py --safety`, and again on the build with `tools/pages/level_glue.py --safety --built`): fed 10-minute infinite holds in HALL and MASSIVE don't grow and their tails hold; a 5-minute freeze with a paint holds still; WEAR full with SHIMMER at infinite doesn't grow; no clicks through the duck's and the floor's onset (DECAY swept 0.7 → 1 → 0.7 while playing).

**Not picked (heard on the page):** the floor at −18 dBFS, the duck at 1.5:1, and easing the duck alone with the floor left at −24.
