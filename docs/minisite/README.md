# Sublustris Versio minisite package

Everything the Sublustris Versio minisite says and shows: the brief (`BRIEF.md`), the copy, the demos and the panel diagram. A sister site to Resilio Versio's (`https://resilio-versio.vercel.app`); the site itself is in this repo's `site/` (Astro, static, on Vercel), built from a copy of Resilio's (`site/README.md`).

| Path | What it is |
|---|---|
| `BRIEF.md` | What the site must say and do, and where it differs from Resilio's |
| `content/*.md` | The copy, one file per page, with front-matter (`title`, `description`, `slug`, `order`): `overview`, `manual`, `install`, `presets`, `changelog`, `faq`, `credits`. **Draft** (29 Oct 2026, at v1.0.59), for the owner's review |
| `demos-plan.md` | The eight demo scenes, their material and settings (approved 29 Oct 2026) |
| `assets/panel/sublustris-versio-panel.svg` | Panel diagram, to scale, panel names only, themeable |
| `panel/versio_panel_coordinates.csv` | The Versio's hole positions, from Noise Engineering's printable panel template (copied from Resilio's `docs/panel/`) |
| `tools/make_site_panel_svg.py` | Makes the panel diagram (run from the repo root) |

Markers in the copy: `<!-- DEMOS -->`, `<!-- DOWNLOADS -->`, `<!-- PANEL -->`, `<!-- SCREENSHOT -->`, `<!-- LATEST -->`, `<!-- HERO -->`, `<!-- STORY -->` say where a component goes. `<!-- OWNER: … -->` marks a question for the owner: never publish it, never invent the answer.

## Assets: usage notes

- **Panel SVG.** `role="img"` with `<title>` and `<desc>` built in. Colours are CSS custom properties with fallbacks, so inline it and set them per theme: `--sv-panel`, `--sv-ink`, `--sv-ink-soft`, `--sv-knob`, `--sv-metal`, `--sv-led`, `--sv-accent` (the button), `--sv-font`. As an `<img>`, use the alt text: *"Sublustris Versio panel map: BLEND and DECAY at the top with four level LEDs between them, TONE, WEAR, SIZE, MOTION and DEPTH below, the SPACE and ONSET switches, the FREEZE / PAINT button, and twelve jacks."* Geometry is from Noise Engineering's printable Versio panel template; the diagram credits it in small type, keep that.

## Renders to come

The custom panel art isn't designed yet, so there are no Blender renders. Every image slot on the site is a labelled placeholder. To fill one, drop the file into `docs/minisite/assets/renders/` under its name below; nothing else changes. Sizes follow Resilio's renders.

| Slot | File | Size (px) | Aspect | Where |
|---|---|---|---|---|
| Hero still | `hero-still.jpg` | 2400 × 1350 | 16:9 | Home, hero (and the poster for the film) |
| Hero film | `hero-loop.mp4` + `hero-loop.webm` | 2400 × 1350 | 16:9 | Home, hero (optional; the still stands in) |
| Front-on panel | `front-on-wide.jpg` | 2116 × 1190 | 16:9 | Home and manual, the annotated panel |
| Panel close-up | `panel-macro.jpg` | 2400 × 1028 | 21:9 | Home, a feature band |
| Patched module | `patched.jpg` | 2400 × 1350 | 16:9 | Home, "Two ways to play" |
| Open Graph image | `og-hero.jpg` | 1200 × 630 | 1.91:1 | Link previews |
| Plugin screenshot | `plugin-panel.png` | 2x of the plugin at 1x | the plugin's | Manual, "In the plugin" |

The plugin screenshot can be made now from the plugin; the rest wait for the panel art.

## Before launch (for the owner)

1. ~~**The design direction.**~~ Direction B, white with the freeze LED's blue, and the small diagrams picked (owner, 29–30 Oct 2026).
2. ~~**The story section's topic.**~~ The endless hold (owner, 29 Oct 2026); the sourced draft to come.
3. ~~**The demos.**~~ Eight clips, approved on their listening page (owner, 29 Oct 2026); `assets/audio/demos.json`.
4. **Which Versio module(s)** it has been played on: not sure yet (29 Oct 2026), so the FAQ names none.
5. ~~**The macOS minimum.**~~ Built for macOS 12+, Apple Silicon and Intel; played on Apple Silicon only, and the copy says so (29 Oct 2026).
6. ~~**The feedback channel.**~~ GitHub Issues with two forms, as Resilio (29 Oct 2026); the forms are in `.github/ISSUE_TEMPLATE/`.
7. **The Vercel project and address** (for example `sublustris-versio.vercel.app`): created by the owner.
8. **The renders,** once the panel art exists.
9. **Go-public switch.** Download links and the source link are live only once the repository is public and the first release is out (`DOWNLOADS_LIVE`). `tools/make_release.sh` builds only the friend package today; teaching it the release upload (`sublustris-versio-firmware.bin`, `sublustris-versio-plugin-macos.zip`, `SHA256SUMS.txt`) is a separate small job.
