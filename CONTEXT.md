# SUBLUSTRIS VERSIO — Context / Glossary

Shared vocabulary for this project. Use these terms exactly in code, comments and conversation. Spec: `SPEC.md`. Decisions: `docs/adr/` (index: `docs/adr/README.md`).

**Panel names (27 Oct 2026, ADR 0029).** The docs and chat use the panel names: BLEND, DECAY, TONE, WEAR, SIZE, MOTION, DEPTH, SPACE, ONSET. Code keeps the ParamSpec keys (`mix`, `decay`, `tone`, `era`, `size`, `drift`, `time`, `space`, `attack`) and identifiers built on them (`ParamId::Drift`, `map::eraMix`), which never change. Older pages, ADRs and changelog lines use the names of their day: each "was" below says which.

| Term | Meaning |
|---|---|
| **BLEND** | P1: dry/wet, equal-power; fully left is a clean passthrough, fully right 100 % wet. Key `mix`. *Was MIX (until v1.0.42).* |
| **DECAY** | P2: the tail's length; past ~3 o'clock, infinite plus ducking. |
| **TONE** | P3, noon flat. In the loop, the highs' share of the decay (darker or brighter with each pass); on the wet, a tilt round 600 Hz and a soft bell riding its bend (v1.0.44). |
| **Resonant tilt / bell** | TONE's soft bell on the wet (v1.0.44, ADR 0031): turned left a warm swell, its corner 3 kHz → 600 Hz; turned right a sheen, 1 → 5 kHz; Q up to 1.3, +4.5 dB at the ends. Noon passes the tilt bit for bit; nothing resonant is in the loop. |
| **WEAR** | P4: one wear amount over the Neve (v1.0.42, ADR 0027). Every stage comes in together near the start of the knob and deepens with it: tape saturation and compression, the BBD's breath and clock, the grain, the radio's band. No darkening (TONE's), no pitch movement (MOTION's). WEAR 0, the default, is the Neve alone. Key `era`. *Was ERA (until v1.0.42: the eras, see Retired terms).* |
| **Wear chain** | WEAR's stages on the wet, after the tank and MOTION's transport, before TONE's tilt (`dsp/EraTexture.h`): **tape** (pre-emphasis around a soft clip, then the de-emphasis), **breath** (the BBD's compander with its noise: level and noise breathe with the playing), **clock** (the BBD's sample-and-hold with no filters: the top folds back), **grain** (a step that follows the level, so a fading tail stays equally grainy), **band** (the AM radio's gentle band-pass, over-modulation and wandering dip). A slow level hold keeps the loudness. |
| **Neve base** | The Neve's warmth (even harmonics, slightly shorter highs) in the loop at every WEAR setting, alone at WEAR 0. Never darker with WEAR. |
| **Hiss and crackle** | WEAR's soft hiss and sparse crackle fed into the tank's input, following the input level, so silence stays silent (ADR 0005). *Was the cassette era's hiss.* |
| **SIZE** | P5: the space's size. In HALL and MASSIVE a move crossfades in four groups of lines once the knob rests (v1.0.45, ADR 0025), so nothing retunes; PLATE's glides like tape. |
| **MOTION** | P6, bipolar. Noon: only the modulation floor. Left: tape (the tape transport). Right: Wander. Key `drift`. *Was DRIFT (until v1.0.42).* |
| **Tape transport** | MOTION's left side (v1.0.46, ADR 0030; `dsp/Transport.h`): one short delay on the wet, read at one moving point, so the whole tail bends together as one tape speed would. Wow and flutter, more variance toward full left, crinkle and dips. It comes in only on the left side (crossfading in, then coming up to speed over 5 s); there the wet arrives 20.8 ms later. The tank's own per-line wobble keeps a third. |
| **Crinkle** | The tape transport's friction and creases, as on Strymon Magneto, over the left side's last 40 %: a bed of scrape roughness (fast random pitch) and bursts of 0.1-0.6 s of jittery pitch. Pitch only; the level loss is the dips. |
| **Dips** | A counted share (43 %) of the crinkle bursts that also roughen the level by 1-3.5 dB and dull the highs for 50-300 ms: about 15 a minute at full left (the owner's "middle ground" between E and C's "dropouts", v1.0.46). |
| **Wander** | MOTION's right side: quick, small random movement of every tank line (up to ±20 cents, 1-2.25 Hz, the lines' rates fanned out so they never sway together), deeper (×1.4) and quicker (×1.2) over the side's last half since v1.0.46. |
| **DEPTH** | P7: the onset's timing or amount, per ONSET. NATURAL: predelay; BLOOM: swell time; SHIMMER: how much shimmer. Key `time`. *Was TIME (until v1.0.42).* |
| **ONSET** | SW1: NATURAL / BLOOM / SHIMMER. Sets the attack in every space. Key `attack`. *Was ATTACK (until v1.0.42), with NORMAL / BLOOM / SHIMMER; REVERSE before v1.0.33.* |
| **NATURAL** | ONSET's first position: the sound enters the tank at once (MASSIVE keeps a gentle built-in mass); DEPTH is predelay. `attack=NORMAL` still reads. *Was NORMAL.* |
| **Bloom** | ONSET's centre: the tail swells in over DEPTH (the swell stage, in every space). |
| **Shimmer** | ONSET's third position (v1.0.33, ADR 0024): what you play, an octave up, joins the input into the reverb; no feedback, so one steady octave. Eight overlapping grains with random onsets, behind a steep 6 kHz filter. Replaced **Reverse** (the hardware reverse reverb, v1.0.9-v1.0.32). |
| **SPACE** | SW0: PLATE / HALL / MASSIVE. Flips crossfade ~100 ms. |
| **FREEZE / PAINT** | The button's panel name: tap to freeze, hold while frozen to paint. The gate reads **FREEZE** (it mirrors the button). |
| **Big tank** | The main reverb: the Plate engine or the FDN (Hall / Massive), per SPACE. Only one runs at a time. |
| **Freeze / pad** | The big tank held at loop gain 1, input closed, in-loop colour bypassed: a still pad to play over. Latched by a tap. |
| **Twin** | What you play over a frozen pad goes into: a quieter layer over the pad that fades back out within 8 s, played by SIZE and DECAY. In PLATE a second Plate. In HALL and MASSIVE **the small room** since v1.0.43; before that a second FDN, the "twin tank" (v1.0.7-v1.0.42), which replaced the **companion** (a small DTCM plate). |
| **The small room** | HALL's and MASSIVE's twin (v1.0.43, ADR 0028; `dsp/TwinRoom.h`): a 150 Hz low cut, eight short lines (8-50 ms by SIZE) at 24 kHz, a slightly softer top. Its level follows the pad, about 8 dB under it; each line wanders by 0.5 % of its length, so it bends what it holds no more than the main tank does (v1.0.47). Its rings are in AXI SRAM. |
| **Paint** | While frozen, holding the button (or a long gate) blends a little of the twin (or the room) into the pad over ~3 s (at most ~1.5 dB); the pad keeps it on release. |
| **Gesture reader** | One reader for the button and the gate: press < 0.3 s = tap (toggle freeze), hold ≥ 0.3 s while frozen = paint. Freeze engages on the press. |
| **FDN** | Feedback delay network: 8 modulated delay lines mixed by a matrix. The Hall and Massive voicings; their tank runs at 24 kHz (v1.0.35). |
| **Swell stage** | The feedforward cascade before the tank that makes a slow attack: BLOOM in every space, and Massive's built-in mass in NATURAL. |
| **Attack time A** | Massive NATURAL: A₀(Decay) · m(Size); 0.5 → 0.2 s with Decay, ×1 → ×1.5 with Size. BLOOM: DEPTH's swell × m(Size). |
| **Mass** | Costello's word for Sagittarius: slow to start, slow to leave. Here: the swell, scaled by Size. |
| **Echoes → wash** | Massive's Size travel: long distinct repeats (Large Magellanic Cloud) blurring into a boundless wash (Andromeda). Since v1.0.6 every SIZE is already a wash. |
| **Floor** | The warm level an infinite tail settles at and holds (loop governor + soft saturation). Not used while frozen. |
| **Infinite** | Decay 0.85-1: loop gain 1 at the floor; ducking grows across this stretch. |
| **Held endless tail** | An infinite tail once the input has been quiet for 2 s: it holds its own settled level (the hold, a flat gain since v1.0.58). PLATE and HALL (v1.0.58): its modulation glides out over 10 s (**still notes**); if what was played was bright, its top end also mellows by up to about 24 dB above the loss shelf over a minute or two, then stops (**the light settle**, in proportion to the playing's brightness: none for a soft chord, all for a noise wash). New playing brings both back. |
| **Modulation floor** | The always-on faint random movement of every tank delay (±1-2 cents) that kills metallic ringing. What remains at MOTION noon. |
| **Air** | HALL's and PLATE's highs lose a steady extra few dB per second at every finite DECAY, as air does in a room, instead of a fixed share of the decay, so short tails stay bright and long PLATEs lose some top end (v1.0.25). MASSIVE keeps the share. |
| **Still lows** | Below ~300 Hz the HALL / MASSIVE tail rings in its own unmodulated tank, so the bass never wavers; MOTION and the modulation floor move only what's above (v1.0.18). |
| **Loop chain** | Everything inside each line per pass: modulated read → TONE's loss + damping → soft saturation → the Neve → WEAR's tape compression. |
| **Duck** | Tail ducking from the input level, only at infinite Decay: 2:1 compression of the wet (attack 30 ms, release 0.4 s), no darkening (v1.0.17; it was DARKEN; 4:1 until v1.0.51). Held playing is ducked less (40 %) and let go faster (0.08 s) when it stops (v1.0.51). |
| **P1-P7** | Pots by panel position, reading order (Resilio ADR 0028), not libDaisy index. |
| **Normalised** | 0-1 parameter value every host passes; mapping curves live only in `core/params/`. |
| **Starting guess *(start)*** | A number to be confirmed by measurement or ear at its milestone. |

### Retired terms (old pages and ADRs)

| Term | Meaning then | Now |
|---|---|---|
| **Era** | One of ERA's five, sweetest first: Neve, Midiverb, Bucket-brigade, Cassette, AM radio, each playing on its own in its fifth of the knob, only neighbours crossfading (v1.0.30-v1.0.41, ADR 0021). Before v1.0.30 they stacked (ADR 0003). | WEAR's one wear amount (v1.0.42). |
| **Midiverb box** | Era 20-40 %: an in-loop 48 → 24 → 48 kHz box with the unit's 13 bits, truncated toward zero. | Gone in v1.0.42; WEAR's grain keeps the grit, on the wet. |
| **Bucket-brigade (BBD)** | Era 40-60 %: a delay chip's dark sound on the wet, the compander's breathing, the clock's aliasing. | WEAR's breath and clock, without the chip's low-pass. |
| **Cassette / tape out** | Era 60-80 %: tape drive and darkening in the loop, hiss, wow and flutter, and tape out on the wet (pre-emphasis, drive, light hysteresis). | WEAR's tape and compression (no darkening); the wow and flutter are MOTION's tape transport. |
| **AM radio** | Era 80-100 %: a station fading on its own path, the AGC's swelling static, a wandering dip, a slow detune. | WEAR's band (band-pass, over-modulation, the dip); no fades, static or detune. |
| **Twin tank** | HALL's and MASSIVE's second FDN while frozen (v1.0.7-v1.0.42), with its own swell in MASSIVE. | The small room. |
| **ERA, DRIFT, TIME, ATTACK, MIX, NORMAL** | The panel names until v1.0.42. | WEAR, MOTION, DEPTH, ONSET, BLEND, NATURAL (the keys keep the old ones). |
