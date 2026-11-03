# Reddit (r/modular): launch post

Draft for the owner, 3 Nov 2026. Modelled on the Resilio Versio r/modular post, which was the Modwiggler post in Markdown without the thanks to the community. This one is the Sublustris Modwiggler draft in the same way. Not posted.

## Where and how

- **Sub:** r/modular. Image post with the render, or a text post with the image inside it.
- **Format:** Reddit Markdown. Switch the editor to Markdown mode before pasting, or the links show up as plain `[text](url)`.
- **Image:** `docs/minisite/assets/renders/front-on-wide.jpg`, the same render as on Modwiggler. Reddit's Markdown can't embed an image from a link. To add it to the published text post, edit the post, switch to the rich-text editor, put the cursor before "Hi all," and use the image button to upload the file (r/modular has to allow images in text posts). If the button isn't there, use the link line at the top of the body instead.
- **Flair:** whichever the sub uses for releases or firmware.
- **Self-promotion:** the first paragraph says what it is and that it's free, and the links go to the site and the source. Answering comments for the first hour or two helps it more than anything in the post.

## Title

Sublustris Versio: free deep reverb firmware for the Noise Engineering Versio, with an endless hold, freeze and a macOS plugin

## Body

```
[Sublustris Versio on the module (picture)](https://raw.githubusercontent.com/jeffebauer/sublustris-versio/main/docs/minisite/assets/renders/front-on-wide.jpg)

Hi all,

Some of you may have seen Resilio Versio, my dub spring reverb for the Versio. Its sibling is out today. Sublustris Versio is a big, dark reverb for ambient, dub techno and dark ambient, the reverb I wanted to sit inside for a whole track. Like Resilio, it's free, open source alternative firmware for any Versio module, and the same sound runs as an AU/VST3 plugin on macOS, so you can try it without reflashing anything.

**What it does**

It's built for long tails and for playing over them.

* Past about 3 o'clock, DECAY goes endless. While you play, the tail follows you at the level of the longest tail below it, so nothing jumps as you cross over. When you stop, it holds the chord, and in PLATE and HALL the notes stay notes instead of smearing into haze.
* Tap the button or send a gate and the tail freezes into a still pad. Whatever you play on top rings in a quieter layer of its own, and holding the button while frozen paints a little of that layer into the pad, where it stays. In the plugin, MIDI notes play the freeze, so a clip can freeze and paint in time.
* SPACE picks the room. PLATE is a dense plate with a quick attack, HALL is a big room that starts right behind the note, and CANYON is long lines and mass, a space with no roof that swells in with each sound.
* ONSET sets how each sound enters. NATURAL comes in at once, BLOOM swells in over anything from a breath to four seconds, and SHIMMER adds one steady octave on top that stays in tune and doesn't climb.
* MOTION bends the tail both ways from noon. To the left it's a cassette on a tired deck, with wow, flutter, crinkle and dropouts that get worse the further you go. To the right it's an old AM radio, its narrow band with a slow notch wandering through, so the tail phases.
* WEAR ages the reverb with tape saturation, the breathing of a bucket-brigade delay, bit grit and a soft hiss, over a warm desk colour. It only colours the reverb. Your dry signal is never touched.

Seven knobs, each with a CV input, two switches, a button and a gate, stereo in and out. It installs with Noise Engineering's own Firmware Swap web app, and you can go back to any of their firmware the same way. It's an independent project, not affiliated with or endorsed by Noise Engineering.

If you know Plateau Versio, Sublustris's PLATE follows the same public Dattorro paper, but none of Plateau's code is in it, and most of what Sublustris does lives in the other spaces, the endless hold, freeze and WEAR.

**A bit of background**

As with Resilio, I'm a designer, not a DSP engineer. I made every sound decision by ear, and Claude (Anthropic's AI) wrote the DSP, the firmware and the tools. The plate follows Dattorro's design, and HALL and CANYON are feedback delay networks after Jot. The reverbs whose character shaped it are the ones I play, like Valhalla Supermassive, the Meris Mercury7 and the Eventide Blackhole. Nothing changed in the sound until I'd heard it next to the version before, and more than forty decisions are written up with their reasons in the repo if you're curious.

**Links**

* Website, with audio demos you can blend dry to wet: [sublustris-versio.vercel.app](https://sublustris-versio.vercel.app/)
* Download the firmware and the plugin (v1.0.68): [sublustris-versio.vercel.app/#download](https://sublustris-versio.vercel.app/#download)
* Install guide: [sublustris-versio.vercel.app/install](https://sublustris-versio.vercel.app/install/)
* Manual: [sublustris-versio.vercel.app/manual](https://sublustris-versio.vercel.app/manual/)
* Eight starting points, from an endless CANYON pad to a dark drone: [sublustris-versio.vercel.app/presets](https://sublustris-versio.vercel.app/presets/)
* Source code: [github.com/jeffebauer/sublustris-versio](https://github.com/jeffebauer/sublustris-versio)

**I'd really like your feedback**

This is the first public release, and I want to keep shaping it with the people playing it. If a tail sounds wrong, if a knob feels dead or too touchy, or if there's a sound you'd want it to make, please tell me. Replies here are great. If you have a GitHub account, there are two short forms for a [sound idea](https://github.com/jeffebauer/sublustris-versio/issues/new?template=sound_idea.yml) or a [bug report](https://github.com/jeffebauer/sublustris-versio/issues/new?template=bug_report.yml). A short recording and your settings help a lot, and ideas that make it into a release get credited in the changelog.

One safety note if you flash it: never have USB and Eurorack power connected at the same time.

Thanks for listening, and I hope it finds a place in your rack!
```
