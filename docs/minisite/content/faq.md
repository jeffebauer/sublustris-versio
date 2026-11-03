---
title: "FAQ: Sublustris Versio"
description: "Short answers about Sublustris Versio: how it differs from Resilio Versio and Plateau Versio, which modules it runs on, how hard it works the chip, going back to stock firmware, the plugin on macOS, stereo, CV and the source code."
slug: "faq"
order: 6
---

# FAQ

### What is it?
A huge, warm, lush reverb. It's alternative firmware for the Noise Engineering Versio Eurorack module, and the same sound runs as an AU/VST3 plugin on macOS.

### Is it made by Noise Engineering?
No. Sublustris Versio is an independent project, not affiliated with or endorsed by Noise Engineering. It runs on their open Versio platform and installs with their Firmware Swap web app, which accepts custom firmware.

### How is it different from Resilio Versio?
They're siblings, by the same maker, for the same module. [Resilio](https://resilio-versio.vercel.app) is a dub spring reverb with a tape echo, made for throwing single hits into springs and riding the filter. Sublustris is a big, deep space for long tails: three spaces, endless decay, freeze and paint, shimmer and wear. You can swap between them with the Firmware Swap app.

### How is it different from Plateau Versio?
Plateau Versio is another reverb firmware for the same platform, built around a Dattorro plate. Sublustris's PLATE follows the same classic plate design (Dattorro's 1997 paper is public), but none of Plateau's code is in it, and most of Sublustris lives elsewhere: the HALL and CANYON spaces, the endless hold, freeze and paint, and WEAR.

<!-- OWNER: check the line about Plateau Versio's character before publishing; describe it only as you know it. -->

### Which modules does it run on?
The Noise Engineering Versio. All Versio modules share one hardware platform with different panels and firmware, so your printed labels will be those of whichever Versio you have. The [panel map](/manual#panel-map) shows which control is which.

### Is flashing safe? Can I go back?
Yes. You install it with Noise Engineering's own Firmware Swap app, and the same app takes you back to any Noise Engineering firmware. Sublustris doesn't install a bootloader or change anything else on the module. There's one rule: **never connect USB and Eurorack power at the same time.** [Install guide](/install)

### How hard does it work the chip?
Hard. A reverb this big is a lot of sums for a small processor, and its busiest moments (a big CANYON, frozen, with someone playing over it) take most of what the chip has. Its busiest moments are timed on the module itself, and the sound was fitted to the chip so it doesn't run out. If you ever hear crackle, tell me the settings: that's a bug.

<!-- OWNER: say more (a number) only once the release build's CPU is measured for the first public release. The late-block light exists only in a diagnostic build, not the release. -->

### Is it free?
Yes. The firmware and plugin are free, and the source will be open. My code is MIT licensed, and the plugin is under AGPLv3 because it's built on JUCE.

### Does the plugin run on Apple Silicon and Intel?
It's built as one plugin for both, for macOS 12 or newer, as Audio Unit and VST3. I play it on an Apple Silicon Mac; if you try it on an Intel Mac, I'd like to hear how it goes. There's no Windows or Linux version.

### Why does macOS block the plugin?
It's free, and Apple hasn't notarised it. Two Terminal lines on the [install page](/install#in-a-daw-macos) let macOS open it.

### Does the plugin sound like the module?
They run the same sound engine with the same controls, so a setting in the plugin is a setting on the module. The plugin is the project's desktop twin, laid out like the module's panel.

### Can it run away?
No. Even endless, the tail settles at a steady level and holds there, and an output limiter catches any peaks. A freeze holds the pad at its own level.

### Is it stereo?
Stereo in and out. Left and right enter the reverb at different points, and your dry signal stays stereo. If you only patch In L, it feeds both sides.

### Can I control everything with CV?
All seven knobs have CV inputs (0–5 V, added to the knob). The gate input freezes and paints, with the same gestures as the button.

### Does it save presets?
No. The module's state is just its knobs and switches. The [starting points](/presets) are written out so you can dial them in by hand.

### Is it a sample or an impulse response?
Neither. It's a set of feedback loops, delay lines that feed back into each other, voiced by ear. There are no samples in it.

### Can I read the code?
It's at [github.com/jeffebauer/sublustris-versio](https://github.com/jeffebauer/sublustris-versio). It includes the full specification and the reasons behind each design decision.

### How do I give feedback or report a bug?
On GitHub Issues, with a free GitHub account. I'd really like to hear from you, because Sublustris gets better through the people playing it. There are two short forms.

- **[Suggest a sound](https://github.com/jeffebauer/sublustris-versio/issues/new?template=sound_idea.yml)** if there's a sound, a move or a control you'd like, or one that could feel better. A reference helps (a track, a recording, another reverb), and so does a note on how close Sublustris gets today.
- **[Report a bug](https://github.com/jeffebauer/sublustris-versio/issues/new?template=bug_report.yml)** if something clicks, crackles, glitches, crashes or doesn't do what its name says. Tell me whether it was the Versio or the plugin, which version, and the settings in panel names (BLEND, DECAY, SPACE, ONSET…). A short recording helps most.

Plain impressions are useful too, like how it sounds next to reverbs you know, or a knob that feels dead or too touchy. I read every issue, and I try out sound ideas and listen to them before anything changes. When a change comes from someone playing it, the [changelog](/changelog/) credits them.
