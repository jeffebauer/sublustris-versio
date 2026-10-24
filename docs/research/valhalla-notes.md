# The owner's notes on `renders/valhalla_compare`, measured (24 Oct 2026)

**Outcome:**
- **Shipped as v1.0.25** (`renders/valhalla_round2`):
  - HALL early taps;
  - HALL floor ×3;
  - air (HALL 5.5, PLATE 8 dB/s);
  - PLATE centred lows;
  - MASSIVE half swell.
  - HALL's level was trimmed −2.3 dB to keep the spaces matched.
- **Full-mix send:** "both" was picked. The hotter send shipped as v1.0.26 (material). The steady-level tape was turned down after a held-note page.
- The prototype hooks named below are gone; their code was on branch `claude/listen` (deleted 25 Oct 2026, last commit fbb283c).

Each note was measured before anything was built (memory: measure owner notes first). The figures come from `docs/research/valhalla-comparison.md` (on its branch) and from new measurements. Prototypes live behind `IV_*` hooks (`Engine::ValhallaProto`), off by default. The page is `renders/valhalla_round2` (`tools/pages/valhalla_round2.sh`).

## The notes

**HALL at 2 and 5 s:** "a slight slap-back effect… Valhalla's examples seem to have a more immediate yet smooth initial response".
- [M] Our first sound comes at 80 ms and the peak at 127–164 ms. Room's first sound comes at 9–32 ms and its peak at 34–57 ms. The HALL has no early path: `setEarlyTaps(massive ? 1 : 0)`.
- **Prototype `IV_HALL_EARLY`:** MASSIVE's six interleaved early taps in the HALL too.
- [M] First sound 79 → 10 ms. The peak is unchanged at 141 ms. Output only.

**HALL at 15 s:** "more resonant mid build-up… less sonically rounded".
- [M] On the tail only (from 1 s after the music's last loud moment, for 5 s), mid-band peaks over a ⅓-octave local average, 95th percentile. Ours / Large Room / Dark Chamber:
  - drums: 11.1 / 8.5 / 8.2 dB;
  - skank: 17.9 / 13.9 / 12.6 dB;
  - pad: 23.4 / 12.1 / 10.5 dB.
- [M] Keeping the in-loop diffusion at every SIZE changed nothing (dropped).
- **Prototype `IV_HALL_FLOOR=3`:** three times the modulation floor gives drums 8.9, skank 16.8, pad 17.0 dB.
  - [M] A held 1 kHz wavers 6.5 → 7.3 cents.
  - [M] Energy: no level more than +0.3 dB over today; holds pass.
  - [S] Modulation smearing resonances is the standard method (Costello; Valhalla's modes).

**MASSIVE at 2 and 5 s:** a recording issue, not a sound difference.
- [M] Supermassive is genuinely grainy at low feedback: separate echoes for 0.7–1.3 s. The 2 s Andromeda take was 0 % feedback, which rings 0.5 s.
- **Advice:** re-record at feedback settings that give a reverb-like tail (about 5 % for Andromeda's 2 s). Prefer the longer settings the owner likes (15 s, 30 s, infinite) over chasing 2 s.

**MASSIVE at 15 s:** "less swell than ours… more smoothed out".
- [M] Our tail rises over about 0.8 s from 0.2 s. Sagittarius at 73 % arrives within 3 ms and peaks at 57 ms; Andromeda peaks at 1.3 s.
- **Prototype `IV_MASSIVE_SWELL=0.5`:** half the built-in swell; the peak moves 1.59 → 1.29 s.
- This is a question of amount only: the owner kept the swell over none on 22 Oct.

**PLATE at 2 s:** "messier… Valhalla more centred… lows/mids centred and highs moving?".
- [M] Below 250 Hz, our L/R correlation was negative: pad −0.38, vibes −0.86, keys −0.28. Valhalla Chrome and Lithium are +0.1 to +0.45.
- **Cause:** Dattorro's output taps subtract the opposite branch. With true-stereo input the branches are independent, so L ≈ 2·b1 − 3·b0 and R ≈ 2·b0 − 3·b1 where the taps cohere (in the lows), which gives a correlation near −12/13.
- [M] The modulation isn't the cause: with it off, vibes still read −0.86 → +0.10 changes only partly.
- **Prototype `IV_PLATE_CENTRED`:** the cross-branch taps added.
  - [M] Lows: pad −0.05, vibes +0.14, keys +0.37.
  - Mids on the vibes turn negative (−0.40 / −0.50), as Valhalla's do (−0.21 / −0.77).
  - Output only.

**PLATE at 15 s:** "more top-end present in our Plates?". Yes.
- [M] Our 8 kHz rings 7.7 s; Chrome rings 2.9 s, Lithium 6.3 s.
- **Prototype `IV_AIR`:** the highs lose a fixed extra few dB per second at every decay, as air does (Room measured 3–7 dB/s), instead of a fixed share of the decay (`absorbent()`, HALL 5.5 dB/s, PLATE 8 dB/s).
- [M] Results, 8 kHz T60:
  - PLATE 15 s: 7.8 → 5.6 s;
  - PLATE 2 s: 1.1 → 1.8 s;
  - HALL 2 s: 1.1 → 1.8 s (Room: 1.8 s);
  - HALL 15 s: 7.9 → 7.0 s.
- Extra loss only, so energy-safe.

## Also asked: the full-mix row

"Saturation and hysteresis are less pronounced on the full mix… the reverb level is quieter".
- [M] By design the reverb hears only the mix's send (stabs and rims at their mix level, `fullmix_send.wav`): −30.4 LUFS against −18 to −19 LUFS for the single instruments. So the wet is about 12 dB lower against the dry, and the level-dependent tape stage is driven 12 dB less.
- **Options (the owner's call):**
  - send hotter, as a dub engineer would;
  - or make tape out respond less to level, e.g. a compressor before the curve as ChowTape does.
