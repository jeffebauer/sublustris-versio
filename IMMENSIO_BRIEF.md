# Immensio Versio — Design Brief

Custom firmware for the Noise Engineering Versio platform (Daisy Seed inside). Name from Latin *immensus*, "boundless". Sibling to Resilio Versio (spring reverb + tape echo).

Status: concept agreed, pre-spec. This brief is input to a spec-driven phase, not the spec itself. Items marked **[OPEN]** need decisions; items marked **[VERIFY]** need checking.

---

## 1. Intent

A huge, warm, lush, characterful reverb for dub techno, ambient and dark ambient. Supermassive / Mercury7 Cathedra scale and drift — the class of reverb mostly missing from Eurorack, which is crowded with Lexicon/Dattorro plates.

**Must:**
- Sound warm and lush, never metallic (reference dislikes: Make Noise Erbe-Verb, Plexophone).
- Sustain near-infinitely without running away.
- Feel like its own instrument, not a clone.

**Must not duplicate:**
- **Plateau Versio** (Valley Plateau / Dattorro port by abluenautilus, aka "Campestria", GPL-3.0) — clean Dattorro plate with distortion.
- **Desmodus Versio** (NE) — owner has it; borrow the *concept* of ducking past a feedback threshold, not its "scream" behaviour.
- Note: Vongon Ultrasheer is also Dattorro-topology (Lexicon 224 plate, 16-bit). So a Dattorro tank alone is not a differentiator — the FDN "Massive" mode and the character chain are.

## 2. Inspirations

| Source | What to take |
|---|---|
| Valhalla Supermassive | FDN structures (Costello: mixed feedforward + feedback); long delays (up to ~2 s each); Warp-style delay-ratio control (echoes → swelling wash); embrace "flaws" as character. See §2a for owner's favourite modes |
| Meris Mercury7 (Cathedra) | Pitch modulation inside the tank — slow drift/bend, Vangelis feel. Owner has the pedal |
| Eventide Blackhole | Shaping the tail's envelope (Gravity), not just length |
| Vongon Ultrasheer | 16-bit vintage colour, random "sun-baked cassette" warble |
| Lexicon 224 / Griesinger | Modulated delays to kill metallic resonance |
| Alesis Midiverb | Low bandwidth + low bit depth; tail gets grainier as it fades |
| Mixing desk send/return loop (Tubby, Basic Channel) | Resonant EQ in feedback path, console saturation, played like an instrument |
| Neve | Gentle, even-harmonic warmth in the loop |
| Tascam Portastudio / cassette | Soft compression, HF roll-off, low bump, hiss, wobble |
| μ-law (from Resilio) | "Sweet" lo-fi: grit on peaks, quiet tail stays smooth |

Dropped: octave-down shimmer (muddy, most CPU-expensive). "Howl"/scream button (too Desmodus).

### 2a. Owner's favourite Supermassive presets → modes

| Preset | Mode |
|---|---|
| CygnusX1 | Sagittarius |
| DarkMatter | Sagittarius |
| BensonArizona | Andromeda |
| Ice Floe Nowhere To Go | Large Magellanic Cloud |

Mode character (from Valhalla's "The Modes" post):
- **Sagittarius** — very long attack tied to Feedback. Below ~70% feedback: slow fade-in, similar fade-out. Higher feedback: faster fade-in, slower decay. Costello describes it as having sonic "mass"/inertia — slow to start, slow to leave. High Warp → massive slabs of reverb.
- **Andromeda** — "supermassive" variant of Sagittarius: very slow attack, notable predelay, decays into the thousands of seconds.
- **Large Magellanic Cloud** — not described in Valhalla's modes post or the 1.5.0 (Orion/Cassiopeia) update post. **[VERIFY: find Valhalla's description of LMC]**

**Design takeaway:** 3 of 4 favourites are the Sagittarius family. The defining trait is a **slow, swelling attack** (sonic mass), not just long decay. Implications for the Massive mode:
- Build in an attack/swell envelope — energy enters the tank gradually (e.g. heavily cascaded input diffusion / feedforward delay stages before the FDN, predelay).
- Couple attack to Decay the way Sagittarius couples it to Feedback: higher Decay = faster swell, longer tail. Strong candidate for the Massive mode's behaviour, and overlaps the Blackhole "shape the envelope" idea.
- Andromeda-scale decay (effectively infinite) must stay tame — reinforces in-loop damping + soft saturation.

## 3. Signal architecture (draft)

```
in → input diffusion → [ TANK: Plate | Hall | Massive (FDN) ] → out
                          ↑ feedback loop contains:
                          - modulated delays (Drift: pitch drift + wow/flutter)
                          - damping / tilt EQ (Tone)
                          - resonant desk EQ + drive (Desk)
                          - Neve-style soft saturation (always on, keeps infinite tails tame)
                          - Era degradation (bandwidth, bits, μ-law, cassette)
                          - ducking stage (envelope follower on input)
small "freeze companion" plate (separate, cheap, internal RAM)
```

Anti-metallic rules: modulate all tank delays, mutually prime delay lengths, damping in loop.
Infinite control: feedback just under 1.0 + damping + soft saturation in loop.

## 4. Control map (draft — take into spec)

**Knobs (all CV-able):**
1. **Mix** — dry → fully wet. **[OPEN]** kill-dry option for send use.
2. **Size** — large room → boundless. **[OPEN]** zipper/pitch-warp behaviour on change: design it, don't leave it accidental.
3. **Decay** — past ~3 o'clock: infinite + ducking eases in (Desmodus-style concept).
4. **Tone** — tilt EQ in loop; tail darkens/brightens as it decays.
5. **Drift** — Cathedra-style pitch drift + tape wow/flutter.
6. **Era** — clean modern → Neve warmth → cassette → μ-law → Midiverb grain. Crackle/hiss scale with it.
7. **Desk** — resonant EQ frequency in loop; drive rises with it.

**Switches:**
- **Duck style:** Darken (filter closes on tail) / Push back (more smear + narrower stereo) / Warble (input kicks tape wow).
- **Space:** Plate / Hall / Massive (FDN).

**Button — two-layer freeze:** big tank locks as background pad; input reroutes to a small companion reverb so playing on top isn't dry. While held, Size + Decay control the small reverb. **[OPEN]** optional slow feed of small reverb into the frozen pad (layering).

**Gate — dub throw:** trigger briefly opens send into tank. **[OPEN]** gate mirrors button vs. dub throw (current lean: dub throw); tap vs. hold behaviours.

## 5. Era — needs definition **[OPEN, priority]**

For each stage define: sample-rate/bandwidth, bit depth / companding, saturation curve, noise (hiss/crackle) level, wow/flutter amount, whether applied in-loop or on output.
- μ-law: compare in-loop (low setting, builds per pass) vs output-only.
- Midiverb end: degradation in-loop so grain grows as tail decays.
- Lower internal sample rate at lo-fi end may also save CPU.

## 6. Hardware budget

Verified (Electrosmith / ST docs): STM32H750 Cortex-M7 @ 480 MHz, 64 MB external SDRAM, 8 MB QSPI flash, **128 KB internal flash**, ~1 MB internal SRAM (split across domains).

Implications:
- ~10k cycles/sample at 48 kHz. 8-line FDN + small plate + character chain should fit; 16 lines is stretch.
- Long tank delays in SDRAM (slower, random access hurts); diffusers + small plate in internal SRAM.
- **128 KB internal flash is a code-size constraint** — check how Resilio handled it (bootloader/QSPI execution?).
- Block processing; control-rate updates for envelope follower, LFOs, param smoothing.
- Cheap saturation approximations instead of true tanh per line.
- Measure CPU load early, before stacking features.

## 7. Process

1. Inspect **Resilio Versio repo**: reuse/refactor JUCE VST/AU harness, test suites, μ-law, tape/saturation DSP, Versio hardware layer, build/flash tooling. Consider a shared DSP library across both firmwares.
2. Spec phase (spec-driven): resolve [OPEN] items, define Era stages, finalise control ranges and curves.
3. Prototype in JUCE plugin; tune by ear.
4. Port to Versio; profile CPU; iterate.

Emulation approach: hybrid — published measurements/papers distilled into cheap models (waveshaper + filters + noise + modulation). Full circuit modelling (WDF) too heavy.

## 8. References

**Valhalla / Sean Costello**
- Supermassive: The Modes — https://valhalladsp.com/2020/05/07/valhallasupermassive-the-modes/
- The Philosophy of ValhallaSupermassive — https://valhalladsp.com/2020/05/06/the-philosophy-of-valhallasupermassive/
- Introducing ValhallaSupermassive — https://valhalladsp.com/2020/05/06/introducing-valhallasupermassive/
- Tips and Tricks: Infinite Supermassive — https://valhalladsp.com/2021/05/19/tips-and-tricks-infinite-supermassive/
- Getting Started With Reverb Design series (parts 1–4) — https://valhalladsp.com/tag/sean-costello/

**DSP / emulation**
- Dattorro, "Effect Design Part 1" (1997) — plate tank topology
- Jot — feedback delay networks
- DAFx conference papers
- Jatin Chowdhury — analog tape modelling paper; ChowTape (open source)
- Julius O. Smith — free CCRMA online books (filters, delay lines, reverb)
- Midiverb reverse-engineering projects **[VERIFY: find them]**

**Daisy / Versio prior art**
- Plateau Versio — github.com/abluenautilus/PlateauNEVersio (GPL-3.0; check licence before borrowing any code)
- Daisy community firmware index — github.com/Ziforge/daisy-patch-algos (also lists a Griesinger reverb and reverb building blocks)

**Hardware to study:** Mercury7, Strymon BigSky (Bloom, Cloud), Ursa Major Space Station, Lexicon 224, Alesis Midiverb/Quadraverb, Roland SRV-2000, Yamaha REV7, EMT 140, AKG BX20, Neve, Tascam Portastudio, MCI desk, Mackie 1604.
