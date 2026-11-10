# The two overloads left on B: the landing, and a flip during a SIZE crossfade, 11 Nov 2026

Research only (read-only), on `proto/flipflick-size` (`fcecb00`, in `.claude/worktrees/flipflick-size`). The data comes from `m3-flipflick-B-lap.log` (B on) and `m3-flipflick-B-OFF-lap.log` (the same build with B off). Anything marked *inferred* has not been measured.

## The problems

The usual 19 lines and the 9 unfreeze lines are 0 late in both laps (worst 97.6 %). Two things are still late:

| Line (SHIMMER, the owner's knobs) | B on | B off |
|---|---|---|
| **1. The landing:** PLATE -> CANYON, SIZE flicked end to end 0.1 or 0.3 s after the switch | 100.8-102.5 %, 29 / 32 / 111 late; one line 98.8 %, 0 late. Max at **3,374-3,386 ms** | 109-115 %, 1,201 late each |
| **2. XFFLIP:** SIZE 0 -> 1, then SPACE switched so its silent point lands 0.2 / 0.8 s into the SIZE crossfade | C -> P: 114.6 % / 148 late (+200), 109.7 % / 265 late (+800). P -> C: 125.5 % / 337 late (+200), 98.3 % / 0 (+800). Max at **104-148 ms** after the switch | the same, to within 0.1 point |

"Max at" counts from the block where the switch moves. The flip's silent point (`morphFlip`) comes 50 ms later (`kSpaceFlipSeconds`, `SpaceVoicing.h:231`).

## Problem 1: what runs at ~3.4 s after the switch

### The timeline (B on, PLATE -> CANYON)

| Time after the switch | What happens | Where |
|---|---|---|
| 0.05 s | The silent point. CANYON's two FDNs are **reset** (`fdn_.reset()`, `lowFdn_.reset()`: a lazy clear, `fill = 0`). The PLATE becomes the old tank. `ffHold`'s snap sets `ffSize_` to SIZE as it is now. | `Engine.cpp:1597-1612`, `Fdn::reset`, `ffHold` `Engine.cpp:504-521` |
| 0.15-0.5 s | SIZE flicks. `ffHold` keeps the new tank on `ffSize_`, so nothing in CANYON moves. Half of SHIMMER's heads rest (into CANYON with SHIMMER on, the owner's pick C). | `Engine.cpp:516-520`, `Engine.cpp:1669` |
| ~3.05 s | `morphX_` reaches 1 and the old PLATE's gain reaches 0. At the end of that chunk `morphOld_ = -1` (`Engine.cpp:1733-1735`). **The old PLATE is fully stopped:** `morphRunOld` only runs while `morphOld_ >= 0` (`Engine.cpp:1366`). | `morphAdvance` `Engine.cpp:1680-1686`, `morphRunOld` |
| next control tick, ~3.05 s | In the same tick, two things happen together: `morphAdvance` calls `setLighter(false)` (the morph has ended, so the resting heads may come back), and `applySpace` -> `ffHold` returns the latest SIZE (one jump, already through the hysteresis; nothing else is added). | `Engine.cpp:1158-1159`, `1669`, `506-512` |
| ~3.10 s | SIZE has rested 50 ms (`xfRest_`), so CANYON's crossfade starts: group 1 (lines 0 and 4 of each FDN) reads twice for 0.3 s, then groups 2, 3 and 4. It ends at ~4.3 s. | `Fdn.cpp:485-561` |
| 3.05-3.45 s | **SHIMMER's resting heads come back one by one,** each at the start of its next grain (a grain lasts 0.4 s, `kShimmerWindowSeconds`). So by ~3.4 s every head is playing again. | `Shimmer.h:94-98, 218, 255` |
| **3.37-3.39 s (the peak)** | Every SHIMMER head is back while group 1's double reads are still running (they end at ~3.40 s). | |

### Why a landing costs more than an ordinary SIZE move in CANYON

**CANYON's lines are still filling when SIZE lands.** Each FDN line is a power-of-two ring of 131,072 samples at the tank's 24 kHz (`kFdnMaxLineSeconds` 2.97 s, `SpaceVoicing.h:17`; `Fdn::memoryFloats`), so it counts as `full()` only **5.46 s after the reset**. The firmware lap says so itself: it runs every line for 6 s untimed "past the lines' first trip after the reset" (`main.cpp`, `RunMorphLine`). B lands about 3.05 s after the reset, so every landing falls inside that window.

A crossfading line that isn't full can't use the fast crossfade path:
- `Fdn::lineXf` starts with `if (!line.full()) return false;` (`Fdn.cpp:1005`), so the line drops to `lineSlow`, the sample-by-sample path.
- The old early taps (CANYON has 4 per line) take their fast stream only on a full line (`w.full() && IV_XF_FAST`, `Fdn.cpp:859`). Otherwise they go through `tapsOld`, which reads both samples of every tap every sample.
- Commit `b4d0aac` (27 Oct) says this was left on purpose: "filling lines keep lineSlow".

**What that costs on the module** (`m3-sizespike-ab.log`, the A / B of exactly this switch, MASSIVE, 2 lines x 2 tanks crossfading at a time as now): **the slow paths cost about 5 points** more than the fast ones (79.2 -> 73.8 %, 82.7 -> 78.0 %, 79.8 -> 74.5 %).

**Plain SIZE moves in CANYON + SHIMMER on full lines** have been lapped. They are the unfreeze lines in this very lap, at the owner's knobs: 0.8 -> 0.4 at 93.7-94.2 %, and twisting 0.5 <-> 0.9 at **95.1 %**, all 0 late. Add the slow paths' ~5 points and you get ~100 %, which is the landing's 100.8-102.5 %. *(Inferred: the heads coming back during group 1 explain why the peak sits at its very end. PLATE -> HALL lands at 95.8-97.5 %, with no resting heads to return and a lighter tank, and HALL's lines are filling too.)*

**B's landing does nothing extra.** It is one SIZE jump through the ordinary crossfade. The overload comes from three things meeting in one tick:
- (a) the landing runs on a 3-s-old tank whose lines are still filling,
- (b) the morph's end lets SHIMMER's heads back in that same tick,
- (c) CANYON + SHIMMER is the heaviest space.

The +300 ms Z0->1 line (98.8 %) is the same thing just below the line. Timing and grain luck decide which side it falls on.

## Problem 2: what overlaps when SPACE flips during a SIZE crossfade

`Engine.cpp:1119` makes a switch wait only for a morph or a reshape (`morphOld_ >= 0 || placeOn_`), **not for a SIZE crossfade**. B (`ffHold`) only holds a SIZE that moves *after* the flip. A crossfade already running belongs to the tank that becomes the old one, and `morphFlip` keeps "everything it has" (`Engine.cpp:1594-1612`). So the old tank carries on crossfading while it fades out, with the new tank starting up beside it. That is why B makes no difference here.

**PLATE -> CANYON (+200 ms): 125.5 %, 337 late**
- **The old PLATE's crossfade is longer than its 0.3 s fade.** `Plate::process` runs the slow sample loop `processXf` for as long as `xfSince_ >= 0` (`Plate.cpp:554`), which lasts until `xfEnd_`. That point is the 0.3 s fade plus the longest element, so that "every sample from before the jump [is] read" (`Plate.cpp:449-453`). At SIZE 1 the longest element is d1, 4453/29761 x 1.59 = 0.24 s, so the slow loop runs about **0.55 s** in all, every element read twice for the first 0.3 s.
- The silent point lands 0.2 s in, which leaves ~0.35 s of it: **337 late blocks is that remainder.** At +800 the PLATE has finished before the flip: 0 late.
- **On top:** the new CANYON's own start-up. Every flip into CANYON peaks about 0.1 s after the switch even with no SIZE move: 97.5 % at 105 ms (REFLIP P->C->P), 98.3 % at 104 ms (XFFLIP +800), 98.6 % at 104 ms (flicked there and back). Half the heads rest only after two control ticks (`morphLightWait_`).
- **Cost of the piece:** the old PLATE's crossfade adds about **+27 points** to that standing ~98 % peak.

**CANYON -> PLATE: 114.6 % / 148 late (+200), 109.7 % / 265 late (+800)**
- The old CANYON keeps running its remaining groups. Its targets stay where SIZE put them, because `applySpace` now sets only the PLATE's: `Engine.cpp:697-714` against `715-790`. At +200 that is the rest of group 1 plus groups 2-4 (~1.0 s); at +800 the rest of group 3 plus group 4 (~0.4 s).
- The peak sits just before the group in flight ends: 136 ms after the switch (group 1 ends at ~150 ms) and 148 ms (group 3 ends at ~150 ms).
- **On top:** a fresh PLATE at full SHIMMER (the heads rest only into CANYON) and the morph. A flip into PLATE with no SIZE move peaks at 91-92.7 % (the B FLIPFLICK C->P lines, at 120-190 ms).
- **Cost of the piece:** the old CANYON's group in flight adds about **+17-22 points**.

**These costs don't add up as they do on one tank.**
- A CANYON crossfade on its own costs ~6-10 points (the 95.1 % twists against CANYON's ~85-89 % average). Inside a two-tank morph the same crossfade costs ~20.
- The new PLATE's own 0.3 s crossfade under an old CANYON did the same thing with B off: 110.3 % against 92.7 %, +17.6.
- *Inferred:* on the M7 the tanks are memory-bound. Two tanks plus double reads mean many more SDRAM streams at once, and interleaved streams cost about 3x a sequential one (round 6: 10 against 33 cycles a float).
- *Not explained:* why +800 has more late blocks than +200 with less crossfade left (265 against 148). *Inferred:* it depends on which lines are in the late groups, and on the new PLATE's reads going live as its lines fill over its first ~0.25 s. A profile SPLIT line would show it.

**The same holds for a switch during SIZE's 50 ms rest** (code reading, not lapped). The old tank's targets have already moved, so it starts its crossfade under the morph once SIZE rests. Any fix has to treat "pending" like "running": `Fdn::xfIdle()` (`Fdn.cpp:250-256`) already counts both.

## What established designs do

| Approach | Who | Source | Read |
|---|---|---|---|
| **A crossfade doubles the work while it runs.** Change a delay by fading from the old setup to the new; "Computational requirements are doubled during the cross-fade." Also, the new read can be "warmed up" before the fade. | J. O. Smith, *Physical Audio Signal Processing*, "Large Delay Changes" | [dsprelated.com/freebooks/pasp/Large_Delay_Changes.html](https://www.dsprelated.com/freebooks/pasp/Large_Delay_Changes.html) | directly |
| **Don't change again while a crossfade runs.** Read the old and new delays, crossfade, and hold any new change until the fade ends; then start the next. (A developer's answer, not a product.) | r/JUCE thread | [redlib mirror](https://redlib.hbubli.cc/r/JUCE/comments/1b105p6/changing_dspdelayline_time_in_realtime_without) | directly |
| **Spikes that meet are the danger:** with several occasional spikes, "you'd better hope you can predict how they all interact"; amortise rather than use bursty algorithms. Problem 1 is two returns (SIZE and the heads) in one tick; problem 2 is two transitions stacked. | Ross Bencina, *Real-time audio programming 101* | [rossbencina.com](http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing) | directly |
| **Spillover is paid for by doing less, or by bigger hardware.** The H9's Space has spillover "only as a result of reducing the complexity and delay size". The H90 applies spillover on every Program change. One forum post (the unit isn't named) says spillover "doubles the DSP and memory requirements, albeit temporarily". | Eventide forum | [eventideaudio.com/?p=175811](https://eventideaudio.com/?p=175811) (yesterday's note), [?p=172555](https://www.eventideaudio.com/?p=172555) | search summaries only |
| **Spillover keeps the old tail; nothing published on knobs during it.** BigSky's Spillover / Persist let tails fade over a patch change or bypass. Nothing says whether turning a knob reaches a tail that has spilled over. | Strymon | [strymon.net/bigsky](https://www.strymon.net/bigsky/), [Sound on Sound review](https://www.soundonsound.com/reviews/strymon-big-sky) | search summaries only |
| **The house rule: serialise, the lighter thing yields.** A second flip waits for the morph (ADR 0054). A freeze makes the old tank go in 0.3 s (ADR 0055). The twin room makes way for SIZE (pick A, `unfreeze-size-2026-11-10.md`). SIZE waits for the morph (pick B, `flipflick-size-2026-11-10.md`). | this project | ADRs 0054 / 0055 (branch `beta/player-feedback`) | directly |

I found no published unit or paper on what a size knob does while a spilled-over tail rings, or on a mode change asked for during a parameter crossfade. Meris, Valhalla and DAFx searches turned up nothing on either. *Inferred:* the established pattern is "one transition at a time, the next waits" (Smith's doubling, the JUCE rule, Bencina's spikes that meet). That is ADR 0054's "finish, then go", applied to SIZE crossfades.

## Options for problem 1 (the landing)

| Option | Cost | What the owner would hear | Flash (rough) |
|---|---|---|---|
| **1a. The fast paths on a line that has filled deep enough.** `lineXf` and the old taps' fast stream accept a filling line once its writes reach every read in the block (old and new, the taps, + block + 4). Then every read gives exactly what `ago()` gives, so the output is the same to the bit (check with `same_render` and the forced-slow build). At the landing the lines hold 3.05 s (73,200 samples), more than the longest read possible (2.97 s, 71,292). | Takes back the slow paths' ~5 points: about 101-102 % -> ~96-97 % (*inferred*, from `m3-sizespike-ab`). | **Nothing:** the same sound. | ~40-80 B (two depth tests instead of `full()`) |
| **1b. SHIMMER rests through the landing.** Half the heads keep resting until the landing's SIZE crossfade has ended (`setLighter` also while `!fdn_.xfIdle()` right after a morph into CANYON), so they come back after it. | Removes the heads' return from under group 1; *inferred* a few points. | The shimmer stays a little thinner for ~1.2 s more after a flip into CANYON when SIZE was moved, as during the morph (the owner already picked this for the morph). | ~20-40 B |
| **1c. Land later**, once the lines are full (5.5 s after the flip). | The fast paths come back on their own. | **SIZE dead for 5.5 s** after a flip: B's 3 s already risks feeling like a dead knob. | small |
| **1d. Land in smaller steps.** | **No help.** A crossfade group costs the same however far SIZE jumps, so more steps mean more crossfades of the same weight. | SIZE arrives in stages. | small |
| **1e. 8 groups for the landing.** | Halves the double reads; *inferred* ~2-3 points. | "Some slight staggering" (the owner on 8 groups); the landing takes 2.4 s. | small |

**Recommendation: 1a.** It is a sound-neutral speed-up, the kind the project already ships (`b4d0aac`), so it needs no listening page, only `same_render`, the forced-slow renders and a lap. If the lap still shows the landing over ~98 %, add 1b, which does need the owner's ear.

## Options for problem 2 (a flip during a SIZE crossfade)

| Option | Cost | What the owner would hear | Flash (rough) |
|---|---|---|---|
| **2a. The flip waits for SIZE's crossfade** ("finish, then go" for SIZE too). At `Engine.cpp:1119`, the switch also isn't read while the tank in use has a SIZE crossfade running or pending. For an FDN tank that is `!fdn_.xfIdle() || !lowFdn_.xfIdle()`; for the PLATE it is `plate_.crossfading()` or a target off its scale. The wait is **capped** (~1.3 s from the switch), so a SIZE under CV, whose crossfades never stop, can't hold SPACE for ever; at the cap the flip goes as now. | Removes the overlap: the old tank never crossfades under a morph. | **SPACE answers late, only when SIZE moved in the second before:** up to ~0.55 s from PLATE, up to ~1.2 s from HALL / CANYON. Then the morph runs as now. | ~40-90 B |
| **2b. The crossfade snaps to its end at the flip** (the old reads dropped, the waiting lines jumped). | Zero after the flip. | *Inferred:* **a click in the old tail:** at the flip the old tank is still at full level (the morph fades it over 3 s), and mid-fade the old reads carry half the sound. The PLATE's slow loop also carries its level corrections, which can't be cut. Hiding it with the 0.3 s fast drop brings the cost back for those 0.3 s. | ~40 B |
| **2c. The old tank stops reshaping.** At the flip the old tank's waiting groups never start: the group in flight finishes and the rest of the lines keep their lengths. | Cuts the CANYON overlap from up to 1.0 s to at most 0.3 s, **but not the peak**, which sits at the end of the group in flight. No help for the PLATE (one fade). | *Inferred:* inaudible: a tail fading out with some lines at the old SIZE. | ~30-60 B |
| **2d. A lighter overlap:** SHIMMER rests half its heads into PLATE too. | *Inferred:* a few points against the 17-27 needed. | A thinner shimmer during flips into PLATE. | ~10 B |

**Recommendation: 2a.** It is the house rule (ADR 0054: a change waits for the last), and the only way, short of a click, to remove a 17-27-point overlap. It also covers the switch during the 50 ms rest, and it leaves B in charge of SIZE moves after the flip. The cap keeps a CV on SIZE from locking SPACE out.

## A listening page (proposed)

The cards are at the owner's knobs: DECAY 0.7, DEPTH 0.8, MIX 1, WEAR / MOTION / TONE noon, ONSET SHIMMER, a chord held across.

- **Control:** `fcecb00` as built (B).
- **"SPACE waits for SIZE" (2a + 1a):** 1a sounds the same as the control, so this card judges 2a's late SPACE.
- **"Shimmer rests through the landing" (1b + 1a):** only if the lap after 1a needs it.

Moves:
- (i) SIZE 0 -> 1 flicked, then SPACE switched 0.2 s and 0.8 s later: PLATE -> CANYON, CANYON -> PLATE, HALL -> PLATE.
- (ii) SPACE switched, then SIZE flicked 0.1 s later: PLATE -> CANYON. Listen at ~3-4.5 s, where SIZE lands and the shimmer comes back.

Level-matched pick buttons.

**Lap lines that judge the CPU:**
- the usual 19 lines and the 9 unfreeze lines;
- the 15 FLIPFLICK lines (the four PLATE -> CANYON S2 lines judge 1a / 1b: they should land under ~98 % with 0 late);
- the 4 XFFLIP lines (they judge 2a: 0 late);
- new lines:
  - XFFLIP with the switch during SIZE's 50 ms rest;
  - XFFLIP PLATE <-> HALL;
  - XFFLIP HALL <-> CANYON (a reshape over a SIZE crossfade, never lapped);
  - SIZE twisting 0.5 <-> 0.9 in CANYON, then a switch (2a's cap).

A profile SPLIT on one landing line and the C->P +800 line would say where the points go.

**Flash:** 1a + 2a come to ~80-170 B, 1b ~20-40 B more. Release has 980 B to spare on this build and profile 4,212 B, so it fits in every variant (*inferred*; check with `make -C firmware all-variants`).

## Open questions

- **Does 1a alone bring the landing under the line,** or is the heads' return enough to need 1b? Only a lap says.
- **Is a SPACE switch that answers up to 1.2 s late acceptable by hand?** It happens only after a SIZE move. A page question for the owner.
- **HALL <-> CANYON during a SIZE crossfade** is a reshape (`placeOn_`, `setXfSpread`) through the same crossfade machinery. It has never been lapped. *Inferred:* it is lighter (one tank).
- **Why C -> P +800 has more late blocks than +200** (265 against 148); a SPLIT line would tell.
- Not checked: the desktop (no renders or builds in this research), and whether 1a's depth test is exactly bit-identical in `lineSlow`'s other uses (a forced-path render is the test).
