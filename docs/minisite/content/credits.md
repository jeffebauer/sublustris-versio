---
title: "Credits: how Sublustris Versio was made"
description: "Who made Sublustris Versio and how: designed by ear, built on published reverb research, timed on the module itself, made with libDaisy and JUCE. References, influences, acknowledgements and trademarks."
slug: "credits"
order: 7
---

# Credits

## How it was shaped

I'm [Jesse Bauer](https://jessebauer.xyz), a designer, and I designed Sublustris Versio by ear. I set the musical goals and made every sound decision. Claude (Anthropic's AI) wrote the DSP, the firmware and the tools.

The spaces follow published designs. PLATE follows Jon Dattorro's plate (1997). HALL and CANYON are feedback delay networks after Jean-Marc Jot: eight delay lines feeding back into each other through a mixing matrix. Every line moves a little, all the time, as David Griesinger did in the Lexicon 224, so long tails don't ring metallic. Each was refined against the literature and against recordings of reverbs I play. I used those recordings for listening and measurement only, and none of them is in Sublustris or on this site.

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

Every clip on this site is Sublustris itself, rendered offline by its own renderer, with a dry and a wet version so you can blend between them. Most parts are synthesised in code: the pad, the leads, the poly synth, the chord stabs and the drone. The rest use free, CC0 samples:

- Drums from [Virtuosity Drums](https://github.com/sfzinstruments/virtuosity_drums) by Versilian Studios and Karoryfer.
- [Emilyguitar](https://github.com/sfzinstruments/karoryfer.emilyguitar) by Karoryfer.
- The Steinway B grand piano from the [Versilian Community Sample Library](https://github.com/sgossner/VCSL) by Versilian Studios.

There are no recordings of other reverbs.

## The endless hold

The sources for the story on the home page:

- Robert Fripp, interviewed in *Musician* (1982), quoted in "Robert Fripp on the device that made him feel truly liberated", *Guitar Player*: the two Revoxes, 1972, and the name Frippertronics (1977). [guitarplayer.com](https://www.guitarplayer.com/guitarists/robert-fripp-on-the-device-that-made-him-feel-truly-liberated)
- Sean Costello, "Early tape loop experiments with Eno, Fripp, Terry Riley, and Pauline Oliveros", Valhalla DSP, 2010: how the two-machine loop is wired. [Archived copy](https://web.archive.org/web/2015/https://valhalladsp.com/2010/05/26/early-tape-loop-experiments-with-eno-fripp-terry-riley-and-pauline-oliveros/)
- "Brian Eno is More Dark Than Shark", on Eno's covers (October 2009): the operational diagram on the back of *Discreet Music*. [moredarkthanshark.org](https://moredarkthanshark.org/eno_int_print-oct09.html)
- Paul White, "Illusions of Space", an interview with David Griesinger, *Recording Musician*, August 1992: the 224's reverb in the PCM70. [muzines.co.uk](https://www.muzines.co.uk/articles/illusions-of-space/2902)
- Curtis Schwartz, "Lexicon PCM 70 Digital Effects Processor", *International Musician & Recording World*, February 1986: the Infinite Reverb program and the held chord. [muzines.co.uk](https://www.muzines.co.uk/articles/lexicon-pcm-70-digital-effects-processor/6783)
- Electro-Harmonix, Freeze product page, and James Lewin, "New Electro Harmonix Pedal Freezes Time", SonicState, 19 July 2010. [ehx.com](https://www.ehx.com/products/freeze/), [sonicstate.com](https://sonicstate.com/news/2010/07/19/new-electro-harmonix-pedal-freezes-time/)
- Strymon, Cloudburst support page: infinite takes in new notes, freeze doesn't. [strymon.net](https://www.strymon.net/support/cloudburst/)

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
- [Paper](https://paper.design), for Paper Mono, the monospaced type on this site (SIL Open Font License 1.1).

## Licence

My code, tools and docs are MIT licensed. The plugin is AGPLv3, because it's built on JUCE. The source is on [GitHub](https://github.com/jeffebauer/sublustris-versio).

## Trademarks

Versio is a Noise Engineering product, and the Daisy Seed an Electro-Smith one. Valhalla, Meris, Eventide, Vongon, Alesis, Lexicon, EMT, Electro-Harmonix, Strymon and Revox are trademarks of their owners. Ableton Live and Audio Units belong to their owners; VST is a trademark of Steinberg Media Technologies GmbH. All product and company names are trademarks or registered trademarks of their respective owners, named here only to describe compatibility and influences. Sublustris Versio is an independent project, not affiliated with or endorsed by any of them.
