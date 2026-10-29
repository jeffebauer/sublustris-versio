---
title: "Sublustris Versio: a huge, warm, lush reverb for the Noise Engineering Versio"
description: "Firmware that turns the Noise Engineering Versio into a huge, warm reverb for ambient, dub techno and dark ambient: tails from a room to minutes long, an endless hold that keeps its notes, a freeze you can play over, an octave of shimmer and tape-style wear. Also an AU/VST3 plugin for macOS."
slug: ""
order: 1
---

# Sublustris Versio

**A huge, warm, lush reverb for the Noise Engineering Versio. Also as an AU/VST3 plugin for macOS.**

I wanted a reverb I could live inside. Tails long enough for dub techno, ambient and dark ambient, a freeze I can play over, and an endless setting that holds a chord for as long as I want without running away or turning metallic. There's tape-style wear too, and an octave of shimmer. It's firmware for the Versio, and the same sound runs as a plugin on a Mac.

*Sublustris*: Latin, "dimly lit, faintly glowing", the half-light of a clouded night.

<!-- HERO: placeholder until the renders exist (docs/minisite/README.md, "Renders to come"). -->

<!-- DEMOS: the demo player goes here (assets/audio/demos.json). -->

<!-- DOWNLOADS: two buttons, with version, date and size from the release data:
     "Firmware for Versio (.bin)" and "Plugin for macOS (AU/VST3)". Link: "How to install".
     "Coming soon" until the first public release. -->

## What it sounds like

- **Warm and lush, never metallic.** Every part of the reverb moves a little, all the time, so a long tail doesn't ring like a can. In HALL and MASSIVE the bass sits still underneath while everything above it moves.
- **From a room to minutes long, then endless.** DECAY runs from a short room to tails of half a minute in PLATE, a minute in HALL and two in MASSIVE. Past about 3 o'clock the tail is endless.
- **Three spaces.** A dense PLATE, a big HALL, and MASSIVE, which swells in slowly and grows into a boundless wash.
- **Tape-style wear.** One WEAR knob brings in tape saturation, a bucket-brigade delay's breathing, bit grit and a radio's narrow band, all at once. MOTION adds tape wow and flutter, or a quick, shimmering wander.

<!-- STORY: the owner picks the topic (BRIEF §4.6): the plate (EMT 140), the endless hold, the name, or the tape transport.
     Written once picked, with checked and cited sources. -->
<!-- OWNER: which story goes here? -->

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

## Its sibling

Sublustris has a sibling, [Resilio Versio](https://resilio-versio.vercel.app): a dub spring reverb and tape echo for the same module. Resilio is for throwing hits into springs. Sublustris is for long, deep spaces.

## Status

Sublustris isn't released yet. The firmware runs on real hardware and I play it at the rack, but the sound still changes from version to version.

I'm a designer, not a DSP engineer. I made every sound decision by ear, and Claude (Anthropic's AI) wrote the DSP, the firmware and the tools. [Credits](/credits)

<!-- OWNER: the feedback channel (GitHub Issues once the repository is public, as Resilio?). -->

Sublustris Versio is an independent project. It is not affiliated with or endorsed by Noise Engineering.
