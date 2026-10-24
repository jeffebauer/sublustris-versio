# New ERA candidates: early sampler, bucket-brigade, AM radio (25 Oct 2026)

*Research only: nothing in the repository was changed. This note answers the owner's request for more variety on ERA, "the knob transitions from sweeter tones to grittier tones", for dub techno, ambient and dark ambient, never metallic. It covers three candidates to replace the μ-law stage, plus the Neve base under every era.*

Tags: **[S]** a source says it; **[M]** we measured it (nothing was measured for this note); **[I]** our inference; **[F]** a forum or user report, not confirmed by a manual.

## Plain-language summary

- **Early sampler (SP-1200, Emulator, Mirage, Fairlight, Akai S900).** The grit comes from **sampling too slowly with weak filtering**, so high notes fold back as new, unrelated tones (aliasing). The other ingredients are **coarse steps on quiet sounds** (12 or 8 bits) and an **analogue filter after the converter** that softens it all. A cheap recipe: hold each value for a set time (a "sample-and-hold"), round it to 12 bits, then put it through a resonant low-pass that follows the rate. It sounds like a sampler rather than a "bitcrusher" when it stays at real rates (26–31 kHz, never 4 kHz), keeps 12 bits, and has the analogue filter after it. It is gritty and slightly bright: the grit sits at the top of the spectrum.
- **Bucket-brigade (Memory Man, Boss DM-2).** A BBD is an analogue chip that passes the signal along thousands of tiny capacitors. Its sound comes from the **steep filters around it** (in echo units, a 3–7 kHz ceiling that drops as the delay gets longer), a **2:1 compressor before it and a 1:2 expander after it** (the "breathing": the noise rises and falls with the music), and a **gentle, level-independent 2nd harmonic** (about 1 % per 1024 stages). On a reverb's wet output we don't need the delay itself, only the filters, the compander, a little noise and the 2nd harmonic. It is warm, dark and soft, and it pumps.
- **AM / shortwave radio.** A **band-pass of about 250 Hz to 4.5 kHz** (narrowing to 2.4 kHz on a shortwave set), **one slowly wandering notch** (selective fading), **slow level fades with the receiver's AGC lifting the static as the signal drops**, **crackle** (lightning static is impulsive, not hiss) and a **one-sided clip** when the transmitter is over-modulated. On a decaying tail, the AGC behaviour is the musical core: as the reverb fades, the static swells to fill the space. Heterodyne whistles are tonal, so leave them out (they risk "metallic").
- **Neve base.** Everyone models it as a gentle asymmetric curve (mostly 2nd harmonic), driven harder in the lows (the transformer), with a slight top roll-off. Lo-fi plugins put the "preamp" colour **before** the media stages (RC-20 runs left to right: Noise → Wobble → Distort → Digital → Space → Magnetic).
- **Suggested order, sweet to gritty:** Neve base → Midiverb → BBD → cassette → sampler → AM radio.
- **Code saving:** the sampler and the BBD share most of their parts (a sample-and-hold at a set rate, a low-pass, a noise source). Two eras can come from one block with different settings.

---

## 1. Early sampler

### The real units

| Unit | Year | Bits | Sample rate | Filtering | Source |
|---|---|---|---|---|---|
| Fairlight CMI Series I/II | 1979–82 | 8 | variable, **8–24 kHz** ("the low sample rate introduced aliasing; however, Vogel felt that the low quality of the sounds gave them their own character") | — | [S] [Wikipedia](https://en.wikipedia.org/wiki/Fairlight_CMI) |
| E-mu Emulator II | 1984 | 8, **companded** DACs | **27.7 kHz** | resonant analogue SSM filters per voice | [S] [Wikipedia](https://en.wikipedia.org/wiki/E-mu_Emulator) |
| Ensoniq Mirage | 1984–88 | 8 | variable **10–33 kHz** (to 50 kHz with the optional input filter) | Curtis **CEM3328 4-pole 24 dB/oct** resonant per voice | [S] [Wikipedia](https://en.wikipedia.org/wiki/Ensoniq_Mirage) |
| E-mu SP-1200 | 1987 | **12, linear** | **26.04 kHz** | **SSM2044 dynamic 4-pole** filters on channels 1–2 (now SSI2144), cutoff and resonance trims; pitch shifting "with aliasing & imaging artifacts" | [S] [Rossum Electro](https://www.rossum-electro.com/pages/education-sp-1200) |
| Akai S900 / S950 | 1986 / 1988 | 12 | variable **7.5–40 kHz** (S900) | — | [S] [Wikipedia](https://en.wikipedia.org/wiki/Akai_S900) |

- [F] SP-1200 outputs 1–2 behave like a filter with a decay envelope, "open" at the start and closing after about 200 ms ([Tapatalk SP-1200 forum](https://www.tapatalk.com/groups/sp1200/backlight-t673-s10.html)). Users say "each of the 8 outputs give different amount of filtering". The often-repeated claims that 3–6 are fixed low-pass and 7–8 unfiltered, and that the input has no anti-alias filter, are **not confirmed** by any manual I could reach.
- [S] Rossum confirms 12-bit **linear** (not companded), contradicting forum claims of a nonlinear converter ([Rossum FAQ](https://www.rossum-electro.com/pages/education-sp-1200)).
- [S] The classic workflow was to sample a sped-up source, then tune it down. The SP-1200 tunes by skipping or repeating samples with no interpolation, so a lower pitch also lowers the effective rate and **the aliasing moves with the pitch** ([Synthtopia on waveTracing SP950](https://www.synthtopia.com/content/2021/08/27/wavetracing-intros-sp950-effect-plugin-delivers-gritty-sound-with-mandatory-aliasing-effects/)).
- [S] Valhalla VintageVerb's 1970s mode does the reverb version of this: "10 kHz maximum output frequency", "downsampled internally, to reproduce the artifacts of running at a lower sampling rate" ([Valhalla](https://valhalladsp.com/?p=60)). (That is close to our Midiverb, so this new era has to go grittier than that: lower rate, weaker filtering, coarser bits.)

### What makes the sound (numbers)

- **Aliasing.** Anything above half the rate (13.02 kHz on the SP-1200) folds back down: a 16 kHz component reappears at 26.04 − 16 = 10.04 kHz. On a dense reverb tail the folded energy is noise-like: a "sizzle", not a tone. [I] On a held, pitched note the folds are inharmonic tones, and those are where "metallic" lives, so keep a weak pre-filter.
- **Zero-order hold (sample-and-hold) roll-off.** Holding each value until the next one gives a sinc response: **−3.9 dB at half the rate**. The images (mirror copies above half the rate) are only partly suppressed: a 5 kHz tone at 26.04 kHz leaves an image at 21.04 kHz about −13 dB down [I] (sinc maths). This is the "imaging" in Rossum's spec.
- **Bits.** 12-bit linear: one step = −66 dBFS for a ±1 full scale. [I] Our tail runs about −30 to −15 dBFS RMS (`tape-out.md`), so the last 30 dB of every decay is made of fewer than ~30 steps. With no dither, that quantisation error follows the signal: the "grainy fade" of early samplers. At 8 bits (Fairlight, Mirage) one step = −42 dBFS, which is rough on a quiet tail.
- **The analogue filter after it.** The SSM2044 / CEM3328 4-pole resonant low-pass is what makes it read as an *instrument*, not a broken plugin. It tames the images and adds a soft resonant edge.

### Cheap zero-order hold at any rate

A phase accumulator. Because the rate isn't a whole fraction of 48 kHz, each hold lasts 1 or 2 host samples. That timing wobble (up to 20.8 µs) is itself a kind of aliasing. Airwindows DeRez (**MIT**, [airwindows.com/derez2](https://www.airwindows.com/derez2/)) fixes the steps cheaply by blending on the sample where the value changes, weighted by how far into that sample the change fell: "interpolates a sample between sample-rate-crushed outputs so the top end is smooth rather than gritty". DeRez2's "Hard" control crossfades between that and plain steps.

```
inc = rate / 48000                       // e.g. 26040/48000 = 0.5425
phase += inc
if (phase >= 1) {                        // a new "converter" sample is due
    phase -= 1
    float frac = phase / inc             // how late in this host sample it fell (0..1)
    float next = quantise(prefilter(x))
    out = held + (next - held) * (1 - frac) * soft   // soft=0: hard steps; 1: DeRez-style
    held = next
} else out = held
```

`quantise(v) = floorf(v * scale + 0.5f) / scale` with `scale = 2^(bits-1)`. Fractional bits work (scale is a float), so the knob can sweep the bit depth smoothly with no zipper steps.

### Recipe: "Sampler" stage, on the wet output (outside the loop)

| Block | Setting at the sweet end | At the gritty end | Scales with knob | Cost |
|---|---|---|---|---|
| Input drive (makeup after) | 0 dB | +12 dB (more of the tail lands on coarse steps) | yes | 2 mults |
| Weak pre-filter | 2-pole low-pass at 0.5 × rate, Q 0.6 | 1-pole at 0.7 × rate (lets more fold) | yes (tracks rate) | 1 SVF |
| Zero-order hold rate | 31.25 kHz | 12 kHz (Fairlight / Mirage low end) | yes, exponential | above |
| Bits (no dither) | 12 | 8 | yes, fractional | 1 floor |
| Step smoothing (`soft`) | 1 (DeRez-style) | 0.3 | yes | 2 mults |
| Post filter (SSM2044 stand-in) | 2-pole SVF low-pass at 0.42 × rate, Q 0.9, then a 1-pole at the same cutoff (≈ 3-pole) | Q 1.3 (a little resonant edge) | cutoff tracks rate; Q yes | 1 SVF + 1 pole |
| Output makeup | −drive | | | |

- **Where:** outside the loop. [I] Inside the loop, every pass would fold the aliasing again and add another round of quantisation, which piles up into exactly the inharmonic ringing the owner calls metallic. Real samplers sampled once and played back once.
- **Keeping it from sounding like a generic bitcrusher** [I, from the sources above]: real rates (≥ 12 kHz), 12 bits by default, a resonant analogue-style filter that follows the rate, and a steady crystal clock (no rate wobble: that's the cassette's job).
- **Optional and cheap:** SP-1200 "pitch" character. Run the hold rate at 26.04 kHz × 2^(−semitones/12) to mimic a sample tuned down. It only changes `inc`. On a reverb this just lowers the ceiling, so it can be folded into the rate scaling.
- **Size:** ~40 lines; no tables.
- **Licence:** the ideas come from the sources above; DeRez is MIT (credit Airwindows if any code is adapted).

---

## 2. Bucket-brigade (BBD) delay and chorus

### The real units

- [S] The BBD was invented in 1969 by Sangster and Teer at Philips. The Reticon **SAD-1024** (c. 1976) had two 512-stage lines ([Wikipedia](https://en.wikipedia.org/wiki/Bucket-brigade_device)).
- Echo units use 4096-stage chips (Panasonic **MN3005**). Examples: **Electro-Harmonix Deluxe Memory Man** (late 1970s) and **Boss DM-2** (1981). Chorus units use 512–1024-stage MN3007 / MN3009 chips. [I] These are common knowledge: the model names are not taken from a datasheet here.

### What makes the sound (numbers, Raffel & Smith, [DAFx-10](https://colinraffel.com/software/bbdmodeling/bbdmodeling.pdf))

- **Clock and bandwidth.** Delay = N / (2·f_clock). A 4096-stage BBD at 300 ms needs a **6.83 kHz clock**, so the audio must stay under **3.41 kHz**. A 1024-stage flanger at 10 ms runs at 51.2 kHz. Filter cutoff is "between ⅓ and ½ of the sampling frequency"; "in some BBD-based echo circuits, the filter cutoff can be as low as **1.5 kHz**". [S]
- **Filters.** Typically a **3rd-order Sallen-Key anti-alias** filter before, and a **3rd-order plus a 2nd-order "corner correction"** after: about 8th order in total, so the top end falls off a cliff. [S]
- **Compander (NE570 / NE571).** **2:1 compression before the BBD, 1:2 expansion after.** Averaging time constant τ = 10 kΩ × C_rect, with C = 0.22–1 µF, so **τ = 2.2–10 ms**. Model: expander `y = avg(|x|)·x`, feedback compressor `y = x / avg(|y|)`, with the average a one-pole low-pass of the full-wave rectified signal. [S]
- **Noise.** The BBD's own S/N is **about 60 dB** at near-maximum input. The compander suppresses it further, but because the expander's gain follows the music, **the noise floor rises and falls with the signal**: the "breathing". [S] for the 60 dB; [I] for the audible effect, which is the standard explanation of compander noise modulation.
- **Distortion.** "About **1 % of harmonic distortion** occurs for every 1024 stages", so about 4 % in a 4096-stage echo. It "does not vary greatly depending on the signal level — it is not a clipping distortion". The harmonics fall off steadily, so 4th and up are almost inaudible. Raffel fits it with `f(x) = x − a·x² − b·x³ + a` (clipped smoothly outside ±1). The coefficients read as **a = 1/8, b = 1/18** in the extracted text (the PDF's fractions are garbled: check the figure before use). [S] The `a·x²` term makes it **mostly 2nd harmonic**, which keeps it clearly apart from the cassette's 3rd [I].
- **Insertion loss.** 0 to +2 dB at low frequencies, falling to **−4…−6 dB at the clock's Nyquist**. That is small next to the filters. [S]
- **Clock whine.** Clock leakage at f_clock: audible on echo units at long delays, where the clock falls into the audio band. [I] It's a pure tone, so **leave it out** (metallic risk).
- **Later model.** Holters & Parker ([DAFx-18](https://dafx.de/paper-archive/2018/papers/DAFx2018_paper_12.pdf)) treat the BBD as a variable-rate sampler and fold the input/output filters into the resampling, so no separate interpolation is needed. That matters for **modulated delay** (chorus, pitch bends), which we don't need on a reverb's output. Its open-source implementation, chowdsp_utils `BBD`, lives in the **GPLv3** `chowdsp_dsp_utils` module ([chowdsp README licence list](https://github.com/Chowdhury-DSP/chowdsp_utils)), so it's **reference only** (ADR 0006).

### Which parts matter on a reverb's wet output

[I] Ranked by audibility on a long, dense tail:
1. **The steep low-pass** (the darkness): by far the biggest.
2. **Compander breathing:** a quiet hiss that swells with each hit and sinks as the tail decays. On dub chords this is the "analogue pump".
3. **The 2nd harmonic:** thickens, never fizzes.
4. **Aliasing at the clock:** with filters this steep, almost nothing folds. Skip it, or reuse the sampler's hold block at f_clock for a hint.
5. **The delay itself, clock whine, insertion ripple:** skip.

### Recipe: "BBD" stage, on the wet output

| Block | Sweet end | Gritty end | Scales | Cost |
|---|---|---|---|---|
| Anti-alias low-pass (3rd order Butterworth: SVF + 1-pole) | 7 kHz (≈ a 16 kHz clock, chorus-like) | 2.2 kHz (≈ a 5 kHz clock, long echo) | yes, exponential | 1 SVF + 1 pole |
| Compressor 2:1 (feedback) | `g = 1/sqrt(max(env_y, floor))` with env = one-pole of abs(y), τ = 10 ms, referenced to −18 dBFS | same | — | 1 pole, 1 sqrt |
| BBD nonlinearity | `x − a·x² − b·x³` with a = 0.02, b = 0.01 (≈ 1 %) | a = 0.08, b = 0.04 (≈ 4 %, a 4096-stage line) | yes | 4 mults |
| Noise inside the compander | white, −66 dB re the compressed level | −54 dB | yes | 1 RNG |
| Expander 1:2 | gain = env_x / ref, τ = **12 ms at the sweet end, up to 25 ms at the gritty end** (a deliberate mismatch with the compressor gives more audible breathing [I]; real 570 halves track only roughly) | | τ yes | 1 pole |
| Reconstruction low-pass (3rd + 2nd order) | same cutoff as the anti-alias filter | | tracks | 2 SVF + 1 pole |

- Noise added between the compressor and the expander gets expanded: it follows the tail down and comes back up with each new note. That is the breathing for free. [I]
- **Where:** outside the loop. [I] An 8th-order low-pass inside the loop would make the tail collapse to a dull hum; Midiverb already supplies the dark loop.
- **Shared code with the sampler:** both are "low-pass, (optional) hold, low-pass, noise". The sampler is a BBD with weak filters and quantisation instead of a compander. [I]
- **Size:** ~50 lines; no tables. The filter coefficients come from the standard SVF formula, recomputed only when ERA moves.
- **Licence:** Raffel & Smith is a paper (ideas are free). His `echo.cpp` / `BBDfilter.m` carry no stated licence, so don't copy them. chowdsp's BBD is GPLv3: reference only.

---

## 3. AM / shortwave radio

### What the real chain does

- **Bandwidth.** NRSC-1 caps AM station audio at **10 kHz**, 9.6 kHz in practice. Receivers are far narrower: the "de facto" IF filter is **±3 kHz**, and an NRSC-linked survey found most receivers **under 5 kHz** of audio. The 1988 NRSC narrow-receiver reference is **−10 dB at 4.2 kHz and −20 dB at 6 kHz**. [S] ([NRSC-1 1988](https://nrscstandards.org/standards-and-guidelines/documents/archive/nrsc-1-1988.pdf), [engineeringradio.us](https://www.engineeringradio.us/blog/2014/03/the-am-receiver-problem/), [AMSTG summary](https://www.nrscstandards.org/standards-and-guidelines/documents/references/amstg-report-summary.pdf)). Shortwave broadcast uses 5 kHz channels, and communications receivers often sit at 2.4–6 kHz. [I, common practice]
- **Selective fading.** Two sky-wave paths, one changing length. "The cancellation effect, or 'null', is deepest at one particular frequency, which changes constantly, sweeping through the received audio" — "a slow, cyclic disturbance", typically at dusk and dawn. [S] ([Wikipedia: Fading](https://en.wikipedia.org/wiki/Fading)). The standard HF test channels (CCIR 520, Watterson model): **Good = 0.5 ms path difference, 0.1 Hz fading; Poor = 2 ms, 1 Hz**. Mid-latitude paths show 1–4 ms and ≤ 1 Hz. [S] ([ULPGC parameter paper](https://accedacris.ulpgc.es/bitstream/10553/42969/1/parameterestimation.pdf), [TAPR HF simulator](https://web.tapr.org/meetings/DCC_1999/DCC1999-HFchannelSimulator-KC7WW.pdf), [MathWorks](https://www.mathworks.com/help/comm/ug/hf-ionospheric-channel-models.html)). A 1 ms path difference puts nulls 1 kHz apart. [I]
- **AGC.** The receiver's automatic gain control raises the gain when the signal fades, **so the static rises as the station drops**. Typical decay is about 0.1–0.25 s ("fast") to 1–2 s ("slow"). [I, standard receiver practice; no manual cited]
- **Static.** Atmospheric noise from lightning ("sferics") is **impulsive crackle**, unlike the steady hiss of the receiver's own circuits. [I, standard radio knowledge]
- **Detector distortion.** A diode envelope detector follows the carrier's top edge ([Wikipedia: Envelope detector](https://en.wikipedia.org/wiki/Envelope_detector)). Two faults are classic: with **over 100 % modulation** the negative peaks are flattened (a one-sided clip, adding even and odd harmonics), and a too-slow RC smears fast deep modulation ("diagonal clipping"). [S] for the detector; [I] for the one-sided-clip model below.
- **Heterodyne whistles.** Beats between neighbouring carriers: 9 or 10 kHz (filtered away) or drifting tones on shortwave. [I] Tonal, so **leave them out**.

### Recipe: "Radio" stage, on the wet output

| Block | Sweet end | Gritty end | Scales | Cost |
|---|---|---|---|---|
| Transmitter clip (over-modulation) | `y = max(x·m, -1)/m` around a carrier of 1; m = 0.9 (no clipping) | m = 1.4 (troughs flatten) | yes | 1 max |
| High-pass (2nd order) | 200 Hz | 400 Hz | yes | 1 SVF |
| IF/audio low-pass (4th order: 2 SVFs, Q 0.54 / 1.31) | 4.5 kHz | 2.4 kHz | yes | 2 SVF |
| Selective fade: **one** peaking-EQ dip | depth −4 dB, Q 1.5 | depth −18 dB, Q 3 | yes | 1 biquad |
| …its centre frequency | wanders 300 Hz–3 kHz (log), driven by smoothed random noise at **0.1 Hz** | **0.5 Hz** | rate yes | 1 RNG + 2 poles |
| Flat fade | ±1 dB, 0.05 Hz random | ±6 dB | yes | shared RNG |
| AGC (after the fade) | attack 5 ms, decay 0.25 s, max boost +6 dB | max boost +15 dB | yes | 1 pole pair |
| Static: crackle | Poisson impulses, 2/s, exponential amplitude, through the same band-pass | 20/s | yes | 1 RNG + compare |
| Static: hiss | −60 dB | −40 dB | yes | shared RNG |
| Noise enters **before** the AGC | so the AGC lifts it as the tail fades | | | |

- **Why one notch and not a two-path comb:** the real two-path channel is a comb. A drifting comb on audio is a flanger, which is the "metallic" sound to avoid. Wikipedia's description (one deepest null, sweeping) is exactly one moving notch, and that costs one biquad. [I] If a page shows the single notch is too tame, try a shallow two-path comb (gain ≤ 0.5, delay 0.5–2 ms drifting at ≤ 0.2 Hz) as a second candidate.
- **The musical core on a tail** [I]: the reverb decays → the AGC raises the gain → the crackle and hiss swell into the space the reverb left. Freeze holds the level, so the noise stays put. Cap the AGC boost so a silent input never becomes a wall of static (it should fade with the tail once the input level falls below a gate around −60 dBFS).
- **Where:** outside the loop. A band-pass inside the loop would starve the tail.
- **Size:** ~60 lines; no tables.

---

## 4. The Neve base (under every era)

### How console warmth is usually modelled

- **The real circuit.** The Neve 1073 (1970) is a discrete class-A amplifier with Marinair / St Ives input and **LO1166 output transformers**. Forum reports say AMS Neve found "unacceptable" low-frequency distortion in the LO1166 (single-figure percent for Marinair, double figures for St Ives). That is anecdotal but consistent with the physics. [F] ([GroupDIY](https://groupdiy.com/threads/ams-neve-article-on-their-transformers.58307), [GroupDIY LO1166](https://groupdiy.com/threads/lo1166-technical-questions.15309), [KMR on the 1073](https://kmraudio.com/blogs/blog/50-years-of-neve-1073)).
- **Why the lows distort more.** A transformer core's flux is the integral of the voltage, so at a given level a low note swings the core 4× harder than a note two octaves up. That is why transformer models put the drive before an integrator or a low shelf. [I, physics; full models: Holters & Zölzer, DAFx-16, Jiles–Atherton for transformers, already cited in `tape-out.md`]
- **The cheap model everyone uses** [I, from Airwindows Console (MIT, [airwindows.com/console7](https://www.airwindows.com/console7/)) and common practice]:
  1. A low shelf of about +2 dB below 120 Hz *into* the curve, a matching cut after it (the lows drive harder, the level stays flat).
  2. An **asymmetric soft curve**: `y = x + k2·x² − k3·x³`, with k2 ≈ 0.05–0.1 and k3 small. The 2nd harmonic dominates, the 3rd is quiet, and both grow with level.
  3. A DC blocker (the x² term makes DC).
  4. A gentle 1-pole roll-off at about 16–18 kHz. (The real units are flat to 20 kHz; this is the "slight darkening" the current Neve era already has, so keep that.)
  5. An optional 0.5–1 dB bump around 60–100 Hz.

### Is layering a base under media stages common?

Yes. Plugins treat the "preamp / console" colour as a separate stage that comes **before** the media:
- **RC-20 Retro Color:** "incoming audio (generally) passes through the six modules from left to right": **Noise → Wobble → Distort → Digital → Space → Magnetic**. Noise can be moved to the very end so the other modules don't colour it. [S] ([RC-20 manual](https://assets.xlnaudio.com/documents/rc-20-retro-color_manual.pdf), [MusicRadar](https://musicradar.com/reviews/tech/xln-audio-rc-20-retro-color-646595)). Its Distort module includes **transformer** and valve types.
- **Aberrant DSP SketchCassette II:** saturation (models A gentle / B harsh), noise-reduction encoding, filtering, hiss, wow/flutter and dropouts. The order isn't documented. [S] ([Plugin Boutique](https://pluginboutique.com/product/2-Effects/96-Tape-Emulation/7024-SketchCassette-II))
- **Decapitator** offers several console/preamp "styles" as alternatives (one of them a Neve channel, per Soundtoys' marketing). [F] I couldn't confirm the exact list from the fetched page. It's a saturator, not a lo-fi chain.
- [I] The common rule across these: **colour (preamp/transformer) → the medium's own distortion → its filtering → its noise → its time wobble**. For Immensio: Neve base first, then the selected era. Each era adds its own noise *after* its distortion, so the noise stays clean.

---

## 5. Suggested sweet-to-gritty order on the ERA knob

| Position | Era | Why it sits here |
|---|---|---|
| 0 | **Neve base alone** | Pure warmth: 2nd harmonic, fuller lows, no noise, no band-limit. The cleanest step. |
| 1 | **Midiverb** | Dark, smooth, 13-bit and 10 kHz, with nothing moving and no noise: "sweet". |
| 2 | **BBD** | Darker still, but with a soft 2nd harmonic and gentle breathing noise: the first era with life in its floor, still never harsh. |
| 3 | **Cassette** | Wider band than the BBD, but the 3rd-harmonic saturation, hiss and wow/flutter make it audibly rougher and unsteady. |
| 4 | **Sampler** | The first era whose grit is *bright*: aliasing sizzle and grainy 12→8-bit fades. It sounds broken in a musical way. |
| 5 | **AM radio** | The most destroyed: telephone band, a moving notch, fades and crackle swelling as the tail dies. The far gritty end. |

[I] Each neighbour pair differs on at least two axes (bandwidth, harmonic order, noise behaviour, motion), so no two eras sit next to each other sounding alike, which was the μ-law/cassette problem. To test before deciding: render all six on the same dub chord stab and pad at matched loudness, with the cassette and the sampler side by side (both "noisy-ish") and the BBD next to the AM radio (both narrow).

## Sources

- Raffel & Smith, "Practical Modeling of Bucket-Brigade Device Circuits", DAFx-10: [PDF](https://colinraffel.com/software/bbdmodeling/bbdmodeling.pdf), [archive](https://dafx.de/paper-archive/details/JhVfAOFXD1lAtctkMTUODg), [author page](https://colinraffel.com/software/bbdmodeling)
- Holters & Parker, "A Combined Model for a Bucket Brigade Device and its Input and Output Filters", DAFx-18: [PDF](https://dafx.de/paper-archive/2018/papers/DAFx2018_paper_12.pdf)
- chowdsp_utils BBD (GPLv3 module, reference only): [docs](https://ccrma.stanford.edu/~jatin/chowdsp/chowdsp_utils/classchowdsp_1_1BBD_1_1BBDDelayLine.html), [repo](https://github.com/Chowdhury-DSP/chowdsp_utils)
- [Bucket-brigade device (Wikipedia)](https://en.wikipedia.org/wiki/Bucket-brigade_device)
- [Rossum SP-1200 specs and FAQ](https://www.rossum-electro.com/pages/education-sp-1200); [SP-1200 forum filter measurements](https://www.tapatalk.com/groups/sp1200/backlight-t673-s10.html); [Gearspace SP-1200 trims](https://gearspace.com/threads/sp1200-internal-knobs-adjusting-dor.134782/); [SSM2044 (sdiy)](https://sdiy.info/wiki/SSM2044)
- [Fairlight CMI](https://en.wikipedia.org/wiki/Fairlight_CMI), [E-mu Emulator](https://en.wikipedia.org/wiki/E-mu_Emulator), [Ensoniq Mirage](https://en.wikipedia.org/wiki/Ensoniq_Mirage), [Akai S900](https://en.wikipedia.org/wiki/Akai_S900) (Wikipedia)
- Airwindows (MIT): [DeRez2](https://www.airwindows.com/derez2/), [BitGlitter](https://www.airwindows.com/bitglitter/), [Console7](https://www.airwindows.com/console7/)
- [Valhalla VintageVerb](https://valhalladsp.com/?p=60)
- [waveTracing SP950 (Synthtopia)](https://www.synthtopia.com/content/2021/08/27/wavetracing-intros-sp950-effect-plugin-delivers-gritty-sound-with-mandatory-aliasing-effects/)
- AM: [NRSC-1 (1988)](https://nrscstandards.org/standards-and-guidelines/documents/archive/nrsc-1-1988.pdf), [AMSTG summary](https://www.nrscstandards.org/standards-and-guidelines/documents/references/amstg-report-summary.pdf), [The AM receiver problem](https://www.engineeringradio.us/blog/2014/03/the-am-receiver-problem/), [Fading (Wikipedia)](https://en.wikipedia.org/wiki/Fading), [Envelope detector (Wikipedia)](https://en.wikipedia.org/wiki/Envelope_detector), [HF channel parameters (ULPGC)](https://accedacris.ulpgc.es/bitstream/10553/42969/1/parameterestimation.pdf), [TAPR HF simulator](https://web.tapr.org/meetings/DCC_1999/DCC1999-HFchannelSimulator-KC7WW.pdf), [MathWorks HF channel models](https://www.mathworks.com/help/comm/ug/hf-ionospheric-channel-models.html)
- Neve: [GroupDIY on Neve transformers](https://groupdiy.com/threads/ams-neve-article-on-their-transformers.58307), [LO1166 questions](https://groupdiy.com/threads/lo1166-technical-questions.15309), [KMR: 50 years of the 1073](https://kmraudio.com/blogs/blog/50-years-of-neve-1073)
- Lo-fi chains: [RC-20 manual](https://assets.xlnaudio.com/documents/rc-20-retro-color_manual.pdf), [RC-20 product page](https://www.xlnaudio.com/products/addictive_fx/effect/rc-20_retro_color), [MusicRadar RC-20 review](https://musicradar.com/reviews/tech/xln-audio-rc-20-retro-color-646595), [SketchCassette II](https://pluginboutique.com/product/2-Effects/96-Tape-Emulation/7024-SketchCassette-II), [Decapitator](https://www.soundtoys.com/product/decapitator/)

## Open questions (not resolved here)

- SP-1200 outputs 3–8 filtering and the input anti-alias filter: needs the service manual.
- Raffel's exact a, b coefficients (the PDF's fractions are garbled in the extracted text).
- CCIR 520 "Moderate" values (recalled as 1 ms / 0.5 Hz, not confirmed).
- Not measured: none of these recipes has been rendered. Per the house rule, energy-check and level-match each before any page.
