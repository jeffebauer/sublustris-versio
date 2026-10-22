# Reference devices: controls and matched settings

**Written:** 22 Oct 2026, researched for `docs/reference-recording-guide.md`.

**Protocol for every unit:** 100 % wet, predelay 0, tone and EQ flat, extra effects off, modulation at default, decay at SHORT ≈ 2 s / MEDIUM ≈ 5 s / LONG ≈ 15 s, plus INF where the unit has it.

Units that don't show seconds get calibrated by measurement (`tools/ingest_reference.py … calibrate`): the Ultrasheer, Mercury7, Supermassive, Swells, H9 Blackhole and H9 Shimmer. Every knob position below for those units is a **guess**.

## Vongon Ultrasheer
Sources: manual https://docs.vongon.com/ultrasheer.pdf · product page https://vongon.com/pages/ultrasheer · https://delicious-audio.com/vongon-ultrasheer-stereo-reverb-vibrato/

**What it is:** a Lexicon 224-style plate (Dattorro topology), 16-bit reverb inside a 32-bit float DSP, 48 kHz.

**Controls**
- Reverb: MIX, DECAY (0.25–30 s, but the knob isn't marked in seconds), BRIGHTNESS (in the feedback path), TRAILS.
- Vibrato: RATE, DEPTH, CYCLE/RANDOM, ORDER.
- Separate footswitches for vibrato and reverb.

**Matched settings**
- MIX fully clockwise. There's no documented kill-dry, so check for leakage.
- No predelay control.
- BRIGHTNESS at noon, by convention.
- Vibrato off.
- **Decay guesses:** 9–10 / 12 / 2–3 o'clock.
- No INF, freeze or reverse.

**I/O:** auto mono/stereo routing; instrument level, 53 kΩ input; headroom unpublished.

## Meris Mercury7
Source: manual v4, https://meris.us/wp-content/uploads/2017/03/Meris_Mercury7_full_Manual-v4.pdf

**Algorithms:** Ultraplate (fast build), Cathedra (slow build).

**Controls (ALT function in brackets)**
- SPACE DECAY (PREDELAY 0–41.5 ms)
- MODULATE (MOD SPEED)
- MIX (PITCH VECTOR MIX)
- LO FREQ (DENSITY)
- PITCH VECTOR (SWELL ATTACK)
- HI FREQ (VIBRATO DEPTH)
- SWELL button

**Matched settings**
- MIX max = 100 % wet. Global KILL DRY = DRY MUTED (hold L at power-up).
- Predelay at minimum.
- LO FREQ and HI FREQ max (no attenuation).
- PITCH VECTOR min, VIBRATO DEPTH min, DENSITY max.
- MODULATE about 9 o'clock and MOD SPEED about 10 o'clock (preset 1).
- **Decay:** unpublished in seconds; calibrate.
- **INF:** hold SWELL (near-infinite).
- **MIDI CCs:** 16 decay, 17 modulate, 18 mix, 19 lo freq, 20 pitch vector, 21 hi freq, 22 predelay, 23 mod speed, 25 density, 27 vibrato, 28 swell, 29 algorithm.

**I/O:** global LINE/INSTRUMENT level (+12.5 / +9 dBu max): use LINE. 48 kHz, 1 MΩ input.

## Eventide H9 (Gen 2 / Max)
Sources: Algorithm Guide V12, https://downloads.eventide.com/audio/Product+Manuals/H9AlgorithmGuide+V12.pdf · User Guide, https://s3.amazonaws.com/com.eventide.downloads/Product+Manuals/H9-UserGuide.pdf · H90 reverbs, https://cdn.eventideaudio.com/manuals/h90/1.9.4/content/algorithms/reverb.html

**Global settings**
- Tempo OFF (decay in seconds).
- KILDRY = YES plus MIX 100.
- Routing NORMAL.
- INGAIN 0 dB (inputs clip at +4 dBu); output 0 dB.
- Mono in gives stereo out.
- Watch HOTSWITCH (an alternate parameter set per preset).

**Algorithms**
- **Hall:** DECAY in s, SIZE, PREDLY, LO/MID/HI-LVL, LO-DCY/HI-DCY, MODLVL.
- **Room:** DECAY in s, SIZE, PREDLY, LO/HI-LVL, HIFREQ, REFLEX, DFSION, MODLVL. 15 s may be out of range.
- **Plate:** DECAY in s, SIZE, PREDLY, LO/HI-DMP, DSTNCE, DFSION, MODLVL, TONE (centre).
- **ModEchoVerb:** DECAY (full right = INF), ECHO, E-FDBK, FX-MIX (no off).
- **BlackHole:** GRVITY/INVGRV (no seconds; inverse gives reverse-like decays), SIZE, PREDLY, LO/HI-LVL, M-DPTH, M-RATE, FEEDBK (to infinite), RESNCE.
- **Reverse Reverb:** DECAY is the build-up in ms; SIZE adds forward reverb and holds INF/FREEZE; FEEDBK, LATE, DIFFUS, MODLVL, CONTUR. For pure reverse: SIZE 0, FEEDBK 0, LATE 0.
- **Shimmer:** pitch is inherent, so record it for flavour only.

**INF vs FREEZE:** Infinity keeps taking input; Freeze holds what's there and blocks new input.

## Valhalla Supermassive
Sources: https://valhalladsp.com/2020/05/06/valhallasupermassive-the-controls/ · https://valhalladsp.com/2020/05/07/valhallasupermassive-the-modes/

**Controls**
- MIX (lockable), WIDTH.
- DELAY: the longest line in ms. It sets the onset; there's no predelay control.
- WARP: echoes up to 50 %, reverberant above.
- FEEDBACK %: decay, and in some modes the attack.
- DENSITY: 100 % is conventional.
- MOD Rate and MOD Depth.
- High Cut and Low Cut: −6 dB/oct, output only.

**Modes**
- **Sagittarius:** long attack; feedback shapes the attack and decay together.
- **Andromeda:** slowest attack, built-in predelay.
- **Large Magellanic Cloud:** long repeating echoes, medium attack.

**Matched settings**
- WARP 100 %, DENSITY 100 %, MOD at the Default preset.
- **FEEDBACK:** calibrate (guesses ~45 / 70 / 88 %).
- **INF:** FEEDBACK 100 %.

## Valhalla VintageVerb
Sources: https://valhalladsp.com/shop/reverb/valhalla-vintage-verb/ · https://www.musicradar.com/reviews/tech/valhalladsp-vintageverb-574692

**Controls**
- Mix, Predelay, **Decay 0.2–70 s**.
- Size, Attack, Early and Late Diffusion, Mod Rate and Mod Depth, High Cut, Low Cut.
- Damping: a high shelf, plus Bass Mult ×0.25–×4 below Bass Xover.
- COLOR: 1970s / 1980s / NOW.
- 22 modes. Nonlin with high Attack gives reverse reverbs.

**Matched settings:** COLOR NOW, Bass Mult 1×, damping open, High Cut max, Low Cut min. No INF.

## Valhalla FutureVerb
Sources: https://valhalladsp.com/2025/10/03/valhallafutureverb-the-controls/ · https://valhalladsp.com/2025/10/03/valhallafutureverb-the-reverb-modes/

**Controls**
- **Echo section:** Delay, Feedback, Level, Drive/Detune, Spread, 12 echo modes (incl. Reverse).
- **Reverb section:** **Decay RT60 0.2–70 s**, Size, Level, Routing, Early/Late or Attack, Density.
- Mod Rate and Mod Depth.
- EQ High and Low Cut (these also set the damping).
- COLOR: Bright / Neutral / Dark / Studio.
- Modes: Room, Chamber, Plate, Hall, Cathedral, Space, Frozen, Nonlin.

**Matched settings**
- Echo Level 0, Delay 0, Feedback 0, Echo Mode Modern, COLOR Neutral.
- No true INF: Frozen sustains for several tens of seconds.

## Valhalla Room
Sources: https://valhalladsp.com/2011/05/02/valhallaroom-the-high-level-sliders/ · https://valhalladsp.com/2011/05/18/valhallaroom-the-late-controls/

**Controls**
- MIX, PREDELAY, **DECAY (mid RT60) 0.1–100 s**, HIGH CUT, DEPTH (early/late balance; no neutral).
- Early section: Size, Cross, Mod, Send, Diffusion, Space.
- Late section: Size, Cross, Mod, Bass Mult/Xover, High Mult/Xover.
- Lo Cut. 12 modes. True stereo.

**Matched settings:** Bass and High Mult 1×, DEPTH 100 %.

## Valhalla Plate
Source: https://valhalladsp.com/2015/11/08/valhallaplate-the-controls/

**Controls**
- MIX, PREDELAY.
- **DECAY:** RT60 measured around 3 kHz. Most of the knob covers 0.5–6 s; the maximum is unpublished.
- SIZE, WIDTH, EQ shelves on the output, MOD Rate and MOD Depth, 12 modes.
- Above about 2–3 s, the lows ring longer than the display and the highs stay fixed.

**Matched settings:** EQ gains 0 dB, Width 100 %.

## Intellijel Swells
Sources: https://intellijel.com/downloads/manuals/swells_quick_start.pdf · https://intellijel.com/shop/eurorack/swells/

**Controls**
- Sliders: PREDLY, SIZE, DECAY (no seconds), Lo-DAMP, Hi-DAMP, EBB (diffusion), FLOW (modulation, bipolar, zero at centre), EQ tilt.
- INPUT, MIX.
- Nine models.
- LO-FI.
- FREEZE, REVERSE, BURST, SWELL.
- Ten attenuverters, normalled to the SWELL generator.
- 96 kHz.

**Matched settings**
- Attenuverters centred, LO-FI off, DRIVE off.
- Models: Fog and Shadow.
- **Decay:** calibrate (guesses 30–35 / 50–55 / 75–80 %).
- **INF:** FREEZE. **Reverse:** REVERSE.

**Levels:** Eurorack, hotter than line.

## Across the units

- **No INF:** Ultrasheer, VintageVerb, FutureVerb, Room, Plate.
- **INF only approximately:** Mercury7 (SWELL hold) and Supermassive (FEEDBACK 100 %).
- **True INF or freeze:** H9 and Swells.
- **Reverse options:** H9 Reverse Reverb and Blackhole inverse gravity, Swells REVERSE, VintageVerb Nonlin, and FutureVerb's Reverse echo modes and high-Attack Space/Frozen/Nonlin.
- **Not found:** the Mercury7's decay in seconds; Supermassive's default modulation values; plugin latency (check with an impulse); the Swells send/return menu path.
