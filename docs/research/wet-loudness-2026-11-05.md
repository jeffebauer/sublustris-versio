# Wet loudness at full wet: how loud the reverb comes back

5 Nov 2026, overnight, on v1.0.70 (`claude/proto-wear-taper-78e1da` at `4b95b8a`; branch `claude/wet-loudness-8e55d6`). The report: lfad on Modwiggler (4 Nov 2026), comparing against Desmodus, Pleroma and their other reverbs: "I always run full wet pretty much, but yeah, the gain makes a difference on mine for sure, and pumping the levels helped a bit (though it does remain quieter on the output than my other reverbs)."

Scripts: `tools/pages/wet_loudness.py` (`grid`, `kit`, `refs`, `table`) and `tools/pages/wet_loudness_page.py` (the page and its gates). Loudness is BS.1770-4 integrated, gated (`wet_loudness_lib.py`, checked against the standard's 997 Hz tones to 0.01 dB), over each render's whole file. "Wet − dry" is the reverb's loudness at MIX full minus the loudness of the sound that went in, in LU (1 LU = 1 dB of loudness).

## The short answer

(The owner's picks, the HOT crackle and the DECAY taper: section 7.)

- **Yes, it's quieter, but only a little against its own input.** At MIX full the reverb comes back about **1 LU under the dry** on average. At DECAY 9 o'clock and noon it's **2 to 3 LU under**. At 0.75 it's level, and at endless 1 LU over.
- **Against other reverbs it's 2 to 5 LU quieter.** On the same input, Valhalla Room comes back **2 to 3 LU over** its dry at every decay. Supermassive sits about 0.5 under, and Valhalla Plate gets louder as it gets longer (−2 to +8). At short and medium DECAYs we're **4 to 5 LU under Room** and about 2 under Supermassive. At endless we're within about 1 of Room.
- **"Pumping the levels helped a bit" is the endless governor.** At finite DECAYs the reverb follows the input exactly: play 3 dB harder and it's 3 dB louder. At endless it follows only part of it: PLATE about 45 %, HALL 55 %, CANYON about 22 %. Played hot, endless PLATE and CANYON fall to 1 to 2 LU *under* the dry.
- **There's clean headroom to spend.** On ordinary material the reverb's peaks sit at a median **−11.7 dBFS**, and the limiter never acts. Played hot (every phrase peaking at −1 dBFS), the median is −10 dBFS, and the limiter takes more than 1 dB in only 4 of 288 renders.
- **Nothing published says how loud Desmodus or Pleroma come back.** Desmodus's manual gives the input's clip point (about 16 V peak to peak) and nothing on wet level. Pleroma's site only mentions fixing clipping.
- **Recommendation: a fixed make-up on the wet, picked by ear on `renders/wet_loudness`.** I lean to **C, +3 dB** for every SPACE. It costs **0 bytes and no CPU**: it folds into TONE's existing make-up table. Pair it with the pending HALL/CANYON level match (`claude/rack-levels`), so HALL's endless doesn't lean on the limiter.

## 1. Our wet against its dry (v1.0.70, desktop)

The grid: PLATE, HALL and CANYON × NATURAL, BLOOM and SHIMMER × DECAY 0.3 (9 o'clock), 0.5 (noon), 0.75 and endless × WEAR 0 and noon. Each was run on the pad, piano, drums and stabs (`renders/_material`). MIX full; SIZE, TONE and MOTION at noon; DEPTH 0.2. That's 288 renders at the material's own level (dry peaks −3 to −7 dBFS, −19 to −23 LUFS). The same 288 were run **hot**, each phrase scaled to a −1 dBFS peak (+2 to +6 dB).

**Wet − dry, LU, mean over ONSET, WEAR and the four phrases:**

| Own level | DECAY 0.3 | 0.5 | 0.75 | endless | all |
|---|---|---|---|---|---|
| PLATE | −3.0 | −2.2 | +0.1 | +0.4 | −1.2 |
| HALL | −1.9 | −2.0 | +0.2 | +1.5 | −0.5 |
| CANYON | −2.6 | −1.6 | +0.1 | +0.6 | −0.9 |
| **all** | **−2.5** | **−1.9** | **+0.1** | **+0.8** | **−0.9** |

| Hot (−1 dBFS peaks) | DECAY 0.3 | 0.5 | 0.75 | endless | all |
|---|---|---|---|---|---|
| PLATE | −3.1 | −2.3 | −0.8 | −1.4 | −1.9 |
| HALL | −1.9 | −2.0 | −0.7 | +0.3 | −1.1 |
| CANYON | −2.6 | −1.7 | −1.3 | −1.8 | −1.8 |
| **all** | **−2.5** | **−2.0** | **−0.9** | **−1.0** | **−1.6** |

- **Spread:** −5.1 to +3.3 LU at the material's own level, −5.2 to +2.9 hot.
- **The loudest 3 s window** (short-term max) reads about 1 LU higher than the integrated figure at long DECAYs (the tail builds up) and the same at short ones: the mean is 0.0 LU (own level) and −0.8 (hot).
- **Peaks** sit 6 to 8 dB under the dry's (a reverb has no transients), so a peak meter makes it look quieter still.
- **ONSET changes nothing:** NATURAL −0.9, BLOOM −0.9, SHIMMER −0.8.
- **WEAR noon costs 0.3 LU** (hot 0.6) against WEAR 0.
- **By phrase:** stabs −0.3, pad −0.7, piano −1.1, drums −1.3.
- **The quietest settings:**
  - PLATE BLOOM at DECAY 9 o'clock on drums: −5.1.
  - CANYON at endless played hot on the pad: −5.0 (the governor; see below).

**How the tail follows the input.** I compared each phrase's hot render against its own-level render: the wet's rise in dB divided by the dry's.

- **Finite DECAY** (noon): 0.96 to 1.00 in every SPACE. The reverb follows the input exactly.
- **Endless**, by phrase (pad, piano, drums, stabs):
  - PLATE 0.46 / 0.42 / 0.43 / 0.47;
  - HALL 0.57 / 0.60 / 0.49 / 0.98;
  - CANYON 0.23 / 0.21 / 0.23 / 0.55.

So the owner's reply on the thread is half right. The reverb follows the input everywhere below endless. At endless, the floor governor (inside the loop: each dB the tank sits above −21 dBFS adds loss, ADR 0008) holds the tail back as you play harder. That's lfad's "pumping the levels helped a bit". There's no duck any more (none at endless since v1.0.61).

## 2. The gain staging, read from the code

What the wet passes through, in order, and what each stage costs or gives in level:

| Stage | Where | Level |
|---|---|---|
| Codec in → Engine | `firmware/main.cpp:2954-2971` | No scaling. The Engine sees the codec's samples as they are |
| DC blocker, predelay, input diffusers, tank input | `core/dsp/Engine.cpp:1031-1110` | Unity (allpass diffusers) |
| Each tank's output trim | `SpaceVoicing.h` | PLATE `kPlateOutputTrim` 0.43 (−7.3 dB); HALL and CANYON's FDN `kFdnOutputGain` 0.6 (−4.4 dB), scaled by √(mean line length / 0.1 s), then HALL `kHallOutputTrim` 0.767 (−2.3 dB) and CANYON `kMassiveOutputTrim` 0.60. **Every trim is a cut**, made to match the SPACEs to each other (and to HALL) |
| DECAY's level make-up | `Mappings.h:259` `decayLevelMakeup` | Up to the finite top's level: a short HALL tail gets (15 s / T60)^0.47 (2 s: +4.1 dB). It makes the DECAYs match each other, not the dry |
| WEAR's make-up | `Engine.cpp:669` `eraMakeup_` | `kMakeupNeveDb` +0.2 dB (plus the radio band's). This is *before* WEAR's tape, so it drives it |
| TONE's tilt and bell, with `kMakeupDb` | `Output.h:176` | 0 dB at noon, −0.2 to −2.3 dB elsewhere. **Never a boost** |
| The wet limiter | `Output.h:196`, `kLimitCeiling` 0.891 | −1 dBFS ceiling, instant attack, 0.2 s release |
| MIX, equal power | `Mappings.h` `mixGains` | Full wet = the wet × 1 |
| Firmware output trim | `main.cpp:2558` `kOutputTrim` −0.874 | −1.17 dB and the polarity on dry and wet together, so MIX 0 is a cable (Resilio's M0 check). It doesn't change the balance |

**Where the level target comes from.** Nothing in the chain aims the full wet at the dry's level:

- The FDN's gain was tuned "so a Hall tail sits near the dry's level at MIX noon" (by ear at M1; `kFdnOutputGain`'s comment).
- PLATE and CANYON were then trimmed *down* to HALL's loudness.
- DECAY's make-up brought the shorter tails *up* to the finite top's level, and endless was matched to that top.

So the reference is "HALL at MIX noon, at a long DECAY, balanced with the dry". At 0.75 and endless that's where we are (±1 LU). Short and medium tails sit 2 to 3 LU lower. The make-up toward the finite top was fitted on HALL, and the long-tail trims (`kDecayLongTrimDb`, up to −2.1 dB) take a little back. The headroom is given away by the SPACE trims, which match the loudest down, and by TONE's make-up, which only cuts.

**Clean gain before the limiter, as built:**

| | Own level | Hot |
|---|---|---|
| Wet peak, median | −11.7 dBFS | −10.0 dBFS |
| Wet peak, 95th percentile | −5.6 dBFS | −2.0 dBFS |
| Renders where the limiter takes > 0.1 dB | 0 / 288 | 8 / 288 |
| Renders where it takes > 1 dB | 0 / 288 | 4 / 288 (HALL at 0.75 and endless, the hot pad; deepest 1.8 dB) |

So about **5 dB of clean gain is free on ordinary material**, and about **2 dB played hot**, before the limiter does real work. The hot pad in HALL at endless is the one case already at the ceiling.

## 3. The owner's Valhalla takes, as typical full-wet behaviour

The takes in `test_audio/reference/incoming/` were made from the owner's Ableton set, which has no takes newer than 25 Oct. All are 100 % wet at unity gain on the reference kit's `music.wav`. I measured each of the kit's nine pieces against the same piece's dry, then took the mean. Ours went through the same kit and the same code (`wet_loudness.py kit`, NATURAL, WEAR 0). No reference audio was copied or committed.

**Wet − dry, LU, mean of the nine pieces:**

| | short | medium | long | infinite |
|---|---|---|---|---|
| ValhallaRoom Large Room (2 / 5 / 15 s) | +2.2 | +2.4 | +1.8 | |
| ValhallaRoom Dark Chamber | +2.8 | +2.2 | +1.7 | |
| ValhallaPlate Chrome | −1.8 | +3.9 | +8.3 (clipped) | |
| ValhallaPlate Lithium | −1.7 | +2.3 | +5.8 | |
| Supermassive Andromeda (feedback 0 / 12 / 54 / 100 %) | −0.6 | −0.6 | −0.4 | +3.5 (clipped) |
| Supermassive Sagittarius (7 / 30 / 73 / 100 %) | −0.6 | −1.0 | −1.8 | −4.1 |
| **Ours, PLATE** (DECAY 0.3 / 0.5 / 0.75 / endless) | −3.1 | −2.5 | −0.6 | −0.2 |
| **Ours, HALL** | −2.7 | −2.8 | −1.2 | +1.1 |
| **Ours, CANYON** | −3.5 | −2.5 | −0.9 | +0.8 |
| Ours + 3 dB (C), PLATE | −0.1 | +0.5 | +2.4 | +2.8 |
| Ours + 3 dB (C), HALL | +0.3 | +0.2 | +1.8 | +4.1 |
| Ours + 3 dB (C), CANYON | −0.5 | +0.5 | +2.1 | +3.8 |

The columns line up by role (short tail to infinite), not by matched T60.

- **Room** is the clearest "typical" reverb: +2 to +3 LU at every decay.
- **Supermassive** is level with its dry at every feedback below infinite.
- **Plate** rises with decay.
- **Ours** is 4 to 6 LU under Room and about 2 to 3 under Supermassive at short and medium settings, and within about 1 of Room at long and endless.
- **With +3 dB** we land at Supermassive's level at short settings and Room's at long ones.

This agrees with `rack-levels.md` (29 Oct, the same takes, ungated). That note found our noon balance no louder than Valhalla's 50 %, because both use the same equal-power law.

## 4. Desmodus and Pleroma

**Desmodus Versio** (Noise Engineering's manual, https://noiseengineering.us/manuals/desmodus-versio/):

- Blend is a plain dry/wet control: fully right, "only the processed signal is heard".
- "The audio inputs clip around 16 V peak to peak."
- LIM mode uses "limiting within the reverb tank to contain feedback"; DST uses saturation instead.
- Past 3 o'clock on Regen, the infinite reverbs are ducked by new input.
- **Nothing on the wet's level against the dry, or on output level.**

**Pleroma** (https://recursivefieldinstruments.com/pleroma) is a free third-party Versio firmware written in FAUST, not a Noise Engineering one.

- Knob 1 is a mix from "100% dry to 100% wet".
- Its changelog, v0.5 (12 Sep 2026), says it eliminated "almost all of the clipping and saturation that could occur on the Versio hardware", and mentions "wet-level surges". So it seems to run its wet hot.
- **No level figures are published.**

**Nothing published** gives either unit's wet loudness. A measured comparison needs a recording (task 2 below).

One inference, unverified for our firmware: if the Versio's codec reaches full scale near the 16 V p-p input clip Desmodus's manual gives (the same hardware), then a typical 10 V p-p Eurorack signal peaks around −4 dBFS. That's close to our "own level" tier, and the "hot" tier (−1 dBFS) is about 14 V p-p.

## 5. The module's absolute level is unmeasured

This is review 2's open item. Volts to dBFS on the input, and dBFS to volts on the output, have never been measured on this module. Resilio's M0 check measured only that the output is 1.17 dB hotter than a cable. The release build already has a calibration point: the In LEDs turn red at an input peak of −1 dBFS (`LedMeter.h` `kInputRedDb`), and the Out LEDs turn red when the limiter takes 0.5 dB.

**Task for the owner (rack power, never USB with it):**

1. **Volts at −1 dBFS** (10 minutes). Patch a steady oscillator sine (about 200 Hz to 1 kHz) into IN L, with MIX fully left. Raise its level until the In L LED just turns red. Measure that signal's peak-to-peak volts with a scope, or with a multimeter on AC volts × 2.83. Repeat on IN R. That one number calibrates the input. At MIX 0 the output is the same signal, a calibrated cable.
2. **Optional, the direct answer to lfad.** Record one take of the kit's `music.wav`, from the interface into the rack, through Sublustris at MIX full (HALL, DECAY noon, the rest at noon). Then record the same take through another reverb you have (Swells, Mercury7, H9, or a friend's Desmodus), also fully wet, with the interface's input and output gains untouched between takes. Put both in `test_audio/reference/incoming/` (never committed). I'd measure them the same way as the Valhalla takes.

## 6. Options, costs and risks

| Option | What changes | Flash | CPU | Risk |
|---|---|---|---|---|
| **A. No change** | Nothing | 0 | 0 | The forum note stands. Players turn the return up on their mixer |
| **B / C / D. A fixed wet make-up of +2 / +3 / +4.5 dB**, every SPACE (on the page) | The wet × a constant after WEAR, before the limiter. Shipped by adding it to the nine `kMakeupDb` numbers | **0 B**: a trial build with +3 folded in, release 111,844 B, the same as v1.0.70 (m0test and profile also unchanged) | 0 | The limiter, the MIX noon balance, the goldens and level tests, and `rack-levels` / `input_stop` (below) |
| Per-SPACE make-up | Each SPACE its own constant | Not measured (a 3-entry table and a multiply a sample; an estimate, tens of bytes) | ~1 multiply a sample | SPACEs differ by only 0.7 LU on average. `rack-levels` already carries the per-SPACE endless trim |
| Per-ONSET make-up | | | | ONSET changes the level by ±0.1 LU: nothing to fix |
| A DECAY-shaped make-up (more at short DECAYs) | Lift `decayLevelMakeup` below 0.75 | 0 B if done in `kDecayLevelPower` | 0 | It sits before WEAR, so it drives the tape harder. It reopens ADR decisions (decay_level C, decay_swell B, decay_range) |
| The tail follows the input at endless | The governor's floor relative to the playing's loudness, not a fixed dBFS | Unknown | Small | The hold, freeze and paint levels; v1.0.61's "endless follows the playing"; `input_stop`. Needs research first (`wet-dry-balance.md` option 3) |

**Risks of B, C and D, measured** (the 288-render grid, the limiter's own count via `IV_LIMIT_STATS`):

| | A | B +2 | C +3 | D +4.5 |
|---|---|---|---|---|
| Wet − dry, own level (mean, LU) | −0.9 | +1.1 | +2.1 | +3.6 |
| Wet − dry, hot | −1.6 | +0.4 | +1.4 | +2.8 |
| DECAY 0.3 / noon, own level | −2.5 / −1.9 | −0.5 / +0.1 | +0.5 / +1.1 | +2.0 / +2.6 |
| Limiter > 1 dB, own level (renders) | 0 | 0 | 1 (0.03 % of its samples) | 3 |
| Limiter > 0.1 dB / > 1 dB, hot (renders) | 8 / 4 | 29 / 16 | 46 / 29 | 74 / 58 |
| Its deepest reduction, hot | 1.8 dB | 3.8 | 4.8 | 6.3 |
| HALL endless, held after the hot pad: limiter > 0.1 dB | 0.6 % | 12.7 % | 22.4 % | 71.5 % |
| The module's output peak at MIX noon, worst page render | −2.2 dBFS | −1.2 | −1.1 | −0.6 |

- **The limiter.** At the material's own level it stays out of the way up to C. Played hot, it starts working in HALL at 0.75 and endless, mostly on the pad. Mean reduction while acting is 0.06 dB (B) to 0.19 dB (D), but the deepest moments reach 4 to 6 dB in C and D.
- **Endless holds.** The make-up is outside the loop, after the governor's level detector. Over 60 s of silence after the hot pad, no version grows: drift −0.15 to +0.43 dB, the same in A. CANYON's +0.43 is as built.
- **MIX noon gets wetter by the same dB.** The owner found noon "a little too loud" on v1.0.57 (`rack_levels`). `rack-levels`' `IV_MIX_TAPER` B (noon wet −1.8, dry +1.2) would hold noon near today's balance with C.
  - The sum at noon has no limiter after it. Its worst peak on the page's hot cards is −1.1 dBFS in C and −0.6 in D, still under the codec's full scale (with the 1.17 dB output trim).
- **`claude/rack-levels`** (HALL −2 dB, CANYON +2 at endless). It stacks with any make-up and suits C:
  - HALL's endless nets +1, where the limiter has least room;
  - CANYON's endless nets +5, where played hot it's furthest under.
  - Re-measure both together.
- **`input_stop`.** The dip at noon is the dry leaving. With the wet 3 dB up, the dry is a smaller share of the noon mix, so the dip should shrink. Re-measure after.
- **Tests.** A level change moves the goldens (`IV_GOLDEN_UPDATE` with its SPEC line) and any test that reads the output against the −21 dBFS floor or a fixed level. Not run with C built; the hook leaves them untouched (below).

**Recommendation: C, +3 dB on the wet, if the owner agrees by ear on `renders/wet_loudness`, with the `rack-levels` trim.**

- C turns the average full-wet level from 1 LU under the dry to 2 over: between Supermassive and Valhalla Room, and Room's level at long DECAYs.
- It costs nothing in flash or CPU.
- It leaves the limiter idle on ordinary material.
- If the owner hears the hot HALL card squash, B (+2) is the safer step.
- The endless-follows-the-input question is the bigger, separate one. Leave it until after this.

## The page and the gates

**`renders/wet_loudness`** (main checkout, 1.5 GB). Seven cards:

- PLATE at DECAY 9 o'clock;
- HALL and CANYON at noon;
- HALL at 0.75;
- PLATE at endless;
- CANYON and HALL at endless played hot.

Each card has the pad, piano, drums and stabs. Versions: A_now, B_plus2, C_plus3, D_plus4_5. The renders are 100 % wet with the dry beside them for the live MIX fader. The page is **deliberately not level-matched** (it starts with level matching off and says so). `_verify.txt` has, per render: the sha1, peak, loudness against the dry, the limiter's count, the 1 ms click frame and the MIX-noon output peak, plus the hold check.

**The prototype:** `IV_WET_MAKEUP="db"` or `"plate,hall,canyon"` (`Engine::setWetMakeupPrototype`), and `IV_LIMIT_STATS=1`, which prints the limiter's count after a render (`Engine::limiterStats`). Both are behind `#ifndef IV_FIXED_VOICINGS`.

**The gates:**

| Gate | Result |
|---|---|
| `tools/same_render.py`, against this branch's renderer before the hook | **SAME 60/60** with the hook off, and again with `IV_WET_MAKEUP=0` |
| Firmware, `make -C firmware all-variants` | release 111,844 B, m0test 83,556, profile 131,028. **`cmp`-identical** to v1.0.70's build from `4b95b8a` (all three) |
| ctest (`build-wl`) | "100% tests passed out of 23". The Plugin's test isn't built in this worktree: `libs/JUCE` is empty |
| The 1 ms click check | Worst frame 12.4 dB over its neighbours, every version (music's own reads up to 15-20). The same in A |
| The endless hold | No growth (above) |
| The limiter per version | The tables above |

## Open questions

- **The module's volts to dBFS** (task 1). Until then, the own-level and hot tiers bracket Eurorack levels only by inference.
- **What lfad's "other reverbs" are**, and their levels. Only a recording answers it (task 2). No figures are published for Desmodus or Pleroma.
- **Whether +3 dB at noon reads as too wet** to the owner, and whether to pair it with the MIX taper.
- **The page wasn't opened in a browser by me.** The pane can't drive a file:// page; it was built by `make_review.py` as every earlier page was.

## 7. After the owner's picks: the HOT crackle and a DECAY taper (6 Nov 2026)

The owner's picks, listening at MIX 50 % and moving the fader, came relayed through the orchestrator session: +4.5 dB on PLATE 9 o'clock and HALL noon; +3 dB on HALL 0.75, PLATE endless (pad +4.5) and CANYON noon (stabs +4.5); +3 dB on the hot endless cards (HALL's pad "no clear preference" between +2 and +3). And: "Hearing a little crackle/distortion on some of the HOT level examples". They didn't say which cards or versions. Measured; nothing is ported. Scripts: `tools/pages/wet_loudness_crackle.py`, `wet_loudness_tone_probe.py`, `wet_loudness_taper.py`; hooks `IV_DECAY_TAPER`, `IV_LIMIT_OFF`, `IV_LOOP_SAT_OFF` (off by default; firmware `cmp`-identical to `4b95b8a`, all three variants; renderer SAME 48/48 against v1.0.70).

### The crackle

Nothing in card 6 (CANYON hot). No saturation, no limiter, no overs at MIX 50 % in any version. Its faint top-end products (-63 dB under the wet on the low-tone control) are the same with saturation and limiter off and don't change with the lift. If the owner heard it there, nothing here explains it.

Card 7 (HALL hot) has three stacked things, on the pad and drums, some on the piano, none on the stabs. Measured with float renders (the WAV writer clips integer output at ±1, which hid the overs on the first pass).

| Mechanism | A | B +2 | C +3 | D +4.5 | On the module? |
|---|---|---|---|---|---|
| Tank saturation (pad): samples over the knee | 0.385 % | same | same | same | Yes; by design, before the lift |
| Limiter (pad): > 1 dB, % of samples | 0.2 | 5.0 | 17.9 | 46.6 | Yes |
| Limiter (drums): > 1 dB | 0 | 0 | 0.8 | 5.8 | Yes |
| Page's sum at MIX 50 %, peak (pad) | -1.03 | -0.04 | +0.05 | +0.18 | **No** (×0.874: -2.20 to -0.99) |
| Page's sum at MIX 50 %, peak (drums) | -1.31 | -0.51 | -0.10 | +0.57 | **No** (-0.60 at worst) |
| Page's sum, version flip's midpoint, peak (pad) | n/a | +1.34 | +1.94 | +2.06 | **No** |
| 1 ms click frames ≥ 10 dB per 10 s (pad; the dry reads 2.3) | 2.3 | 2.3 | 3.3 | 5.9 | Limiter's part, yes |

- **Page's overs are real and page-only.** At MIX 50 % they happen 0.3 to 0.7 times per 10 s, 4 to 24 samples long. A's and B's don't go over at noon; at fader 60-70, B's pad does (+0.13). The page's sum nulls with the real chain's MIX 0.5 render at -124 to -138 dB, so the sum itself is right; what's missing is the module's output trim (0.874, -1.17 dB). With it, no card goes over at any fader position measured. By size, a clipped peak leaves about -45 dBFS (pad C), -33 (pad D), -23 (drums D). A flip's +3 dB bump, two versions that differ only in gain crossfading, can leave about -11 dBFS when it lands on a loud moment: 1.1 / 4.9 / 7.7 % of moments (pad B / C / D), 1.1 % on drums D.
- **The limiter adds bursts that grow with the lift.** On a control with no top end of its own (three low tones on the hot pad's envelope; anything over 3 kHz is the chain's), the loudest 1 ms frame of products reads -59 dBFS without the limiter, -55.6 in A, -51.4 B, -50.6 C, -49.2 D, and -53.3 on drums D, a burst 38 dB over its median. The click count rises only where the limiter works: pad A to D 2.3 / 2.3 / 3.3 / 5.9, 2.0 with the limiter off.
- **The tank's saturation is there in A too, as a steady fuzz.** Same control: products over 3 kHz at -80 dB under the wet with saturation off, -55 on (pad); -89 and -76 on drums. Only 3 dB less input gives -65, and 6 dB less gives -89, the same as off. It isn't a crackle in the frames (11 dB over its median, not bursts). It's the tank's design ("only a hot tank saturates") and only HALL reaches it. The earlier reading of -9 dB in the pad's own top end was mostly the two tails drifting apart, not products; the tone control replaced it.
- **The music's own.** The stabs' 3.1 frames per 10 s are the dry's, in every version.
- **The unfreeze overload is ruled out for the page** (it's module CPU; the page plays files). On the module, not checkable offline.
- **The hot input is at the edge, not past it.** Each phrase peaks at -1 dBFS, where the In LED turns red, the pad scaled up 6 dB to get there. The codec takes it; the volts are unmeasured (task at the rack).

**Which one the owner heard can't be told from numbers.** My reading: A's fuzz sits too low to be called crackle; C and D on card 7's pad and drums have limiter bursts plus the page's overs, the page's being far the louder per event. So it's partly the page's own, and the module wouldn't have that part. Not measured: the listening device (a browser holding 1.5 GB of decoded audio can drop out by itself, and nothing here can see that).

The owner can separate them on today's page, without a rebuild, on card 7's pad and drums, without flipping while listening:
1. MIX full, version C or D. Crackle gone: the page's sum. Still there: limiter or saturation.
2. If still there: MIX full, version A. Gone: limiter. Still there: saturation.

### The taper

**The orchestrator's reading holds, in tail length rather than knob or SPACE.** HALL has both picks (noon +4.5, 0.75 +3), so a SPACE trim can't do it. T60 sorts every pick: at or under 11.9 s +4.5, at or over 25.8 s +3 (plain DECAY mapping; CANYON noon's real tail is 36 s at SIZE noon). One correction: the picks don't aim at one level. Against the dry they land at about +1.6 (PLATE 9 o'clock), +2.4 (HALL noon), +1.5 (CANYON noon), +3.3 (HALL 0.75), +3.5 (PLATE endless). Short tails get more lift and still land 1 to 2 LU under long ones. The evidence is one cluster per side: nothing was picked between 12 and 26 s, nor at PLATE noon or CANYON 9 o'clock. The +3 on long tails stands on non-hot cards too (HALL 0.75, PLATE endless), so the crackle can't have skewed it.

**Recommended:** flat +3 dB in `Output.h`'s `kMakeupDb`, plus an extra +1.5 dB on the wet at T60 ≤ 12 s, fading to none at 26 s, shared by the SPACEs. It keys on the plain T60 (`1 / decayRate`), so SIZE doesn't move it and `decay_curve`'s remap carries it along. Extra lift by T60: 12 s 1.50, 15 s 1.34, 19 s 0.78, 23 s 0.19, 26 s 0.

| DECAY knob where the bend sits (12 s to 26 s) | PLATE | HALL | CANYON |
|---|---|---|---|
| Today | 0.63-0.77 | 0.50-0.65 | 0.35-0.50 |
| decay_curve C (the owner's pick) | 0.66-0.88 | 0.50-0.68 | 0.33-0.50 |

Curve C keeps noon where it is, so the picks' noon cards don't move. HALL's 0.75 card becomes 33 s instead of 46 s: still +3.

| | Flat +3 only | Taper, after WEAR | Taper, inside `decayLevelMakeup` (before WEAR) |
|---|---|---|---|
| Picks' 28 cards off by more than 0.5 dB | 10 | 3 | 3 |
| Largest miss | -1.5 (PLATE 9, HALL noon, 8 cards) | -1.5 (CANYON noon stabs, PLATE endless pad), -0.6 (HALL hot pad, limiter) | the same |
| Release, bytes | 111,844 (+0) | 112,020 (+176) | 111,900 (+56) |
| Profile, bytes (limit 131,072) | 131,028 (+0) | 131,196: **124 over** | 131,084: **12 over** |
| WEAR's tape | Unchanged | Unchanged | Driven 1.5 dB harder at short tails: drums' click frame 20.9 against 15.8 at WEAR 0.75, 20.8 against 17.2 at WEAR 1; top end up to +0.5 dB; loudness within 0.07 LU |
| Rising when DECAY comes down | n/a | A 40 ms glide (no cap yet) | Inherits the rise cap |

After WEAR is the better sound: it's what the owner heard (the page's lift is after WEAR) and WEAR's drive was tuned by ear yesterday (ADR 0045). It needs the profile build to find 124 bytes, and v1.0.71's port adds its own. These are trial builds on v1.0.70, not a final port.

**Turning DECAY.** No new click or step: click frames and the worst 10 ms jump match flat +3 to 0.2 dB on 12 s sweeps and 50 ms flicks, every SPACE, pad and drums. The one thing to listen for is a tail swelling up to 1.5 dB as DECAY comes down, in the after-WEAR version (peak -4.1 to -2.6 on HALL drums, 0.8 to 0.2). A cap as on `decayLevelMakeup` would fix it, for a few more bytes.

**The steady level.** The taper narrows the swing across DECAY, but makes two shapes (wet against dry, LU, pad and drums):

| | Flat +3 range | Taper range | Shape |
|---|---|---|---|
| PLATE | +0.3 to +3.7 | +1.8 to +4.4 | A hump at 0.70 (+4.4), 1.8 LU over endless (+2.6); flat's hump is 1.1 |
| HALL | +0.7 to +4.7 | +1.8 to +4.7 | A valley at 0.60-0.65 (+1.8) between +2.8 at 0.40 and +4.7 from 0.90; flat's valley is 0.6 |
| CANYON | +0.9 to +2.6 | +1.6 to +2.6 | Nearly flat |

The steepest new step is -0.5 LU per 0.05 of knob. Per-SPACE bends could ease PLATE's hump (PLATE noon wasn't picked), but cost bytes and have no pick behind them. Shared, unless the owner hears the hump.

**The limiter.** The extra lift does reach it when played hot at short tails. Of 288 hot renders (WEAR 0 and 0.5), more than 1 dB of reduction shows up in 29 with flat +3 (DECAY 0.3: 3, noon: 7) and 43 with the taper (0.3: 13, noon: 11), the same counts as D at those decays. 0.75 and endless don't change (10 and 9). The worst render stays at 23.7 %, and HALL's hot endless pad at 17.9 %. None of the picks' cards at short tails touches it: those were played at their own level.

**The earlier lean (flat +3 plus `rack-levels` trim) is withdrawn.** It can't give HALL two lifts, and the HALL-to-CANYON gap it meant to fix is now 0.9 LU own level, 2.1 hot, not the ~4 of before v1.0.61, so ±2 would overshoot: HALL +1, CANYON +5, against the owner's +3 on both hot cards.

**v1.0.71's fuller tail will shift the CANYON picks; it adds more than a little.** Fuller C (`claude/fuller-tail-6dcc8d`, the prototype, not the port): CANYON noon +1.6 to +3.1 LU, CANYON hot endless +0.5 to +1.9, HALL noon +0.5; PLATE 0; HALL 0.75 and hot endless within 0.2; the hot HALL's saturation and limiter unchanged (pad 17.7 % against 17.9). The owner's +3 on CANYON noon was heard without it, so CANYON noon would land near +4 against the dry, louder than HALL noon's +2.4. CANYON's cards need a listen on v1.0.71 before the lift's numbers are final. The bend's own figure there (T60 ≥ 26 s, none) doesn't change.
