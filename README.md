# Sublustris Versio

*Formerly Immensio Versio (renamed 27 Oct 2026, [ADR 0032](docs/adr/0032-the-name-sublustris-versio.md)).*

A huge, warm, lush reverb for the [Noise Engineering Versio](https://noiseengineering.us/collections/versio) platform, for dub techno, ambient and dark ambient. Supermassive-scale tails with tape-era character, a freeze you can play over, and a swell or reverse on the switch. Sibling to [Resilio Versio](https://github.com/jeffebauer/resilio-versio).

**Status:** in development. Spec v1.0 frozen; M0 (toolchains and scaffold) built. Not yet a reverb.

| Panel | Function |
|---|---|
| P1 MIX | Dry/wet, equal-power |
| P2 DECAY | Tail length; past ~3 o'clock infinite, with ducking |
| P3 TONE | Darker or brighter tail with each pass |
| P4 ERA | Clean → Neve → cassette → μ-law → Midiverb, stacking |
| P5 SIZE | The space; in MASSIVE, long echoes → boundless wash |
| P6 DRIFT | Left: tape wow/flutter. Right: slow pitch drift |
| P7 TIME | Predelay / bloom time / reverse length |
| SW0 SPACE | PLATE · HALL · MASSIVE |
| SW1 ATTACK | NORMAL · BLOOM · REVERSE |
| Button / Gate | Tap: freeze. Hold while frozen: paint into the pad |

Details: [SPEC.md](SPEC.md), decisions in [docs/adr/](docs/adr/).

## Build

See [CLAUDE.md](CLAUDE.md) "Commands" and [firmware/README.md](firmware/README.md).

## Licence

Our code is MIT ([LICENSE](LICENSE)). The plugin binaries are AGPLv3 through JUCE. Third-party notices: [NOTICE](NOTICE). Not affiliated with or endorsed by Noise Engineering.
