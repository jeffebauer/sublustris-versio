# IMMENSIO VERSIO — Firmware Spec

**Name:** Immensio Versio (Latin *immensus*, "boundless"). Firmware target name `immensio_versio`. Sibling to Resilio Versio.
**Target:** Noise Engineering Versio platform (Electro-Smith Daisy Seed inside).
**Goal:** a huge, warm, lush, characterful reverb for dub techno, ambient and dark ambient. Supermassive / Cathedra scale and drift, played like an instrument. Never metallic.
**Status:** Spec **v1.0 (frozen)**, 21 Oct 2026. Input: `IMMENSIO_BRIEF.md`. Vocabulary: `CONTEXT.md`. Decisions: `docs/adr/` (0001–0017; index in `docs/adr/README.md`). Session record: `docs/spec-session.md`. Changes after freeze: new ADR + changelog entry. Numbers marked *(start)* are starting guesses; tuned numbers replace them as milestones confirm them.

### Changelog
- v1.0.5 — owner's M5 listening notes (21 Oct 2026): **bug fixed:** the PLATE's left output read one tap past the end of its line, so at full SIZE it clicked once a lap ("rhythmic flickering static in the left channel"); the taps now follow Dattorro's table (each output starts from the other branch) and a compile-time check keeps every tap inside its line. A frozen PLATE's chorus drops to 15 % depth (it piled up into a seasick wobble); a frozen MASSIVE goes to full in-loop diffusion (painted material came back as audible repeats); DARKEN's release is 150 ms (0.4 s dwelt between skanks). **Pending the owner's pick** (`renders/freeze_options`): how playing over a frozen pad works: the companion plate on top (today, ADR 0009/0010), the input feeding the pad itself, or both.
- v1.0.4 — M7 build (21 Oct 2026): DARKEN is a two-pole low-pass on the **wet output**, not in the loop: a loop filter takes the highs for good, and §4.9 asks for a tail that "brightens back when you stop" (measured: the wet's > 4 kHz share 4 % while playing, 31 % a second after); no ducking while frozen (playing over a pad mustn't darken it). The soft limiter holds the wet at −1 dBFS and reports its gain to the LEDs. In-loop allpass-interpolated reads keep their fraction in [0.5, 1.5) (Dattorro): with the fraction near 0 the interpolator's pole sat by Nyquist and a moving read leaked energy into the top octave (a frozen MASSIVE pad's share above 12 kHz doubled in 5 min; now steady).
- v1.0.3 — M6 build (21 Oct 2026; ADR 0008 amendment, **for the owner to approve**): ERA and DRIFT in the loop, built to the §4.7 table with these changes, each measured: ERA's darkening is a shorter high-end T60 (Neve × 0.7, cassette × 0.4) through the Jot loss, not a low-pass in the loop (a filter takes its loss every pass, so short lines darkened and drained far faster than long ones and infinite tails died); the always-on soft clip is exactly linear below 0.84 of full scale (a cubic curve shaved ~0.1 % a pass); Neve's even harmonics carry no DC (their own mean is subtracted) and fade out at infinite DECAY (they add energy in proportion to the level: a runaway in a lossless loop); the cassette drive is × 1.4 (× 1.8 pushed held tails' peaks into the knee); the head bump is left out (a boost in the loop breaks rule 2); the Midiverb box truncates toward zero on finite tails and rounds at infinite (truncation ate infinite tails), with no dither in the loop (it would pile up as noise); per-layer makeup keeps ERA's loudness within ±0.5 dB; TONE fully CW lets the highs outlast the lows (high-end T60 × 1.3). **Rule 2 exception:** a *held* tail (a frozen pad, or an infinite tail once the input is quiet) that sags below its own settled level is raised back toward it, never above, at most ~0.3 dB/s, because moving allpass reads (fast DRIFT, the cassette's wow and flutter) lose a little energy and nothing else could keep it.
- v1.0.2 — M5 build (21 Oct 2026): every in-loop read is **allpass-interpolated** (Dattorro), so a frozen pad loses nothing pass after pass (a cubic read cost ~5 dB in 5 min); the floor governor also **holds** a still pad at its own settled level (taken 20 s after the freeze, once a MASSIVE pad has spread out) and an infinite tail where it settled once the input goes quiet, judged on an 8 s level, so neither creeps up; the PLATE's loss is calibrated (+9 %) to DECAY's mapping, and its shortest tails are set by its size (one pass round the figure 8 is ~0.65 s at SIZE noon); SPACE flips wait while frozen; the M5 pad criterion is measured from 25 s after the freeze.
- v1.0.1 — M1/M4 build (21 Oct 2026, ADR 0007 amendment): §3 SIZE in MASSIVE made consistent with ADR 0007's "long echoes at low SIZE" (mean line 0.35 → 2 s, was 60 ms → 2 s, which can't give long echoes); MASSIVE gets in-loop diffusion (two allpasses per line, scaled by SIZE) so its long lines blur into a wash; the FDN's output is level-compensated for line length (√(mean / 0.1 s)) so SIZE doesn't turn the tail down, and MASSIVE is trimmed −3.3 dB to match HALL; the swell stage is a 24-tap rising feedforward (§4.5); the infinite floor stops darkening once the tail is at its floor (§4.4). No change for the owner to re-approve except the MASSIVE SIZE range, flagged for listening in `docs/TASKS.md`.
- v1.0 — frozen after owner approval (21 Oct 2026); decisions split into ADRs 0001–0017. TIME = predelay in NORMAL and the P1–P7 layout approved (ADR 0015, 0017).
- v0.2 — after the P7 / SW1 demos (owner, 21 Oct 2026): DESK removed; P7 = TIME; SW1 = ATTACK (NORMAL / BLOOM / REVERSE); ducking fixed to DARKEN; the switch governs the attack in every space, Massive included (ADR 0014, 0015).
- v0.1 — first draft from the spec session (21 Oct 2026).

---

## 1. Hardware facts

| Item | Detail | Source |
|---|---|---|
| MCU | STM32H750 Cortex-M7, 480 MHz (boost) | docs.daisy.audio/hardware/Seed; Resilio SPEC §1 |
| Memory | 64 MB SDRAM; 8 MB QSPI flash; **128 KB internal flash** (code); internal SRAM: 128 KB DTCM (zero-wait), 512 KB AXI SRAM (behind a 16 KB D-cache), 288 KB D2 SRAM | ST RM0433; Resilio `firmware/main.cpp` |
| Knobs | 7 (`KNOB_0`…`KNOB_6`), each with a CV jack, summed in hardware → one 0–1 value | libDaisy `daisy_versio.h` |
| Switches | 2 × 3-position (`SW_0`, `SW_1`) | same |
| Button | 1 momentary | same |
| Gate in | 1, digital (~+2 V), no jack detection: unpatched reads low | same; NE manuals |
| LEDs | 4 × RGB | same |
| Audio | stereo in / out. **In L is normalled to In R in hardware**, so mono on L needs no firmware logic | NE Versio manuals; Resilio SPEC §3 |

Flashing: NE Firmware Swap web app, "Select Custom File" (Resilio ADR 0011). That means a standard libDaisy app in internal flash, no bootloader.

---

## 2. Sound target

### 2.1 Core characteristics
- **Warm and lush, never metallic.** Reference dislikes: Make Noise Erbe-Verb, Plexophone.
- **Sustains near-infinitely without running away.** At infinite, the tail evolves to a warm floor and holds there (ADR 0008).
- **Massive mode has mass:** a gentle built-in swell in NORMAL (or a timed BLOOM), and SIZE from long repeating echoes to a boundless wash (§4.5, §4.11).
- **Era is a journey back in time**, from clean to Neve, cassette, μ-law and Midiverb. The layers stack (§4.7).
- **Its own instrument:** not a Dattorro plate with distortion (Plateau Versio), and no Desmodus-style "scream".

### 2.2 Inspirations → what we take
Valhalla Supermassive (Sagittarius, Andromeda, Large Magellanic Cloud: swell, mass, echoes → wash), Meris Mercury7 Cathedra (in-tank pitch drift), Eventide Blackhole (shaping the envelope), Vongon Ultrasheer (vintage bits, warble), Lexicon 224 / Griesinger (modulated delays against metallic ringing), Alesis Midiverb (12-bit, ~10 kHz, ~23.4 kHz rate), the dub mixing desk send/return (EQ and drive in the loop, played like an instrument: Tone and the saturation), Neve (even-harmonic warmth), Portastudio (compression, head bump, hiss, wobble). Details and sources: `IMMENSIO_BRIEF.md` §2 and `docs/spec-session.md` "[VERIFY] results".

### 2.3 Design principles
1. Every tank delay is modulated (the anti-metallic floor), delay lengths are mutually prime, and damping sits in the loop.
2. In-loop gain is ≤ 1 at every frequency for every setting. Infinite means exactly 1 at the floor, held there by the soft saturation. No setting adds energy.
3. Bit loss truncates toward zero, so a fading tail always reaches silence or its floor and never holds a stuck buzz (a limit cycle). This follows Resilio's `EchoBits`.
4. Every control move is smooth: no zipper, no clicks. Size moves bend in pitch on purpose (ADR 0012); nothing else does.
5. Sound work happens on the desktop first (Renderer, then Plugin). Hardware is for profiling and final feel.
6. Deterministic: every random source is seeded, so the same input and params give bit-identical output.

---

## 3. Panel map

### Knobs (all CV-able, 0–5 V + pot offset)

Pots are named by panel position P1–P7, in reading order, as in Resilio ADR 0028. Layout (ADR 0017): it keeps Resilio's muscle memory under the same printed panel. P1 MIX (= BLEND), P2 DECAY, P3 TONE, P4 ERA (where SPLASH was), P5 SIZE (where TENSION, "which tank", was), P6 DRIFT (where the bipolar WOBBLE was), P7 TIME (where DRIVE was).

| Pot | Name | Function | Range and curve *(start)* | Smoothing |
|---|---|---|---|---|
| P1 | **MIX** | Dry/wet, equal-power (ADR 0012) | θ = x·π/2; dry = cos θ, wet = sin θ. Fully CCW = clean passthrough (dry untouched, even with Era up: the μ-law box is wet-only); fully CW = 100 % wet | snappy 5 ms |
| P2 | **DECAY** | Tail length; past ~3 o'clock, infinite plus ducking (ADR 0008, 0014); in Massive NORMAL it also shortens the built-in mass (ADR 0007) | 0–0.80: T60 exponential. Plate 0.4 → 12 s, Hall 0.8 → 25 s, Massive 2 → 120 s. 0.80–0.85: crossover to infinite. 0.85–1: infinite (loop gain 1 at the floor), duck depth grows 0 → full (§4.9) | gliding 80 ms |
| P3 | **TONE** | Tilt EQ in the loop: the tail darkens or brightens with each pass. Noon is flat (ADR 0012) | Pivot ~800 Hz. Per pass, gain at 10 kHz relative to 100 Hz: −6 dB (CCW) → 0 (noon) → +2 dB (CW). The loop gain is renormalised so that "bright" never exceeds 1 at any frequency. Noon dead zone ±2 % | snappy 5 ms |
| P4 | **ERA** | Clean → Neve → Cassette → μ-law → Midiverb, stacking (ADR 0003, 0004, 0005) | Travel weighted to the middle: 0–10 / 10–35 / 35–65 / 65–85 / 85–100 %. Each layer fades in across its stretch, then stays. §4.7 | gliding 60 ms (rate changes crossfade) |
| P5 | **SIZE** | The space. Plate / Hall: scale. Massive: long echoes → boundless wash, predelay grows (ADR 0007). Moves bend pitch like tape (ADR 0012). While frozen it plays the companion | Plate: tank scale 0.5 → 1.6. Hall: mean line delay 40 → 180 ms. Massive: mean line delay 0.35 → 2 s, exponential; delay-ratio spread (Warp) 0.08 → 1; in-loop and input diffusion 0.15 → 1 (×); predelay 0 → 500 ms (∝ x²) (v1.0.1). Slew-limited to ≤ ±1 semitone of bend (≤ 6 % delay change per second); a fast move becomes a swoop over up to ~1.5 s | slew-limited (§4.3) |
| P6 | **DRIFT** | Bipolar pitch movement in the tank (ADR 0012) | Dead zone 0.47–0.53, which leaves only the anti-metallic floor (±1–2 cents per line, slow random). Left: tape wow 0.2–1 Hz up to ±15 cents, flutter 5–9 Hz up to ±3 cents, random and never repeating. Right: smooth Cathedra drift, 0.05–0.3 Hz up to ±35 cents at full, different rate and phase per line | gliding 80 ms |
| P7 | **TIME** | The attack's timing, per ATTACK (ADR 0015). NORMAL: predelay. BLOOM: swell time. REVERSE: reverse length | NORMAL: predelay 0 → 250 ms (∝ x²) *(start)*. BLOOM: swell time 0.1 → 4 s, exponential (shorter keeps a swell from clashing with the harmony coming in). REVERSE: segment length 0.2 → 2 s, exponential | gliding 80 ms (REVERSE length changes at the next segment) |

### Switches

| Switch | Left | Centre | Right |
|---|---|---|---|
| SW0 **SPACE** | **PLATE**: Dattorro-family plate, dense, quick attack | **HALL**: the FDN voiced as a big room, normal attack | **MASSIVE**: the FDN voiced with swell, mass and long delays |
| SW1 **ATTACK** | **NORMAL**: the sound enters the tank at once (Massive keeps a gentle built-in mass) | **BLOOM**: the tail swells in over TIME | **REVERSE**: classic reverse swell: each stretch of reverb plays backwards, rising to a cut; TIME sets the length |

Which toggle is SW0 and which is SW1 follows the printed panel (confirmed at M0).
Switch changes: SPACE crossfades ~100 ms; the old tail fades out while the new space takes the input, and only one big tank ever runs (ADR 0013). ATTACK crossfades ~50 ms on the wet; into or out of REVERSE, the forward and reversed wet crossfade over 50 ms.
Ducking has no switch: it is always DARKEN (ADR 0014, from the demos).

### Audio I/O
True stereo in (ADR 0013): L and R enter the tank at different points. Mono on L reaches both sides via the hardware normal. Outputs are not normalled.

### Button + Gate (one gesture reader, ADR 0009–0011)
- **Tap** (press < 0.3 s) **latches the freeze.** The big tank becomes a still pad (ADR 0009); the input moves to the small companion reverb, which Size and Decay now play. Tap again to unfreeze: the pad decays naturally, and the big tank glides to the current Size and Decay over ~1 s (ADR 0009).
- **Timing:** freeze engages **on the press** (no latency, so you catch the chord). While frozen, a press opens a 0.3 s window. Released inside it: unfreeze on release. Still held at 0.3 s: **paint**.
- **Hold to paint** (while frozen): what is playing, through the companion, blends into the pad over ~3 s *(start)*; on release the pad keeps the new layers. With no hold, the pad is untouched.
- **Gate = the same gestures:** a rising edge is a press, a falling edge is a release. A short pulse toggles; a gate held over 0.3 s while frozen paints. Unpatched, the gate reads low and does nothing.
- Ramps *(start)*: input to the big tank closes over 20 ms on freeze; the in-loop colour glides out over 0.2 s; the companion fades in over 20 ms.

### LEDs (ADR 0016)
Level meters as Resilio (ADR 0031): LED 1–2 In L / In R, LED 3–4 Out L / Out R, green → amber with level, red on clip or limiter. When frozen, the output pair turns cool blue under the meter; painting pulses it slowly (~1 Hz).

---

## 4. DSP architecture

### 4.1 Signal flow

```
 In L/R ─┬──────────────────────────────────────────────────────────────┐ dry
         │                                                              │
         ├─► input gain ─► envelope follower (duck, control rate) ──┐    │
         │                                                         │    │
         ├─(+ hiss, Era ≥ Cassette, ducked by input)               │    │
         ▼                                                         │    │
   [freeze router] ──normal──► predelay (NORMAL: TIME; Massive: + Size)   │
         │                       ─► input diffusion                 │    │
         │                       ─► swell stage (BLOOM: TIME; Massive NORMAL: built-in mass, §4.5)
         │                       ─► BIG TANK: Plate | Hall/Massive FDN    │
         │                            loop contains, per line / pass:     │
         │                             modulated delay (floor + DRIFT)    │
         │                             TONE tilt + damping                │
         │                             loop soft saturation (always on)   │
         │                             ERA in-loop layers (Neve, Cassette,│
         │                               Midiverb 24 kHz box + bits)      │
         │                             DARKEN duck (from follower) ◄──────┘
         │                                   │
         │                     tank out ─► REVERSE segmenter (ATTACK = REVERSE, §4.11)
         │                                   │
         └─frozen──► COMPANION plate ─(paint)─┘ (blends into frozen pad)
                          │
   tank out + companion out ─► ERA output layer (μ-law 24 kHz / 12→10-bit)
                          ─► output limiter ─► MIX (equal-power) ◄── dry
                          ─► Out L/R
```

### 4.2 Plate engine (also the freeze companion)
- Dattorro-family figure-8 tank: input diffusers, 2 cross-coupled branches of modulated allpasses and delays, damping, multi-tap stereo output. Written from the 1997 paper; **no Plateau Versio code** (it is GPL-3.0).
- What makes it ours: every delay is modulated (floor + DRIFT), and the loop carries the full character chain (Tone, saturation, Era).
- **Companion:** the same code at scale 0.4 *(start)*, with its own state, always in DTCM. Its Size and Decay come from the knobs while frozen: scale 0.25 → 0.6, T60 0.3 → 6 s. It has no in-loop Era; the output μ-law still applies.

### 4.3 FDN engine (Hall and Massive)
- **8 lines** *(start; 16 is a stretch goal, decided at M3)*. Mixed feedforward + feedback as Costello describes. Householder or Hadamard feedback matrix (fast 8-point Walsh transform, 24 add/sub).
- Delay lengths are mutually prime, spread by a Warp-style ratio control (Size in Massive). Each line has a modulated read: cubic interpolation, floor modulation plus DRIFT.
- Per-line loop gain is set from T60 and the line length (Jot), so Decay doesn't retune.
- **Size moves:** each line's length target is slew-limited (≤ 6 %/s, about ±1 semitone of bend), so the tail swoops like tape. The read interpolates; no crossfade.
- **Hall voicing:** mean delay 40–180 ms, moderate input diffusion, no built-in mass (BLOOM still applies), predelay 0–60 ms (TIME's predelay adds in NORMAL).
- Stereo: L and R inject into different lines (true stereo in); the outputs are taken from opposite-signed line sums.

### 4.4 Infinite and the floor (ADR 0008)
- Decay 0.85–1: the loop gain reaches 1 at the floor level. Tone, Era and the saturation stay in the loop, so the tail keeps darkening and graining. The always-on soft saturator plus a slow RMS loop-level governor (control rate) hold the tail at a warm floor *(start: −24 dBFS RMS)* so it never turns to mud or falls silent.
- **Freeze is different (ADR 0009):** the pad holds still. Gain is exactly 1, input is closed, the in-loop colour is bypassed (glided over 0.2 s), and only the modulation floor and Drift remain.

### 4.5 Massive mode: mass, echoes → wash (ADR 0007)
- **Swell stage:** a feedforward stage between the input diffusion and the tank: 24 taps of one long delay per side, spread over 1.25 × A with rising gains (total energy 1), jittered and sign-scrambled per side (v1.0.1; was "4 long modulated delays and allpasses", which can't be steered to a chosen attack time). The energy reaches the loop gradually, so the attack is a slab, not a hit; the 10 → 90 % rise lands at A. Below A ≈ 40 ms it hands over to the direct path. **Shared by every space** (it is also BLOOM, §4.11).
- **ATTACK governs the attack in every space (ADR 0015).** In Massive, NORMAL keeps a gentle built-in mass: A = A₀(Decay) · m(Size), A₀ 0.5 → 0.2 s across Decay (log), m 1 → 1.5 across Size *(start)*. BLOOM replaces it with TIME's swell, still × m(Size), so Size adds a little mass. Longer swells are BLOOM's, where TIME sets them.
- **Echoes → wash (Size):** low Size gives long, near-equal lines (mean 0.35 s, spread 0.08) with little diffusion, i.e. distinct repeating echoes (Large Magellanic Cloud). Turning Size up lengthens and fans out the lines and raises the diffusion, input and in-loop (two allpasses per line, 24–91 ms, coefficient 0.62 × diffusion), into a boundless wash, with predelay up to 500 ms (v1.0.1).
- **Andromeda-scale tails stay tame** through §4.4: in-loop damping, soft saturation and the floor governor.

### 4.6 Loop character chain (per pass)
Order inside each line or branch *(start)*: modulated delay read → TONE tilt (one-pole shelf pair) + fixed damping → soft saturation (cheap polynomial or rational soft clip, no tanh, no oversampling in the loop) → ERA in-loop layers → DARKEN duck. The dub-desk inspiration lives on through Tone, the saturation and Era; there is no resonant Desk EQ (ADR 0015).

### 4.7 Era (ADR 0003, 0004, 0005)
Stacking: each stage's layer fades in across its stretch of the knob and then stays.

| Stage (layer added) | Bandwidth | Bits / companding | Saturation | Noise | Wow / flutter | Where |
|---|---|---|---|---|---|---|
| Clean (0–10 %) | full | float | always-on loop soft-clip only | none | none (Drift only) | – |
| + Neve (10–35 %) | loop LP 20 → 16 kHz; +1.5 dB shelf at 80 Hz | float | even-harmonic, asymmetric; 2nd harmonic ~−30 dB at nominal | none | none | in loop |
| + Cassette (35–65 %) | 16 → 9 kHz; head bump +2 dB at ~70 Hz | float; slow 2:1 soft compression above −18 dBFS (gain at control rate) | tape curve on top | hiss into the tank input, up to ~−66 dBFS, ducked by input level (ADR 0005) | wow 0.3–0.8 Hz up to ±4 cents, flutter 6–8 Hz up to ±1 cent, added to DRIFT | in loop (hiss at tank input) |
| + μ-law (65–85 %) | unchanged | 24 kHz / 12-bit μ-law → 10-bit at 85 % | – | – | – | wet output (ADR 0004) |
| + Midiverb (85–100 %) | ~10 kHz ceiling via a 48 → 24 → 48 kHz box | 12 → 8-bit, truncation toward zero, TPDF dither | – | sparse crackle, growing to the end | – | in loop (ADR 0004) |

- The Midiverb box uses polyphase IIR halfbands (Resilio `Oversampler`). Its images are deliberately not fully removed, for a little aliasing grit *(start: stopband −40 dB)*. It **saves no CPU**: the tank stays at 48 kHz, because a live tail's tank rate can't crossfade cleanly (ADR 0004).
- Midiverb character only (rate, bits, halving accumulator maths); no ROM microcode. emeb/MIDIVerb_RE is MIT and may inform it; thement/midiverb_emulator is GPL-3.0, reference only.

### 4.8 (removed) Desk
Removed before the freeze, after the owner's demo listening: the knob became TIME (ADR 0015).

### 4.9 Ducking: always DARKEN (ADR 0014)
- An envelope follower on the input (attack 5 ms, release 400 ms; −42 dBFS → no duck, −24 dBFS → full *(start, from the demos)*, control rate) drives the duck. The depth is 0 below Decay 0.85, then grows to full at Decay 1.
- **DARKEN:** a two-pole low-pass on the wet (after ERA's μ-law layer, before the limiter; v1.0.4, not in the loop) closes, down to ~1.5 kHz at full depth, and the tail dips ~3 dB. Not while frozen. While you play the tail gets out of the way, and it brightens back when you stop.

### 4.10 Output stage
Era's μ-law layer (wet only) → soft limiter (ceiling −1 dBFS, Resilio-style) → MIX. The dry path is untouched.

### 4.11 ATTACK and TIME (ADR 0015)
- **NORMAL:** TIME is predelay, 0 → 250 ms *(start)*. Massive adds its gentle built-in mass (§4.5) and Size's predelay.
- **BLOOM:** the swell stage (§4.5) is set so the tail's envelope rises over TIME, 0.1 → 4 s, in every space (× m(Size) in Massive). A short TIME keeps a swell from clashing with the harmony coming in (owner's demo note).
- **REVERSE (classic reverse swell):** the tank's output is written into segments of TIME length (0.2 → 2 s). Each finished segment plays backwards, so each stretch of reverb rises to a cut, then the next. The edges are crossfaded over 5 ms *(start)*. The reversed wet is **TIME late** (inherent to real-time reverse reverb); the dry is not delayed. Two segment buffers, stereo. While frozen, REVERSE is bypassed (a still pad reversed is the same pad) with a 50 ms crossfade; the companion is never reversed.

---

## 5. Performance budget

48 kHz, block 48 *(start)*. 480 MHz ÷ 48 kHz ≈ **10,000 cycles/sample**. Target **≤ 75 % peak** worst case, **80 % ceiling** for a release that passes the click check by ear (Resilio's numbers, ADR 0030).

### 5.1 CPU per block *(desktop estimates, ×2 safety for M7 reality; verified at M3)*

| Block | Cycles/sample (est.) | Notes |
|---|---|---|
| Input gain, follower, hiss | ~40 | follower at control rate |
| Predelay + input diffusion (8 allpasses, stereo) | ~120 | diffusers in DTCM |
| Swell stage (BLOOM / Massive, 4 stages × 2 ch) | ~250 | SDRAM reads, sequential per line |
| REVERSE segmenter (stereo write + reversed read + edge fades) | ~40 | SDRAM |
| FDN 8 lines: modulated cubic reads | ~350 | SDRAM; cost dominated by cache misses (to measure) |
| FDN matrix (8-pt Walsh) | ~30 | |
| Loop chain × 8 (tilt, damping, saturation) | ~250 | cheap soft clip, `Select.h` VSEL |
| ERA in loop (Neve/Cassette cheap; Midiverb box × 8) | ~350 | worst case, Era at full |
| Companion plate (when frozen) | ~300 | DTCM |
| Output μ-law box (stereo) + limiter + mix | ~200 | Resilio `OutputBits` scale |
| Control-rate work (amortised) | ~150 | LFOs, coefficient redesigns, governor |
| **Total (Massive, Era full, BLOOM, frozen + painting)** | **~2,080 → ×2 ≈ 4,200 (≈ 42 %)** | leaves room; 16 lines decided at M3 |

Plate mode ~1,500 × 2 ≈ 30 %. Mitigations, if short: stagger coefficient redesigns across ticks (Resilio ADR 0030); `-ffp-contract=off`; Midiverb box on a subset of lines; block 96.

### 5.2 Memory placement

| Buffer | Size | Where | Why |
|---|---|---|---|
| FDN lines (8 × 2.05 s mono, incl. modulation headroom) | ~3.2 MB | SDRAM | too big for internal RAM; reads are sequential per line, so the D-cache helps |
| Swell stage (4 × 2 ch × 1 s) | ~1.5 MB | SDRAM | |
| Predelay (stereo, 0.75 s: Size + TIME) | ~0.3 MB | SDRAM | |
| REVERSE segments (2 × 2 s, stereo) | ~0.75 MB | SDRAM | |
| Plate engine (scale up to 1.6, stereo taps) | ~150 KB | AXI SRAM | random-ish taps; internal is faster than SDRAM |
| Companion plate (scale ≤ 0.6) | ~55 KB | DTCM | always-ready, zero-wait |
| Input diffusers | ~20 KB | DTCM | hottest loop |
| Filter states, Era boxes, μ-law, LFOs | < 8 KB | DTCM | |
| Stack, HAL/USB buffers | as libDaisy | AXI SRAM | (Resilio moved its pool to DTCM and kept the stack in AXI) |

DTCM total ~85 KB of 128 KB. SDRAM total ~6 MB of 64 MB. **Unverified:** SDRAM random-read cost under modulation. Measured first at M3 with an on-chip microbenchmark, before features are stacked.

### 5.3 Flash budget (128 KB internal, the tightest limit)
Resilio ended at 97.7 %. Immensio tracks flash per milestone: **release ≤ 110 KB at v1.0** (≥ 18 KB free). From M0 it uses Resilio's techniques: no QSPI chip setup, no USB device stack in release, `-Os` on control-rate code (`IV_SIZE_OPT`), only shipped voicings compiled in, a size gate in every build. Two engines, not three (ADR 0006). The fallback if the budget can't be met, BOOT_SRAM with a custom linker script, loses the Firmware Swap install, so it is the **owner's call via a new ADR** (ADR 0002).

---

## 6. Architecture: one DSP core, three hosts
As Resilio §6: `core/` (params + dsp, no platform includes), `host/render` (CLI: WAV + JSON → WAV, automation, sweeps, metrics), `plugin/` (JUCE AU + VST3 test bench, params generated from `ParamSpec`; MIDI notes = the gate, so a note-on is a press and a note-off a release; the button is a momentary plugin control), `firmware/` (libDaisy `DaisyVersio`). Namespace `iv::`. Every host passes normalised 0–1 values; mappings live only in `core/params/`.

### 6.1 Seeded from Resilio Versio (copied, provenance in `NOTICE`; ADR 0001: new repo, no shared lib yet)
- **As-is:** `ParamSpec` pattern, `host/common` (Wav, Json, Fft, Spectrogram, Metrics, Automation, Sweep), the test runner and ctest wiring, the plugin shell + `plugin_host_test`, firmware `PotEndStops`, `LedMeter`, LED PWM, build variants (release / m0test / profile), `ProfileHook`, `Filters`, `Oversampler`, `Select`, `Seed`, `SizeOpt`, and the tools (stimulus, review pages, sweetspot, install and release scripts).
- **Refactored (maths kept, Resilio voicings dropped):** μ-law (`OutputBits` → the output layer, `EchoBits` → the Midiverb box), `Drive` / `EchoWear` (saturation, tape), `Wobble` (→ DRIFT).

### 6.2 Repo layout
```
immensio-versio/
  CMakeLists.txt
  core/params/   ParamSpec.h, Mappings.h, EraVoicing.h, SpaceVoicing.h, GestureReader.h
  core/dsp/      Plate.h/.cpp, Fdn.h/.cpp, Swell.h, Reverse.h, LoopChain.h, Era.h, Duck.h,
                 OutputBits.h, Oversampler.h, Filters.h, Select.h, Seed.h, SizeOpt.h, ProfileHook.h
  host/render, host/common, host/tests
  plugin/
  firmware/      Makefile (size gate), main.cpp, LedMeter.h, PotEndStops.h
  tools/  presets/sweeps/  test_audio/  docs/ (adr, handoff, TASKS.md)
```

---

## 7. Build milestones and test plan

**[A]** automated (Renderer / tests), **[L]** owner listening check, **[H]** on hardware. Shared definitions are as in Resilio: the stimulus set (`tools/make_stimulus.py`, plus held chords, pads and dub stabs), review pages, and the click detector (*start: 20 dB above the local HF level*).

### M0 — Toolchains and seed
- [A] The seeded repo builds: Core + Renderer + tests + Plugin (AU/VST3) + firmware. Passthrough is bit-identical. Firmware ≤ 128 KB, with the size reported.
- [H] Test firmware via Firmware Swap: passthrough within 0.5 dB, every control read correctly, the P1–P7 → libDaisy table and switch orientation confirmed.

### M1 — FDN Hall, clean
- [A] Stable at every Decay × Size corner: no NaN/Inf, no growth, and loop gain ≤ 1 at every frequency (magnitude-response test).
- [A] T60 within 10 % of the mapping across Decay (Plate/Hall/Massive tables, §3).
- [A] **Metallic check:** no spectral peak in the tail more than 6 dB *(start)* over its 1/3-octave neighbourhood (Resilio's resonance-ratio metric).
- [A] Deterministic (bit-identical reruns). Size sweep: no click-detector hits, bend ≤ ±1 semitone.
- [L] "Warm, lush, not metallic" on the stimulus set, compared with Supermassive / Cathedra recordings (never committed).

### M2 — Plugin shell
- [A] Params generated from `ParamSpec`. Plugin output equals Renderer output (≤ −120 dBFS). Works at 44.1/48/96 kHz (T60 within 5 %). Latency reported. MIDI note = gate, sample-accurate.

### M3 — Hardware profiling (early, before features stack)
- [H] FDN + diffusers on the chip: CPU average/peak per corner; an SDRAM read microbenchmark (sequential vs modulated). ADR: 8 or 16 lines, block size, memory placement confirmed.

### M4 — Massive mode + ATTACK
- [A] Attack: the 10 → 90 % rise of the envelope on a burst matches the target within 20 %: NORMAL in Massive across Decay × Size (A₀·m); BLOOM across TIME in every space.
- [A] REVERSE: each segment's envelope rises to its end, then cuts; no click-detector hits at segment edges; wet latency = TIME (±1 block), dry not delayed; frozen bypass is click-free.
- [L] BLOOM at short TIME doesn't smear the harmony of a changing chord; REVERSE sounds like a classic reverse reverb.
- [A] Echoes → wash: at Size 0, the autocorrelation shows distinct repeats; at Size 1, echo density exceeds a threshold *(start)* within 300 ms.
- [A] Infinite: a 10-minute render at Decay 1 holds the floor (RMS within ±3 dB of the target, no growth, no NaN), in every Era stage.
- [L] CygnusX1 / DarkMatter / BensonArizona / Ice Floe "family": a slab with mass that swells in (NORMAL's gentle mass, and BLOOM for the slow Sagittarius / Andromeda swells).

### M5 — Plate + companion + freeze/paint
- [A] Gesture reader (button and gate share tests): tap < 0.3 s toggles, freeze engages on press, a hold ≥ 0.3 s while frozen paints, pulses from a sequencer, bounces.
- [A] Freeze: the pad's level stays within ±0.5 dB over 5 min (from 25 s after the freeze, v1.0.2) with in-loop colour bypassed, and its pitch is stable (pitch tracker). Unfreeze glide has no clicks.
- [A] SPACE flips mid-tail: no clicks, and the loudness of Plate/Hall/Massive is within ±1.5 dB.
- [L] Playing over the pad feels like playing into a small room, not dry.

### M6 — Character chain: Tone, Drift, Era
- [A] Era: each stage's measured bandwidth, bit depth (noise floor vs signal) and THD match §4.7 within tolerance. Grain grows as the tail fades (SNR falls with level). No limit cycle (a tail reaches silence or its floor). Loudness across Era within ±2 dB.
- [A] Aliasing check (HF sine sweeps) passes at Era's top and with the saturation hit hard.
- [A] Drift dead zone exact (noon = floor only). Hiss is ducked to below −90 dBFS with no input, in a 2-minute silent render.
- [L] Era travel feels like going back in time. Blind pick of each stage at its centre.

### M7 — Ducking + integration
- [A] Duck depth is 0 below Decay 0.85, then rises monotonically; DARKEN's low-pass reaches ~1.5 kHz at full and recovers after the input stops.
- [A] Every knob at every corner: clicks, NaN, clip, CPU (on hardware).
- [H] Full CPU corner run ≤ 75 % peak; flash ≤ 110 KB release; click check by ear.

### M8 — Tuning pass
- [L] `sweetspot.py` per knob (dead zones, cliffs); owner tuning sessions against the references; starting guesses replaced by tuned numbers in the spec.

### M9 — Polish and release
- Manual, panel SVG, release notes, `make_release.sh`, licences (`NOTICE`: libDaisy, STM32 HAL, CMSIS, JUCE, VST3 SDK; our code MIT).

---

## 8. Flashing / distribution
As Resilio §8: NE Firmware Swap → Select Custom File → `immensio_versio.bin`. Never connect USB and rack power at once. Stock firmware is restorable through the same app.

---

## 9. Risks

| Risk | Mitigation |
|---|---|
| **Flash over 128 KB** (Resilio hit 97.7 %) | budget per milestone; two engines; Resilio's size techniques from M0; BOOT_SRAM only by owner ADR |
| SDRAM read cost under modulation | microbenchmark at M3, before features stack; swell stage and lines can trade length for speed |
| Metallic ringing | modulation floor on every delay, prime lengths, metallic metric from M1 |
| Infinite runaway or mud | gain ≤ 1 rule, soft saturation, floor governor, 10-minute renders |
| Era stages sound like separate boxes | stacking, plus the travel weighting and blind tests at M6 |
| Midiverb box aliasing ugly rather than charming | stopband is a tuning number; owner listening at M6 |
| Size bend feels seasick | slew limit is a tuning number; noon-ish default |
| Hiss builds up in infinite tails | hiss ducked by the input level; silent-render test |
| Plugin ≠ hardware | shared `ParamSpec`, validate at 48 kHz, Renderer = Plugin test |
| Licence contamination | no Plateau (GPL) or unlicensed code; Midiverb character only |

---

## 10. Open questions
- None at the freeze. Every *(start)* number is tuned at its milestone; SW0/SW1 orientation is confirmed at M0 (ADR 0017). Demos behind ADR 0014/0015: `renders/p7_sw1_demos/` (sketch in `docs/prototypes/p7-sw1-demos/`).

---

## 11. References
See `IMMENSIO_BRIEF.md` §8, plus:
- Valhalla, "ValhallaSupermassive updated to 1.1.1" (Large Magellanic Cloud, Triangulum), 29 Sep 2020.
- Valhalla, "Tips and Tricks: Infinite Supermassive", 19 May 2021.
- emeb/MIDIVerb_RE (MIT); thement/midiverb_emulator (GPL-3.0, reference only).
- Resilio Versio repo: SPEC.md, ADRs 0011, 0028, 0030, 0031, 0039, 0042.
