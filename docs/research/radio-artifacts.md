# Radio artifacts that feel real: research for RadioStage v2 (25 Oct 2026)

Tags: [S] sourced, [I] inference or standard radio practice (no single citation), [M] my proposal.

## Plain-language summary

- **Why round 2 sounds fake.** Every artifact is one fixed thing repeated: the bursts are the same white noise at a steady pace, the whistle is a steady-level sine that slides around in pitch. A real radio is the opposite: **one or two slow random "weather" processes drive everything**, and every event has its own colour, length and loudness.
- **The single biggest realism fix is order, not new noise.** On a real set, the station fades but the static does not; the AGC then turns the volume up, so **hiss and crackle swell exactly when the station dips**. In the current code the noise is added *before* the flat fade, so it fades with the signal and the AGC never lifts it. Add the noise after the fade, before the AGC. [S/I]
- **Softer hiss:** receiver hiss is darker than white (the IF filter and the audio stage roll it off), and it sits 15-40 dB under a weak station. Make it pink-ish, about 6-10 dB quieter on average, and let the AGC make it breathe.
- **Static:** lightning arrives as a *flash* of 1-6 strokes about 40-60 ms apart, each a click with a few-ms ring, with flashes at irregular (exponential) intervals and the storm itself coming and going over minutes. Distant storms are a fine "bacon frying" crackle; near ones are crashes. Give each event a random colour (low-pass 600-3000 Hz), length and level.
- **Whistles:** two stations beating hold a nearly **fixed pitch** (only a few Hz of wobble from the ionosphere) and **fade in and out** with their own fading; they do not glide over octaves. Pitch moves only when you tune, as a quick "whoop" through zero beat, or for a drifting power-supply "birdie".
- **Tuning flutter:** use the HF standard's own fading model (random noise, smoothed, two components → a Rayleigh/Rician gain), at 0.1-0.5 Hz normally and 5-15 Hz in rare "flutter" episodes. A small slow "detune" wander ties together distortion, dullness and flutter.
- **Keep the wandering dip** exactly as it is. It is the right picture of selective fading. The new fading process may deepen it during fades, so dip, hiss and whistle all move together.
- **Risky, keep tiny or rare:** mains buzz and hum (periodic), whistles (tonal), fast flutter (tremolo). Organic recipe: random walks, two slow modulators at unrelated rates, short lifetimes, Poisson timing.

## Sources

- ITU-R Rec. F.1487, *Testing of HF modems with bandwidths up to about 12 kHz using ionospheric channel simulators* (Watterson model, quiet/moderate/disturbed channels): <https://www.itu.int/rec/R-REC-F.1487/en>
- MathWorks, *HF Ionospheric Channel Models* (F.1487 / Watterson implementation; LM channel = 2 ms, 1.5 Hz spread; Gaussian Doppler spectra; bi-Gaussian components): <https://www.mathworks.com/help/comm/ug/hf-ionospheric-channel-models.html>
- Watterson, Juroshek, Bensema, *Experimental confirmation of an HF channel model*, IEEE Trans. Comm. Tech. 1970: <https://doi.org/10.1109/TCOM.1970.1090417>
- Johnson (KC7WW), *A Low-Cost HF Channel Simulator*, TAPR DCC 1999 (CCIR 520 good/poor/flutter channels): <https://web.tapr.org/meetings/DCC_1999/DCC1999-HFchannelSimulator-KC7WW.pdf>
- ITU-R Rec. P.372, *Radio noise* (atmospheric, galactic, man-made; man-made model is Gaussian only, impulsive part excluded): <https://www.itu.int/rec/R-REC-P.372/en>
- Chrissan & Fraser-Smith, clustering Poisson model of atmospheric noise interarrival times (Radio Science 2003): <https://earthref.org/ERR/72948>; Chrissan thesis abstract (non-stationary by season, hour, storm): <https://nova.stanford.edu/orals/97.09.17_DougChrissan.html>
- Dalke, Middleton Class A impulsive noise (NTIA, IEEE EMC 1998): <https://its.ntia.gov/publications/download/Dalke-IEEE-EMC-1998-1.pdf>
- Qi & Wang, bursty mixed Gaussian-impulsive noise (Markov on/off bursts): <https://arxiv.org/pdf/2402.06395>
- IOP Spark, *Sferics, tweeks and whistlers* (sferics last a few ms; sound like crackling): <https://spark.iop.org/sferics-tweeks-and-whistlers>
- Rakov & Uman, *Lightning: Physics and Effects* (Cambridge UP 2003): typical flash 3-5 return strokes, ~60 ms between strokes, flash ~0.2-0.5 s. [S, from the standard text; no free link]
- ARRL New England Division, *Power Line RFI: what it looks and sounds like* (microgap discharge on each half cycle, 8 ms spikes, buzz strongest at 120 Hz falling with frequency, several sets of peaks, vanishes in wet weather): <https://nediv.arrl.org/power-line-rfi-what-does-it-look-and-sound-like-and-how-do-i-locatethe-source/>; ARRL power-line noise FAQ: <https://www.arrl.org/power-line-noise-faq>
- Fallows, *Understanding synchronous detection* and *Legacy AM receivers: how envelope detection works* (selective fading drops the carrier 10-15 dB under the sidebands → looks over-modulated → distortion): <https://play.fallows.ca/wp/radio/shortwave-radio/understanding-synchronous-detection>, <https://ergo.fallows.ca/wp/radio/shortwave-radio/legacy-am-receivers-how-envelope-detection-works/>
- Electronics Notes, synchronous AM detection: <https://www.electronics-notes.com/articles/radio/modulation/am-synchronous-demodulation-detection-detector.php>
- US patent 4,192,970 (adjacent-channel 10 kHz whistle; receivers low-pass audio ~3 kHz to hide it; "monkey chatter" sideband splatter): <https://image-ppubs.uspto.gov/dirsearch-public/print/downloadPdf/4192970>
- EP 0666637 (switch-mode supply noise demodulates to an audible tone that drifts with temperature): <https://data.epo.org/publication-server/rest/v1.2/publication-dates/19990303/patents/EP0666637NWB1/document.html>
- Samlex, switching supplies and RF noise (harmonics of the switching frequency + broadband noise): <https://samlexamerica.com/?p=980>
- 47 CFR 73.1545 (US AM broadcast carrier tolerance ±20 Hz): <https://www.ecfr.gov/current/title-47/chapter-I/subchapter-C/part-73/subpart-H/section-73.1545>
- Evaton RF Nomad manual (a Eurorack direct-conversion receiver: slowly sweeping the tuning gives "lots of heterodyne noises"): <https://www.analoguehaven.com/evaton-technologies/rf-nomad/manual.pdf>
- Paul Kellet's economy pink-noise filter (3 one-poles, widely published, no licence issue for a formula): <https://www.firstpr.com.au/dsp/pink-noise/>

No GPL code was read or copied; everything below is from descriptions and formulas.

---

## 1. Receiver hiss

**What is real.** [S/I]
- Hiss is the receiver's own front-end noise plus galactic/man-made noise (P.372). It is **constant at RF**: it does not fade with the station. After the IF filter (a band about ±3-5 kHz around the carrier) and the envelope detector, it becomes audio noise limited to roughly half the IF width, then rolled off further by the IF skirts and the set's audio stage and speaker. So it is **band-limited and tilted darker**, not white. [I]
- Level: a weak but listenable shortwave station is 10-25 dB above the noise; a strong local AM station 40 dB or more. [I, standard receiver S/N practice]
- AGC: as the station fades by X dB, the AGC turns up by up to X dB (until its maximum), so the hiss rises by the same amount. This "breathing" is what tells the ear it is a radio. [I, matches the first design's sourcing]
- What makes hiss soft: little energy above 2-3 kHz, a smoother (near-Gaussian) amplitude, and a level that moves slowly with the station instead of sitting fixed on top. [I]

**Recipe.** [M]
- `w = 0.5*(rng()+rng())` (triangular, closer to Gaussian than uniform; one extra RNG call).
- Pink-ish tilt: Kellet economy filter (3 one-poles) or simply `h = lp1(w, 1.2-2 kHz)` mixed `0.6*h + 0.4*w`, then through the stage's existing HP/LP (it already enters before them).
- Add it **after the flat fade and the dip, before the AGC** (fix in section 7).

| Parameter | Sweet | Gritty | Notes |
|---|---|---|---|
| Hiss level vs the tail's slow envelope (before AGC) | -44 dB | -26 dB | today's noise mode is -36…-30 dB; owner asked softer |
| Tilt: one-pole LP on the hiss | 1.2 kHz | 2.2 kHz | brighter only at the gritty end |
| Amplitude | triangular (2 RNGs) | same | |
| AGC lift during fades | follows the fade, ≤ +6 dB | ≤ +15 dB | existing maxBoost |

---

## 2. Atmospheric static (QRN) and crashes

**What is real.**
- A sferic is a broadband impulse lasting a few ms (initial pulse ~1-2 ms, tail to ~10 ms). [S, IOP; patent-cited figure] Through an AM receiver the impulse rings the IF filter (sub-ms), the detector turns it into a **click**, and the audio filters colour it: distant storms arrive filtered and dull, near ones bright. [I]
- One lightning flash = typically **3-5 return strokes ~40-60 ms apart, 0.2-0.5 s overall**, so a near crash is a short *rattle* ("krrk-k-k"), not a single blip and not a steady burst of white noise. [S, Rakov & Uman]
- Many distant storms → a dense carpet of small clicks: "twigs snapping / bacon frying". [S, IOP]
- Timing: impulses **cluster** (strokes inside flashes, flashes inside storm cells): a clustering Poisson process fits measured interarrival times. [S, Chrissan & Fraser-Smith] Storm activity is non-stationary over minutes to hours. [S, Chrissan]
- Amplitude: heavy-tailed (Middleton Class A/B). Its "impulsive index" A runs from small (isolated, spiky impulses) to large (overlapping → hiss-like). [S, Dalke] Bursty versions add a hidden on/off Markov state. [S, Qi & Wang]
- A big crash also **ducks the AGC**: the station drops for a fraction of a second after it and recovers with the AGC decay. [I] This "pump" is very characteristic.

**Why round 2's bursts sound fake:** fixed white colour, fixed level (0.1), uniform interval 3-8 s (too regular), uniform length 0.15-0.4 s, and a flat rectangle-ish envelope.

**Recipe: three layers, all cheap.** [M]

*Layer A, storm activity* `S(t)` in 0..1: the existing `wander()` helper at 0.01-0.03 Hz (a new value every ~40-90 s). Multiplies both other layers' rates. Gives "quiet minute, busy minute".

*Layer B, crackle (distant storms)*: per sample, `if (u < rateB*S/sr)` start a click:
- amplitude `a = aB * min(u^(-1/alpha), 8)` (Pareto, heavy tail; alpha 1.5-2.5; one `pow` per event, events are rare);
- decay time τ random 0.2-2 ms (`exp(-1/(τ·sr))` per event);
- colour: a dedicated one-pole LP whose cutoff is redrawn per click, 800-3000 Hz (log-uniform);
- source: `click = env * rng()` (decaying noise, so no two clicks are the same).
- Clustering for free: with probability `pc` the next click is scheduled 1-15 ms later (a "sub-click").

*Layer C, crashes (near flashes)*: event clock with **exponential** intervals `-ln(u)/rateC` (one `log` per flash):
- strokes per flash: 1 + geometric (p = 0.35 → mean ~2.9, cap 6);
- gap between strokes: 20-120 ms, log-uniform;
- stroke envelope: attack 0.5-2 ms, decay τ 8-60 ms; first stroke strongest, each later stroke ×(0.4-0.9) random;
- optional rumble tail after the last stroke: low-passed noise, τ 80-300 ms, 0.2× the stroke level;
- colour per flash: SVF band-pass, centre 500-2500 Hz log-uniform, Q 0.6-1.5 (one EraSvf retuned at flash start; per stroke re-randomise the centre ±30 %);
- level per flash: log-normal, median per table, σ 6 dB (`exp` of a sum of 3 uniforms, once per flash).

| Parameter | Sweet | Gritty | Notes |
|---|---|---|---|
| Storm activity wander | 0.01 Hz, range 0.3-1 | 0.03 Hz, range 0-1 | 0 = storm gone |
| Crackle rate (×S) | 3 /s | 40 /s | Poisson |
| Crackle Pareto alpha | 2.5 (tame) | 1.5 (spiky) | low alpha = Class A small-A |
| Crackle level vs envelope | -40 dB median | -22 dB | |
| Crackle decay τ | 0.2-0.8 ms | 0.3-2 ms | |
| Crackle colour LP | 700-1800 Hz | 800-3000 Hz | per click |
| Sub-click probability | 0.1 | 0.4 | clustering |
| Crash mean interval (exponential) | 25 s (or off) | 5 s | never a fixed grid |
| Strokes per flash | 1-2 | 1-6 | geometric |
| Stroke gap | 30-100 ms | 20-120 ms | log-uniform |
| Stroke decay τ | 8-25 ms | 15-60 ms | |
| Crash colour BP centre | 500-1200 Hz (distant, dull) | 600-2500 Hz | per flash |
| Crash level (median, σ 6 dB) | -30 dB | -10 dB | enters before the AGC so it ducks the tail |

Cost: ~5 RNG calls per sample at most, 2 one-poles, 1 SVF, per-event `log`/`pow`/`exp`.

---

## 3. Heterodyne whistles

**What is real.**
- A whistle is the difference between two carriers in the passband. Adjacent AM channels are 9/10 kHz apart, so that beat is filtered out by the receiver's ~3 kHz audio cut. [S, US 4,192,970] Audible whistles come from **off-channel or co-channel stations** (shortwave broadcasters and "split" frequencies), from **receiver birdies** (internal oscillator harmonics) and **image** stations, and from **power-supply spurs**. [S for birdies/SMPS; I for the rest]
- **Two off-air carriers beat at a nearly fixed pitch**, because tuning the receiver shifts both carriers equally. Broadcast carriers are held to ±20 Hz (US AM) [S, 47 CFR 73.1545], so co-channel beats are often **sub-audio to tens of Hz: a throb or warble of the level**, not a tone. Audible tones need a station a few hundred Hz to a few kHz off. [I]
- The pitch wobbles slightly with ionospheric Doppler (fractions of a Hz to a few Hz; F.1487 spreads 0.1-10 Hz) and slow transmitter drift. [S for Doppler spreads; I for the audible wobble]
- The **level fades independently** of the wanted station (the interferer has its own path), and it comes and goes as propagation opens and closes, typically at dusk/night. [I]
- A beat that **moves with tuning** happens when one of the two is internal to the receiver: an image (moves at twice the tuning rate) or a birdie, or with a BFO. Turning the dial then sweeps the whistle down through **zero beat** and back up: the classic "wheeeoo". [I; the RF Nomad manual describes slowly sweeping tuning to get "lots of heterodyne noises" [S]]
- An envelope detector makes a strong whistle intermodulate with the programme (rough, "monkey chatter" when sidebands overlap). [S for monkey chatter, US 4,192,970]

**Why round 2's whistle sounds fake:** constant level, always on, and its pitch roams 600 Hz-2.4 kHz, which no off-air beat does. It reads as a theremin.

**Recipe: a whistle "voice" with lifetimes.** [M]
- State: off / on. Off-time exponential (mean per table); on-time 4-30 s log-uniform.
- At each birth: pick pitch `f0` 400-2500 Hz log-uniform (or, 30 % of the time, a **sub-audio throb**: 2-15 Hz, rendered as a level warble of the hiss + signal, depth per table, not as a tone).
- Pitch while alive: `f = f0 + d1 + d2`, `d1` = smoothed noise ±0.5-3 Hz at ~0.3 Hz (Doppler), `d2` = very slow drift ±5-20 Hz over the lifetime.
- Level while alive: its **own Rician fading** (section 4 generator, Doppler 0.1-0.5 Hz) × a fade-in/out of 1-3 s at birth and death × the stage's station envelope `E`.
- Slight phase noise: add `0.002*f0` worth of filtered noise to the increment (kills the "test tone").
- Optional 2nd harmonic at 0.1-0.2× (detector nonlinearity). Keep it below the LP.
- **Whoop event** (see section 5): during a drift-off, a separate short voice with `f(t) = |k·(t - t0)|`, sweeping e.g. 3 kHz → 0 → 2 kHz in 0.4-1.5 s, level shaped by the stage's LP (it naturally thins at the top).

| Parameter | Sweet | Gritty | Notes |
|---|---|---|---|
| Whistle present | never (or mode ≥ 2 only) | yes | owner may want none at the sweet end |
| Mean gap between whistles | 60 s | 15 s | exponential |
| Lifetime | 4-12 s | 8-30 s | log-uniform |
| Pitch | 600-1800 Hz | 300-2500 Hz | fixed per life |
| Pitch wobble (Doppler) | ±0.5 Hz @ 0.2 Hz | ±3 Hz @ 0.5 Hz | smoothed noise |
| Slow drift per life | ±5 Hz | ±20 Hz | |
| Own fading (Rician K) | 6 dB, 0.1 Hz | 0 dB (Rayleigh), 0.4 Hz | dips to silence often |
| Peak level vs envelope | -36 dB | -20 dB | today: fixed 0.012 × 4 env |
| Sub-audio throb share | 50 % | 30 % | 2-15 Hz, depth 10-30 % |
| Max voices | 1 | 2 | never more |

---

## 4. Imperfect tuning and fading flutter

**What is real.**
- **Watterson / F.1487:** each path's gain is a complex Gaussian process with a Gaussian Doppler spectrum; its envelope is Rayleigh (or Rician with a steady part). F.1487 channels (frequency spread = 2σ): low latitude quiet 0.5 Hz / moderate 1.5 Hz / disturbed 10 Hz; mid latitude 0.1 / 0.5 / 1 Hz; high latitude 0.5 / 10 / 30 Hz; differential delay 0.5-7 ms. CCIR 520: good 0.5 ms / 0.1 Hz, poor 2 ms / 1 Hz, **flutter 0.5 ms / 10 Hz**. [S, MathWorks confirms LM = 2 ms, 1.5 Hz; the full table from F.1487 Annex 3 and the TAPR paper]
- So: **slow fading 0.1-1 Hz** is the normal mid-latitude sound; **flutter fading 10-30 Hz** is the polar/auroral "watery, fluttery" sound, a fast random tremolo with a grainy roughness. [S for rates, I for the description]
- **Selective fading** happens because the two paths' delay difference makes a comb that rotates; the deepest notch sweeps. When the notch sits on the **carrier**, the carrier drops 10-15 dB under the sidebands, the signal is effectively over-modulated and the detector distorts. [S, Fallows] So distortion should **come and go with the fade**, not sit constant.
- **Mistuning** (carrier off the centre of the IF filter): one sideband is cut more than the other, highs drop, the carrier sits on the filter skirt so it is weakened (more over-modulation distortion), and any frequency wobble of the carrier (drift, Doppler, a microphonic LO) is turned into **level flutter** by the skirt (slope detection). [I, standard receiver theory]
- **LO drift:** old valve sets drift hundreds of Hz to kHz during warm-up, slowly; **microphonics** (a knock, a speaker vibrating the valves) add brief fast wobble. [I]

**Recipe: one fading generator, used three times.** [M]
- `fadeGen(σHz, K)`: two RNG noises (I, Q) at block rate → each through a 2-pole low-pass at ≈ σ Hz (close enough to Gaussian) → `g = |K_lin + I + jQ| / norm`. Interpolate per sample to avoid zipper. Cost: 2 RNG + 4 one-poles per block, 1 sqrt per block.
- Use it for: (a) the **station** flat fade (replaces `wander()` for `flat_`), (b) each whistle's level, (c) optional flutter episodes (same generator, σ jumped to 5-15 Hz, K lowered, for 2-10 s, Poisson-rare).
- Rician K limits depth so a tail never vanishes: K = 10 dB ≈ ±2-3 dB swings; K = 0 dB ≈ Rayleigh, deep but rare nulls (clip the gain at -12…-20 dB).
- **Detune** `δ(t)` in 0..1: a slow random walk (wander at 0.03-0.1 Hz) plus the drift-off events of section 5. It drives:
  - LP cutoff × (1 - 0.35 δ) (dull when off-tune);
  - over-mod factor `m_eff = m_ · (1 + 0.6 δ) · (1 + 0.5·(1 - g_carrier))` so distortion rises with detune and with fades;
  - slope flutter: level × (1 + δ · fl · n_fast), `n_fast` = noise smoothed at 3-12 Hz, `fl` per table.

| Parameter | Sweet | Gritty | Notes |
|---|---|---|---|
| Station fade spread σ | 0.08 Hz | 0.5 Hz | F.1487 mid-lat quiet → moderate |
| Station fade K | 10 dB (±2 dB) | 3 dB (±6-10 dB) | floor -12 dB / -20 dB |
| Flutter episodes | off | mean gap 45 s, length 2-10 s | σ 5-15 Hz, K 3 dB |
| Detune wander | 0.03 Hz, range 0-0.15 | 0.1 Hz, range 0-0.5 | |
| LP drop at full detune | -10 % | -35 % | |
| Over-mod rise with fade/detune | small | as formula | distortion breathes with the dip |
| Slope flutter depth fl | 0.02 | 0.15 | rate 3-12 Hz, smoothed noise |

---

## 5. Scanning through the dial; a "drift off and back" gesture

**What is real.** [I, plus RF Nomad [S] for the sweeping heterodynes]
Moving between stations you hear: the station thinning and distorting as it slides to the filter skirt, the AGC opening so **hiss rushes up**, whoops as carriers (via images/birdies/BFO) sweep through zero beat, fragments of other stations' audio, then the next station locking in. The whole gesture lasts 0.5-3 s per station.

**Would an occasional "drift off and back" be musical on a reverb tail?** [I/M] Yes, plausibly, as a rare event at the gritty end: it is a natural "breath" in a long tail (dull, noisier, a whoop, then the tail returns clear), similar to how the dip is loved. Risks: (1) any regular timing becomes a gimmick → Poisson with a minimum gap; (2) on short material it can sound like a fault → only fire when the tail has been sustained (`E` above a threshold for > 2 s), never during the first second after an onset; (3) "other stations' audio" needs material: the only cheap source is a short, heavily band-passed read from the reverb's own delay memory (a different, earlier part of the tail), which crosses stage boundaries; **leave it out of v2** and decide by ear later.

**Recipe.** [M]
- Event clock: exponential, mean per table, min gap 15 s.
- Shape: `δ` bump with raised-cosine up 0.3-1 s, hold 0-1.5 s, down 0.4-1.2 s; peak δ 0.4-1.0 (random); 30 % of events overshoot and come back from the other side (a second smaller bump).
- During the bump: station level × (1 - 0.8·δ) (the AGC then lifts hiss and static automatically), LP falls via δ, over-mod rises via δ, and **one whoop** voice fires when δ crosses 0.5 (section 3).

| Parameter | Sweet | Gritty | Notes |
|---|---|---|---|
| Drift-off events | off | mean 40 s, min gap 15 s | needs owner's ear first |
| Duration | n/a | 0.7-3.5 s | random |
| Peak detune | n/a | 0.4-1.0 | |
| Station drop at peak | n/a | -6…-18 dB | AGC fills with hiss |
| Whoop sweep | n/a | 2-3 kHz → 0 → 1-2 kHz in 0.4-1.5 s | -24 dB peak |

---

## 6. Antenna and man-made interference

**What is real.**
- **Power-line microgap buzz:** discharges at the voltage peaks, so **every half cycle: 100 Hz (50 Hz mains) or 120 Hz (60 Hz)**, ~8 ms apart on a 60 Hz grid; on AM a buzz loudest at the fundamental and weaker with frequency; often **several sets of peaks** from several sources; disappears in wet weather. [S, ARRL NE Division] Corona (high-voltage lines) is a frying hiss. [S, ARRL]
- **Modulation ("tunable") hum** [I, standard radio servicing]: in AC-powered sets, hum can modulate the *station* rather than add to it, so it is heard only on a station and goes silent between stations. On a reverb this is ideal: a multiplicative 100/120 Hz ripple on the wet, which disappears with the tail on its own.
- **Switch-mode supplies:** harmonics of the switching frequency plus broadband noise; demodulated as a tone that **drifts with temperature and load**. [S, EP 0666637; Samlex] This, not a station beat, is the one whistle whose pitch slowly slides; it is rough and buzzy rather than a clean sine. [I]
- **Bad antenna** [I]: weaker signal → AGC higher → more hiss and local noise relative to the station; a loose or corroded connection adds crackle and **brief dropouts** (5-50 ms) when it moves; hand capacity or a swaying wire gives slow level wobble.
- P.372's man-made noise model is Gaussian only; it notes impulsive noise can be far higher for short periods. [S] So the impulsive layers above carry the "character".

**Recipe.** [M]
- **Modulation hum (cheap, recommended):** `x *= 1 + h·r(t)`, `r` = full-wave-rectified sine at the mains frequency minus its mean (`|sin(π·f_m·t)| - 0.637`), so its strongest line is 100/120 Hz with soft harmonics. Phase accumulator + `fabs(sin)`. Depth `h` with slow random wander (×0.5-1.5 at 0.05 Hz).
- **Microgap buzz (gritty only):** pulse train at 2·f_m; each pulse = decaying noise burst (τ 0.3-1 ms) through a one-pole LP at 1-2.5 kHz; per-pulse jitter ±0.3 ms and amplitude ×(0.4-1.2); intermittent via a Markov on/off gate (on 0.3-5 s, off 3-40 s, exponential); optional second source: same at a fixed random phase offset and lower level. Enters before the AGC.
- **SMPS birdie (optional):** the whistle voice with `d2` drift raised to ±50-150 Hz over 5-20 s and its tone replaced by a narrow pulse (`phase < 0.08 ? 1 : 0` minus mean) through the stage LP: buzzy, not pure.
- **Loose-antenna dropout:** Poisson rate per table; each event: station gain → 0.1-0.4 for 5-50 ms with 2 ms ramps, plus 2-6 crackle clicks forced during it.

| Parameter | Sweet | Gritty | Notes |
|---|---|---|---|
| Mains frequency | 50 Hz (100 Hz ripple) | same | owner's choice: 50 (EU) or 60 (US) |
| Modulation hum depth h | 0.5 % | 4 % | multiplicative, silent with the tail |
| Microgap buzz | off | -34 dB vs envelope, intermittent | periodic: keep low |
| Buzz on/off gate | n/a | on 0.3-5 s / off 3-40 s | Markov |
| SMPS birdie | off | optional, 1 voice, rare | |
| Antenna dropouts | off | 1 per 20-60 s | 5-50 ms, with crackle |

---

## Flags: tonal or periodic things that can turn metallic or gimmicky

| Risk | Why | Keep it organic by |
|---|---|---|
| Whistle | pure tone on a reverb = test tone | lifetimes, own Rician fading, Doppler wobble, phase noise, ≤ 1 voice at the sweet end, or none |
| Mains buzz / hum | 100/120 Hz harmonic comb, metallic | multiplicative hum only at the sweet end; buzz intermittent, jittered, low |
| Flutter fading | fixed-rate LFO = tremolo | Gaussian-filtered noise (never a sine), Rician floor, episodes only |
| Two-path comb | flanger | keep the single dip; any comb ≤ 0.5 gain |
| Crash timing | uniform intervals feel rhythmic | exponential intervals + storm-activity wander |
| Whoop / drift-off | a "radio effect" cliché if frequent | Poisson, min gap, only on sustained tails, owner listens first |
| All of it at once | radio pastiche | at the sweet end use only hiss, crackle and the fades; add whistles, buzz, drift-offs toward gritty |

---

## 7. Radio stage v2: combined signal flow (in order)

Inputs: wet L/R, `t` (0 sweet … 1 gritty), noise mode. Block-rate processes update in `set()`; per-sample work in `process()`. All additive noise is **mono** (a radio has one speaker) and scaled by the **pre-fade** station envelope `E` so silence stays silent.

1. **Station envelope** `E`: existing `slow_` (0.5 s), measured on the input **before** any fade. [unchanged]
2. **Block-rate controllers** (all from one EraRng):
   - storm activity `S` (0.01-0.03 Hz wander);
   - station fade `g = fadeGen(σ = 0.08→0.5 Hz, K = 10→3 dB)`, floor -12→-20 dB; flutter episodes at the gritty end;
   - detune `δ` = slow wander (0.03→0.1 Hz, 0→0.5) + drift-off bumps;
   - dip centre: **existing `notchA_/notchB_` wander, unchanged** (0.1→0.5 Hz, 300 Hz-3 kHz). Optional tie: dip depth × (1 + 0.5·(1 - g)), so the dip digs deeper while the station is down;
   - whistle voice state, crash/flash scheduler, buzz gate, dropout scheduler.
3. **Modulation hum**: `x *= 1 + h·r(t)`, h 0.5→4 %. (Multiplicative, before the transmitter clip.)
4. **Transmitter / selective-fade over-modulation**: `m_eff = m_·(1 + 0.6δ)·(1 + 0.5(1 - g))`, m_ 0.9→1.4 (existing clip formula).
5. **Propagation (signal only)**: `x *= g · dropout · (1 - 0.8δ_bump) · (1 + δ·fl·n_fast)`; then the **existing peaking dip** (−4→−18 dB, Q 1.5→3). Today the noise goes through `flat_`; in v2 it must not.
6. **Add receiver-side noise** `n·E·4` (the existing scaling), all mono:
   - hiss: triangular white → tilt LP 1.2→2.2 kHz, -44→-26 dB;
   - crackle: Poisson 3→40 /s × S, Pareto α 2.5→1.5, τ 0.2-2 ms, per-click LP 700-3000 Hz, -40→-22 dB;
   - crashes: exponential mean 25→5 s × S, 1-6 strokes 20-120 ms apart, τ 8-60 ms, per-flash BP 500-2500 Hz, log-normal level -30→-10 dB;
   - whistle voice(s): 0→2, lifetimes, own fading, -36→-20 dB; whoop during drift-offs;
   - microgap buzz (gritty only, intermittent), SMPS birdie (optional).
7. **IF/audio filter** (existing): HP 200→400 Hz; 4th-order LP 4.5→2.4 kHz, cutoff × (1 - 0.35δ). The noise passes through it, so it is coloured like the station.
8. **AGC** (existing): 5 ms attack, 0.25 s decay (try 0.4-0.6 s at the sweet end for a lazier breathe), max boost +6→+15 dB, target −20 dBFS. Because the noise now sits after the fade, the AGC lifts it as `g` and the dip fall, and big crashes duck the tail for a moment: the two signature behaviours of a real set.
9. **Out** to the Neve base / output as today.

**What stays and how the new parts relate to the dip.** The dip and its wander stay exactly as tuned. The new station fade `g` is the "weather"; it (a) modulates the dip depth slightly (optional), (b) drives the over-modulation distortion, and (c) through the AGC sets the hiss and static level. Whistles fade on their own independent generator (a different path), which keeps them from pumping in lock-step with the dip.

**Size and cost estimate** [I]: ~120-160 lines replacing ~95 today; extra state ~40 floats + 3 EraSvf/one-poles; per sample ≈ 6-8 RNG calls, 4-5 one-poles, 1 SVF, 1-2 `sin`/`fabs`; per event a handful of `log`/`pow`/`exp`. No tables. Flash: likely +1.5-3 KB of code over today's stage; check with `make -C firmware all-variants`.

**Suggested listening order for a page** [M]: (1) order fix + softer hiss only; (2) + new crackle/crashes; (3) + whistle voices; (4) + flutter episodes and detune; (5) + drift-off gesture; (6) + hum/buzz. Each step at sweet, middle and gritty, same chord stab and pad, so the owner can stop where it stops sounding like a radio and starts sounding like an effect.
