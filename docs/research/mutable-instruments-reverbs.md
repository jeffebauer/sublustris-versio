# Mutable Instruments' reverbs (Clouds, Beads): what Immensio could adapt

**Written:** 22 Oct 2026, at the owner's request. Source: `pichenettes/eurorack` (shallow clone, master) and `pichenettes/stmlib`. Every file discussed has an MIT header (© Emilie Gillet), so under ADR 0006 they may be adapted with a NOTICE credit. Nothing has been adapted yet. Paths below are in the MI repo.

## How they work

**One design shared by Clouds (2014), Rings, Elements and Beads.** `clouds/dsp/fx/reverb.h:52-55` calls it "the Griesinger topology described in the Dattorro paper", so it is a close relative of our PLATE.

- **Input:** mono (L+R summed), then 4 Schroeder allpasses.
- **Tank:** a figure 8 of two halves. Each half does this:
  1. reads the other half's long delay × the decay gain;
  2. one-pole lowpass;
  3. two allpasses;
  4. writes its own long delay.
- **Output:** one tap per side. Each tap is the signal just written into that side's long delay, so the reverb starts almost at once.
- **Modulation:** two slow LFOs (0.5 and 0.3 Hz), updated every 32 samples, with linear-interpolated reads.
  - **Clouds:** modulates only one long delay, plus an "AP1 smear" that sweeps the first input allpass's length by about 1–2 ms (`reverb.h:93-94`).
  - **Beads:** modulates **both** long delays (about 2 cents each) and smears AP1 more deeply (`beads/dsp/fx/reverb.h:120, 138, 150`).
- **Decay:** raw gain per half-trip, not T60. The damping coefficient is applied per pass.
- **Memory:** one packed ring buffer of int16 values.
  - **Clouds:** 12-bit format, with ±8 of headroom (`fx_engine.h:57-96`). This truncation is part of Clouds' lo-fi shimmer.
  - **Beads:** 16-bit.
- **Beads' changes:**
  - **Lengths:** ×1.32; the round trip is about 425 ms at 48 kHz, the small end of our HALL.
  - **Gain staging:** `input_gain` falls as decay rises, and `output_gain` makes it up (`reverb.h:92-93`), so the level inside the tank stays constant across decay.
  - **Mix law:** turning the reverb down closes the *send* while the wet stays fixed, so tails ring out instead of being cut (`reverb.h:98-108`).
  - **Controls:** one knob sets mix and time together.
- **Freeze:**
  - **Elements:** freezes like our ADR 0009 (time 1, input 0, no damping), which confirms our approach.
  - **Clouds:** freeze only raises the reverb amount.

## Worth adapting (ranked)

1. **Modulate the PLATE's other delays, as Beads does.** Our `core/dsp/Plate.cpp` modulates only `ap1`; `d1`, `ap2` and `d2` read at fixed lengths. That breaks our own SPEC §2.3 rule 1 ("every tank delay is modulated"), so it's a **spec gap**, confirmed in the code on 22 Oct 2026.
   - **Change:** a slow, distinct LFO plus a floor wander on each long delay, about 2 cents.
   - **Musical gain:** less plate ping and flutter on held notes.
   - **Cost:** almost no CPU.
   - **Risk:** low, but re-check the frozen-pad wobble.
2. **Modulate the input diffusers (the AP1 smear).** Our `core/dsp/Diffuser.h` allpasses are static.
   - **Change:** sweep stage 0 of each side by 1–3 ms at 0.3–0.5 Hz, L and R in opposite phase.
   - **Musical gain:** repeated drum hits or plucks stop entering the tank identically, so onsets turn cloud-like.
   - **Cost:** about 15 cycles a sample per side.
   - **Risk:** low to medium; start shallow (1–2 cents), since piano can wobble.
3. **Modulate one in-loop allpass per line in MASSIVE.** MASSIVE's 16 in-loop allpasses are static, and a frozen MASSIVE runs at full diffusion. Static allpasses in a lossless loop are a classic source of ringing, and **this may be the "upper harmonic building up" the owner heard in a frozen MASSIVE**, so measure it first.
   - **Cost:** about 1 % CPU.
   - **Risk:** medium; keep the depth well below the floor's ±1.5 cents.
4. **Give the right input diffuser its own lengths.**
   - **What we have now:** the right side is a 1.071× copy of the left (`kDiffuserRightScale`), so both sides have the same echo pattern. Clouds uses independent sets.
   - **Musical gain:** a wider, less phasey onset.
   - **Cost:** free.
5. **Trim the tank input as DECAY rises, with output makeup (Beads).** This would keep the loop's saturation and ERA grit consistent across DECAY.
   - **Risk:** medium. It interacts with the floor governor and the loudness trims, so it needs re-measuring.
6. **Fallback only: 16-bit SDRAM storage for the FDN lines.** Use it only if M3 profiling shows SDRAM bandwidth is the bottleneck. It conflicts with exact-unity frozen pads (ADR 0008/0009).

## Tried (23 Oct 2026, session 3)

- **#1** is in (v1.0.12, the owner's pick).
- **#2, a wandering input diffuser:** the first allpass on each side wanders 0.18 ms at 0.4 Hz, opposite phase (the buffers' spare room caps it under a cent). Measured hit-to-hit similarity of the first 80 ms: PLATE 0.74 → 0.51; HALL and MASSIVE already low (0.01–0.12), so only the PLATE is asked (`renders/diffuser_motion`, `IV_DIFF_MOTION`).
- **#3, a modulated MASSIVE in-loop allpass:** not built. Its motive, the owner's "upper harmonic building in a frozen MASSIVE", didn't reproduce when measured (22 Oct). Revisit if the owner hears it again.
- **#4, independent right diffuser lengths:** built (`IV_DIFF_INDEPENDENT`), no measurable gain: onset L/R correlation stays 0.00–0.30 either way, since the tanks already decorrelate. Not put to the owner.
- **#5:** not yet.

## Deliberately not copied

- **Linear interpolation in the loop.** It dulls and flutters long tails. Our allpass interpolation suits infinite tails better.
- **Raw per-pass gains for decay and damping.** RT60 and brightness would shift with SIZE and sample rate. Our Jot filters keep DECAY and TONE independent of SIZE.
- **Mono input and one tap per side.** We're true stereo, and the PLATE has Dattorro's 7-tap outputs.
- **12-bit tank storage in the clean spaces.** In Immensio, grit belongs to ERA's Midiverb layer.
- **Beads' single macro knob and reverb-as-insert.** We have separate knobs. Beads' send-closes mix law *is* worth a thought for MIX; that needs an ADR.
- **Clouds' freeze** (raising the amount). It fits a granular processor, not our freeze and twin.
