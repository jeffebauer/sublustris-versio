# 0003 — ERA stacks, weighted to the middle

**Status:** Accepted, 21 Oct 2026 (owner). Spec-session decisions 1, 5.

**Context:** ERA runs Clean → Neve → Cassette → μ-law → Midiverb. The stages could **stack** (each keeps the colour of the ones before it) or **morph** (only the two nearest blend). The brief asks for a tail that "gets grainier as it fades", warm and never metallic.

**Decision:**
- **Stacking:** each stage's layer fades in across its stretch of the knob, then stays. Turning ERA up means "further back in time, more worn".
- **Travel weighted to the middle:** Clean 0–10 %, Neve 10–35 %, Cassette 35–65 %, μ-law 65–85 %, Midiverb 85–100 %. Most room for the warm dub sweet spot; lo-fi is the last stretch.
- Per-stage numbers (bandwidth, bits, saturation, noise, wow/flutter, in-loop vs output): SPEC §4.7, starting guesses tuned at M6/M8.

**Rejected:**
- **Morph:** more distinct colours, but crossfade spots in the middle that sound like two things at half strength.
- **Equal fifths** or **weighted to lo-fi** travel.
