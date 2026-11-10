# SIZE flicked right after a SPACE flip (PLATE <-> CANYON), 10 Nov 2026

## The problem

The owner, on the fixed-room build: "switching between PLATE and CANYON (or CANYON to PLATE) and quickly moving the SIZE from one extreme to the other" crackles, with no freeze involved. SIZE moves "right after the flip", as a flick, and the crackle comes during it.

The laps reproduce it: `m3-flipflick-fixedroom-lap.log` and `m3-flipflick-control-lap.log`, at the owner's knobs (DECAY 0.7, DEPTH 0.8, MIX 1, WEAR/MOTION/TONE noon). Both builds give the same result to within 0.5 points, so it's in v1.0.79 and not something the fixed room or A added.

| Line (SIZE flicked end to end in 0.2 s) | Worst | Late blocks (1 ms each) |
|---|---|---|
| PLATE -> CANYON, SHIMMER, 0->1 or 1->0, +100 or +300 ms | 107.8-115.2 % | **1,201** every line |
| CANYON -> PLATE, SHIMMER, same four | 109.7-110.4 % | **300** every line |
| BLOOM (S1), P->C / C->P | 101.8 / 106.7-107.2 % | 111-167 / 300 |
| P->C, flicked there and back | 101.4 % | 7-8 |

The fixed room's other 28 lines are 0 late, worst 97.5 %.

## What runs at that moment (file:line, `proto/unfreeze-size`)

- **A flip between the PLATE and an FDN space is a two-tank morph.** `morphFlip` (`Engine.cpp:1518`, the `else` branch) starts the new tank from silence and keeps the old one ringing on its held settings (`morphRunOld`, `Engine.cpp:1652`). The old fades out over **3 s** (`morphAdvance`, `Engine.cpp:1631`), or over 0.3 s when a freeze makes it go (`morphFast_`). HALL <-> CANYON doesn't do this: it reshapes the one tank in place.
- **SIZE plays only the new tank** (`applySpace` sets `activeSpace_`'s tank, `Engine.cpp:654-679`), and it plays it as usual. Once SIZE rests 50 ms (`kSizeRestSeconds`), its lengths jump and the old reads fade out:
  - CANYON: four staggered groups of 0.3 s, **1.2 s** in all, with a quarter of the lines reading twice (`Fdn.h:143-159`).
  - PLATE: one **0.3 s** crossfade (`kSizeCrossfadeSeconds`, `SpaceVoicing.h:50`; `Plate.h:118`).
- **The late-block counts are exactly those lengths:** 1,201 ms and 300 ms on every line, whatever the direction or timing of the flick. The overload is the new tank's SIZE crossfade running while the old tank still rings. It lasts just as long as that crossfade, and it lands inside the morph's 3 s whenever SIZE moves within about 2.5 s of the flip.
- **Why it's worst into CANYON:** the new tank is CANYON's two FDNs (the heaviest space, SHIMMER's heads on top) reading a quarter of its lines twice, with the PLATE still running underneath. Into PLATE, the old tank is CANYON at full cost, under the PLATE's own crossfade.
- **Not covered by the laps (inferred):** PLATE <-> HALL is the same two-tank morph, so the same overload is likely but smaller (HALL is lighter). It needs lap lines.

## What established units and designs do

| Approach | Who | Source |
|---|---|---|
| **Spillover costs the main effect** | Eventide staff on the H9's Space: it "does have true spillover, but only as a result of reducing the complexity and delay size of the main effect". The two at once only fit because the new one does less. | [Eventide forum, via search summary](https://eventideaudio.com/?p=175811) (not read directly) |
| | H90: spillover is a global setting applied on every Program change. A third algorithm was suggested on the condition that spillover is off, i.e. the overlap is what takes the room. That was a user wish, not a feature. | same thread |
| **Spillover keeps the old tail, nothing more** | Strymon BigSky: Spillover and Persist let tails fade naturally when you change patches or bypass. Nothing published on what a knob does to the ringing tail. | [Sound on Sound, BigSky](https://www.soundonsound.com/reviews/strymon-big-sky) |
| **One algorithm at a time** | Meris Mercury7: Ultraplate / Cathedra on a toggle, a full DSP chip per channel. Nothing published on spillover between them. | [Mercury7 manual](https://www.analoguehaven.com/meris/mercury7/manual.pdf), [SOS review](https://www.soundonsound.com/reviews/meris-mercury7-reverb-pedal) |
| **Design for the worst case; no bursts** | Ross Bencina: amortise work rather than use "bursty" algorithms. Two transitions on top of each other are a burst. | [Real-time audio programming 101](http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing) |
| **The house rule: serialise** | ADR 0054 (a change waits for the last), ADR 0055 (the freeze makes the old tank go in 0.3 s), the owner's pick A on `renders/unfreeze_size` (the twin room makes way for SIZE). One heavy thing at a time; the lighter one yields. | `docs/adr/0054-space-morph.md`, `0055-freeze-on-flip.md`, `unfreeze-size-2026-11-10.md` |

No published unit describes what a size knob does during spillover. *Inferred:* the units that spill over either budget for two full algorithms (the H90's ARM platform) or make one of them cheaper (the H9).

## The options

| Option | Cost during the overlap | What you'd hear |
|---|---|---|
| **A. The old space makes way.** A SIZE move during a two-tank morph sends the old tank out in 0.3 s (the freeze's fast drop, `morphFast_`), and the new tank's SIZE crossfade waits until it has gone. | Zero once the old tank stops: the SIZE crossfade runs alone, as any SIZE move does. During the 0.3 s drop only the morph runs; the SIZE move waits. | **SIZE answers about 0.3 s late**, and only in the 3 s after a PLATE <-> FDN flip. **The old space's tail is cut short** to 0.3 s once you touch SIZE, instead of fading over 3 s under the new one. The same rule as the owner's pick for the twin, and as a freeze during a morph. |
| **B. SIZE waits for the morph.** The new tank holds the SIZE it started with until the old tank has faded on its own (up to 3 s after the flip), then the latest SIZE lands once. | Zero. | **The knob seems dead for up to 3 s after a flip**, then the whole move lands as one crossfade. The old space's tail fades as now. |
| **C. The new tank starts at the new SIZE.** While the new tank is very young (its first ~0.5 s, only fresh input in it), take SIZE straight in with no crossfade, so a jump of its lengths costs nothing. | Zero. | *Inferred:* a click or a smear on the first notes in the new tank, since its lines already hold some sound. It needs the click check, and it only covers flicks inside that window. A flick at 1-2 s still overlaps. |
| **D. A lighter overlap (the H9's way):** during a two-tank morph the old tank stops hearing the input and SHIMMER rests half its heads (as ADR 0054's pick C does into CANYON), so a crossfade fits. | Removes work. *Inferred:* not enough: at 115 % it needs about 17 points, and the old PLATE is the cheaper tank of the two in the worst case. | Little: the old tail stops taking in new playing a moment early. |
| **E. Fewer lines reading twice** (8 groups) during a morph. | About half the double reads. *Inferred:* not enough on its own, as for the unfreeze. | "Some slight staggering" (the owner on 8 groups, `Fdn.h:148-149`); a SIZE move takes 2.4 s. |

The owner turned down gliding SIZE (a pitch bend) for HALL and CANYON (ADR 0025), so it isn't on the list.

## Recommendation

**A, "the old space makes way",** following the house rule and the owner's own pick for the twin: a SIZE crossfade never runs while a second tank is still ringing out.
- During a two-tank morph (`morphOld_ >= 0`), a SIZE off the one the new tank started with makes the old tank go fast (`morphFast_ = true`, as a freeze during a morph does, `Engine.cpp:1078-1084`), and the new tank keeps its lengths.
- Once `morphOld_ < 0`, the latest SIZE lands, once, as ever.

It removes work rather than adding it. It reuses code that's already in the build (the fast drop, A's "waiting" flag), so it should cost a few dozen bytes of flash (release has about 1.1 KB to spare on the fixed-room build). Nothing changes outside the 3 s after a PLATE <-> FDN flip.

**For the listening page** (PLATE <-> CANYON and PLATE <-> HALL, a chord across the flip, SIZE flicked 0.1 s and 1 s after it, at the owner's knobs):
1. **"Old space makes way" (A):** the old tail goes in 0.3 s, then SIZE lands.
2. **"SIZE waits for the morph" (B):** the old tail fades over its 3 s as now, then SIZE lands.

Control: today's build (the fixed room). The desktop renders it without the crackle, so the page judges only how each fix sounds. CPU is judged on a lap: the 11 FLIPFLICK lines, PLATE <-> HALL flicks added, plus the usual 19 and the unfreeze lines.

Both would go on top of the fixed-room build (A for the twin plus the room keeping its size), so beta 2 carries all three.

## Open questions

- **Is cutting the old space's tail to 0.3 s audible in practice?** The old space ducks under the new one over the morph's first second anyway. That's A against B, for the page.
- **Is a 0.3 s SIZE lag after a flip acceptable by hand,** and does B's up-to-3 s lag feel like a dead knob?
- **PLATE <-> HALL** isn't lapped yet. It is expected to overload less.
- **The CANYON -> PLATE 101.5 % from ADR 0055** (a freeze within 220 ms of the flip) is a different overlap (the freeze's fast drop under the new PLATE). A doesn't address it.
