---
title: "Credits: how Sublustris Versio was made"
description: "Who made Sublustris Versio and how: designed by ear, built on published reverb research, timed on the module itself, made with libDaisy and JUCE. References, influences, acknowledgements and trademarks."
slug: "credits"
order: 7
---

# Credits

## How it was shaped

I'm [Jesse Bauer](https://jessebauer.xyz), a designer, and I designed Sublustris Versio by ear. I set the musical goals and made every sound decision. Claude (Anthropic's AI) wrote the DSP, the firmware and the tools.

The spaces follow published designs. PLATE follows Jon Dattorro's plate (1997). HALL and MASSIVE are feedback delay networks after Jean-Marc Jot: eight delay lines feeding back into each other through a mixing matrix. Every line moves a little, all the time, as David Griesinger did in the Lexicon 224, so long tails don't ring metallic. Each was refined against the literature and against recordings of reverbs I play. I used those recordings for listening and measurement only, and none of them is in Sublustris or on this site.

I heard each change before it shipped. Every candidate sound became a listening page next to the version before it, usually level-matched, and I picked. More than thirty of those decisions are written up with their reasons in the source repository.

Sound work happens on the desktop first, in an offline renderer and the plugin. The module is where I time the busiest moments and judge the final feel. The firmware fits in the Versio's 128 KB of flash.

## Influences

These are the reverbs whose character shaped Sublustris, named only to say where its ideas came from. It isn't a copy of any of them.

- Valhalla Supermassive and VintageVerb
- Meris Mercury7
- Eventide Blackhole
- Vongon Ultrasheer
- Alesis Midiverb
- Lexicon 224
- EMT 140

## The demos

<!-- OWNER: filled in at the demos stop: every clip is Sublustris itself, rendered offline by its own renderer, and the samples' sources and licences are credited here (as Resilio's page does). -->

## References

- J. Dattorro, "Effect Design, Part 1: Reverberator and Other Filters", *Journal of the Audio Engineering Society* 45(9), 660–684, 1997.
- J.-M. Jot and A. Chaigne, "Digital Delay Networks for Designing Artificial Reverberators", AES 90th Convention, Paris, 1991.
- S. J. Schlecht and E. A. P. Habets, "Time-Varying Feedback Matrices in Feedback Delay Networks and Their Application in Artificial Reverberation", *Journal of the Acoustical Society of America* 138(3), 1389–1398, 2015.
- V. Välimäki, J. D. Parker, L. Savioja, J. O. Smith and J. S. Abel, "Fifty Years of Artificial Reverberation", *IEEE Transactions on Audio, Speech, and Language Processing* 20(5), 1421–1448, 2012.

<!-- OWNER/verify: the references are checked against docs/research/ before publishing (the fact-check pass). -->

## Thanks

- Noise Engineering, for the open Versio platform, the Firmware Swap app, and the printable panel template I drew the panel diagram from.
- Electro-Smith, for the Daisy Seed and libDaisy.
- JUCE, which the plugin is built with.

## Licence

My code, tools and docs are MIT licensed. The plugin is AGPLv3, because it's built on JUCE. The source will be on [GitHub](https://github.com/jeffebauer/sublustris-versio) once the repository is public.

## Trademarks

Versio is a Noise Engineering product, and the Daisy Seed an Electro-Smith one. Valhalla, Meris, Eventide, Vongon, Alesis, Lexicon and EMT are trademarks of their owners. Ableton Live and Audio Units belong to their owners; VST is a trademark of Steinberg Media Technologies GmbH. All product and company names are trademarks or registered trademarks of their respective owners, named here only to describe compatibility and influences. Sublustris Versio is an independent project, not affiliated with or endorsed by any of them.
