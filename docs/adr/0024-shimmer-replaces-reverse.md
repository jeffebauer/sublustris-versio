# 0024 — SHIMMER replaces REVERSE on ATTACK's third position

**Status:** Accepted, 25 Oct 2026 (owner: "Option B wins out of these options — it's the most musical and pitch-accurate" on `renders/shimmer4`; then "Shimmer wins hands-down" on `renders/attack_alts2`). SPEC v1.0.33. Amends ADR 0015 (its REVERSE parts).

**Context:** The owner disliked REVERSE in the plugin and asked for alternatives. Researched and heard, round by round:
- **Round 1** (`alt_attack`): shimmer up "too static... almost like bit crushing"; sub shimmer "a bit muddy"; grains "more like modulation than grains".
- **Shimmer rounds 2-4:** three measured causes of the grit, each fixed:
  - the splices fluttered the octave 14-25 dB at 23 Hz (round 1's 2 heads, jittered windows);
  - fold-back: an octave up doubles every frequency, and above 12 kHz that passes 24 kHz and folds back as inharmonic tones (13 / 16 / 19 kHz came out at 22 / 16 / 10 kHz, 8-16 dB down), what a sample-rate reducer does;
  - when fed back, the splices' sidebands and the tank's own movement piled up trip after trip.
  
  The input-only kind (no feedback) was cleanest; its last fault, "inharmonic resonances that make certain tones sound a little out of tune", was heads at fixed spacings combing certain notes' octaves away (110 / 165 / 330 Hz nearly gone, 59 dB apart across notes). Random grain onsets fixed it (within 2.6 dB; the detuned copies 38-40 dB down).
- **Alternatives round 2** (`attack_alts2`, each from researched designs): reverse into the reverb (FutureVerb), a reversed tail, a dub echo-verb (Space Echo / King Tubby) and one re-echoing the reverb (Supermassive EchoVerb), a nonlinear gate (AMS RMX16 Nonlin, Strymon Gate) and a gated room (Padgham). The shimmer won.

**Decision:**
- **ATTACK = NORMAL / BLOOM / SHIMMER.** SHIMMER: the diffused input, shifted an octave up, is sent into the tank with it; no feedback.
- **The shifter** (`dsp/Shimmer.h`): a delay line read by 8 heads, each sweeping a 0.4 s grain at twice the speed of time, faded by sin² (Bhaskara's approximation, no libm per sample), each grain starting at a random extra delay up to a grain. In front, an 8th-order Butterworth low-pass at 6 kHz (-68 dB at 16 kHz); after, a 9 kHz one-pole and an 80 Hz high-pass.
- **TIME** sets the send, 0 → 1.2 × the input (linear). At TIME 0 SHIMMER is exactly NORMAL without its predelay.
- **Behaviour kept from REVERSE:** it rests while frozen (the send fades over 50 ms); it keeps recording, so switching in finds the last moments ready; its recording fades with a SPACE flip as the tank's input does (recorded unfaded, the predelay's jump at the flip's silent point came back as a click).
- **REVERSE is removed:** its taps, short reverb, mappings, voicing and level tables. The ParamSpec key `attack` and its positions are unchanged (saved sets keep working; position 3 now plays SHIMMER).

**Considered:** fed-back shimmers (climbing octaves; still grittier than the plain reverb in HALL after three fixes), shimmer inside the FDN (collapsed the tail or grew held chords without the DAFx 2026 energy compensation), sub shimmer, grains, two reverses, two echo-verbs, two gates.

**Consequences:**
- **Measured:** off-pitch content in the reverb equal to the plain reverb's (worst note -19 dB HALL, -20 dB MASSIVE, both); a note's octave added in every SPACE (median +36 to +40 dB over TIME 0 on held sines); no clicks coming in, while frozen, or across flips (a flip's own restart edge is NORMAL's, x10.5 against x10.7, MASSIVE to PLATE).
- **Flash:** release 111,836 → 112,060 B (+224; 580 B under budget): REVERSE out freed nearly what the shimmer costs. sinf / cosf / expf were already linked.
- **Memory:** ~0.5 MB SDRAM for its line, in place of REVERSE's ~0.75 MB and its short reverb.
- **CPU:** ~300 cycles a sample with 8 heads (estimate), only while SHIMMER is on (it records always); to measure at M3.
- **Tests:** `test_massive` (octave added and even; TIME 0 is the plain reverb; no clicks), `test_plate_freeze` (rests while frozen), three soak regressions kept for the third position (flips, coming in).
