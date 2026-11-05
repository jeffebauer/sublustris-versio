# DECAY's curve: where the long tails and endless sit (6 Nov 2026)

Branch `claude/proto-decay-curve-01af2c`, off `claude/proto-wear-taper-78e1da` (v1.0.70). A prototype; not pushed, not merged.
Page: `renders/decay_curve` in the main checkout. Hook: `IV_DECAY_CURVE` (A-D), Renderer only, firmware byte-identical.

**The owner on the module (5 Nov 2026):** "the decay seems to bias towards shorter decays until noon, then 12-3 controls
longer decays, which is a small zone to dial in longer decays with precision… going past 3 o'clock shifts into endless
tails, but is there any granularity in effect past 3?… Should we shift endless to fully CW or last 'hour' of the knob?"

Today (`core/params/Mappings.h`, SPEC §3, ADR 0034): T60 is exponential over 0-0.80 (×1.7 per hour of the knob), a
crossover over 0.80-0.85, then endless over 0.85-1. The last 45° (an hour and a half) of the knob does nothing new.

## 1. What established units do

A web survey (a research subagent's report; "per search excerpt" means a search engine's summary of the page, not a
reading of it, because valhalladsp.com, manualslib and several PDFs refused automated fetches).

| Unit | Where infinite sits | Zone | Finite part | Source |
|---|---|---|---|---|
| Make Noise Erbe-Verb | DECAY fully CW | "0 - 120% reflection gain. Infinity at Full CW": if linear, unity is at ~83 % of travel, so a self-oscillating zone above it | a feedback gain, curve undocumented | [manual p.6](https://www.manualsdir.com/manuals/698358/make-noise-erbe-verb.html?page=6) |
| NE Desmodus Versio (our hardware) | REGEN from noon up | the top half of the knob: 100 % at noon, "more and more aggressive infinite regeneration" above, input ducking past 3 o'clock | 0-100 % feedback over the first half | [manual](https://noiseengineering.us/manuals/desmodus-versio/), [NE blog](https://noiseengineering.us/blog/desmodus-versio) |
| Valhalla Supermassive | FEEDBACK 100 %, the end stop | a single point | feedback %, with DELAY | [Infinite tips](https://valhalladsp.com/?p=567197) (per search excerpt) |
| Valhalla VintageVerb | none | – | 0.2-70 s, RT60 in seconds, "biased towards the shorter decays" | [MusicRadar](https://musicradar.com/reviews/tech/valhalladsp-vintageverb-574692) |
| Eventide Blackhole | top of FEEDBACK: Infinite, then Freeze further CW; Freeze also a footswitch | two named end positions | feedback % | [reference guide](https://cdn.korn.eu/documents/product/272812_m.pdf) (per search excerpt) |
| Eventide H9 | named values Infinity / Freeze past the longest decay; often on the HotSwitch | discrete values | per algorithm | [H9 algorithm guide](https://com-eventide-drupaluserfiles.s3.amazonaws.com/download/file/H9AlgorithmGuide.pdf) (per search excerpt) |
| Strymon BigSky | a footswitch hold (Infinite or Freeze per preset); DECAY can also read "INFNTE" | separate | RT60 in seconds, e.g. Hall 0.5-20 s, Cloud 1-50 s | [manual](https://www.manualslib.com/manual/963033/Strymon-Bigsky.html?page=4) (per search excerpt) |
| Strymon NightSky | the Hold button | separate | LENGTH "<1 s to nearly infinite" | [plugin manual](https://www.strymon.net/manuals/NightSky_Plugin_UserManual_RevA.pdf) (per search excerpt) |
| Meris Mercury7 | hold SWELL to "maximize Space Decay sustain" | separate | undocumented | [manual](https://www.analoguehaven.com/meris/mercury7/manual.pdf) (per search excerpt) |
| MI Clouds / Rings / Beads | never infinite: the reverb's per-pass gain is linear in the knob and capped below 1 (Clouds 0.35 + 0.63 × amount, amount ≤ 0.95); FREEZE freezes the grain buffer, not the reverb | – | linear in gain | [Clouds code](https://github.com/pichenettes/eurorack/blob/master/clouds/dsp/granular_processor.cc), [Rings part.cc](https://github.com/pichenettes/eurorack/blob/master/rings/dsp/part.cc), [Beads manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/beads/manual/) |

**What the field does:**
- **Two camps.** Knobs in seconds (BigSky, NightSky, VintageVerb, the H9's reverbs) keep infinite *off* the knob: a hold
  switch, or a named value past the longest time. Knobs in feedback % (Supermassive, Erbe-Verb, Blackhole, Desmodus)
  put it at or near fully CW.
- **Where infinite is a zone, the zone does something.** Desmodus spends half its knob above unity, but that half keeps
  changing (more aggressive regeneration, then ducking); Erbe-Verb's 100-120 % self-oscillates harder. Nobody spends a
  wide zone on a sound that doesn't change, which is what our 0.85-1 is.
- **Infinite and Freeze are separate states** on Eventide and Strymon, as they are here (endless follows the playing,
  FREEZE latches a pad).
- **Seconds-based knobs lean short** (VintageVerb's own wording). The owner hears ours the same way.

**The literature:**
- The just-noticeable change in reverberation time is a near-constant *fraction*: Seraphim (1958) about 4 %, ISO 3382
  quotes 5 %; later studies 6-39 % with the stimulus ([Tsolias & Davies, Forum Acusticum 2014](https://salford-repository.worktribe.com/OutputFile/1491017),
  [Blevins et al.](https://digitalcommons.unl.edu/archengfacpub/63)). So equal steps of the knob should be equal *ratios*
  of T60: an exponential (log-T60) map, which is what we have. The question is only how the ratio per degree is spread.
- Jot's loop gain, g = 10^(−3·M/(T60·fs)) ([JOS, PASP](https://ccrma.stanford.edu/~jos/pasp/Achieving_Desired_Reverberation_Times.html)):
  a knob linear in gain (Mutable's) crowds the long tails into the last few degrees, which is why seconds-based units map
  T60, not gain. No paper on choosing a decay knob's curve was found.
- Pot tapers: an audio taper is ~10 % at mid-travel against linear's 50 % ([Wikipedia](https://en.wikipedia.org/wiki/Potentiometer));
  real log pots are piecewise approximations. Our pots are linear; the taper is in the firmware's maths.
- A tiny range between "long" and "self-oscillating" at the end of a knob is hard to ride by hand
  ([KVR](https://www.kvraudio.com/forum/viewtopic.php?p=7503884)). Ours can't self-oscillate (the floor governor and the
  hold), but a single point at the end stop would be easy to miss, or to wobble off.

## 2. Pot reach and jitter on the Versio

- **The end stops** (`firmware/PotEndStops.h`, 4 Oct 2026): every pot's reading within 2.5 % of either end snaps to it,
  and the rest is stretched linearly. So "fully CW" is already a zone: any raw reading ≥ 0.975 reads exactly 1.0. M0 found
  the pots reach "within 2 %" of their ends (`docs/m0-hardware-check.md`), so there's about half a percent of margin
  between the worst pot seen and the snap.
- **The knob path** (`firmware/main.cpp`, `AudioCallback`): libDaisy's 2 ms smoothing, then a 10 ms one-pole
  (`kKnobSmoothMs`), then a deadband: a new value reaches the Engine only when it has moved 0.001 (0.003 for SIZE and
  TIME) from the last one sent, or lands exactly on 0 or 1. The Engine glides DECAY over 80 ms (ParamSpec).
- **Jitter** (the 26 Oct rack check, `docs/tasks-done-archive.md`; `docs/research/review-2026-10-29.md` item 10): a still
  knob's raw reading wobbles under 0.001 peak-to-peak (~65 LSB); after the 10 ms one-pole ~0.0002. Playing audio pushed
  SIZE's reading a little more, but never past ±0.0005.
- **CV:** each jack is summed with its knob in hardware, so CV adds to the knob's position and clips at the rails: the
  knob fully CW plus any positive CV still reads 1.0; CV alone can reach endless only if it gets the sum past 0.975.

**So, the zone width.** Endless must cover the snap (1.0) and enough below it that jitter can't drop a held endless back
into the crossover. Jitter is ~0.0002 after smoothing, so the electrical margin needs almost nothing; the musical margin
is the hand: a player turning "all the way" should land on endless without hunting. After the snap:
- **D's 2 % (0.98-1) = about 6° of the knob, plus the snap's 2.5 % raw: ~4.4 % of the physical rotation (~13°).** Safe
  electrically (jitter is ~100× smaller), and a knob turned to the stop lands there.
- **B/C's 5 % (0.95-1) = 15°, ~18° physically.** Easy to find by eye as well as by the stop.
- Today's 15 % (0.85-1) = 45°.
I'd not go below 2 %: a knob that tops out short of 0.975 raw (one M0 found "within 2 %") would then sit in D's crossover
(0.95-0.98) and play a 60 s tail that holds once you stop, which is close but not endless. A rack check of the owner's
DECAY pot at fully CW (the M0 build's serial readout) settles it.

## 3. The audit: what depends on where endless starts

Every one reads DECAY on today's scale (0.80 / 0.85 and the numbers keyed to them). The prototype remaps the knob onto
today's scale before anything reads it (`decayCurveKnob`, applied in `Engine::setParam`), so each one moves with endless
and stays tied to the same T60. In each version, the knob's finite top f maps to today's 0.80 and its endless start e to
today's 0.85.

| What | Where | Keyed to | How it moves in B / C / D |
|---|---|---|---|
| The T60 curve | `Mappings.h` `decayRate`, `massiveDecayRate` | `kDecayFiniteTop` 0.80, `kDecayInfiniteAt` 0.85 | finite to f (0.92 / 0.92 / 0.95), endless from e (0.95 / 0.95 / 0.98); shortest and longest T60s unchanged |
| Endless follows your playing (v1.0.61) | `Engine::applySpace` `decayFin = min(decay, kDecayFiniteTop)`, `endlessHold = governorWeight(decay) × holdMix_` | 0.80 / 0.85 | plays as the finite top from f, the hold comes in across f-e; checked below |
| The floor governor's weight | `governorWeight`, `floorFin`, `floorHold` | 0.80-0.85 | across f-e |
| The floor's ease (ADR 0008, v1.0.56) | `floorWeight`, `voicing::kGovernorEaseFrom` 0.70 | 0.70-0.85, t³ | from the knob position that now plays today's 0.70 T60 (PLATE 17.5 s, CANYON 80 s at SIZE noon): B 0.805, C 0.76, D 0.775; still a t³ glide into e |
| The decay level make-up | `decayLevelMakeup`, `kDecayLongTrimFrom` {0.15, 0.35} / `To` {0.85, 0.75}, the rise cap below 0.80 | today's knob | at the same T60s as today, letting go into e |
| HALL's and CANYON's output trims | `hallSizeLevelTrim`, `massiveDecayLevelTrim`, the twin's trim | `governorWeight` | across f-e |
| CANYON's high share | `massiveHighRatio` | `governorWeight` | across f-e |
| The Neve's even harmonics fading out | `loopEra.even`, the twin's `tEra.even` | `governorWeight` | across f-e |
| The held tail's mellowing and calm (v1.0.58) | `endlessTop`, `endlessCalm_`, `tailCaptured_` | `governorWeight ≥ 1` | from e |
| HALL's tail prototype tilt | `setHallTailPrototype` (Renderer only) | `governorWeight` | from e |
| CANYON's SIZE floor and its level trim | `massiveDecayRate`'s `lo` (6 mean lines), `massiveDecayLevelTrim` = sqrt(its rate / `decayRate(v, 2)`'s) | today's knob scale (both rates at the same remapped value) | the same trim at the same T60; 1 again from endless (rate 0) |
| CANYON's built-in attack | `massiveNormalAttackSeconds(decay, size)`, 0.5 → 0.2 s over the whole knob | the whole knob | the same attack at the same T60; the endless zone's last bit (today 0.85-1) is squeezed into e-1 |
| HALL's floor depth (v1.0.57) | `kHallFloorLongFromSeconds` … | T60 in seconds | unchanged per T60 |
| The radio's band on MOTION | `radioBand` | MOTION only ("whole at infinite DECAY too", v1.0.61) | not affected |
| SHIMMER | `shimmerAmount_` (TIME), into the tank | no DECAY dependence | not affected; its endless is the tank's |
| HALL/CANYON endless trim | branch `claude/rack-levels`: `spaceTrimDb × floorW` | `floorWeight` | moves with the remap, since it reads `floorW` |
| Input stop | branch `claude/input-stop`: `cushDb … governorWeight(smoothed(Decay))` | `governorWeight` | moves with the remap |
| Tests | `test_fdn` (finite top, `floorWeightRamp` 0.70 / 0.85), `test_plate_freeze`, `DecayV1052.h`, `test_set_args` (`param(Decay) == 0.8`) | the map functions' own scale | pass as they are (they test the map functions, which don't change); a port needs a test of the remap itself |
| Goldens | `iv_test_golden` | renders at fixed DECAYs | a port moves every golden whose DECAY isn't 0, so they need `IV_GOLDEN_UPDATE` with a SPEC line |
| Sweetspot tool | `tools/sweetspot.py` | sweeps DECAY 0-1 | re-run after a port: the dead zone past 0.85 should be gone |
| Firmware | `main.cpp` | no DECAY special cases (knob path only) | not affected |

**A note for the port:** applying the remap in `setParam` changes what `Engine::param(Decay)` returns, and
`test_set_args` reads it back. Ported, the remap belongs where DECAY's smoother reads its target (the knob stays the knob,
the smoother and everything after it see today's scale), or as one inline at `smoothed(ParamId::Decay)`'s callers.

## 4. The versions

Each keeps every SPACE's shortest and longest finite tail. The finite part is today's exponential (B), or bent with
`x + b·x·(1−x)` (C, D): steeper at the short end, gentler at the long end, with noon kept where it is today. A quadratic
bend, not a power, so its slope never goes flat at the top (a power curve's would, just before the crossover).

- **A today:** finite to 3 o'clock (0.80), crossover to 0.85, endless from ~3:30 (45° of endless).
- **B stretched:** finite to 0.92, crossover to 0.95, endless from 4:30 (15°). Everything else comes later, so noon is
  shorter (HALL 8 s, not 12).
- **C bent:** B with b = 0.328: noon as today (HALL 12 s), HALL's 15-60 s over 110° of knob (today 77°).
- **D late:** C's bend (b = 0.395), finite to 0.95, endless from 0.98 (~4:50, 6°): HALL's 15-60 s over 119°.

T60 in seconds at each hour (`tools/pages/decay_curve_table.py`; 7 o'clock = 0, 5 o'clock = 1; HALL at full SIZE, CANYON
at SIZE 0 (S0), noon (S½) and full (S1); "endless" plays as the finite top while you play, then holds):

| SPACE | ver | 7 | 8 | 9 | 10 | 11 | 12 | 1 | 2 | 3 | 3:30 | 4 | 4:30 | 4:50 | 5 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| PLATE | A | 0.4 | 0.7 | 1.2 | 2.0 | 3.5 | 5.9 | 10 | 17 | 30 | endless | endless | endless | endless | endless |
| PLATE | B | 0.4 | 0.6 | 1.0 | 1.6 | 2.6 | 4.2 | 6.7 | 11 | 17 | 22 | 27 | endless | endless | endless |
| PLATE | C | 0.4 | 0.7 | 1.3 | 2.2 | 3.7 | 5.9 | 9.2 | 14 | 20 | 24 | 28 | endless | endless | endless |
| PLATE | D | 0.4 | 0.7 | 1.3 | 2.3 | 3.7 | 5.9 | 9.1 | 13 | 19 | 22 | 26 | 30 | endless | endless |
| HALL | A | 0.8 | 1.4 | 2.4 | 4.0 | 6.9 | 12 | 20 | 35 | 60 | endless | endless | endless | endless | endless |
| HALL | B | 0.8 | 1.3 | 2.0 | 3.3 | 5.2 | 8.4 | 13 | 21 | 34 | 43 | 55 | endless | endless | endless |
| HALL | C | 0.8 | 1.5 | 2.6 | 4.5 | 7.4 | 12 | 18 | 28 | 40 | 48 | 56 | endless | endless | endless |
| HALL | D | 0.8 | 1.5 | 2.6 | 4.5 | 7.5 | 12 | 18 | 27 | 38 | 45 | 52 | 60 | endless | endless |
| CANYON S0 | A | 2.1 | 3.5 | 5.8 | 9.6 | 16 | 26 | 44 | 72 | 120 | endless | endless | endless | endless | endless |
| CANYON S0 | B | 2.1 | 3.3 | 5.1 | 7.9 | 12 | 19 | 29 | 46 | 71 | 88 | 110 | endless | endless | endless |
| CANYON S0 | C | 2.1 | 3.7 | 6.3 | 11 | 17 | 26 | 40 | 58 | 82 | 97 | 113 | endless | endless | endless |
| CANYON S0 | D | 2.1 | 3.7 | 6.4 | 11 | 17 | 26 | 39 | 56 | 78 | 91 | 105 | 120 | endless | endless |
| CANYON S½ | A | 5.0 | 7.5 | 11 | 17 | 25 | 36 | 54 | 81 | 120 | endless | endless | endless | endless | endless |
| CANYON S½ | B | 5.0 | 7.1 | 10 | 14 | 20 | 28 | 40 | 56 | 79 | 94 | 112 | endless | endless | endless |
| CANYON S½ | C | 5.0 | 7.8 | 12 | 18 | 26 | 36 | 50 | 68 | 89 | 101 | 115 | endless | endless | endless |
| CANYON S½ | D | 5.0 | 7.9 | 12 | 18 | 26 | 36 | 50 | 66 | 86 | 97 | 108 | 120 | endless | endless |
| CANYON S1 | A | 12 | 16 | 21 | 28 | 38 | 51 | 67 | 90 | 120 | endless | endless | endless | endless | endless |
| CANYON S1 | B | 12 | 15 | 20 | 25 | 33 | 42 | 54 | 69 | 89 | 101 | 114 | endless | endless | endless |
| CANYON S1 | C | 12 | 17 | 23 | 30 | 39 | 51 | 64 | 79 | 97 | 106 | 116 | endless | endless | endless |
| CANYON S1 | D | 12 | 17 | 23 | 30 | 39 | 51 | 63 | 78 | 94 | 103 | 111 | 120 | endless | endless |

Knob travel for the long tails: HALL 15-60 s A 77°, B 89°, C 110°, D 119°; CANYON 30-120 s A 105°, B 121°, C 143°,
D 152°; PLATE 8-30 s A 73°, B 84°, C 106°, D 114°. Per hour above noon: today ×1.7, C ×1.4-1.5 (closer to the T60 JND's
way of hearing: each hour a similar-sounding step).

**CANYON's SIZE floor (added 6 Nov at the owner's question).** SIZE raises CANYON's shortest tail to at least six trips
round its mean line (`massiveDecayRate`, `kMassiveMinPasses`, ADR 0023): 2.1 s at SIZE 0, 5 s at noon, 12 s at full. It
**stretches DECAY from that floor to 120 s; it doesn't clamp it.** Measured on a noise burst (EDC −5 to −25 dB),
against the maths: SIZE full, DECAY 0 / 0.1 / 0.2 / 0.3 / 0.4 / 0.5 = 11.9 / 15.8 / 21.8 / 28.6 / 38.3 / 50.8 s
(maths 12.0 / 16.0 / 21.3 / 28.5 / 37.9 / 50.6); SIZE 0 2.1 / 3.5 / 5.8 / 9.5 / 15.9 / 26.4. So **no hour of DECAY
is flat at any SIZE, today or in any version**; at big SIZEs each hour is a smaller step:

| CANYON SIZE | shortest | T60 × per hour: A | B | C (mean) | D (mean) | flat hours |
|---|---|---|---|---|---|---|
| 0 | 2.1 s | 1.66 | 1.55 | 1.55 | 1.53 | 0 |
| noon | 5.0 s | 1.49 | 1.41 | 1.41 | 1.40 | 0 |
| full | 12 s | 1.33 | 1.28 | 1.28 | 1.27 | 0 |

Even full SIZE's ×1.28 an hour is far above the ~5 % a listener can hear (§1), so every hour does something. What the
owner may hear as "little happening" on the left at big SIZEs is that 12 s and 16 s are both a long canyon. Spreading
from the floor is already how it works, and it keeps ADR 0023's reason (a huge canyon never rings briefly as separate
echoes), so no extra version was needed. C and D's bend puts a little more of the change on the left (C at full SIZE:
12, 17, 23 s at 7, 8, 9 o'clock against A's 12, 16, 21), and a little less at 3-4 o'clock (C: 97 → 116 s over the last
hour of finite, ×1.2). The page's card 20 is CANYON's DECAY sweep at full SIZE.

## 5. Gates (6 Nov 2026, this branch)

- ctest: **"100% tests passed out of 23"** (`build-dc/ctest.log`). The Plugin wasn't built: `libs/JUCE` is empty in this
  worktree.
- `tools/same_render.py`, hook off, against v1.0.70 (`4b95b8a`) built in a scratch worktree: **SAME 60/60**.
- `make -C firmware all-variants`: **byte-identical** to `4b95b8a` (`cmp`): release 111,844 B, m0test 83,556, profile 131,028.
- `tools/pages/decay_curve_checks.py` (every SPACE, SIZE 0.6, WEAR 0.3): **all pass**.
  - Clicks, DECAY swept from 0.1 under the finite top to fully CW and back, and flicked across the crossover in 50 ms:
    9.2-11.4 dB in every version (A 9.2-11.0); the music's own transients read 15-20.
  - Holds at the endless start, 0.005 past it and 0.005 under it, MOTION noon / full left / full right, and frozen at the
    endless start: every endless and frozen hold flat (no window above the first + 1 dB); the endless numbers are the
    same in every version, to the tenth of a dB, because the endless start plays today's 0.85. Just under it (in the
    crossover) the tail fades as it should: HALL −7.5 dB over 50 s in A, −12 in B-D (0.005 is a larger share of a
    narrower crossover).
  - Endless follows the playing at the new point: while the pad plays, the endless start is within 0.02 dB of the
    finite top (PLATE, HALL) and −0.71 dB (CANYON, the same as today); held, it stays within 0.4 dB over 50 s.

## 6. Flash, to port

Measured: the remap compiled into the release build with each version's constants (a scratch copy of v1.0.70):
- **release: B +152 B, C +160 B, D +160 B** (111,844 → 112,004 B; the budget is 112,640, so ~640 B left after).
- **profile: doesn't fit** (it has 44 B). The profile build ignores the knobs, so a port leaves the remap out of it
  (`#if !IV_MODE_PROFILE`), 0 B there.
- A cheaper port may exist: B is two constants if the 0.70 ease and the long trims are re-keyed by hand (a few bytes,
  but every keyed number moves, and section 3's list is the checklist); C and D need the bend either way.

## 7. Open questions (for the owner, in the morning)

1. Which version (the page's pick).
2. If C or D: is noon right where it is (HALL 12 s), or should the bend give the long end even more?
3. Endless zone: D's 6° is safe electrically, but does a knob turned to the stop always land there on the owner's
   module? A rack read of DECAY fully CW on the M0 build settles it.
4. Should the endless zone *do* something, as Desmodus's does (e.g. the held tail mellowing faster further CW)? Not
   prototyped; it would be new behaviour.

## 8. Not checked

- On the module: the feel of the narrower crossover by hand, and the pot's reach at fully CW.
- The Plugin (no JUCE in the worktree) and `auval`.
- The 10-minute holds of `energy_check.py` (the 80 s holds here show no growth; since every endless setting is today's
  0.85, the 10-minute behaviour is today's).
- SHIMMER and BLOOM at the new endless point were not rendered separately (neither reads DECAY's position).

## 9. SIZE, measured (no prototype, no page)

The owner (6 Nov): "Are our size floor and ceiling values appropriate? Is there anything there we should tune, including
the curve?" SIZE is exponential in every SPACE: PLATE 0.5-1.59 × Dattorro's tank; HALL 35-260 ms mean line, small HALLs'
T60 × 0.49; CANYON 0.35-2.0 s mean, predelay up to 100 ms.

### 9.1 The sweetspot check, SIZE in every SPACE (v1.0.70)

`tools/sweetspot.py --steps 21 --only PLATE:size,HALL:size,MASSIVE:size` (5 % steps, MASSIVE = CANYON). Step sizes are in
just-audible units (1 dB, 10 % of T60, a semitone of brightness, 15 % of arrival or peak time, 0.1 of width); a dead zone
is 20 % of travel under 0.25 a step, a cliff one step over 3 and over 3× the median.

| SPACE | Step sizes, CCW → CW (each 5 %) | Dead zones | Cliffs | Biggest steps and what moves |
|---|---|---|---|---|
| PLATE | 2.7 3.0 0.4 1.3 2.1 2.4 0.9 0.8 2.1 1.5 2.9 2.2 1.4 1.9 2.0 0.6 3.0 3.9 0.7 1.5 | – | – | 50-55 % and 80-90 %: when the wet peaks (2.9, 3.0, 3.9); loudness, T60 and brightness smooth |
| HALL | 2.5 1.5 2.2 1.3 1.9 0.7 0.7 1.9 2.3 1.9 2.8 2.9 0.6 3.6 3.3 1.5 1.2 1.5 1.6 1.6 | – | – | 50-75 %: arrival and peak time (2.8-3.6); T60 5.0 → 10.2 s with SIZE (the small-HALL scale, by design) |
| CANYON | 1.2 2.4 0.6 1.4 2.7 0.9 1.1 3.4 1.3 1.6 2.7 0.7 2.4 1.0 1.5 1.9 3.4 4.2 1.4 1.0 | – | – | 35-40 % and 80-90 %: brightness (3.4, 4.2: the tail darkens at big SIZEs); loudness −2.5 dB from noon to full |

- **No dead zones and no cliffs** in any SPACE: SIZE does something all the way along.
- **PLATE is still uneven around 1 and 4 o'clock** (0.50-0.55 and 0.80-0.90), as the old runs said, but only in *when the
  wet peaks*: the loudest early reflection changes as the tank grows. Its loudness, length and brightness step evenly. A
  peak-time step of 3-4 units is a 50-60 % shift of a ~10 ms moment; probably not heard as a jump on music.
- **CANYON's darkening at 3-4 o'clock** is the largest step anywhere (4.2 semitones of tail centroid over 5 % of the
  knob). Worth listening for on a SIZE sweep; not a cliff by the tool's rule.
- Caveat: the tool reads T60 from a click within 11 s, so CANYON's long tails (20-50 s at DECAY noon) aren't measured
  right there; its T60 column is not used above.
- `docs/sweetspot.md` was not rewritten (it covers every knob; this run was SIZE only, in `build-dc/sweetspot_size.md`).

### 9.2 Echo smoothness and metallic ringing at SIZE 0 to full

`tools/pages/size_texture.py`: a single click, DECAY noon, MOTION noon, WEAR 0, TIME 0, NORMAL, 100 % wet.
- **NED** is Abel & Huang's normalised echo density over the first 300 ms. 1 is noise-like; well under 1 is separate echoes.
- **mix** is when NED first reaches 0.9.
- **kurt** is the median excess kurtosis in 10 ms windows. 0 is Gaussian; high is spiky.
- **ring** counts narrow tail peaks 1.5-4.5 s after the click: the share of bins over +10 dB against the running median, and the highest peak.

| SPACE | SIZE | arrives | NED | mix | kurt | ring ‰ (max) |
|---|---|---|---|---|---|---|
| PLATE | 0 / ¼ / ½ / ¾ / 1 | 4-14 ms | 0.79 / 0.69 / 0.63 / 0.57 / **0.51** | 140 / 220 / – / – / – ms | 1.4 / 3.3 / 5.6 / 10.4 / **14.1** | 0 (7.4-8.3 dB) |
| HALL | 0 / ¼ / ½ / ¾ / 1 | 1-5 ms | 0.97 / 0.96 / 0.96 / 0.94 / **0.89** | 20-70 ms | 0.04 / 0.14 / 0.24 / 0.19 / **0.64** | 0 (4.5-7.0 dB) |
| CANYON | 0 / ¼ / ½ / ¾ / 1 | 122-245 ms | 0.90 / 0.96 / 0.97 / 0.97 / 0.94 | 0-60 ms | 0.63 / 0.32 / 0.12 / 0.07 / 0.29 | 0 (4.2-6.3 dB) |

- **HALL's top end is a little grainier than its middle, but not grainy.**
  - At full SIZE the onset is slightly less dense (NED 0.89 against 0.96, kurtosis 0.64 against ~0.2).
  - It's still mixed by about 30 ms, and that's measured from the click.
  - Its tail has the lowest ringing of any SPACE: no peak above +4.5 dB at SIZE ¾ and full.
  - So the Engine.cpp note "no less grainy" holds as "slightly more grainy", and only in the first ~30 ms.
  - The 260 ms mean is far larger than a real hall's mean free path (~60-100 ms), but it doesn't measure metallic.
  - **Neither a lower ceiling nor more diffusion at the top is worth a page on these numbers.**
- **PLATE is the grainy one, and it gets grainier with SIZE.**
  - From noon up its first 300 ms never mixes (NED 0.63 → 0.51, kurtosis 5.6 → 14).
  - The early part is a train of distinct spikes, not a wash, and it's the spikiest early part of any SPACE.
  - This is the plate's input diffusion spread over a bigger tank. It's not ringing: tail peaks are at most +8.3 dB, none over +10.
  - A plate is expected to start a little sparse, so it may be character, not a fault.
  - Before any page, measure it against the owner's Valhalla Plate takes, which `ingest_reference.py` can read.
- **CANYON is smooth at every SIZE** (NED 0.90-0.97). Its built-in swell spreads the onset over 120-245 ms.
- **Flutter is inconclusive.** I tried the early envelope's detrended autocorrelation. It gives 0.1-0.5 with no pattern across SIZE: the highest peaks are PLATE noon 0.44 and HALL noon 0.51, and the lags sit mostly at the 14-16 ms edge of the search. That looks like the measure's noise, not an echo repeating. I wouldn't use it as evidence either way.
- **Ringing:** no SPACE or SIZE has a narrow tail peak above +10 dB at DECAY noon. Longer DECAYs weren't measured here. The fuller-tail work (branch `claude/fuller-tail-6dcc8d`) measured reedy lines in long tails by another method.

### 9.3 Which SIZE ends deserve a page

- **PLATE's big end: the only candidate.**
  - Its onset is the spikiest of any SPACE, kurtosis 14 at full SIZE against HALL's 0.6.
  - Next step: the comparison against Valhalla Plate takes.
  - Then a page only if the owner hears the large PLATE as "grainy" or "clicky" on drums.
  - What a page would try: more input diffusion as SIZE grows.
- **HALL's ceiling: no page.** It measures smooth and the least ringing. A lower ceiling would mainly shorten its longest tails, because SIZE also scales HALL's T60.
- **CANYON: no SIZE page.**
  - The 2.5 dB loudness drop and the darkening from noon to full belong with the level work: `claude/rack-levels`, `claude/wet-loudness-8e55d6`.
  - Its floor follows SIZE without flattening DECAY (§4).
- **The floors** (PLATE 0.5×, HALL 35 ms and its 0.49 T60 scale, CANYON 0.35 s) show nothing to fix: no dead zones or cliffs, and HALL at SIZE 0 is the densest of all (NED 0.97).

## 10. The audit again, for the port (v1.0.72, on v1.0.71)

The owner picked C. The port (`map::decayScale`, ADR 0048) applies the remap where DECAY's smoother reads its target, not in `setParam` as the prototype did. Every reader of DECAY in `core/dsp/Engine.cpp` goes through `smoothed(ParamId::Decay)` or `heldDecay_` (taken from it), so every row of §3 reads the scale.

| Row of §3 | On v1.0.71 |
|---|---|
| T60 curve, follows the playing, governor weight, floor ease, make-up and trims, output trims, CANYON's high share, the Neve's even harmonics, held tail's mellowing and calm, HALL's tail prototype, CANYON's SIZE floor, CANYON's attack, HALL's floor depth | all still read the scale; unchanged |
| **New: HALL's fuller ease** (`Engine::applySpace`, `fuller = 2 - governorWeight(decay)`, ADR 0047) | reads the scale, so it eases across the knob's 0.92-0.95 with endless |
| The radio's band, SHIMMER, firmware's `main.cpp` | not affected |
| `claude/rack-levels` (`floorW`), `claude/input-stop` (`governorWeight(smoothed(Decay))`) | move with the remap when merged, since they read the same values |
| Tests | §3 said they would pass as they are. Those testing the map functions did, but seven Engine-level tests pin a tail by DECAY, and setting the knob now means another tail. They set `decayKnobFor(scale)`. `test_set_args` passes unchanged, since `param(Decay)` is the knob |
| Goldens | moved once, for v1.0.72 |

Proof that no row was missed: at a knob position the port is bit-identical to v1.0.71 at that position's scale value (126/126, `tools/pages/decay_port_checks.py`), at MOTION noon and both extremes, which covers the HALL ease.
