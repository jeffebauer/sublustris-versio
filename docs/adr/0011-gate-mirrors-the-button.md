# 0011 — The gate mirrors the button: one gesture reader

**Status:** Accepted, 21 Oct 2026 (owner). Spec-session decisions 12, 13.

**Context:** The brief leaned toward a dub throw on the gate (as Resilio ADR 0039). The owner chose instead to have the gate do what the button does, so a sequencer can freeze, unfreeze and paint. A tap that unfreezes and a hold that paints both start with a press while frozen, so the reader must wait to tell them apart.

**Decision:**
- **Button and gate feed one gesture reader.** A gate's rising edge is a press, its falling edge a release. Unpatched, the gate reads low and does nothing.
- **Freeze engages on the press** (no latency: you catch the chord).
- While frozen, a press opens a **0.3 s window**: released inside it → unfreeze on release; still held at 0.3 s → paint (ADR 0010). So unfreezing can lag the press by up to 0.3 s.
- A short gate pulse (< 0.3 s) toggles the freeze; a gate held longer while frozen paints.
- Plugin: MIDI note-on = press, note-off = release; the button is a momentary plugin control.

**Rejected:**
- Dub throw on the gate (the brief's lean): the tap/hold throw questions are moot.
- Gate level = frozen (momentary, no painting from the gate); every rising edge toggles (no painting from the gate).
