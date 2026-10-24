# IMMENSIO VERSIO — Context / Glossary

Shared vocabulary for this project. Use these terms exactly in code, comments and conversation. Spec: `SPEC.md`. Decisions: `docs/adr/` (index: `docs/adr/README.md`).

| Term | Meaning |
|---|---|
| **Big tank** | The main reverb: the Plate engine or the FDN (Hall / Massive), per SPACE. Only one runs at a time. |
| **Twin** | A second tank of the frozen space (an FDN or a Plate, the same code) that takes the input while the big tank is frozen: a quieter layer over the pad that fades back out within 8 s. Size and Decay play it while frozen. Replaced the **companion** (a small DTCM plate) in v1.0.7. |
| **Freeze / pad** | The big tank held at loop gain 1, input closed, in-loop colour bypassed: a still pad to play over. Latched by a tap. |
| **Paint** | While frozen, holding the button (or a long gate) blends a little of the twin into the pad over ~3 s (at most ~1.5 dB); the pad keeps it on release. |
| **Gesture reader** | One reader for the button and the gate: press < 0.3 s = tap (toggle freeze), hold ≥ 0.3 s while frozen = paint. Freeze engages on the press. |
| **FDN** | Feedback delay network: 8 modulated delay lines mixed by a matrix. The Hall and Massive voicings. |
| **Swell stage** | The feedforward cascade before the tank that makes a slow attack: BLOOM in every space, and Massive's built-in mass in NORMAL. |
| **Attack time A** | Massive NORMAL: A₀(Decay) · m(Size); 0.5 → 0.2 s with Decay, ×1 → ×1.5 with Size. BLOOM: TIME × m(Size). |
| **Mass** | Costello's word for Sagittarius: slow to start, slow to leave. Here: the swell, scaled by Size. |
| **Echoes → wash** | Massive's Size travel: long distinct repeats (Large Magellanic Cloud) blurring into a boundless wash (Andromeda). |
| **Floor** | The warm level an infinite tail settles at and holds (loop governor + soft saturation). Not used while frozen. |
| **Infinite** | Decay 0.85–1: loop gain 1 at the floor; ducking grows across this stretch. |
| **Modulation floor** | The always-on faint random movement of every tank delay (±1–2 cents) that kills metallic ringing. What remains at DRIFT noon. |
| **Air** | HALL's and PLATE's highs lose a steady extra few dB per second at every finite DECAY, as air does in a room, instead of a fixed share of the decay, so short tails stay bright and long PLATEs lose some top end (v1.0.25). MASSIVE keeps the share. |
| **Tape out** | ERA's cassette stage on the wet, outside the loop: pre-emphasis, up to +12 dB of drive taken off again, a light hysteresis and a gently lopsided curve. Quiet sound passes unchanged; peaks and highs take the tape (v1.0.22, v1.0.26, v1.0.27). |
| **Wander** | DRIFT's clockwise side: quick, small random movement of every tank line (up to ±20 cents, 1–2.25 Hz, the lines' rates fanned out so they never sway together). The tape side is the slower wow plus flutter (v1.0.20). |
| **Still lows** | Below ~300 Hz the HALL / MASSIVE tail rings in its own unmodulated tank, so the bass never wavers; DRIFT and the modulation floor move only what's above (v1.0.18). |
| **Loop chain** | Everything inside each line per pass: modulated read → TONE tilt + damping → soft saturation → ERA in-loop layers. |
| **Era** | One of ERA's five, sweetest first: Neve, Midiverb, Bucket-brigade, Cassette, AM radio. Each plays **on its own** in its fifth of the knob; only neighbours crossfade (v1.0.30, ADR 0021). |
| **Neve base** | The Neve's warmth (even harmonics, slightly shorter highs), under every era and alone at ERA fully CCW (v1.0.30). |
| **Bucket-brigade (BBD)** | ERA 40–60 %: an old analog delay chip's sound on the wet: dark, the compander's breathing noise, the clock's aliasing; grittier across its stretch. |
| **AM radio** | ERA 80–100 %: a station fading on its own path while the receiver's static swells with the AGC; a wandering dip, soft hiss, crackle and lightning, a slow detune. |
| **Midiverb box** | Era 20–40 %: an in-loop 48 → 24 → 48 kHz box with the unit's 13 bits, truncated toward zero; the tail gets grainier as it fades. |
| **Hiss** | Cassette-stage noise fed into the tank input, ducked by the input level. |
| **Duck** | Tail ducking from the input level, only at infinite Decay: 4:1 compression of the wet (attack 30 ms, release 0.4 s), about 3 dB while you play, no darkening (v1.0.17; it was DARKEN). |
| **ATTACK** | SW1: NORMAL / BLOOM / REVERSE. Sets the attack in every space. |
| **TIME** | P7: NORMAL predelay, BLOOM swell time, or REVERSE swell length. |
| **Bloom** | The tail swells in over TIME (the swell stage, in every space). |
| **Reverse** | The hardware reverse reverb (AMS RMX16-style): each note blooms into a swell that starts with it, rises over TIME and stops. Rising taps into a short reverb of its own; nothing plays backwards. |
| **SPACE** | SW0: PLATE / HALL / MASSIVE. Flips crossfade ~100 ms. |
| **P1–P7** | Pots by panel position, reading order (Resilio ADR 0028), not libDaisy index. |
| **Normalised** | 0–1 parameter value every host passes; mapping curves live only in `core/params/`. |
| **Starting guess *(start)*** | A number to be confirmed by measurement or ear at its milestone. |
