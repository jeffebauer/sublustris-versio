# Architecture decision records

Every decision behind `SPEC.md` (v1.0, frozen 21 Oct 2026). Changes after the freeze: a new ADR (or an amendment to one) plus a changelog line in `SPEC.md`. The spec session that produced 0001–0017 is recorded in `docs/spec-session.md`; its decision numbers map here:

| ADR | Title | Spec-session decisions |
|---|---|---|
| [0001](0001-new-repo-seeded-from-resilio.md) | A new repo, seeded from Resilio Versio | Phase 1 finding |
| [0002](0002-internal-flash-and-flash-budget.md) | Internal flash, Firmware Swap, and a flash budget | Phase 1 finding |
| [0003](0003-era-stacks.md) | ERA stacks, weighted to the middle (superseded by 0021) | 1, 5 |
| [0004](0004-era-grain-and-midiverb-box.md) | ERA's grain in two places; the Midiverb box at 24 kHz | 2, 4 |
| [0005](0005-hiss-into-the-tank.md) | Cassette hiss goes into the tank, ducked by the input | 3 |
| [0006](0006-two-engines-and-licence-rules.md) | Two engines: a Plate (also the companion) and one FDN; licence rules | 22 |
| [0007](0007-massive-mass-and-echoes-to-wash.md) | MASSIVE: mass, and SIZE from echoes to wash | 6, 7, 9 (amended by 29) |
| [0008](0008-infinite-evolves-to-a-floor.md) | Infinite tails evolve to a warm floor (its level: ADR 0033; amended v1.0.56: eases in from DECAY 0.70 in PLATE and MASSIVE; v1.0.58: a flat correction, a held tail keeps still notes, only a bright one settles a little; v1.0.61: endless follows the playing, held 0.75 s after you stop, a −6 dBFS safety ceiling, no duck) | 8 |
| [0009](0009-freeze-tap-latches-pad-holds-still.md) | Freeze: a tap latches, the pad holds still, unfreeze glides (amended v1.0.58: its hold a flat gain) | 10, 25, 26 |
| [0010](0010-hold-to-paint.md) | Hold to paint into the frozen pad | 11 |
| [0011](0011-gate-mirrors-the-button.md) | The gate mirrors the button: one gesture reader | 12, 13 |
| [0012](0012-mix-size-tone-drift.md) | MIX, SIZE, TONE and DRIFT (HALL's shelf and long-tail floor: ADR 0035) | 14, 15, 18, 20, 25 |
| [0013](0013-space-crossfade-and-true-stereo-in.md) | SPACE flips crossfade; true stereo in | 17, 23 |
| [0014](0014-ducking-is-always-darken.md) | Ducking is always DARKEN, growing with DECAY (amended v1.0.17: 4:1 compression of the wet; v1.0.51: 2:1, eased on held playing; superseded v1.0.61 by 0008's amendment: no duck) | 16, 24, 27 |
| [0015](0015-time-and-attack.md) | P7 is TIME, SW1 is ATTACK (NORMAL / BLOOM / REVERSE) | 27, 28, 29, 30 (supersedes 19) |
| [0016](0016-leds-meters-and-freeze-state.md) | LEDs: level meters plus the freeze state | 21 |
| [0017](0017-panel-layout.md) | Panel layout: P1–P7 mirror Resilio's positions | owner approval of v1.0 |
| [0018](0018-brand-ombra-audio.md) | Brand: Ombra Audio (maker code Ombr) for both plugins | owner, 23 Oct 2026 |
| [0019](0019-vintage-reads-and-tape-out.md) | ERA gains vintage reads and tape out | owner, 24 Oct 2026 |
| [0020](0020-decay-keeps-the-loudness.md) | DECAY keeps the loudness | owner, 24 Oct 2026 |
| [0021](0021-era-sweet-to-gritty.md) | ERA runs from sweet to gritty: five eras, each on its own, over the Neve (superseded by 0027) | owner, 25 Oct 2026 |
| [0022](0022-massive-highs-ring-with-the-body.md) | MASSIVE's highs ring as long as its body | owner, 25 Oct 2026 |
| [0023](0023-massive-shortest-tail-six-trips.md) | MASSIVE's shortest tail lasts six trips round its lines | owner, 25 Oct 2026 |
| [0024](0024-shimmer-replaces-reverse.md) | SHIMMER replaces REVERSE on ATTACK's third position | owner, 25 Oct 2026 |
| [0025](0025-size-and-time-crossfade.md) | HALL's and MASSIVE's SIZE, and TIME's predelay, move by crossfade (amended: four groups; phase 2 done, PLATE in ADR 0043) | owner, 25 Oct 2026 |
| [0026](0026-fitting-the-cpu.md) | Fitting the CPU: the HALL / MASSIVE tank at 24 kHz, a 12-point swell (amended: the candidate lap) | owner, 25 Oct 2026 |
| [0027](0027-wear-replaces-era.md) | WEAR replaces ERA: one wear amount over the Neve (amended v1.0.60: the band moves to MOTION) | owner, 26-27 Oct 2026 |
| [0028](0028-small-room-replaces-the-twin.md) | HALL's and MASSIVE's freeze twin is a small room | owner, 26-27 Oct 2026 |
| [0029](0029-panel-names.md) | The panel names: BLEND, WEAR, MOTION, DEPTH, ONSET, FREEZE / PAINT | owner, 27 Oct 2026 |
| [0030](0030-motion-tape-transport-and-stronger-wander.md) | MOTION: a tape transport on the left, a stronger Wander on the right (amended v1.0.59: the dips come in from ~7:30; v1.0.60: the right side is the radio band, ADR 0036) | owner, 27 Oct 2026 |
| [0031](0031-tone-resonant-tilt.md) | TONE gets a resonant tilt on the wet | owner, 27 Oct 2026 |
| [0032](0032-the-name-sublustris-versio.md) | The name: Sublustris Versio (was Immensio Versio); amended: the repo and folder renamed too | owner, 27 Oct 2026; amended 29 Oct 2026 |
| [0033](0033-governor-floor-21.md) | The endless tail settles 3 dB higher: the floor at −21 dBFS (amended v1.0.52: HALL's at −18) | owner, 28 Oct 2026 |
| [0034](0034-decay-range.md) | DECAY reaches longer tails before the hold: PLATE 30 s, HALL 60 s | owner, 28 Oct 2026 |
| [0035](0035-hall-long-tails.md) | HALL's long tails keep their upper mids and move a little more (its shelf at 6 kHz; its floor ×2 from 15 s) | owner, 29 Oct 2026 |
| [0036](0036-radio-band-on-motion.md) | The radio band moves from WEAR to MOTION's right side; Wander goes (amended v1.0.61: whole at endless DECAY too) | owner, 1 Nov 2026 |
| [0037](0037-canyon.md) | SPACE's third position is CANYON (was MASSIVE); the name only | owner, 1 Nov 2026 |
| [0038](0038-wear-without-the-clock.md) | WEAR without the BBD's clock: tape, breath and grain | owner, 2 Nov 2026 |
| [0039](0039-onset-centred.md) | The onset centred: HALL's and CANYON's early taps re-laid so the first moment sits evenly in the stereo field | owner, 3 Nov 2026 |
| [0040](0040-more-tape.md) | More tape: WEAR's tape driven 5 dB harder, a more worn cassette on MOTION's left (crinkle and dropouts from about 9:30) | owner, 3 Nov 2026 |
| [0041](0041-tape-drive-back.md) | The tape's drive back to 11.25 dB (reverses ADR 0040's drive); the worn cassette stays | owner, 3 Nov 2026 |
| [0042](0042-radio-lighter.md) | The radio's over-modulation 8 dB lighter on MOTION's right side: a faint edge | owner, 3 Nov 2026 |
| [0043](0043-plate-size-crossfade.md) | PLATE's SIZE by crossfade, like HALL and CANYON; small and frozen moves glide (ADR 0025 phase 2 done) | owner, 3 Nov 2026 |
| [0044](0044-wear-taper.md) | WEAR's taper: the wear arrives earlier on the knob (1 − (1 − v)^2.2), the grit from noon; full WEAR unchanged | owner, 5 Nov 2026 |
| [0045](0045-wear-drive.md) | The tape's drive back to 16.25 dB on the new taper (reverses ADR 0041's drive, amends ADR 0044) | owner, 5 Nov 2026 |
| [0047](0047-fuller-tail.md) | A fuller tail: the still lows in step with the main tank, more movement in the floor (HALL x2 easing to x1 at endless, CANYON x4 at x2 pace); PLATE unchanged | owner, 6 Nov 2026 |
