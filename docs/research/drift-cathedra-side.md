# DRIFT's clockwise side (23 Oct 2026)

**Decided (SPEC v1.0.20):** Wander at 2.5× round 1's depth: ±20 cents, 1 → 2.25 Hz, rates fanned ±50 %. Owner, round 2: "Wander strongest is the winner. I prefer wander stronger, but it's still quite a subtle effect, so I'd rather the maximum value to be obvious, so maybe a half-way point between stronger and strongest." Also: "orbit stronger is too intense and creates pitch-y swirling." Every other prototype is removed from the code.

Owner, M6: "Tape and still sound great, but drift at 1 is too extreme and sounds too seasick. How can we better tune this side of the potentiometer? Does drift make the most sense? Are there other ways we can modulate the sound to make it distinct from tape?"

Tags: **[S]** a source says it; **[M]** we measured it; **[I]** our inference.

## What the clockwise side does today

SPEC §3 P6: a sine per FDN/PLATE line, 0.05 → 0.3 Hz and 0 → ±35 cents across the clockwise half. The rate skew between lines is only 0.85–1.15, and the phases are offset by up to 0.61 of a cycle (`Drift.h`). The tape side is a smoothed random wow, 0.2–1 Hz up to ±15 cents, plus flutter.

[M] A held 1 kHz through SIZE 0.6 / DECAY 0.7 (`tools/pitch_track.py`, the tail 3–7 s), p95 pitch deviation:

| DRIFT | HALL | MASSIVE | PLATE |
|---|---|---|---|
| tape, full (0) | 21 c | 11 c | 20 c |
| noon (0.5) | 5 c | 2 c | 16 c |
| 0.75 | 34 c | 30 c | 48 c |
| full CW (1) | **64 c** | 37 c | **57 c** |

At full CW, HALL and PLATE also show a coherent **±25–27 cent sway at 0.3 Hz**: the whole tail bends together. The tape side has no such sway (≤ 5 c).

[I] Two causes:
1. **Depth.** The tail passes through modulated lines many times, so each pass's bend adds to the last.
2. **Coherence.** The lines' rates are nearly equal and their sines in near-step, so the bends add up instead of averaging out. Random wow (the tape side) averages out.

## Sources

- **Meris Mercury7.**
  - [S] The quick-start ([PDF](https://www.meris.us/wp-content/uploads/2017/01/Mercury7-Pedal-QS.pdf)) lists three kinds of movement: Modulate ("overall modulation depth of the reverb algorithm"), Mod Speed, and a hidden **Vibrato Depth that "adds vibrato to the reverb input for lush, haunting trails"**. Pitch Vector is separate: pitch-shifted reflections.
  - [S] The MercuryX manual: "Pair the Vibrato with Swell and Cathedra to enter the realm of the sonic visionary from Hull, Quebec" ([v1.5](https://meris.us/wp-content/uploads/2026/07/MercuryX_v1.5.pdf)).
  - Meris publishes no depths.
  - [I] Cathedra's big movement sits *outside* the loop (input vibrato plus swell), so it doesn't compound.
- **Lexicon, Spin and Wander.** [S] Randomisation "to control the spectral purity", "at some settings… a more noticeable modulation effect". Spin "values between 1–2 Hz are appropriate"; Wander is in milliseconds ([PCM Native Room manual](https://adn.harmanpro.com/product_documents/documents/1001_1335994619/PCM_Native_Room_Manual_original.pdf)). 480L: voices "go backwards and forwards randomly in respect to each other" ([manual](https://freeverb3-vst.sourceforge.io/doc/Lexicon/lexicon_480l.pdf)).
- **Valhalla.**
  - [S] Room: "crank both mod depths for that seasick decay. For less obvious detunings, use lower Mod Depth settings, but higher Mod Rates" ([tips](https://valhalladsp.com/2011/05/27/valhallaroom-tips-and-tricks-unnatural-hall-reverbs/)).
  - [S] VintageVerb's Random Space reduces metallic artefacts "without the pitch change" of chorused modes ([modes](https://valhalladsp.com/2023/02/10/valhallavintageverb-the-modes/)).
  - [S] FutureVerb uses "several dozen random LFOs" with spread rates ([controls](https://valhalladsp.com/2025/10/03/valhallafutureverb-the-controls/)).
- **Strymon BigSky** ([manual](https://www.strymon.net/manuals/BigSky_UserManual_RevD.pdf)).
  - [S] Room's Mod randomises delay lengths for the first half of the knob, then adds modulation to the tank's *input* for the second half.
  - [S] Cloud modulates the input diffusers, which "allow[s] high degrees of modulation without muddying up the sustaining reverberation tail".
- **Dattorro (1997).** [S] ~8 samples of peak excursion at 29.8 kHz, 1 Hz. [I] That is about ±3 cents, an order of magnitude below our ±35.
- **Schlecht & Habets (JASA 2015; thesis ch. 6).** [S] Rotating the feedback matrix modulates amplitude, not pitch. It is smooth up to ~4 Hz with rates spread ±50 %, gives "liveliness" and a moving spatial image, and is stable while every matrix is unitary. Tonal sources pick up sidebands.
- **Perception.**
  - [S] Valhalla ties seasickness to high depth at low rate.
  - [S] At slow rates and large extents, listeners hear the cycle's high and low parts as separate pitches (d'Alessandro & Castellengo 1994).
  - [I] A 0.05–0.3 Hz sine outlasts a note, so the ear hears the chord going out of tune and back.

## Candidates for the clockwise half (each distinct from tape)

1. **Halo** (Mercury7 / Lexicon 224 input vibrato, BigSky Cloud): vibrato on the tank's *input* (≈ 4–6 Hz, ±5–12 cents), with only a few cents of random movement inside the tank. Shimmery, choir-like; the tail glows but stays in tune.
2. **Wander** (Lexicon Random Hall, Valhalla Random Space): smoothed random delay movement at 1–2 Hz, a few ms, rates spread per line. The room breathes; no audible pitch bend.
3. **Orbit** (Schlecht–Habets matrix rotation): the feedback mix rotates at 0.1 → ~0.8 Hz (slow-Leslie speed at the end), rates spread ±50 %, energy-preserving. The tail circles around you; it shimmers in space, not in pitch.
4. **Today's drift, tamed**: the same slow sine, but its depth capped (≈ ±8–12 cents), rates spread ±50 % and phases random, so the lines don't sway together.

The recipes are ours [I], following the sources named.

## Prototypes (23 Oct 2026; `IV_DRIFT_CW` = 1 Halo, 2 Orbit, 3 Wander; owner chose these three)

- **Halo:**
  - Vibrato on the tank's input: 4 → 6 Hz, up to ±12 cents, L and R at different rates.
  - Inside the tank: the floor's depth × (1 + 2t), with today's sine drift off.
- **Orbit:**
  - After the Hadamard mix, pairs of lines rotate through angles up to ±45°.
  - Each pair swings at its own rate, (0.1 + 0.7 t) Hz × {0.5, 0.83, 1.17, 1.5}.
  - Orthogonal, so the loop is energy-preserving. No delay modulation besides the floor.
  - HALL and MASSIVE only, including the twin; the PLATE keeps just its floor.
- **Wander:**
  - The wow generator at 1 → 2 Hz, up to ±8 cents.
  - Line rates fanned out ±50 % (`DriftAmounts::rateSpread`). No flutter, no sine.

[M] Energy check:
- **Holds:** 72/72 ten-minute holds pass (every SPACE × SIZE 0/0.5/1 × DRIFT 0.75/1 × mode).
- **Build-up:** none in the full sweep.
- **PLATE level:** with DRIFT CW, all three sit ~3 dB louder than today with equal decay slopes, because today's deep drift loses energy in the PLATE.

[M] Held 1 kHz, p95 cents at DRIFT 0.75 / full:

| | HALL | MASSIVE | PLATE |
|---|---|---|---|
| today | 34 / 64 | 30 / 37 | 48 / 57 |
| Halo | 6 / 20 | 10 / 17 | 18 / 23 |
| Orbit | 5 / 4 | 1 / 1 | 16 / 16 (floor only) |
| Wander | 5 / 10 | 3 / 8 | 12 / 19 |
| tape (CCW, full) | 21 | 11 | 20 |

Page: `renders/drift_cw` (HALL and MASSIVE at 3/4 and full; E = tape for comparison).

## Round 2 (23 Oct 2026)

Owner, round 1 (no card picks): "My preference is for Orbit, but I'd like to hear a stronger version as it's quite subtle. Wander is also quite subtle, so I'd like to hear a stronger version of that too. Replace Halo with a phaser exploration."

**Phaser sources** (research agent):
- **Placement.** [S] Established gear puts the phaser before or after the reverb: Eventide H90 (two algorithms in series), Hologram Chroma Console (Movement then Diffusion, re-orderable), Cymatics Space (on the wet). Meris MercuryX has no phaser element; its "Feedback" slot is the pre-delay's loop.
- **Old Blood Noise Dweller.** [S] Puts a delay inside each phaser stage: "like putting a delay inside of a phaser". 4 or 8 stages, with Regen.
- **In-loop stability.** [S] Dattorro (1997, part 2): analogue phasers are cascades of time-varying allpasses, and "global feedback enhances the effect". [S] A direct-form allpass doesn't preserve energy as its coefficient moves. Bilbao (IEEE SPL 2005) and Werner (DAFx-20) give an orthogonal-rotation form that does.
- **No notches in the loop.** [I] An allpass chain alone is flat; notches come from summing it with an unphased copy. So a phaser *inside* the loop swirls the tail's resonances rather than making notches.

**Prototypes** (`IV_DRIFT_CW`):

| Mode | What it does |
|---|---|
| 4 Orbit stronger | Two stages of pair rotations, up to ±180°, 0.1 → 1 Hz |
| 5 Orbit + pan | Mode 4, plus a constant-power pan of the tank's output (up to ±0.8, 0.05 → 0.3 Hz). A rotation of L/R did nothing: the outputs are uncorrelated. Balance swing 1.5 → 2.6 dB sd, range 13 dB. The low tank stays centred |
| 6 Wander ×2 | 16 cents, 1 → 2 Hz |
| 7 Wander ×3 | 24 cents, 1 → 2.5 Hz |
| 8 Jet | 6 first-order stages on the tank's output, swept 200 Hz → 4 kHz, 0.05 → 0.8 Hz, feedback up to 0.5, mixed up to 50/50 |
| 9 Dweller | 4 swept Schroeder allpasses (1.3–11.3 ms, g 0.4 ± 0.3, 0.1 → 2 Hz) on the tank's input, mixed up to 50/50 |
| 10 Tide | 4 rotation-form first-order allpasses per FDN line in the loop, g up to ±0.75, 0.03 → 0.5 Hz, per-line rates |

[M] Energy:
- Holds pass for modes 4–7 (72/72) and modes 9 and 10 (36/36).
- Jet passes 13/18. Its output level drifts up to ~1.7 dB over 10 min as the notches sweep over a held tail. It sits outside the loop, and the tank's own level holds.
- No build-up in any sweep.

[M] Held 1 kHz at full, p95 cents, HALL / MASSIVE:

| Mode | HALL | MASSIVE |
|---|---|---|
| Orbit stronger | 22 | 15 |
| + pan | 23 | 13 |
| Wander ×2 | 18 | 10 |
| Wander ×3 | 20 | 16 |
| Jet | 13 | 12 |
| Dweller | 5 | 5 |
| Tide | 8 | 6 |

The stronger Orbit's fast, wide swings add pitch shimmer comparable to the tape side (21 cents).

Cost: release 106,724 B with every DRIFT prototype compiled in (+5.7 KB). The modes not picked get removed.
