# Immensio Versio — spec session log

Historical record of the spec session (input: `IMMENSIO_BRIEF.md`), kept as it was. The owner approved the spec on 21 Oct 2026 (TIME = predelay in NORMAL and the P1–P7 layout included). The decisions below now live in `docs/adr/` (0001–0017); `docs/adr/README.md` maps each decision number here to its ADR. `SPEC.md` v1.0 is frozen: changes go through a new ADR.

## Phase 1 findings (21 Oct 2026)

- **Repo:** new repo, seeded by copying Resilio Versio's generic pieces (provenance noted in `NOTICE`). No monorepo; no shared library until both firmwares settle on the same code.
- **Reuse:** three-host layout (Core / Renderer / JUCE Plugin / Firmware), `ParamSpec`, `host/common`, the test runner, plugin shell and `plugin_host_test`, firmware utilities (`PotEndStops`, `LedMeter`, LED PWM, build variants, `ProfileHook`), `Filters` / `Oversampler` / `Select` / `Seed` / `SizeOpt`, tools. Refactor before reuse: μ-law (`OutputBits`, `EchoBits`), saturation and tape (`Drive`, `EchoWear`), `Wobble`.
- **Flash:** Resilio ended at 97.7 % of 128 KB internal flash. Immensio tracks flash as a budget from M0; the BOOT_SRAM fallback is the owner's call (it loses the NE Firmware Swap install).
- **Docs format:** `SPEC.md` + `CONTEXT.md` + `docs/adr/NNNN-*.md` + `CLAUDE.md`, as in Resilio.

## [VERIFY] results

- **Large Magellanic Cloud** (Supermassive v1.1.1, 29 Sep 2020): "LOOOONG repeating echoes, long decay, medium attack"; Density 0 → echoes at 4× DELAY; Density up + Warp ≥ 50 % → lush wash. Takeaway: Massive mode's Size should travel from long echoes to wash.
- **Midiverb (1986):** 12-bit converters, ~10 kHz bandwidth, ~23.4 kHz rate (emulator figure, not a factory spec). emeb/MIDIVerb_RE is MIT; thement/midiverb_emulator is GPL-3.0 (reference only). No ROM microcode is used.
- **Licences:** Plateau Versio is GPL-3.0-or-later (concept only); the Daisy Griesinger reverb has no licence (don't use); daisy-patch-algos index is MIT.
- **Infinite Supermassive:** Valhalla keeps EQ outside the loop and sets mod depth to 0 for pitch-stable freezes. Immensio puts damping, saturation and Era in the loop by design; the floor that stops the tail turning to mud is settled in topic 1b.

## Decisions

1. **Era stacks** (owner, 21 Oct 2026): each stage keeps the colour of the ones before it; turning Era up means "further back in time, more worn".
2. **Grain in two places** (owner, 21 Oct 2026): μ-law on the wet output from the μ-law stage on (gritty peaks, smooth quiet tail); Midiverb-style bit loss inside the loop at the far end (tail grainier as it fades, truncation toward zero so it can't hold a buzz).
3. **Hiss into the tank** (owner, 21 Oct 2026): cassette hiss enters at the tank input, so it is reverberated and shaped by Tone; it is ducked by the input level so an infinite tail doesn't fill with noise.
4. **Midiverb end at the old rate** (owner, 21 Oct 2026): from the Midiverb stage on, the loop is band-limited and resampled at ~24 kHz (true ~10 kHz ceiling, a little aliasing grit). Implementation note: the tank keeps running at 48 kHz and the loop passes through a 48 → 24 → 48 kHz box (as Resilio's `EchoBits`), because switching a live tail's whole tank rate can't crossfade cleanly. So this stage saves **no** CPU, unlike the brief's hope.
5. **Era travel weighted to the middle** (owner, 21 Oct 2026): Clean 0–10 %, Neve 10–35 %, Cassette 35–65 %, μ-law 65–85 %, Midiverb 85–100 %. Each layer fades in across its own stretch and then stays (stacking).

6. **Massive swell ~1 s at low Decay** (owner, 21 Oct 2026): in Massive, Decay also sets the fade-in (Sagittarius coupling: higher Decay = faster swell, longer tail). A gentle, playable bloom rather than an Andromeda-slow arrival.
7. **Massive Size goes from echoes to wash** (owner, 21 Oct 2026): low Size = long, distinct repeating echoes (Large Magellanic Cloud); turning it up blurs them into a boundless wash, and the predelay grows with it (Andromeda).
8. **Infinite tails evolve to a floor** (owner, 21 Oct 2026): at the top of Decay, Tone, Era and the saturation stay in the loop, so the tail keeps slowly darkening and graining, then settles at a warm floor and holds there. It never turns to mud and never falls silent.

9. **Size adds mass** (owner, 21 Oct 2026): in Massive, Decay speeds the swell up (1 s → ~0.4 s, Sagittarius-style), and a bigger Size slows it down again, up to ~3 s at max Size. The boundless end keeps slab-like mass; small Size always allows a fast bloom.

10. **Button taps latch the freeze** (owner, 21 Oct 2026): a tap freezes the big tank into a pad, and the next tap unfreezes it so the pad decays naturally. While frozen, Size and Decay play the small companion reverb.
11. **Hold to paint** (owner, 21 Oct 2026): while frozen, holding the button blends what's playing into the pad over a few seconds; on release the pad keeps its new layers. With no hold, the pad stays untouched.
12. **The gate mirrors the button** (owner, 21 Oct 2026): no dub throw. The brief's lean is overruled, and the tap/hold throw question is moot.

13. **Gate = the same gestures as the button** (owner, 21 Oct 2026): a short pulse (< ~0.3 s) toggles the freeze; a gate held longer while frozen paints into the pad. Button and gate feed one gesture reader, so either can do every move.

14. **Mix: equal-power, fully right = 100 % wet** (owner, 21 Oct 2026): fully left is a clean passthrough, fully right is 100 % wet for a send/return. No separate kill-dry setting.
15. **Size moves bend the pitch, tape-like** (owner, 21 Oct 2026): the delays glide, slew-limited, so fast knob or CV moves become a smooth swoop, never zipper. While frozen, Size plays the companion, so the pad never bends.
16. **Ducking grows with Decay** (owner, 21 Oct 2026): from ~3 o'clock (infinite eases in) to full, the duck deepens. The Duck style switch picks how it ducks: Darken / Push back / Warble.
17. **Space flips crossfade over ~100 ms** (owner, 21 Oct 2026): the old tail fades out while the new space takes the input. Only one big tank ever runs.

18. **Drift is bipolar** (owner, 21 Oct 2026): noon is still (only the faint anti-metallic movement remains). Left of noon: random tape wow/flutter. Right of noon: smooth Cathedra-style pitch drift and bend. Era's cassette wow/flutter adds on top of Drift's left side.
19. **Desk: off → resonant sweep 150 Hz–4 kHz** (owner, 21 Oct 2026): fully left is flat with no drive. Turning up sweeps a resonant peak from ~150 Hz to ~4 kHz while its resonance and the console drive rise together, Tubby-style. The loop saturation keeps a singing tail from howling.
20. **Tone: noon is flat** (owner, 21 Oct 2026): left darkens the tail on every pass, right brightens it.
21. **LEDs: meters + freeze state** (owner, 21 Oct 2026): in L/R and out L/R level meters, as Resilio. When frozen, the output pair turns cool blue under the meter; painting pulses it slowly.

22. **Two engines** (owner, 21 Oct 2026): Plate is a Dattorro-family plate, and the same code at a small size is the freeze companion. Hall and Massive are one 8-line FDN voiced two ways. The character chain is what keeps the Plate from being Plateau.
23. **True stereo in** (owner, 21 Oct 2026): L and R enter the tank at different points, so stereo placement survives. A signal on L alone comes out of both sides.
24. **Duck styles: Darken / Push back / Level** (owner, 21 Oct 2026): Warble is replaced by a plain level duck, since Drift already covers wow. Darken: a filter closes on the tail. Push back: more smear and a narrower image. Level: the tail dips.

25. **The frozen pad holds still** (owner, 21 Oct 2026): it keeps its pitch and colour. Only the anti-metallic floor and Drift stay. The in-loop colour glides out over ~0.2 s on freeze and back in on unfreeze.
26. **Unfreeze glides to the knobs** (owner, 21 Oct 2026): the big tank glides to the current Size and Decay over ~1 s as the pad releases, which is a tape-like swoop if Size moved.

### After the P7 / SW1 demos (`renders/p7_sw1_demos/`, 21 Oct 2026)

Owner picks: Desk off on the skank, Desk sweep on vibes; Speed 0.1 Hz on both; Darken on both; low cut off on both; Bloom on vibes with "I'd want to control the timing of it. When the swell is too long it can interfere with harmonies"; idea: bloom vs reverse on the switch.

27. **DESK removed; ducking is always DARKEN** (owner, 21 Oct 2026): supersedes decisions 19 and 24. The low cut and the Speed knob were heard and dropped (slowest Speed picked both times; Drift's right side covers it).
28. **P7 = TIME** (owner, 21 Oct 2026): NORMAL predelay 0–250 ms *(start, to confirm)*; BLOOM swell time 0.1–4 s; REVERSE length 0.2–2 s.
29. **SW1 = ATTACK: NORMAL / BLOOM / REVERSE, governing every space** (owner, 21 Oct 2026): amends decisions 6 and 9. In Massive, NORMAL keeps a gentle built-in mass (0.5 → 0.2 s with Decay, ×1–1.5 with Size); BLOOM uses TIME, still × Size's mass.
30. **REVERSE is the classic reverse swell** (owner, 21 Oct 2026): each TIME-long segment of the tank output plays backwards, rising to a cut. The wet is TIME late; the dry is not. Bypassed while frozen.

### Era stages (starting guesses, tuned by ear at the tuning milestone)

| Stage (layer added) | Bandwidth (loop low-pass) | Bits / companding | Saturation | Noise | Wow / flutter | Where |
|---|---|---|---|---|---|---|
| Clean (0–10 %) | full | float | always-on loop soft-clip only | none | none (Drift only) | – |
| + Neve (10–35 %) | 20 → 16 kHz; +1.5 dB shelf at 80 Hz | float | even-harmonic, asymmetric; 2nd harmonic ~−30 dB at nominal level | none | none | in loop |
| + Cassette (35–65 %) | 16 → 9 kHz; head bump +2 dB at ~70 Hz | float; slow 2:1 soft compression above −18 dBFS | tape curve on top of Neve | hiss into the tank, up to ~−66 dBFS, ducked by input | wow 0.3–0.8 Hz up to ±4 cents, flutter 6–8 Hz up to ±1 cent, added to Drift | in loop (hiss at tank input) |
| + μ-law (65–85 %) | unchanged | 24 kHz / 12-bit μ-law → 10-bit at 85 % | – | – | – | wet output |
| + Midiverb (85–100 %) | 9 → ~10 kHz ceiling at a 24 kHz rate | 12 → 8-bit truncation toward zero, TPDF-dithered | – | crackle, sparse, scales to the end | – | in loop |
