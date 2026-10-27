# 0028 — HALL's and MASSIVE's freeze twin is a small room

**Status:** Accepted, 26-27 Oct 2026. Owner's picks:
- `renders/twin_alternatives` (MIX 50 %): "I quite like small room, but it sounds a little boomy and low-end heavy. If we can skew the frequencies to remove some lows and increase the highs a bit, I think this can replace today's twin".
- `renders/small_room` (MIX 50 %): E "room softer" on every card, the repeated cards agreeing.
- `renders/room_level` (MIX 50 %): "follows pad B is the winner from a level-perspective".
- `renders/room_motion` (MIX 76 %): B on every card.

SPEC v1.0.43 and v1.0.47. Amends ADR 0009 (what plays over a frozen pad) and the twin parts of ADR 0010 (Paint copies the room) and ADR 0026 (the twin tank at 24 kHz). PLATE keeps its twin, a second Plate.

**Context:**
- **CPU.** The twin, a second big tank, was ~28 points of a frozen MASSIVE. On the module (round 15) a frozen MASSIVE with the cassette ran ~105 % in real time and crackled. With the twin crossfading too, a frozen SIZE move reached 110-122 %: the drop-outs the owner heard at the rack.
- **The owner's own goal for the layer** (v1.0.7): "a more subtle layer on top of the frozen pad, not the main focus", about 8 dB under it (v1.0.41: "Let's shoot for 8db under the pad").
- **Four things to play over the pad** were heard first, on their own and over an in-key pad, all at −8 dB: today's twin, blur and width (the twin's input diffuser and a decorrelator, no tail), a small room (eight short lines, after Undulae Music's Tinyspace: ideas only, no code), and the twin without its swell.

**Decision:**
- **HALL's and MASSIVE's twin is a small room** (`dsp/TwinRoom.h`). In order:
  - a 150 Hz high-pass (2nd-order Butterworth) on its input, which takes out the lows the owner heard as boomy;
  - two allpasses a side;
  - eight short lines (8-50 ms at the longest, set by SIZE) mixed by a Hadamard matrix, at 24 kHz inside the twin's half-rate stage.

  Each line's loss is the twin's: DECAY's T60, at most 8 s, so it always fades. The highs' T60 is TONE's share × 0.6, a slightly softer top ("room softer"). SIZE glides the lines, at most a semitone's bend, with no crossfade.
- **Its level follows the pad** (v1.0.43, `room_level` B). At SIZE 0.9 with DECAY infinite it sits at the page's gains (−8.63 dB HALL, −7.65 dB MASSIVE, `kTwinRoomHallDb` / `kTwinRoomMassiveDb`): there it matched v1.0.41's twin, about 8 dB under the pad. Elsewhere it follows the pad as the twin tank did: v1.0.41's lift for a tail cut short (up to +5.4 dB, `kTwinRoomLift`), the twin tank's level trims, and its line length against SIZE 0.9.
- **Its movement scales with SIZE** (v1.0.47, `room_motion` B). Each line's read wanders by 0.5 % of its own length. The page level-matched B to the room as before, and the owner picked it at that level, so the room's output is trimmed by −0.93 / −0.88 / −0.53 dB at SIZE 0 / 0.5 / 0.9 (`kTwinRoomTrimDb`; straight between, held above 0.9).
- **Memory:** its rings (~42 KB at 24 kHz) are in AXI SRAM on the module (`Engine::axiFloats`), not DTCM. The twin tank and its SDRAM are gone.
- **Paint** copies a little of the room into the pad, as it did the twin (ADR 0010).
- **Removed:** the twin tank (`twinFdn_`), and the twin prototypes and their hooks (`dsp/TwinAlt.h`; the renderer ignores `IV_TWIN_ALT`, `IV_TWIN_LIGHT`, `IV_ROOM_LEVEL` and `IV_ROOM_WANDER`).

**The pitch-bend finding** (the owner on `room_level`: "some slight pitch bending when played over the top ... character of the room reverb, or imperfect tuning of the samples?"). Measured before anything was changed:
- **The room.** Its reads all wandered by the same 0.15 ms. Its short lines go round 20-125 times a second, so the bend builds up pass after pass: 4-9 cents at SIZE 0.9 and up to ~40 cents at SIZE 0. For comparison, v1.0.41's twin bent 0-3 cents and the main tank bends 2-8.
- **The samples.** Their in-key pads were spread ±14 cents (a chorus that beats), so the page's material was retuned first.
- **After B:** a held 1.3 kHz through the frozen room alone moves 6.0 / 6.0 / 7.5 cents p95 at SIZE 0 / 0.5 / 0.9 (was 56 / 15 / 6.9). Its ringing is 13.7 / 7.2 / 7.1 dB (was 16.9 / 7.4 / 8.5).

**Rejected:**
- **Blur and width** (`twin_alternatives` B): the cheapest, but it has no tail of its own and loses the space's colour.
- **The twin without its swell** (D, MASSIVE only): it saves little CPU and still runs a second big tank.
- **Lighter twins** (`renders/frozen_twin`: B the swell at 24 kHz, C a plainer twin, D the twin at 12 kHz): set aside when the small room was picked. [VERIFY: no owner pick for `frozen_twin` is recorded in `docs/TASKS.md`.]
- **The room unvoiced** (`small_room` A) and the other voicings on that page: E was picked on every card.
- **Louder twins** (`renders/twin_louder` C, 1.5 dB under the pad): B, about 8 dB under, was picked (v1.0.41) and the room keeps that level.
- **The room's level fixed at the page's gains** (`room_level` A): it moved against the pad across SIZE and DECAY.
- **The room's wander as before or off** (`room_motion` A and C).
- **Holding or gliding the ducking at the freeze** (`renders/freeze_moment` B, C): "A today sounds more seamless. B and C seem to drop in amplitude when frozen/pad alone." The quick let-go is kept.

**Consequences:**
- **Sound:** only HALL's and MASSIVE's freeze and Paint change (`tools/same_render.py`: hall/massive freeze and paint). The PLATE is bit for bit as before.
- **Level:** across SIZE 0 / 0.5 / 0.9 × DECAY 0.5 / 0.9 the room stays within 3 dB of v1.0.41's twin's distance under the pad, and HALL at SIZE 0 within 4 dB (`test_twin_level`). Paint's ceiling holds (`test_plate_freeze`).
- **CPU** (the candidate lap, `docs/research/m3-candidate-lap1.log`): a frozen MASSIVE with the old cassette peaks at 79.9-82.6 % with no late block (with the twin: 105-122 %, late on every block). Frozen SIZE moves cost 1-2.5 points (the twin: 13-18).
- **Flash:** release 102,508 → 105,356 B for the room (+2,848 B, the twin tank's code out), +160 B for its level following the pad, +152 B net for its movement and trims (v1.0.47). WEAR's chain (ADR 0027) made room for it.
- **Tests:** `test_twin_level`, `test_room_motion` (pitch, ringing, level, Paint's ceiling, 10-minute painted holds, no clicks at freeze, frozen SIZE moves, paint and unfreeze), `test_plate_freeze`.
