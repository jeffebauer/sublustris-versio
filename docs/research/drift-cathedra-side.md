# DRIFT's clockwise side (23 Oct 2026)

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
