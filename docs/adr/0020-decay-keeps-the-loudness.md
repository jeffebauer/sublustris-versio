# 0020 — DECAY keeps the loudness

**Status:** Accepted, 24 Oct 2026 (owner's picks on `renders/decay_level`: C "all made up" on every card, the repeated cards agreeing; "levels should ideally be consistent across reverb types"). SPEC v1.0.28.

**Context:** The Valhalla comparison (`docs/research/valhalla-comparison.md`, suggestion 4) found our wet getting quieter as DECAY shortens, while Valhalla Room and Supermassive keep their level. Measured on music (wet loudness against the dry, LUFS, six materials), v1.0.27:
- **2 s → 15 s:** HALL −9.6 → −1.4 dB, PLATE −5.4 → −2.2, MASSIVE −13.4 → −6.0. Room stays at about +2 to +3 and Supermassive at about 0; Valhalla Plate gets louder with decay.
- **The cause:** a tail's energy goes with its T60, and nothing made it up. HALL's 8.2 dB over 2 → 15 s is what that predicts. Only SIZE had a level correction (`hallSizeLevelTrim`).
- **Between the SPACEs:** the output trims were matched at the same DECAY knob position (noon), where MASSIVE rings about 26 s. At the same tail length, MASSIVE was 5 dB below HALL and PLATE, which the owner heard on the page.

**Decision:**
- **The wet is made up to the level of the SPACE's longest finite tail** (`map::decayLevelMakeup`, on the forward wet and the twin, ~40 ms glide): level ∝ T60^p, with p measured per SPACE (`kDecayLevelPower`, `kDecayLevelPowerLong`). Below 15 s: PLATE 0.18, HALL 0.47, MASSIVE 0.445. From 15 s to the finite top: HALL 0.09, MASSIVE 0.29.
- **Nothing changes from the finite top up:** the crossover, infinite tails and a frozen pad (which keeps its held DECAY) are as before.
- **REVERSE gets its own make-up** (`map::reverseLevelMakeup`, a 4-point table in dB): its stop's T60 follows DECAY, but its level doesn't follow a T60 power (TIME's swell carries the long end), and it fell 14 dB across DECAY. A short REVERSE now comes up to the forward wet's level; from DECAY 0.75 up, it's as before.
- **PLATE +0.4 dB** (`kPlateOutputTrim` 0.41 → 0.43): the small PLATE was the lowest point both on music (2 s, −2.8 dB) and in `test_plate_freeze`'s noise-burst match (3.2 dB spread against its 3.0 limit).
- `Engine::setDecayMakeup(0)` (renderer `IV_DECAY_MAKEUP=0`) gives v1.0.27's levels, for demos.

**Consequences:**
- **Levels:** every finite setting in every SPACE now measures −2.8 to −0.7 dB against the dry (it spread 13 dB), and infinite −2.2 to −2.9. REVERSE is within ~1.5 dB of the forward wet (MASSIVE's REVERSE up to ~3 dB under it, as before).
- **Peaks:** the shortest tails are up to ~11 dB hotter; the loudest render (drums, MASSIVE, shortest DECAY) peaks at −5.2 dBFS, so the limiter doesn't act at ERA 0. With ERA's tape out, a short tail now meets the tape's soft limiting sooner.
- **Energy:** the make-up is a gain after the loop, so the loop and its holds are untouched.
- **Cost:** two `std::pow` per control block; flash in the handoff.

**Amendment (28 Oct 2026, ADR 0034, SPEC v1.0.53-decay):** PLATE's and HALL's finite tops are 30 / 60 s. The make-up is still measured to the old tops (`voicing::kDecayLevelTopSeconds` 12 / 25 / 120 s), and the longer tails are trimmed (`kDecayLongTrimDb`) so each DECAY keeps the level it had while playing (within 0.44 dB on the decay_range chord). MASSIVE and endless are unchanged.
