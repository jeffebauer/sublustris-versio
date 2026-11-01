# Sublustris Versio minisite: brief

The public home for Sublustris Versio, built as a **sister site** to Resilio Versio's (`https://resilio-versio.vercel.app`, source in `~/Documents/Sites/resilio-versio/site/` and `docs/minisite/`). It says what the site must **say** and **do**; the visual design is decided with the owner on a style tile (section 10). Written 29 Oct 2026, at v1.0.59.

**Start from Resilio's brief** (`resilio-versio/docs/minisite/BRIEF.md`) and its design revision 2 (`docs/minisite/design/DESIGN-v2.md`, `tokens.css`). This brief only states where Sublustris differs. Where it's silent, Resilio's rules hold: accessibility, motion, performance, fonts, releases.

## 1. Purpose

Sublustris Versio is a huge, warm, lush reverb: alternative firmware for the Noise Engineering Versio Eurorack module, plus an AU/VST3 plugin for macOS that runs the same sound engine. The site's jobs, in order:

1. **Make someone hear it** within ten seconds: long tails, the endless hold, a freeze played over.
2. **Explain what it does** in the language of ambient, dub techno and dark ambient players, not a DSP engineer's.
3. **Get it onto their module or into their DAW** safely. Downloads stay "coming soon" until the first public release (section 8).
4. **Be the manual** people keep open next to the rack.
5. **Sit next to Resilio** as a sibling: the same family, its own character, each site linking to the other.

## 2. Audience

| Who | What they need first |
|---|---|
| **Versio owners** | What it sounds like; that it's safe to try and easy to undo; the panel map (their printed labels won't match) |
| **Plugin users without a Versio** (macOS producers) | Sound, the plugin download, the macOS install steps |
| **Ambient, drone, dub-techno and dark-ambient players** | That the moves they want are built in: endless tails that don't turn metallic, freeze and paint, swells, an octave shimmer, tape wear and warble |
| **Resilio's visitors** | That this is its sibling, and how the two differ (a spring and tape echo for dub; a big, deep space for long tails) |

## 3. Pages

The same seven as Resilio, in the same order of importance: **Overview, Manual, Install, Starting points, Changelog, FAQ, Credits**.

- The copy lives in `docs/minisite/content/*.md`, one file per page, with front-matter (`title`, `description`, `slug`, `order`), and the site reads it at build time.
- **Unlike Resilio's, this copy doesn't exist yet.** Writing it is part of the job (section 7).

## 4. Landing page, top to bottom

1. **Name + one line:** "Sublustris Versio: a huge, warm, lush reverb for the Noise Engineering Versio" (and "also as an AU/VST3 plugin for macOS"). The name's meaning in a line: Latin *sublustris*, "dimly lit, faintly glowing", the half-light of a clouded night.
2. **Hero:** a placeholder until the renders exist (section 9).
3. **Listen:** the demo tiles (section 6).
4. **Get it:** the two downloads, in their "coming soon" state.
5. **What it sounds like:** four short points (warm and lush, never metallic; tails from a room to minutes long, then endless; three spaces; tape-era wear).
6. **A story section,** like Resilio's "Big Knob". One page of history behind a control, with a sourced fact or two. Candidates for the owner to pick from:
   - **the plate:** the EMT 140, a steel sheet in a box, and why studios loved it;
   - **the endless hold:** infinite reverb, from tape loops to the Lexicon 224 and freeze pedals;
   - **the name:** *sublustris* in Virgil, the half-lit night;
   - **the tape transport:** wow, flutter and dropouts.

   Sources must be checked and cited; nothing unverified.
7. **The panel:** annotated, with each control's name and one line (section 5). It uses the panel diagram until the front-on render exists.
8. **Features:** bento tiles, for example:
   - "3 spaces" and "minutes-long tails";
   - "endless, still notes";
   - "freeze and paint";
   - "an octave of shimmer";
   - "one WEAR knob";
   - "tape on the left, radio on the right";
   - "7 CV inputs".
9. **Two ways to play:** on the module and in your DAW.
10. **Download band, then the footer:** the not-affiliated line, the licence, and links to the source and to Resilio Versio.

## 5. The panel and the words for it

**Panel names in capitals, exactly as printed:**

| Control | Positions or words |
|---|---|
| BLEND | dry ↔ wet, equal-power; fully right is 100 % wet |
| DECAY | a short room to minutes-long tails; endless from about 3 o'clock |
| TONE | darker or brighter, noon flat |
| WEAR | one amount of tape, BBD and bit wear over a warm Neve colour |
| SIZE | the space's size; in MASSIVE, long echoes to a boundless wash |
| MOTION | left: tape (wow, flutter, crinkle, dropouts near fully left); right: an old AM radio's narrow band with a wandering notch (since 1 Nov 2026; was a wander); noon: a faint, still movement |
| DEPTH | what the ONSET does: NATURAL's predelay, BLOOM's swell time, SHIMMER's amount |
| SPACE switch | PLATE · HALL · MASSIVE |
| ONSET switch | NATURAL · BLOOM · SHIMMER |
| Button | FREEZE / PAINT: tap to freeze, hold while frozen to paint |
| Gate | FREEZE, the same gestures as the button |
| LEDs | input and output meters; a cool blue under the output pair while frozen, pulsing slowly while painting |

- **Never the old names:** Immensio, MIX, ERA, DRIFT, TIME, ATTACK, NORMAL, REVERSE, except where the changelog explains history.
- **The facts come from the project, not memory:**
  - `SPEC.md` §3 (every control, its ranges, clock positions), §4.4 (endless) and the button/gate section;
  - `CONTEXT.md` (the vocabulary);
  - the ADRs;
  - the changelog at the top of `SPEC.md`.

  Check each fact against `core/params/` where SPEC is ambiguous. If a fact needs the owner, mark it `<!-- OWNER: … -->`.
- **Some numbers to use** (check before quoting):
  - DECAY reaches about 30 s in PLATE, 60 s in HALL, 120 s in MASSIVE, then endless;
  - DEPTH's predelay goes up to 250 ms;
  - CV is 0-5 V, added to the knob.

## 6. Demos

Eight to ten clips, 15-30 s each, as on Resilio: a title, a caption, the settings in panel names, a transcript and a duration, all in `assets/audio/demos.json`. Loudness-matched (Resilio used about −20.5 LUFS), MP3, never autoplay.

- **Material:** our own renders of Sublustris (the renderer `build/iv_render`, or the plugin).
  - Use material the project may publish: `tools/make_material.py`, `test_audio/stimulus`, or samples whose licence allows it. Read how Resilio's `docs/minisite/tools/demos/v2/` sourced and credited its samples, and follow the same rules.
  - **Never the owner's reference recordings** (the Valhalla takes in `test_audio/reference/`), and never a commercial unit's sound.
- **Scenes to cover** (the owner chooses the final list):
  - an ambient pad into an endless MASSIVE tail that keeps its notes;
  - a freeze played over, then painted;
  - a dub-techno chord stab into HALL, ducking under the kick;
  - PLATE on a drum loop;
  - a BLOOM swell;
  - SHIMMER on a slow melody;
  - WEAR up on a guitar;
  - MOTION's tape side and its radio side;
  - a dark-ambient drone with SIZE moving.
- **The owner listens and approves each clip before it ships,** on a showcase page (CLAUDE.md: `tools/review/make_review.py --showcase`).

## 7. Copy

- **Write the seven pages fresh in `docs/minisite/content/`, in Resilio's tone** (its brief, section 5): plain, musical, short sentences, active voice, second person, clock positions, no hype words, no algorithm names. The owner is a designer with good ears. Use the words they've used in their picks and notes (the TASKS history and SPEC changelog quote them).
- **Use the same markers as Resilio:** `<!-- DEMOS -->`, `<!-- DOWNLOADS -->`, `<!-- PANEL -->`, `<!-- SCREENSHOT -->`, `<!-- LATEST -->`; `<!-- OWNER: … -->` for questions, never published.
- **The changelog** is for players: what changed for the ear, newest first. Group the development history into a few paragraphs (it isn't released yet), and keep the SPEC changelog's technical detail out.
- **The FAQ should answer at least these:**
  - how it differs from Resilio, and from Plateau Versio (another reverb for the same platform);
  - which Versio modules it runs on (`<!-- OWNER -->` until confirmed);
  - how hard it works the chip, said plainly. Note that the late-block light exists only in a diagnostic build, not in release; check `firmware/README.md` before saying anything about the LEDs and CPU.
  - the plugin on Apple Silicon and Intel;
  - going back to stock firmware.

## 8. Downloads and releases

- **Today:** no public release, and the repository `jeffebauer/sublustris-versio` is private. Build the buttons for the same flow as Resilio (its brief, section 9; `site/src/lib/release.ts`): GitHub's releases API read at build time, stable asset names, and a single `DOWNLOADS_LIVE` switch, off for now.
- **Proposed stable names:** `sublustris-versio-firmware.bin` and `sublustris-versio-plugin-macos.zip` (plus `SHA256SUMS.txt`), as Resilio's `tools/make_release.sh` uploads.
  - `tools/make_release.sh` here builds only the friend package today. Teaching it the release upload is a separate, small job; propose it, don't do it unasked.
- **System requirements:**
  - Firmware: a Versio, Chrome, a micro-USB data cable.
  - Plugin: macOS on Apple Silicon or Intel. The minimum macOS version is `<!-- OWNER/verify -->` from the plugin build's deployment target.
- **The safety line** wherever flashing is mentioned: *Never connect USB and Eurorack power at the same time.*

## 9. Images and placeholders

**The custom panel art isn't designed yet,** so the Blender renders Resilio's site uses (the hero film, the close-up, the front-on panel, the patched module; Resilio's `site/renders/`) don't exist for Sublustris.

- **Build every image slot with a clear placeholder:** Resilio's `Placeholder` component, labelled with what will go there and its aspect ratio. Swapping in a real render must be dropping a file into `docs/minisite/assets/renders/` with the agreed name, nothing else.
- **List the slots, names and sizes** in `docs/minisite/README.md` under "Renders to come".
- **The panel diagram** (linework, to scale, our panel names) can be made now: adapt Resilio's `docs/minisite/tools/make_site_panel_svg.py` and its coordinates (`docs/panel/versio_panel_coordinates.csv` there). The Versio's geometry is the same; only the labels change.
- **No Noise Engineering product photography or logo.**

## 10. Design: a sister, not a copy

- **Keep Resilio's structure and components:** the three-column header, centred reading column, bento tiles, annotated panel, audio tiles, pictograms and tiny diagrams, accessibility and motion rules, fonts (MD UI / MD IO, licensed: never committed, fetched by the prebuild) and the performance budget.
- **Give it its own character:** Resilio is a bright dub tool, white body with a hot signal red. Sublustris is "dimly lit, faintly glowing".
- **Propose two or three directions on a style tile** (Resilio's `/style` page is the model), each with a palette, its accent, how the dark bands and tiles feel, and contrast ratios. Directions to explore:
  - a dark-first page with a faint glow accent;
  - Resilio's white body with a cool dusk accent;
  - a night-blue band system.

  The owner picks before any page is built.
- **New pictograms and tiny diagrams for Sublustris's features:**
  - a long decay envelope that holds;
  - a freeze (a held waveform);
  - a swell;
  - an octave above;
  - a tape wobble;
  - three space sizes.

  Drawn on Resilio's grid in the same style; not copied from anyone.
- **The family resemblance is intended:** the same logo construction and header, each site linking to the other in the footer.

## 11. Must-nots

All of Resilio's (its brief, section 7), adapted. In particular:

- **No implied endorsement** by Noise Engineering, Valhalla DSP or anyone else. Name influences only descriptively, in the credits: Valhalla Supermassive and VintageVerb, Meris Mercury7, Eventide Blackhole, Vongon Ultrasheer, Alesis Midiverb, Lexicon 224, EMT 140.
- **No claim that it sounds like any of them,** and no unverified claims about modules, DAWs, macOS versions or signing.
- **No reference recordings of commercial units** anywhere on the site.
- **No autoplay,** and no audio on hover.
- **The plugin is the project's free desktop twin,** as-is, not a supported product.

## 12. Hosting

- **Same stack as Resilio:** Astro, static output, a Vercel project with Root Directory `site`, and "Include files outside the Root Directory" on.
- **The owner creates the Vercel project and any domain.** The building session must not create projects, domains, environment variables or secrets, or make the repository public. It writes the exact settings for the owner instead (Resilio's `site/README.md` "Vercel settings").
- **The address,** for example `sublustris-versio.vercel.app`, is the owner's choice.

## 13. Open items for the owner

These live in `docs/minisite/README.md` under "Before launch":

- the design direction;
- the story section's topic;
- the final demo list and each clip's approval;
- the Versio modules played on;
- the macOS minimum;
- the feedback channel;
- the Vercel project and address;
- the renders, once the panel art exists;
- the go-public switch.
