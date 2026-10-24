# Tape out, stronger (24 Oct 2026)

**Outcome (24 Oct 2026):**
- **Round 1 (stronger, with hysteresis on the strongest):** "the strongest saturation is making it too difficult to hear the hysteresis".
- **Tape distortion, measured:** the full bias put a strong 2nd harmonic on held notes (14–22 dB under them). It became a third (v1.0.26, `renders/tape_bias`). With the tape bypassed, held-note harmonics are −56…−78 dB.
- **Steady-level tape** (`renders/tape_level`): turned down, "jittery saturation" in long tails.
- **Round 2 on the settled tape:** E, "a little stronger + light hysteresis", on all 24 cards. **Shipped as v1.0.27** (`core/dsp/Tape.h`, constants in `EraVoicing.h`).
- **Gone:** the +24 dB strength and the Jiles–Atherton model. The prototype code is on branch `claude/listen`.

Owner, on `renders/dsp_audit`: tape out (v1.0.22) is liked but "very subtle — really only audible on MASSIVE drums. Can we make the saturation a little stronger? Should we explore adding tape hysteresis as well?"

Tags: **[S]** a source says it; **[M]** we measured it; **[I]** our inference.

## Why it's subtle

[I] Today's drive is at most ×3 (+9.5 dB). A reverb tail runs about −30 to −15 dBFS RMS, so its peaks reach only about −5 dBFS after the drive. That is where the curve x(27+x²)/(27+9x²) is still nearly straight.

## Sources

- **Record pre-emphasis** boosts the highs before the tape, so they are "pushed harder into limiting". Under saturation the boost is diminished, so after de-emphasis the result is fat and darker on peaks and transparent at low level [S] (Strymon, [tape white paper](https://www.strymon.net/studio-reel-reel-tape-dynamics-double-tracking-white-paper/)).
- **Cassette time constants** of 120 / 70 µs put the corners at about 1.3 / 2.3 kHz [S] ([IASA](https://www.iasa-web.org/node/490)).
- **Input drive with automatic makeup**, so the level stays put [S] (u-he Satin [guide](https://u-he.com/downloads/manuals/plugins/satin/_Satin-user-guide.pdf); ChowTape).
- **Hysteresis.**
  - [S] Jiles–Atherton (JA) in ChowTape (Chowdhury, [DAFx-19](https://ccrma.stanford.edu/~jatin/420/tape/TapeModel_DAFx.pdf)): a trapezoidal derivative and an RK2 / RK4 / Newton–Raphson solver. It is unstable near Nyquist without oversampling, and the source guards it with input clipping and a state reset. ChowTape's code is GPL-3.0, so only the paper's equations may be used here (ADR 0006).
  - [S] Holters & Zölzer (DAFx-16): JA "is often deemed too complex and replaced by simpler heuristics".
  - [S] The play operator is the textbook rate-independent hysteresis.
  - No published cheap audio hysteresis was found.
- **CPU [I]:** a float JA step at RK2 costs roughly 250–350 cycles on the Cortex-M7, so 2 channels at 4× would take about 24 % of the CPU. Only a cheap model is realistic on the module.

## Prototypes (`core/dsp/Tape.h`, `IV_TAPE_MODE`)

All modes use a first-order high-shelf pre-emphasis at 2 kHz, drive, the curve at 4×, the exact de-emphasis (corner 2 kHz / gain), and the drive taken off again. The result is unity for small signals, so only peaks and highs change. The cassette layer c scales everything.

| Mode | Drive at full | Emphasis at full | Curve |
|---|---|---|---|
| 1 a little stronger | +12 dB | +6 dB | today's asymmetric curve |
| 2 stronger | +24 dB | +12 dB | today's asymmetric curve |
| 3 stronger + light hysteresis | +24 dB | +12 dB | play operator (width 0.15 × the 50 ms RMS × drive), then the curve. Our own, unsourced as audio |
| 4 stronger + full hysteresis | +24 dB | +12 dB | JA, normalised (Ms 1, a 0.6, α 1e-3, k 0.45, c 0.2), RK2 at 4×, input clipped ±10, reset on NaN or \|M\| ≥ 2. The normalised values are ours |

[M] At ERA 0.55, against today:
- the difference from today is 10–20 dB below the signal (today's tape out vs none: about 25 dB);
- the drums' peaks fall from 0.35 to 0.14;
- every mode stays finite with no resets on the page material;
- JA plays 3–4 dB quieter on sparse material (its small-signal gain isn't unity), so level-match it before judging.

Page: `renders/tape_out` (`tools/pages/tape_out.sh`).
