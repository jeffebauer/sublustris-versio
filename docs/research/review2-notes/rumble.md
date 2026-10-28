# The rumble at endless DECAY (overnight review 2, 28 Oct 2026)

The owner's note (TASKS, found in session 9): "PLATE (and HALL, less) at DECAY 1 with TONE fully CW slowly turns into sub-20 Hz rumble over ~10 minutes while the audible tail fades ~40 dB." Core v1.0.55 (`eac761a`), desktop renderer, branch `review2/rumble`.

Tags: **[M]** measured here (desktop: the module rounds differently, so these are evidence about the desktop); **[E]** estimated; **[L]** literature.

## In one paragraph

The rumble is not DC from saturation and not TONE's output tilt. It is the **floor governor's correction acting as a rectifier on the tank's spectrum.** At endless DECAY the loop is lossless and the governor alone sets its level. Its two directions have different shapes. **Loss** (the tail is over its hold or the floor, by any amount) adds to `lowRate`, and `absorbShared()` then switches the v1.0.25 "air" fully on: the highs lose an extra 3.4 dB/s at TONE CW (11 dB/s at noon) however small the loss is. **Make-up** (the tail is under its hold) makes `lowRate` negative, and the high rate is clamped at zero, so it lifts only the band below the 2.5 kHz loss shelf. A held tail dithers round its hold, so the highs drain while the lows are pumped and the total stays where the hold wants it. Over 10 minutes that's −35 to −46 dB above 2 kHz and +6 to +9 dB everywhere below 200 Hz, flat down to ~3 Hz, because nothing in the loop takes the sub-audio out (DC gain exactly 1, no DC blocker). PLATE's air is 8 dB/s, HALL's 5.5, MASSIVE's 0: the owner's "PLATE, HALL less" (MASSIVE drifts only through the make-up's shape). **Frozen pads have the same leak** (highs −19 dB in 100 s, starting ~30 s after the freeze), as do held pads. The fix is to make the governor's correction a flat gain (`IV_RUMBLE_FIX=1`). In HALL and MASSIVE, the still-lows tank must also hold its own level (`+16`, so `IV_RUMBLE_FIX=17`); otherwise HALL's lows still rise 11 dB in 10 minutes after loud input. Separately, **an input with a DC offset builds up in an endless tank** (DC gain 1, so a −60 dBFS offset reaches −25 dBFS in 2 minutes, and the governor's loss then kills the audible tail). That's a second, cheaper guard: a 5 Hz DC blocker on the wet's input (`IV_RUMBLE_FIX=2`).

## Recommendation

1. **`IV_RUMBLE_FIX=17` (flat correction + the still lows' own hold) to a listening page.** Show endless PLATE / HALL / MASSIVE after a hit, a pad and loud noise, plus a freeze, as built vs 17, with 0–30 s and the 9–10 minute stretch. It is a sound change after the first seconds of every endless tail and freeze; it needs the owner's ears, then the module's.
2. **`+2` (input DC blocker at 5 Hz) as insurance**, mainly for the plugin. It's inaudible (−0.17 dB at 20–40 Hz) but not bit-identical.
3. **Flash comes first**: ~450–500 B against 76 B spare.
4. **A spectral criterion for the hold tests** (the soak missed this; see the end).

## How it was found

Tools (all in `tools/review2/`): `run_cond.sh` / `quick.sh` render 10- or 2-minute conditions at 16 bit, measure per-second band energy (`band_env.py`: <1, 1–20, 20–40, 40–200, 200–2k, >2k Hz) and delete the WAV; `summ.py` compares them. `plate_probe.patch` is the temporary in-loop instrumentation (build with `-DIV_RUMBLE_PROBE` in a scratch build dir). Once a second it prints the governor's state and the **per-trip gain of the loop's loss filters** at DC, 1 kHz, 5 kHz and Nyquist; it also dumps the loop signal (`IV_PROBE_RAW`), and `IV_PROBE_NOHOLD=1` turns the hold off as a control. `raw_bands.py` reads the dump.

Stimuli: a 50 ms Hann noise burst then silence, and a 20 s pad (a soft A-major chord + pink noise down to ~1 Hz) then silence, each 10 minutes; 100 % wet. These conditions use the default SIZE, MOTION, DEPTH and ONSET.

### 1. The symptom, 10 minutes [M]

Band change from 25 s to 590 s (dB), as built (ERA 0; ERA 0.5 has the same shape: lows within 0.5 dB, >2k −40 / −35 / −31 dB where ERA 0 has −46 / −37 / −34):

| | 1–20 | 40–200 | 200–2k | >2k | total |
|---|---|---|---|---|---|
| PLATE burst TONE 1 | +9.1 | +9.0 | −2.4 | **−46.2** | −4.3 |
| PLATE burst TONE 0.5 | +6.3 | +5.9 | −3.5 | −34.7 | −0.3 |
| PLATE pad TONE 1 | −0.9 | +0.5 | −0.7 | −33.7 | −0.1 |
| HALL burst TONE 1 | +7.9 | +8.0 | −0.7 | −37.4 | −3.9 |
| HALL pad TONE 1 | +0.9 | +1.2 | −2.3 | −36.4 | −2.1 |
| MASSIVE burst TONE 1 | +4.3 | +4.0 | +3.0 | −0.4 | +0.4 |
| MASSIVE pad TONE 1 | −5.2 | +2.4 | −6.9 | −29.0 | −0.5 |

At 590 s the PLATE burst tail is flat per Hz from ~10 Hz to 500 Hz (−64 to −69 dB/Hz) and empty above 2 kHz: a low rumble whose bottom octave sits below 20 Hz. The pad's lows hold their level, but its highs go the same way.

### 2. Inside the loop [M]

The probe on PLATE, burst, TONE 1:

- t = 3 s: the tail's hold is taken at −34.1 dB while the tail is still at −29 (the 8 s level lags), so the governor pulls (extra rate +0.047/s). The per-trip loss is then DC −1.83 dB, 1 kHz −2.12, 5 kHz −3.60, Nyquist −4.05: the air, switched on by the governor's loss.
- t = 7–45 s: the tail is under its hold, and the make-up runs at its cap (−0.005/s). Per trip that's DC +0.194 dB, 1 kHz +0.168, 5 kHz +0.037, Nyquist 0: a low shelf.
- From then on the governor dithers round the hold. At t = 119 s the extra rate is **+0.000118/s** (tiny), and the trip is DC −0.005 dB, 1 kHz −0.29, **5 kHz −1.78, Nyquist −2.22**. In some seconds 80 % of the control chunks are on the loss side.
- The loop signal itself (not the output) shows the same drift: >2k −40 → −50 dB and 1–20 Hz −60 → −52 dB from 20 s to 119 s, with no DC build-up (mean ~1e-4).
- Peaks in the loop are ~0.1, far under the soft clip's knee (0.84), and the Neve's even term is 0 at infinite (`Engine.cpp` `loopEra.even *= keep * (1 - governorWeight)`). So no in-loop saturation acts and no DC is made in the loop.

### 3. Controls: what it is and isn't [M]

PLATE, burst, TONE 1, 2 minutes (band change 20–30 s → 110–120 s):

| Condition | 1–20 | 40–200 | >2k |
|---|---|---|---|
| as built | +6.4 | +6.8 | −8.3 |
| MOTION 0 / MOTION 1 | +6.3 / +7.1 | +6.5 / +6.1 | −7.2 / −6.7 |
| modulation off (`IV_MOD_SCALE=0`) | +6.2 | +6.4 | −5.3 |
| WEAR 0.5 (10 min) | +9.4 | +9.3 | −40.0 (WEAR 0: −46.2) |
| **hold off** (probe, `IV_PROBE_NOHOLD`; loop signal) | ~0 (−56) | ~0 | **−0.5** (holds at −24.5) |
| **flat governor** (`IV_RUMBLE_FIX=1`) | +1.3 | +2.4 | **+1.8** |

- Not MOTION, not modulation (allpass-interpolated reads), not WEAR (saturation, the BBD and its compander are gone since v1.0.42; tape compression doesn't act at these levels), not TONE's tilt or bell (both on the output, after the loop: `Output.h`). The loss filters' DC gain is exactly 1 at infinite and never above 1 unless the hold's make-up asks for it (by design).
- The still-lows tank follows its leader's governor (`Fdn.cpp:356`), so it carries the same make-up shape (HALL and MASSIVE).
- **With the hold off the loop holds its spectrum** (>2k −0.5 dB in 100 s), and it settles 8 dB louder than with the hold (−28.2 vs −36.6 dBFS out). The drain comes from the governor's correction, not from the loop.

### 4. The code [M]

- `core/dsp/Governor.h:177`: `highRate = air > 0 && lowRate > 0 ? lowRate + air·(0.45/highRatio) : lowRate/highRatio`. At finite DECAY `lowRate` is DECAY's, and the air is the room's (v1.0.25, as intended). At infinite, `lowRate` is only the governor's correction (`Plate.cpp:184`, `Fdn.cpp:394`), so the sign of a correction of any size switches several dB/s of high loss on or off.
- `Governor.h:180`: `if (highRate < infiniteHigh) highRate = infiniteHigh;`. A negative (make-up) high rate is clamped up to `infiniteHigh`, which is 0 below the floor (darken 0) or for TONE ≥ ~0.6, so the make-up lifts the band under the 2.5 kHz shelf and not the highs.
- `Governor.h:83-95`: the floor's loss, the hold's loss and the hold's make-up all land in the one `extraRate`.
- `Era.h:74`: "the loop needs no DC blocker, which would drain the lows of an infinite tail". That holds for DC made in the loop (the even term's mean is taken off). It doesn't hold for DC that comes in at the input; see 6.

### 5. Freezes have it too [M]

PLATE and HALL, the pad frozen at 12 s (DECAY 0.5, TONE 1), >2k band: as built −33 dB until ~40 s, then −41 (60 s), −48 (90 s), −55 (119 s). With the fix it holds at −34 the whole time. Lows and mids are the same either way (±0.3 dB). A frozen pad's documented intent is "loss → 0, highs kept (ratio → 1)" (`Engine.cpp` applySpace). The leak contradicts that, and the soak's ±0.2 dB pad criterion can't see it because it measures total level.

### 6. DC at the input [M]

The burst plus a constant −60 dBFS offset, PLATE, DECAY 1, TONE 1, 2 minutes: 1–20 Hz **+26 dB** and >2k **−61 dB**; the total, −25 dBFS, is almost all DC. The DC integrates (loop DC gain 1), the governor's floor loss holds it at the floor, and the audible tail is what goes. HALL: 1–20 Hz +11 dB, >2k −23 dB. The flat governor alone does not help this case (PLATE >2k still −34 dB in 100 s). The 5 Hz input DC blocker (`IV_RUMBLE_FIX=2`) removes it completely: the DC case with 1 + 2 measures the same as the clean burst with 1 above 20 Hz (1–20 Hz within 1.4 dB). Noise at −80 dBFS on the input changes nothing.

How likely is DC on the module? [L, not checked on hardware] libDaisy's WM8731 init writes 0x00 to the ADC path register, which leaves the codec's digital ADC high-pass on (the datasheet gives ~3.7 Hz at 48 kHz, as I recall it). The PCM3060 (newer Seeds) also has an ADC high-pass; its default state isn't checked here. So static DC is probably filtered on the module, but not slow sub-audio. **In the plugin nothing filters DC**: DAW material with an offset (some samples, analogue-modelled synths) would do this at endless DECAY.

## The fix (prototype, off by default)

`IV_RUMBLE_FIX` (renderer, `host/render/main.cpp`; `Engine::setRumbleFixPrototype`; all under `#ifndef IV_FIXED_VOICINGS`, so the firmware is unchanged; release 112,564 B with it in the tree). With the hook off: `tools/same_render.py` SAME 60/60 against ref-main, ctest 100 % of 21, release / m0test / profile 112,564 / 83,556 / 127,808 B. Bits add:

- **1, flat governor correction** (`Governor.h` `absorbSharedFlat`; `Plate.cpp`, `Fdn.cpp`). TONE's ratio, the air and ADR 0008's darkening shape DECAY's own loss (plus the tape compression's) as today. The governor's correction, loss or make-up, is added to the low and high rates alike, so it changes the level and not the colour. Every tank uses it: PLATE, its twin, HALL / MASSIVE and the still-lows tank.
- **2, a one-pole DC blocker on the wet's input** at 5 Hz (`Filters.h` `DcBlocker`, Välimäki's form with the (1+R)/2 gain, before the predelay; the dry is untouched).
- **16 (with 1, so 17), the still-lows tank holds its own level.** It keeps its leader's floor loss, but takes its own hold, captured from its own 8 s level whenever its leader's hold is (re)taken, in place of the leader's hold correction (`Fdn.cpp`; `Governor.h` keeps the hold's share apart as `holdRate`). Why: with 1 alone, HALL after loud input still had its lows rising 11 dB in 10 minutes (see below). The low tank is lossless and unmodulated, but it takes the make-up the shared governor works out for the whole wet, which mostly makes up the main tank's small losses. So it grows.
- 8, a cheap partial alternative to 1: the air faded out across the infinite crossover and while frozen. **Not enough**: it fixes pads and freezes, but on the burst the make-up's low shelf pumps the lows +10 dB in 100 s, faster than as built. Kept only as evidence.
- (Tried and dropped: making only the hold's share flat. The pad, sitting at the floor, dithers on the floor's loss too, and drained as built.)

### What the fix does, 10 minutes [M]

| | as built: 1–20 / 40–200 / 200–2k / >2k / total (25 → 590 s) | `IV_RUMBLE_FIX=1` | fix − as built at 590 s (1–20 / 40–200 / 200–2k / >2k / total) | fix's max total after 60 s vs 60 s |
|---|---|---|---|---|
| PLATE burst T1 | +9.1 +9.0 −2.4 −46.2 −4.3 | +2.0 +1.7 +1.8 +0.5 +0.6 | −12.9 −13.7 +0.5 +53.2 +9.1 | +0.5 |
| PLATE burst T.5 | +6.3 +5.9 −3.5 −34.7 −0.3 | +2.0 +1.7 +1.8 −0.2 +0.2 | −14.0 −14.7 −2.1 +43.3 +0.5 | +0.3 |
| PLATE pad T1 | −0.9 +0.5 −0.7 −33.7 −0.1 | −1.3 −0.1 +1.8 +0.9 +0.7 | −0.5 −0.6 +2.6 +37.6 +1.2 | +1.0 |
| PLATE pad T.5 | −0.3 +1.4 −9.2 −13.0 +0.7 | +0.3 +1.2 −5.1 −15.9 +0.7 | +0.4 −0.2 +4.1 +1.4 0.0 | +1.1 |
| HALL burst T1 | +7.9 +8.0 −0.7 −37.4 −3.9 | +2.9 +2.8 +2.2 −0.2 +0.1 | −10.0 −10.1 +0.5 +43.2 +8.1 | +0.3 |
| HALL burst T.5 | +4.5 +4.6 −2.6 −33.7 −1.0 | +2.9 +2.8 +2.2 −0.5 +0.2 | −11.0 −11.1 −1.3 +41.2 +1.2 | +0.3 |
| HALL pad T1 | +0.9 +1.2 −2.3 −36.4 −2.1 | 0.0 +0.3 −0.4 −1.1 −0.4 | −1.1 −0.9 +1.9 +38.8 +2.3 | +0.6 |
| HALL pad T.5 | +3.7 +1.7 −6.7 −13.1 −0.8 | +3.4 +1.9 −5.9 −17.0 −0.8 | −0.5 +0.3 +0.7 −0.9 0.0 | +0.6 |
| MASSIVE burst T1 | +4.3 +4.0 +3.0 −0.4 +0.4 | +0.3 −0.1 −0.2 −0.3 −0.3 | −5.3 −5.5 −3.8 +2.5 +1.3 | +0.1 |
| MASSIVE pad T1 | −5.2 +2.4 −6.9 −29.0 −0.5 | −4.9 +2.2 −5.4 −7.6 −0.4 | +0.3 −0.1 +1.5 +21.2 +0.1 | +1.8 |
| MASSIVE pad T.5 | −3.3 +2.3 −10.9 −11.3 +0.2 | −3.3 +2.2 −10.0 −13.0 +0.3 | 0.0 0.0 +0.7 −0.7 0.0 | +1.9 |

- **The rumble is gone**: sub-20 Hz now rises at most 2–3 dB in 10 minutes (it was +6 to +9), and at 590 s it is 10–14 dB lower than as built in PLATE and HALL. The highs hold within ±1 dB at TONE CW (as built: −34 to −46 dB).
- **ADR 0008's darkening is kept**: pads at TONE noon still darken to their warm floor (>2k −13 to −17 dB either way). That's `darken` working as designed, not the leak.
- **Energy is bounded**: total level within ±0.8 dB from 25 s to 590 s in every case; the worst excursion after 60 s is +1.9 dB (MASSIVE pad).
- **Clicks**: DECAY swept 0.7 → 1 → 0.75 → 1 and TONE 1 → 0 → 1 over a fed pad, every SPACE, modes 0 / 1 / 3: click=0 everywhere; the analyser's largest step is within 0.1 dB of as built with 1, and within 0.7 dB with 3 (the DC blocker's own effect on my sub-audio-heavy pad).
- **Unchanged**: every finite DECAY, bit for bit (PLATE, HALL, MASSIVE at DECAY 0.6 and 0.8 with the hook on: `cmp` same; the governor's rate is 0 there, and the sums are the same operations). An endless tail's first ~3 s after a hit is unchanged too (≤0.1 dB), as is a fed endless tail below 2 kHz (pad, fed 5–20 s: ≤0.1 dB; >2k +0.4 to +1.2 dB, because the air leaks today even while fed at the floor).
- **What changes is audible and needs the owner's ears**: from ~3 s after a hit into an endless tail, the as-built tail has a bass bloom and a fast darkening. With the fix, 5–30 s after a burst (PLATE TONE 1), 40–200 Hz is −6 dB, 200–2k −4 dB, >2k +3 to +5 dB and the total +1.7 to +3.4 dB against as built. The as-built early colour is the same leak in its first seconds; whether the owner has come to like it is a question for him.
- **The still lows (HALL, MASSIVE) after loud input** [M]. 20 s of noise at −9 dBFS RMS, then silence, TONE 1, band change 25 → 590 s:

  | | 1–20 | 40–200 | 200–2k | >2k | total |
  |---|---|---|---|---|---|
  | HALL as built | +13.7 | +14.3 | +1.7 | −39.6 | −6.0 |
  | HALL, 1 | +11.3 | +11.4 | +3.0 | −0.7 | −0.2 |
  | **HALL, 17** | **+2.8** | **+2.9** | +2.4 | −0.1 | +0.2 |
  | MASSIVE, 17 | +0.4 | +1.2 | +0.2 | 0.0 | 0.0 |

  The control: with the still lows off (`IV_STILL_LOWS=0`, one tank) and 1, HALL's lows rise only +0.5 dB in 100 s (with two tanks, +5.2); with the modulation off and 1, still +5.9. So it is the two tanks sharing one hold, not the modulation. With 17 the other cases match 1 (HALL burst: lows +2.6, >2k −0.2; HALL pad: all bands within ±1.2), and MASSIVE's pad keeps its highs better (>2k +2.6 dB in 10 minutes, where 1 gives −7.6 and as built −29). A frozen HALL pad holds within +2.4 dB at >2k with 17 (+0.2 with 1, −32 as built): a slight lift, not a drain. On its first second-by-second numbers, 17 is the best candidate.
- The DC blocker alone: the burst's 20–40 Hz −0.17 dB, 40–200 Hz −0.01 dB, nothing above. My pad's synthetic sub-1 Hz noise shifts the governed level by +0.1 to +0.5 dB; music has little there.

### Cost

- **CPU** [E]. Bit 16: a dozen operations per low-tank chunk on levels its governor already works out (no new log10); effectively 0 %. Bit 1: two float adds per tank per control chunk; the loss filters already work their `pow`s out again every chunk at infinite DECAY (the governor moves), and at finite DECAY the memo's hit rate is unchanged (the governor's rate is 0). Effectively 0 % of the module's 10,000 cycles a sample. Bit 2: one-pole HP on 2 channels, ~4 arithmetic operations + 2 loads/stores each, ~15–20 cycles a sample, **~0.2 %**. An in-loop DC blocker would be ~4 instructions per line-sample: PLATE 2 lines at 48 kHz (~0.1 %), HALL / MASSIVE 8 lines at 24 kHz + 8 at 12 kHz (~0.2 %). It isn't needed, because no DC is made in the loop at infinite.
- **Flash** [M] (release built with the fix compiled in, then reverted). First form (a separate `absorbSharedFlat` + memo change) +480 B. Lean form (the flat rate as a 7th input of `absorbShared` / `AbsorbMemo`, `tools/review2/firmware_trial_lean_flat.patch`): **+200 B** (absorbent and AbsorbMemo::get +24, Fdn::control +44, applySpace +52, the constructors and reset +56 from the memos' wider initialisers, the rest small shifts). Input DC blocker: **+144 B** (with `setCutoff` at prepare; a firmware-only constant coefficient would save some). Bit 16, not built for the module: ~100–150 B [E]. Together ~450–500 B against **76 B spare**: the fix needs flash found elsewhere first (firmware/README's techniques; the CPU/flash agent's notes).
- **RAM**: +40 B (memo inputs), +16 B (the blocker's state), +12 B (bit 16).

### Risk

- Bit 1 changes every endless and frozen tail after its first seconds; it needs the owner's ears (a page: burst, pad, freeze; 2-minute and 10-minute excerpts; as built vs fix) and then the module's ears (the governor's dithering on the module rounds differently).
- The tape compression's loss (`extraRate_`, WEAR ≥ 0.35, tails above −18 dBFS) still goes through the air's on/off gate at infinite DECAY. It isn't reached in these renders (tails at −22 to −37 dBFS); a loud endless tail at high WEAR could show a milder version. Making the air depend on DECAY's own rate (not the summed rate) would close it.
- The hold is taken from the 8 s level ~3 s after a hit, while the tail is still settling, so the tail is pulled ~8 dB under where the loop would hold it on its own (−36.6 vs −28.2 dBFS, PLATE burst). That's a separate finding; with the fix the pull-down is flat, so it no longer colours the tail.
- Bit 2 high-passes the wet's input at 5 Hz: −0.26 dB at 20 Hz, −0.07 dB at 40 Hz for one pass. Inaudible, but it is a change to every SPACE and DECAY (not bit-identical).

## The soak audit: why it missed this

`docs/research/soak-audit.md` judged held tails on level only: the "hold" episode required the 4 s level, from 25 s after it settled, not to fall 3 dB or grow 2 dB, over 3–6 minutes; pads were judged at ±0.2 dB on noise. This mechanism holds the total level by construction (the make-up replaces what the air takes), so every check passed. Its notes did see the shape once: "+8.8 dB (an infinite PLATE at TONE fully dark: the highs fading and the makeup lifting)" and finding 12 (the Midiverb box's low-pass at infinite). Both are the make-up's low shelf at work. The bigger half, the air's on/off gate, couldn't be seen: the air arrived in v1.0.25 (24 Oct), after the soak's Core (v1.0.20, 23–24 Oct). Since then the endless checks (`test_massive`, the golden holds) have measured total level only. **Suggestion:** give the hold checks a spectral criterion, e.g. the >2 kHz and <40 Hz band levels at 10 minutes within ±3 dB of 30 s, in PLATE / HALL / MASSIVE, burst and pad, TONE 0.5 and 1. A 2-minute version fails today by −8 dB (>2k, PLATE burst) and passes with the fix.

## Sources

- [L] J.-M. Jot, A. Chaigne, "Digital delay networks for designing artificial reverberators", AES 90th Convention (1991), preprint 3030: absorbent filters per line set from T60(ω). The governor's correction is a change of T60 for the whole tail, and a level control in a lossless loop is frequency independent unless the voicing asks otherwise.
- [L] J. O. Smith, *Introduction to Digital Filters*, "DC Blocker": https://ccrma.stanford.edu/~jos/filters/DC_Blocker.html (the one-pole form used; pole at R ≈ 1 − 2π·fc/fs).
- [L] V. Välimäki, J. Parker, L. Savioja, J. O. Smith, J. Abel, "Fifty Years of Artificial Reverberation", IEEE TASLP 20(5), 2012 (the repo's `Filters.h` cites Välimäki for a DC blocker in the loop).
- [L] Cirrus / Wolfson WM8731 datasheet, "ADC high pass filter" (ADCHPD bit; ~3.7 Hz at 48 kHz, quoted from memory, to re-check), and libDaisy `src/dev/codec_wm8731.cpp` (`CODEC_ADC_HPF = 0x00`).

## What I couldn't check

- The module: whether the codec's high-pass removes input DC on this Versio, what DC and sub-audio its input carries with nothing patched or with typical sources, and how the governor's dithering behaves there (it rounds differently: no FMA, flush-to-zero).
- The plugin in Ableton (not run; no installs tonight).
- A listening page. The fix needs one before anything ships.
