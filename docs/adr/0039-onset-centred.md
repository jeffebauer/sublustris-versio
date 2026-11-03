# 0039 — The onset centred: HALL's and CANYON's early taps re-laid in the stereo field

**Status:** Accepted, 3 Nov 2026 (owner's pick B on all 16 cards of `renders/onset_phase`, the repeated cards included; their note on every card: "centred"). SPEC v1.0.63.

**Context:**
- **Review 2 (`docs/research/review-2026-10-29.md` §1.1; `review2-notes/litfdn.md` finding 4) found the first 50 ms of HALL and MASSIVE (now CANYON) leaning anti-phase:** L/R correlation −0.22 / −0.48 on a centred click, the mono sum −4 to −6 dB below the channels, where reference units sit near 0. The late tails were already uncorrelated in every SPACE.
- **The cause:** each FDN line's early taps (ADR 0026) go out with another line's Walsh signs, line i + 3 (`kOutL` / `kOutR`). While one tap is all that sounds, L and R carry it fully in or out of phase. A centred source feeds lines 0, 2, 5 and 7 only (`kInL + kInR`), and with i + 3 the two earliest of them (lines 0 and 2: their taps sit nearest the lines' starts) both went out anti-phase.
- **Measured on v1.0.62** (centred click, snare, kick, chord and pluck; SIZE 0.2 / 0.5 / 0.8; DECAY 0.5; 50 ms windows): HALL from the hit, mean correlation −0.46, worst mono loss −13.2 dB; from the wet's onset, HALL −2.8 dB worst, CANYON −0.27 mean, −0.67 most negative, −4.9 dB worst (CANYON's first 50 ms from the hit is only its swell, far below the tail).

**Decision:**
- **Each line's early taps go out on line `i ^ ((i >> 1) & 1) ^ 2`** (`Fdn::earlyOut`) instead of `(i + 3) & 7`. Still a permutation, so every sign pair is used once and L and R stay balanced. Lines 0 and 2 now go out on opposite phases, the earliest in phase, and lines 5 and 7 likewise.
- The page's B was `((i ^ ((i >> 1) & 1)) + 2) & 7`. Lines j and j ^ 4 carry the same signs, so the form built gives the same output to the bit in fewer instructions. A page render re-made with the built renderer equals the page's B file sample for sample.
- **The prototype switch (`IV_ONSET_PHASE`, layouts 1-3) is gone:** the firmware carries only this layout.

**Rejected:**
- **Layout 1** (the same bit flip, + 3: the earliest tap anti-phase): as good in HALL, but CANYON's worst mono loss stayed −4.4 dB.
- **Layout 3** (i + 2: lines 0 and 2 both in phase): strongly correlated (+0.5 to +0.95), a narrow, near-mono start.
- **Separate early-tap sign tables:** the same result for 64 B of tables.

**Consequences:**
- **Sound:** the first moment of HALL and CANYON on a centred source sits evenly between in and out of phase. Measured with B: HALL from the hit, mean correlation −0.06, worst mono loss −2.0 dB; from the wet's onset HALL −1.3 dB worst, CANYON +0.11 mean, −0.33 most negative, −1.7 dB worst. The tails after the early taps are untouched (the taps are output only), and PLATE has no early taps, so it doesn't change.
- **Goldens refreshed** (`IV_GOLDEN_UPDATE`, v1.0.63). `tools/same_render.py` against v1.0.62 differs in 41 of 60 cases: every HALL and CANYON case and the SPACE flips. All 19 PLATE cases stay SAME.
- **Tests re-pinned:** test_twin_level's five twin-against-pad levels that moved 0.3-0.5 dB; test_motion's MASSIVE p95 at 9 o'clock and 3/4 left. Its right-side "no Wander" check now takes the median of four held tones. The re-laid taps nearly cancel MASSIVE's 1.3 kHz in one channel at full right, and that one tone read 64 c. Over 31 single tones from 0.5 to 3 kHz the p95s are as before (CANYON median 1.7 against 1.4 c, none over 10 c; HALL 13.2 against 12.9).
- **No new clicks** (1 ms check per channel against v1.0.62: HALL and CANYON at SIZE noon and 0.8 on hits, drums and keys, SPACE flips, SIZE moves). In the mono sum a centred hit's first reflection now adds where it used to cancel.
- **Flash:** +16 B in release (108,796 B), one more instruction where the four early-tap paths pick their line. The profile is 16 B smaller (128,712 B).
