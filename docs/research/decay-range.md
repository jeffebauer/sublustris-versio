# DECAY's range: longer tails before the hold (28 Oct 2026)

**Built as SPEC v1.0.53 (ADR 0034): the owner picked B on every card of `renders/decay_range`.** The page was rendered with a renderer-only hook (`IV_DECAY_PLATE_MAX`, `IV_DECAY_HALL_MAX`, `IV_DECAY_TOP`, `IV_DECAY_GAMMA`, at ed11f65), removed at the build; `tools/pages/decay_range.py --built <59ce205's iv_render>` checks the build against the page.

The owner said: "i often find myself wanting longer lush decays, but the knob biases towards smaller decays until you hit the 3 oclock mark and it turns into perpetual tails. I'd like more usable range so i can access longer decays without hitting the perpetual holds. I'd still like the held tails to kick in at the 3 oclock position. This applies to plate and hall".

Tags: **[S]** a source says it; **[M]** we measured it; **[I]** our inference.

## Where 3 o'clock is

- [I] The Versio's pots read linearly. On a typical 300° pot, which runs from 7 to 5 o'clock, each clock hour is 0.1 of the travel, so 3 o'clock is 0.80. The firmware stretches 0.025-0.975 to 0-1 (`firmware/PotEndStops.h`). On the module that puts 9 / 10 / 11 / noon / 1 / 2 / 2:30 / 3 o'clock at DECAY 0.184 / 0.289 / 0.395 / 0.5 / 0.605 / 0.711 / 0.763 / 0.816. The Plugin's knob is JUCE's default rotary of about 288°, which puts 3 o'clock at about 0.81.
- Today the finite curve ends at 0.80 and the tail turns endless at 0.85, which is about 3:10 on the module. 3 o'clock itself (0.816) falls inside the crossover, where PLATE rings about 18 s and HALL about 37 s. Every version keeps the endless point at 0.85. The held-tail governor and the ducking still grow across 0.80-0.85, so the hold "kicks in at 3 o'clock" exactly as it does now.
- Not checked: the Versio's actual pot part and its rotation angle.

## How established units lay out DECAY

| Unit | Longest finite decay | Taper | How it reaches endless |
|---|---|---|---|
| Strymon BigSky | [S] 20 s on most machines, 50 s on Cloud, 10 s on Spring ([manual p.4](https://www.strymon.net/manuals/BigSky_UserManual_RevD.pdf)) | not documented | [S] off the knob: a footswitch hold, Infinite Sustain or Freeze |
| Strymon NightSky | [S] "less than one second to nearly infinite sustain" | not documented | [S] the knob only nears endless; a separate INFINITE switch |
| Valhalla FutureVerb / VintageVerb | [S] 0.2-70 s ([FutureVerb controls](https://valhalladsp.com/2025/10/03/valhallafutureverb-the-controls/); VintageVerb per MusicRadar's review) | [S] "biased towards the shorter decays" | none |
| ValhallaPlate | not found | [S, search snippet only] DECAY "warped": most of the travel covers normal plate times, the last quarter much longer ones | none |
| ValhallaSupermassive | feedback 0-100 % | [S] "larger values will result in longer decays" | [S] 100 % = loop gain 1, in some modes, mod depth 0 ([infinite tips](https://valhalladsp.com/2021/05/19/tips-and-tricks-infinite-supermassive/)): the very end of travel |
| Eventide Blackhole | not found | not found | [S] FEEDBACK "clockwise to Infinite", input still open |
| Mutable Clouds / Rings (MIT) | [S] loop gain 0.35-0.98 (`set_time(0.35 + 0.63 * x)`, clouds/dsp/granular_processor.cc, rings/dsp/part.cc) | linear in gain | [S] Clouds: a FREEZE fade; Rings: none |
| Mutable Elements (MIT) | [S] `reverb_time = 0.35 + 1.2 * amount`, capped at the knob's top | linear | [S] a hard freeze past a CV threshold (space ≥ 1.75) |
| Valley Plateau (GPL, concept only) | knob 0.1-0.9999 | [S] a quadratic ease toward gain 1, 1 − g = (1 − x)² ([repo](https://github.com/ValleyAudio/ValleyRackFree)) | [S] a separate Hold button |
| Make Noise Erbe-Verb | — | [S] "0 – 120% reflection gain. Infinity at Full CW." Typical settings: Room 12:00, Plate 1-2:00, Hall 11-2:00 ([manual pp. 4, 11](https://www.makenoise-manuals.com/erbe-verb/erbe-verb-manual.pdf)) | [I] if the gain is linear, unity sits at about 83 % of the travel |
| Lexicon 480L | [S, via Costello] the 224 / M200 go to about 70 s | not found | [S] "When RTM is moved to full scale, infinite reverb is switched on": the last step only |
| Meris Mercury7, Walrus Slö | not found | not found | [S] a held button maximises sustain |

General writing:
- [S] The just-noticeable difference of reverb time is 5 % (ISO 3382-1, after Seraphim 1958). Other studies measured 6-39 % (Blevins et al. 2013). Either way the step we hear grows with the time itself, which is why DECAY knobs are exponential (log T60).
- [S] Costello (Valhalla) writes that a loop gain of 1 is endless, that filters and modulation in the loop dull or detune a held tail, and that very long tails pile up the loop's own noise.

## What this means for DECAY

1. [I] Big reverbs that keep endless on the knob do it in one of two ways. Some have a long finite top: 20-70 s, with Valhalla and Lexicon at 70 s and Strymon Cloud at 50 s. Our tops are 12 s (PLATE) and 25 s (HALL). Others approach endless gradually: Plateau's ease and Erbe-Verb's feedback that runs past unity. None we found has a 5 % sliver between a modest top and endless, which is today's layout and what the owner is hearing.
2. [I] Keep the exponential (log T60) finite stretch, so each step sounds alike, and keep the short end short.
3. [I] To approach endless gradually, extend today's crossover. 1/T60 falls in a straight line to 0 at 0.85, and starting that line earlier (0.70 rather than 0.80) turns the last 2 o'clock to 3 o'clock into a stretch where the tail keeps lengthening (×1.7 at 2:30, ×4-5 at 3) before it holds. Plateau's (1 − x)² does the same in loop-gain terms.
4. [I] Endless keeps everything it has today: the floor governor, the ducking, the Neve's evens fading, MASSIVE's high share. They are all keyed by `governorWeight` (0.80-0.85), which doesn't move.

## The versions (`renders/decay_range`)

The tail length (T60) at each knob position on the module. PLATE is the same at any SIZE; HALL is shown at SIZE full (×0.87 at SIZE 0.8, the page's).

```
                                9       10       11     noon        1        2     2:30        3
PLATE A  today              0.9 s    1.4 s    2.1 s    3.4 s    5.2 s    8.2 s     10 s     18 s
PLATE B  longer tops        1.1 s    1.9 s    3.4 s    5.9 s     10 s     19 s     25 s     44 s
PLATE C  gentle run-in      1.0 s    1.6 s    2.7 s    4.5 s    7.6 s     13 s     21 s     53 s
PLATE D  both               1.1 s    2.0 s    3.6 s    6.5 s     12 s     22 s     34 s     88 s
HALL  A  today              1.8 s    2.8 s    4.4 s    6.9 s     11 s     17 s     21 s     37 s
HALL  B  longer tops        2.2 s    3.8 s    6.7 s     12 s     21 s     37 s     49 s     88 s
HALL  C  gentle run-in      2.0 s    3.3 s    5.6 s    9.4 s     16 s     27 s     43 s    110 s
HALL  D  both               2.2 s    4.0 s    7.3 s     13 s     24 s     43 s     69 s    176 s
```

- **A** is as built: exponential over 0-0.80, then a crossover over 0.80-0.85.
- **B** keeps today's shape with higher tops: PLATE 30 s and HALL 60 s at 0.80 (`IV_DECAY_PLATE_MAX=30 IV_DECAY_HALL_MAX=60`). This is the "long finite top" family (Valhalla, Lexicon, Strymon Cloud).
- **C** keeps today's tops but reaches them at 0.70, then runs a wide, gentle crossover over 0.70-0.85 (`IV_DECAY_TOP=0.70`). This is the "gradual approach" family (Plateau, Erbe-Verb).
- **D** does both: PLATE 20 s and HALL 40 s at 0.70, then C's run-in.
- The level make-up is unchanged as a function of the tail's length (`map::decayLevelMakeupForRate`). A tail as long as one of today's gets today's gain, and a longer one gets none, as today's crossover does. [M] While the chord plays, the long settings of B-D sound 1-2.5 dB louder than A. The page matches that level while playing.

## Recommendation

[I] The research favours a gradual approach to endless over a longer cliff, which means C or D. D gives the most usable long-tail range: HALL 24 s at 1 o'clock and 43 s at 2, close to Strymon Cloud's 50 s and Valhalla's 70 s, with endless still at 3 o'clock. The cost is less room for medium tails (noon about 2× today). C leaves the bottom half almost as it is. B is the simplest change, but it keeps today's step into the hold. The owner chooses by ear on the page.

Before any of these ships, the level make-up must decide what a tail longer than today's top does. It could be trimmed back to today's top level, or the 1-2.5 dB could be kept as part of the bigger sound. A SPEC changelog line, an ADR 0020 amendment and the T60 test tables (§8 "T60 within 10 % of the mapping") would follow.
