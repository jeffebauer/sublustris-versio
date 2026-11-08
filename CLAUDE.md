# Sublustris Versio

Called Immensio Versio until 27 Oct 2026 (ADR 0032): internal identifiers (`iv::`, `IV_*`, the plugin codes `ImVs` / `Ombr`, the bundle ID, the CMake names) keep the old name on purpose.

A huge, warm, lush reverb firmware for the Noise Engineering Versio (Daisy Seed, STM32H750), sibling to Resilio Versio. One DSP Core, three Hosts: Renderer (offline CLI), JUCE Plugin (AU/VST3 test bench), Versio Firmware.

**Start a session with `/session-start`, clear context mid-session with `/session-checkpoint`, end one with `/session-wrap`.** These are the shared skills; this project's facts for them (handoff, gates, hazards, push and branch rules) are in `.claude/session.md`. The handoff lives in `docs/handoff/HANDOFF.md`. The main session orchestrates (the owner, decisions, reviews, commits); prototypes, pages, debug and flash hunts and hardware-log reads go to nested sessions or subagents: read `docs/orchestrator.md` before the first delegation.

## Read first
- `SPEC.md`: the spec (frozen v1.0; changes via ADR + changelog line). `CONTEXT.md`: the glossary (use its terms in code, comments and chat). `docs/adr/`: every decision (index `docs/adr/README.md`). `docs/TASKS.md`: the owner's to-do list.
- `docs/research/`: sourced research (the DSP audit, Mutable Instruments' reverbs, testing practice, the owner's reference units). **Research how established units and papers do a thing before designing it** (REVERSE failed twice when improvised, 22 Oct 2026).
- `IMMENSIO_BRIEF.md` is the original brief; `docs/spec-session.md` the session that turned it into the spec. Both are history: the spec and ADRs win.

## The owner
A senior designer, new to DSP and embedded C++. Brings the ears and musical goals. Explain DSP in plain language. Ask technical decisions as musical questions ("should the tail fade or hold?"). The owner decides sound by ear: render a listening page (`tools/render_review_pages.sh <page>`) and let them listen before asking (that's how P7 and SW1 were settled, ADR 0014/0015). A page asking for a choice gets pick buttons and level-matching; a page that only shows settings is a showcase (`--showcase`: notes only). Keep `docs/TASKS.md` current whenever something changes for the owner, in the same commit.

## Hard rules (carried over from Resilio, each learned the hard way there)
- `core/` never includes libDaisy, JUCE or any host header. Every Host uses the one ParamSpec table (`core/params/ParamSpec.h`); mappings live in `core/params/`.
- Sound work happens on desktop first (Renderer, then Plugin); hardware is for profiling and final feel.
- **Flash is the tightest limit** (128 KB internal, ADR 0002): release ≤ 110 KB (112,640 B). Check `make -C firmware all-variants` after any Core change and record the sizes in the handoff.
- **Git:** stage explicit paths; never `git commit -a` / `-am`. `git apply --3way` also stages, so check `git status --short` for pre-staged files before any commit. Keep the main checkout on `main`; do branch work in a worktree. Commit after each meaningful step. End messages with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- **Releases and branches (ADR 0046):** `main` is what's released, since every push redeploys the site. A picked sound change goes out first as a beta from its own `beta/<topic>` branch (`tools/make_release.sh --beta <ref> <n> --publish`: a pre-release tagged `<sound>-beta.<n>`, never Latest, so the site's downloads don't move), with a pull request into `main` opened at beta 1 (linking the feedback, ADRs and pre-releases; no repeated text), and merges into `main` (the PR) for a full release only once players and the owner are happy; a dropped beta's PR is closed and its branch deleted, never its tag. Prototypes live on `proto/<topic>` (or the app's `claude/<topic>-<suffix>`) and aren't pushed; docs, tools and site work on `claude/<topic>` go to `main` with the owner's OK. Only `main` and `beta/` branches are pushed.
- **Gate on ctest's summary line, not its exit code:** a pipe (`| tail`) reports 0 even when suites fail. Log to a file and check `100% tests passed`.
- **Never delete or recreate `build/`**: agents and tools share `build/iv_render`. Experiments use a scratch build dir or a worktree.
- **Audio system:** never run `auval -a`, kill `AudioComponentRegistrar` or `coreaudiod`, or reset AU caches (it once froze CoreAudio and Ableton for Resilio). Validate only our AU (`auval -v aumf ImVs Ombr`), and only with Ableton closed.
- **Plugin in Ableton changes only on purpose:** dev builds don't install (`IV_INSTALL_PLUGIN` off). Install a milestone with `tools/install_plugin.sh <commit>`, then update the "Plugin installed" line in `docs/TASKS.md`.
- **Agent briefs** carry the list in `docs/orchestrator.md` ("Every brief states"). Review every agent's diff and re-run the gates yourself before committing.
- Never connect USB and rack power at once.
- **Licences (ADR 0006):** no Plateau Versio code (GPL-3.0), no unlicensed code (the Daisy Griesinger reverb), no Midiverb ROM microcode; thement/midiverb_emulator (GPL-3.0) is reference only. Reference recordings of commercial units (Supermassive, Mercury7) are never committed or published.

## Commands
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # once
cmake --build build && ctest --test-dir build > build/ctest.log 2>&1; grep 'tests passed' build/ctest.log
export PATH="$HOME/.local/arm-gnu-toolchain/bin:$PATH"
make -C libs/libDaisy -j8                                   # once
make -C firmware all-variants                               # release / m0test / profile, each <= 128 KB
python3 tools/make_stimulus.py                              # test_audio/stimulus (WAVs are not in git)
build/iv_render --sweep presets/sweeps/<name>.json --out-dir renders/<name>
python3 tools/review/make_review.py renders/<name>
.venv/bin/python tools/sweetspot.py                        # dead zones / cliffs per knob -> docs/sweetspot.md
IV_GOLDEN_UPDATE="why" build/iv_test_golden                # after a deliberate sound change + its SPEC changelog line
.venv/bin/python tools/make_material.py                     # listening material (python3 -m venv .venv; pip install numpy soundfile)
.venv/bin/python tools/make_reference_kit.py                # the kit for the owner's reference units; tools/ingest_reference.py reads them back
tools/install_plugin.sh <commit>                            # put a build in Ableton
tools/make_release.sh --beta <ref> <n> --publish            # a beta (pre-release) from a beta/<topic> branch; --publish alone releases main
```

## Where things are
`core/` DSP + params (`dsp/Engine.h` is the Core's one entry point) · `host/render`, `host/common`, `host/tests` · `plugin/` (+ `plugin/tests`) · `firmware/` (`README.md` for the variants and flash techniques) · `tools/` · `docs/` (ADRs, M0 check, prototypes, handoff) · `renders/` (gitignored).
