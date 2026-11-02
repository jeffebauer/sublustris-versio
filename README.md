# Sublustris Versio

**A huge, warm, lush reverb for the Noise Engineering Versio.** I wanted a reverb I could live inside: Supermassive-scale tails for dub techno, ambient and dark ambient, with tape-era wear, a freeze I can play over, and an endless setting that holds a chord for as long as I want without running away or turning metallic. It's alternative firmware for the Versio Eurorack module, and the same sound runs as an AU/VST3 plugin on macOS.

*Sublustris*: Latin, "dimly lit, faintly glowing", the half-light of a clouded night. By [Jesse Bauer](https://jessebauer.xyz). Not affiliated with or endorsed by Noise Engineering. A sibling to [Resilio Versio](https://github.com/jeffebauer/resilio-versio).

> **Status:** in development, not yet released. The firmware runs on real hardware and I play it at the rack, but the sound still changes from version to version. There are no public downloads yet.

## What it is

Sublustris is alternative firmware for the [Versio](https://noiseengineering.us/products/versio), Noise Engineering's open Eurorack platform. It replaces the module's firmware entirely, and you can go back just as easily with Noise Engineering's own Firmware Swap web app.

It's built for long, deep tails. Three spaces sit on the SPACE switch, and each one is its own instrument:

- **PLATE** is a dense plate with a quick attack.
- **HALL** is a big room, tuned against the Valhalla rooms and plates I play.
- **CANYON** is long lines and mass, like a space with no roof: a gentle swell that starts with the sound, and SIZE runs from a long wash to a boundless one. (It was called MASSIVE until 1 November 2026; the code and the plugin still use that name until the rename lands.)

And the rest of the panel:

- **DECAY** runs from short rooms to minutes-long tails. Past about 3 o'clock the tail is endless. While you play, it follows you at the level of the longest tail just below, so crossing into endless doesn't jump; when you stop, it holds where it is and keeps its notes instead of dissolving into haze.
- **WEAR** is one amount of wear over a warm, desk-like colour: tape saturation, a bucket-brigade delay's breathing, the delay chip's clock, bit grit and a soft hiss deepen together. It only touches the reverb. Your dry signal stays clean.
- **MOTION** moves the tail both ways from noon. Left is tape: wow, flutter and crinkle, with dropouts near the end of the knob. Right is an old AM radio: the tail goes through its narrow band, with a slow notch wandering through the middle, and the pitch stays put. At noon, only a faint movement that keeps the tail from ringing.
- **ONSET** picks how the sound enters: NATURAL, BLOOM (the tail swells in over DEPTH) or SHIMMER (an octave up joins the tail).
- **The button and gate freeze** the tail into a still pad that you can keep playing over. Hold the button while frozen to paint new layers into the pad.

There are seven knobs, each with a CV input, plus two switches, a button and a gate. Every control is described in [`SPEC.md` §3](SPEC.md).

## How it was made

I designed Sublustris by ear. I'm a designer, and I set the musical goals and made every sound decision. Claude (Anthropic's AI) wrote the DSP, the firmware and the tools. The plate follows Dattorro's design and the hall and canyon spaces are feedback delay networks after Jot. Each was refined against the literature and against recordings of the reverbs I use.

Nothing changed in the sound until I'd heard it: every option went onto a listening page next to the version before it, usually level-matched, and I picked. More than thirty decisions are written up with their reasons in [`docs/adr/`](docs/adr/), and every version's change is in the changelog at the top of [`SPEC.md`](SPEC.md).

## For builders

One DSP core with no platform code, and three hosts: an offline renderer (WAV in, WAV out), the JUCE plugin and the Versio firmware (libDaisy). They all read the same parameter table, so a setting in the plugin sounds the same on the module. The firmware fits in the Versio's 128 KB of internal flash, with its busiest moments timed on the module itself.

- The specification is [`SPEC.md`](SPEC.md), the vocabulary is in [`CONTEXT.md`](CONTEXT.md), and the decisions are in [`docs/adr/`](docs/adr/).
- Firmware variants, CPU runs and the flash budget are in [`firmware/README.md`](firmware/README.md).
- The build commands are in [`CLAUDE.md`](CLAUDE.md), under "Commands".

## Licence

- **Code, tools and docs:** MIT, see [`LICENSE`](LICENSE).
- **Plugin binaries:** AGPLv3. The plugin is built on [JUCE](https://juce.com), used under its AGPLv3 licence (full text in [`LICENSES/AGPL-3.0.txt`](LICENSES/AGPL-3.0.txt)).
- **Firmware:** my MIT code with libDaisy (MIT), the STM32 HAL (BSD-3-Clause) and CMSIS (Apache-2.0). No JUCE, and no Noise Engineering code.
- Third-party notices: [`NOTICE`](NOTICE).

## Acknowledgements

- Reverb design: J. Dattorro ("Effect Design, Part 1"), J.-M. Jot and A. Chaigne (feedback delay networks), D. Griesinger and the Lexicon 224 (modulated delays against metallic ringing), and S. Schlecht and V. Välimäki's work on modulated FDNs. Full references are in [`docs/research/`](docs/research/).
- The reverbs whose character shaped it: Valhalla Supermassive and VintageVerb, the Meris Mercury7, the Eventide Blackhole, the Vongon Ultrasheer and the Alesis Midiverb.
- [Electro-Smith](https://electro-smith.com) for libDaisy, and [Noise Engineering](https://noiseengineering.us) for the open Versio platform.

## Trademarks

Versio is a trademark of Noise Engineering. VST is a registered trademark of Steinberg Media Technologies GmbH. Audio Units is a trademark of Apple Inc. Valhalla, Meris, Eventide, Vongon, Alesis and Lexicon are trademarks of their owners; their names are used only to describe influences.
