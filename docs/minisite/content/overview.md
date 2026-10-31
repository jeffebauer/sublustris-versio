---
title: "Sublustris Versio: a huge, warm, lush reverb for the Noise Engineering Versio"
description: "Firmware that turns the Noise Engineering Versio into a huge, warm reverb for ambient, dub techno and dark ambient: tails from a room to minutes long, an endless hold that keeps its notes, a freeze you can play over, an octave of shimmer and tape-style wear. Also an AU/VST3 plugin for macOS."
slug: ""
order: 1
---

# Sublustris Versio

**A big, dark reverb for ambient and dub techno. Turn the decay up and a chord holds forever while you play over it. Then let it age, from warm tape saturation to a thin, crackly radio, wavering like a cassette on a tired deck.**

<!-- The lede (owner's pick, 31 Oct 2026, hero study lede Q; replaced lede 2). The hero adds "For the Noise Engineering Versio, and as an AU/VST3 plugin for macOS." under it. -->

I wanted a reverb I could live inside. Tails long enough for dub techno, ambient and dark ambient, a freeze I can play over, and an endless setting that holds a chord for as long as I want without running away or turning metallic. There's tape-style wear too, and an octave of shimmer. It's firmware for the Versio, and the same sound runs as a plugin on a Mac.

*Sublustris*: Latin, "dimly lit, faintly glowing", the half-light of a clouded night.

<!-- HERO: placeholder until the renders exist (docs/minisite/README.md, "Renders to come"). -->

<!-- DEMOS: the demo player goes here (assets/audio/demos.json). -->

<!-- DOWNLOADS: two buttons, with version, date and size from the release data:
     "Firmware for Versio (.bin)" and "Plugin for macOS (AU/VST3)". Link: "How to install".
     "Coming soon" until the first public release. -->

## What it sounds like

## The endless hold

Infinite reverb began as a trick with two tape machines.

In 1972 Robert Fripp spent an evening with Brian Eno, who "had a system of recording with two Revoxes." The tape ran from the first machine to the second, and the second machine's playback fed back into the first one's record head. Each sound came round again a few seconds later, a little quieter each time. Fripp played into it, and the result became *(No Pussyfooting)*. By 1977 his version had a name: Frippertronics. In 1975 Eno released *Discreet Music* and printed a diagram of his echo setup on the back cover.

A reverb works the same way inside. Each trip round its loop loses a little level, and that loss is the decay. Set the loop to lose nothing and the tail goes on forever. By 1986 Lexicon's PCM70, which used David Griesinger's reverb from the Lexicon 224, had a program called Infinite Reverb. One reviewer held the reverb of a single note, then added a second to build a chord.

Pedals brought it to the floor. The Electro-Harmonix Freeze (2010) holds any note or chord while you press its footswitch. Reverbs now often offer two versions: infinite keeps taking in what you play, and freeze holds what's there and lets nothing new in.

**Sublustris has both.** Past about 3 o'clock, DECAY makes the tail endless. It settles at a steady level and ducks a little under new playing, and in PLATE and HALL its notes hold still when you stop. FREEZE holds the tail as a still pad you play over. Hold the button while frozen and it paints a little of what you play into the pad.

[Sources](/credits#the-endless-hold)

## Built for long tails

- **The endless hold.** Turn DECAY past about 3 o'clock and the tail never fades. It settles at a steady level, steps back about 3 dB while you play and comes back when you stop. Stop playing and, in PLATE and HALL, its notes stay notes: they hold still instead of dissolving into haze. MASSIVE keeps its slow wash.
- **Freeze and paint.** Tap the button and the tail freezes into a still pad. Play over it, and what you play rings in a quieter layer of its own on top. Hold the button while frozen and a little of that layer is painted into the pad, and stays.
- **Swells.** Set ONSET to BLOOM and the reverb swells in behind each sound, over anything from a breath to four seconds. MASSIVE swells a little on its own.
- **An octave of shimmer.** ONSET's SHIMMER sends what you play into the reverb an octave up. It's one steady octave, in tune, that doesn't climb.
- **Tape on the left, wander on the right.** MOTION's left side bends the whole tail like a tape transport, with crinkle and dropouts near the end. Its right side is a quick, wide wander, every voice of the tail moving on its own.

[Read the manual](/manual)

## The panel

Sublustris runs behind your Versio's printed panel, so the labels on your module won't match. Here's what's where.

<!-- PANEL: assets/panel/sublustris-versio-panel.svg -->

- **4 LEDs** Meter the input and output; the output pair turns blue while frozen
- **7 knobs** BLEND, DECAY, TONE, WEAR, SIZE, MOTION, DEPTH
- **2 switches** SPACE (PLATE · HALL · MASSIVE) and ONSET (NATURAL · BLOOM · SHIMMER)
- **1 button** FREEZE / PAINT
- **7 CV inputs** One per knob, 0–5 V, added to the knob's position
- **1 gate input** FREEZE, the same gestures as the button

## Two ways to play

**On the module.** Patch it after a voice, a drum machine or the end of your mix. Ride DECAY into endless, tap a freeze on the chord you want to keep, and send CV to SIZE or MOTION for slow changes.

**In your DAW.** The same sound engine runs as an Audio Unit and VST3 plugin for macOS, laid out like the module's panel. MIDI notes play the freeze, so a clip can freeze and paint in time. You don't need a Versio to use it.

## Safe to try

Sublustris replaces the module's firmware completely, and it's just as easy to go back. You install it with Noise Engineering's own Firmware Swap web app, and the same app puts any Noise Engineering firmware back on. Sublustris doesn't change anything else on the module.

[How to install](/install)

## Help shape it

If something sounds wrong, or you want a sound it can't make yet, tell me. Ideas from the people playing it are how Sublustris gets better.

- **Play it.** On the module or in your DAW, with your own sounds.
- **Tell me.** Open a GitHub issue with an idea for the sound, or a bug.
- **Hear it change.** I try ideas by ear, and credit the ones that ship.

## Meet Resilio Versio

Sublustris has a sibling, [Resilio Versio](https://resilio-versio.vercel.app): a dub spring reverb and tape echo for the same module. Resilio is for throwing hits into springs. Sublustris is for long, deep spaces.

## Status

Sublustris isn't released yet. The firmware runs on real hardware and I play it at the rack, but the sound still changes from version to version. Once it's public, I'd like to hear what you think on [GitHub Issues](https://github.com/jeffebauer/sublustris-versio/issues).

I'm a designer, not a DSP engineer. I made every sound decision by ear, and Claude (Anthropic's AI) wrote the DSP, the firmware and the tools. [Credits](/credits)

Sublustris Versio is an independent project. It is not affiliated with or endorsed by Noise Engineering.
