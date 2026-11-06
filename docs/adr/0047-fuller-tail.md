# 0047 — A fuller tail: the still lows in step, and more movement in the floor

**Status:** Accepted, 6 Nov 2026 (owner, pick C on `renders/fuller_tail`, B on "3 hall endless" for piano and vibes; "Steadier: B" for HALL endless as a whole). SPEC v1.0.71. Amends ADR 0012's still lows, ADR 0035's HALL floor and the v1.0.25 floor scale.

**Context:**
- **The note:** lfad on Modwiggler, 4 Nov 2026, heard the reverb as "thin/reedy" and "thinner, wispier" than Desmodus and Pleroma. The owner: fuller and sweeter "without being muddy or metallic".
- **Measured** (`docs/research/fuller-tail-2026-11-05.md`, against the owner's Valhalla takes), two causes, neither an EQ:
  - **A hole in the body.** The 300 Hz split's two bands are half a turn apart at the corner. The comment and SPEC called the tanks uncorrelated, so they'd add in power; early on they ring nearly in step, so they cancel across 150-500 Hz. CANYON lost 7.5 dB at 250 Hz in the first 2 s and about 3 dB by 6-20 s; HALL lost 1-2 dB.
  - **Reedy lines.** The tail held the notes' upper partials (1.6-6.4 kHz) as narrow lines, because the floor barely moved. Peaks at a 5 s tail: HALL 6.8 ‰ (Room 2.0-2.4), CANYON 14.5 ‰ (Supermassive 1.5-1.8).
- **The page** (100 % wet, level-matched per card; HALL and CANYON at DECAY noon, both at endless, HALL with the radio half in, CANYON with TONE at 9 o'clock; pad, piano, vibes, drums). A today; B the hole filled; C B plus more floor movement; D more again.
- **The picks:** C everywhere, except piano and vibes on "3 hall endless", which went to B. The knob can't tell what's being played, so the owner was asked which should win at HALL endless: "Steadier: B".

**Decision:**
- **Body (HALL and CANYON, everywhere):** the still lows' tank takes its input in the opposite sign (`Engine::processTwoTanks`), so its output meets the main tank's in step across the split. No EQ, no change of decay.
- **CANYON's movement (everywhere, endless too):** the floor 4× as deep (`voicing::kMassiveFloorScale`) at twice the pace (`kMassiveFloorPace`, `Fdn::setFloorPaceExtra`: its smoothing and the time between its random targets).
- **HALL's movement:** the floor 2× as deep up to DECAY's crossover, easing to 1× as `map::governorWeight` goes 0 → 1 across DECAY 0.80-0.85, and glided over `kHallFloorGlideSeconds`, so a flick doesn't retune. Both played and held notes at endless are B's. The ease moves with the crossover if DECAY's remap changes it.
- **PLATE is unchanged** (it measured in line with Valhalla Plate).
- **The flip is on the tank's input, not its output.** At finite DECAY the Neve's even harmonics in the loop make the tank not sign-symmetric, so negating the output instead is a different sound (−20 dB re peak on a HALL pad) from the one the owner picked. It costs 16 B more, and it's what was heard.

**Rejected:**
- **A:** the note stands.
- **D:** more movement than picked.
- **C at HALL endless:** the owner chose steadier notes there.
- **CANYON by depth alone (×8 at today's pace):** 0 B instead of the pace's cost, but not the sound picked.
- **HALL's in-loop diffusion, a shelf, a longer low T60:** the diffusion changed the lines by less than the method's spread, and nothing measured pointed at the others (`docs/research/fuller-tail-2026-11-05.md`).

**Consequences:**
- **Sound:** CANYON's low-mids come up 3-4 dB, HALL's 1-1.5 dB. The lines at a 5 s tail fall to HALL 4.2 ‰, CANYON 7.1 ‰. A held 1 kHz wavers more below HALL's crossover (16 → 20 cents p95) and in CANYON (0.7 → 7.4 cents).
- **Proof:** the port against the prototype's renderer (a3478b8, `tools/pages/fuller_port_checks.py`): bit-identical to C on CANYON and on HALL below the crossover (33/33), to B at HALL endless (12/12), and to v1.0.70 on PLATE (9/9), including WEAR, MOTION and SIZE moves.
- **The 13.0 dB click reading** on the HALL SIZE sweep (24.825 s) is a chance peak, not a click: 30 dB under the tail, centred at 3.3 kHz at the detector's edge, among 1 ms frames already varying by ±6 dB, and gone when the sweep ends a second earlier or later.
- **Beyond the page:** MOTION's left side wavers more in HALL and CANYON (p95 on a held 1.3 kHz, 9 o'clock: HALL 14.6 → 20.4 cents, CANYON 5.9 → 12.9). A frozen pad comes up against its twin by up to 2.7 dB (CANYON at SIZE noon, noon DECAY). The page was level-matched and the wet's level wasn't changed here, so CANYON's wet is louder than the page sounded (its pad was matched by about −2.5 dB at noon and at endless).
- **Tests re-pinned, each with its reason:** `test_motion`, `test_twin_level`, `test_fdn` (off-note limit −15 → −10 dB), `test_era` (100 Hz limit 1.5 → 2 dB).
- **Flash:** release 111,884 B (+40), m0test 83,556 B (unchanged), profile 131,068 B (+40, **4 B left**). The pace is in every build, so the profile's measurements stay honest. The two floor glides share one `blockK` to fit. Anything further in the Core needs a profile trim first.
- **Not checked:** the module's CPU and feel; nothing has been heard on hardware. The floor's added depth is the same work per sample.
