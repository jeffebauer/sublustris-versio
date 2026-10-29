# Prompt: build the Sublustris Versio minisite

Paste everything below the line into a **new** Claude Code session started in **`~/Documents/Sites/sublustris-versio`**. If the folder hasn't moved yet, use `~/Documents/Sites/immensio-versio`; the folder moves at the end of the session that wrote this prompt.

---

Let's build the Sublustris Versio minisite together: a sister site to the Resilio Versio site we built (`https://resilio-versio.vercel.app`). The brief is `docs/minisite/BRIEF.md`; read it first and follow it.

## Read first
1. **This repo's rules and state:** `CLAUDE.md` (the rules bind you), then run `/sublustris-start`, but take only what the website needs. Don't pick up sound work: other sessions do that.
2. **The brief:** `docs/minisite/BRIEF.md`.
3. **Resilio's site, the model:**
   - its brief, `~/Documents/Sites/resilio-versio/docs/minisite/BRIEF.md`;
   - its package, `docs/minisite/README.md`;
   - its design, `docs/minisite/design/DESIGN-v2.md`, `DESIGN.md`, `PLAN.md` and `tokens.css`;
   - the site's build notes, `site/README.md`, and the `site/` code;
   - its demo tools, `docs/minisite/tools/`;
   - and the live site, in the built-in browser: home, `/manual`, `/install`, `/style`.

   Read Resilio's repository; never write to it.
4. **Sublustris's facts:**
   - `SPEC.md`: §3 the panel, §2 the sound target, the changelog at the top;
   - `CONTEXT.md`;
   - `docs/adr/`;
   - `README.md`.

## How we'll work
**I'm the owner:** a designer, new to web build tooling, with good ears. I decide on pages I can look at or listen to, one question at a time, phrased visually or musically. Show me; don't ask me to imagine.

**Work in phases. Stop for my pick at the end of each phase, and don't start the next one until I've picked.**
1. **Content.**
   - Draft the seven pages in `docs/minisite/content/` from the brief and the project's facts. Mark anything you aren't sure of `<!-- OWNER: … -->`; never invent.
   - Draft the demo list (8-10 scenes, each with its settings in panel names).
   - Make the panel diagram SVG with our panel names (adapt Resilio's `make_site_panel_svg.py`).
   - Show me the copy as a readable page (an Artifact or a local preview) and the demo list as a table.
2. **Demos.**
   - Render the approved scenes with this repo's renderer on material the project may publish, never the owner's reference recordings.
   - Loudness-match them, encode them, and write `demos.json` (captions, settings, transcripts).
   - Put them on a showcase listening page (`tools/review/make_review.py --showcase`) for my approval. Redo what I flag.
3. **Style tile.**
   - Two or three visual directions for "dimly lit, faintly glowing" (brief section 10), each a `/style` page: palette with contrast ratios, type, header, tiles, a few new pictograms and tiny diagrams, an audio tile, placeholders for the renders.
   - Show me each in the browser at desktop and phone widths, with screenshots, and I pick.
4. **Build.**
   - Copy Resilio's `site/` into this repo as the base and refactor it, rather than starting over: same structure, our tokens, pages, content collection, demos, release lookup with `DOWNLOADS_LIVE` off, and placeholders for every render (named slots listed in `docs/minisite/README.md`).
   - Resilio's code is MIT and mine; note the provenance in `NOTICE`.
   - Run it locally with the built-in browser's preview (`.claude/launch.json`), and walk me through it.
5. **Ready to host.**
   - Write the exact Vercel settings for me (Resilio's `site/README.md` "Vercel settings").
   - I create the project and the domain myself.

## Rules for this job
- **Work in a worktree on a branch `feat/site`** made by hand off `main`. Commit there with explicit paths, never `git commit -a`. Don't merge into `main` or push without my OK.
- **Stay out of `core/`, `firmware/`, `plugin/` and `host/`.** The website reads the project; it doesn't change it.
- Use the renderer read-only (`build/iv_render`). **Never delete or recreate `build/`,** and render into your own folders.
- **Never commit:**
  - font files (MD UI / MD IO: fetched by the prebuild, per Resilio's `site/README.md`);
  - reference recordings;
  - `renders/`;
  - anything whose licence doesn't allow publishing.
- **Don't do these yourself:** create Vercel projects, domains, environment variables or secrets; make the repository public; run system-wide Audio Unit commands; or install the plugin.
- **Hand the long, noisy work to subagents or nested sessions,** and keep the decisions and my questions here (`/sublustris-start`, "Working as the orchestrator"). That covers demo renders, copy fact-checks, the style-tile directions and the build's accessibility and Lighthouse runs. Review their work before showing me.
- **Quality gates before you show me a build:**
  - `npm run build` and `npx astro check` clean;
  - axe-core with no violations on every page;
  - Lighthouse mobile: Accessibility 100, Performance ≥ 90;
  - no horizontal scroll at 320 and 375 px;
  - everything visible without JavaScript;
  - reduced motion respected;
  - a keyboard pass of the header, the audio tiles and the annotated panel.
- **Keep `docs/TASKS.md` current** whenever something changes for me, in the same commit.

## At each stop, report in plain words
- what's ready;
- where to look;
- what you need from me (one question);
- what you couldn't check.

No raw logs.
