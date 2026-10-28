# Sublustris Versio

Called Immensio Versio until 27 Oct 2026 (ADR 0032): the repo, folder and internal identifiers (`iv::`, `IV_*`, the plugin codes `ImVs` / `Ombr`, the bundle ID) keep the old name on purpose.

A huge, warm, lush reverb firmware for the Noise Engineering Versio (Daisy Seed, STM32H750), sibling to Resilio Versio. One DSP Core, three Hosts: Renderer (offline CLI), JUCE Plugin (AU/VST3 test bench), Versio Firmware.

**Start a session with `/sublustris-start`, end one with `/sublustris-wrap`.** The handoff lives in `docs/handoff/HANDOFF.md`. The main session orchestrates (the owner, decisions, reviews, commits); prototypes, pages, debug and flash hunts and hardware-log reads go to nested sessions or subagents (`/sublustris-start`, "Working as the orchestrator").

## Read first
- `SPEC.md`: the spec (frozen v1.0; changes via ADR + changelog line). `CONTEXT.md`: the glossary (use its terms in code, comments and chat). `docs/adr/`: every decision (index `docs/adr/README.md`). `docs/TASKS.md`: the owner's to-do list.
- `docs/research/`: sourced research (the DSP audit, Mutable Instruments' reverbs, testing practice, the owner's reference units). **Research how established units and papers do a thing before designing it** (REVERSE failed twice when improvised, 22 Oct 2026).
- `IMMENSIO_BRIEF.md` is the original brief; `docs/spec-session.md` the session that turned it into the spec. Both are history: the spec and ADRs win.

## The owner
A senior designer, new to DSP and embedded C++. Brings the ears and musical goals. Explain DSP in plain language. Ask technical decisions as musical questions ("should the tail fade or hold?"). When a control's job is in doubt, render a demo and let the owner listen before deciding (that's how P7 and SW1 were settled, ADR 0014/0015). Listening pages that only show settings are built with `make_review.py --showcase` (no "My pick", no blind mode, notes only); pick buttons appear only on pages that ask the owner to choose. Pages play through Web Audio (sample-locked versions, crossfaded switching, never a click); renders for them are 100 % wet with `--wet-only --dry` so the page's MIX fader blends the dry in live (`tools/render_review_pages.sh` does all of this). Keep `docs/TASKS.md` current whenever something changes for the owner, in the same commit.

## Hard rules (carried over from Resilio, each learned the hard way there)
- `core/` never includes libDaisy, JUCE or any host header. Every Host uses the one ParamSpec table (`core/params/ParamSpec.h`); mappings live in `core/params/`.
- Sound work happens on desktop first (Renderer, then Plugin); hardware is for profiling and final feel.
- **Flash is the tightest limit** (128 KB internal, ADR 0002): release ≤ 110 KB at v1.0. Check `make -C firmware all-variants` after any Core change and record the sizes in the handoff.
- **Git:** stage explicit paths; never `git commit -a` / `-am`. `git apply --3way` also stages, so check `git status --short` for pre-staged files before any commit. Keep the main checkout on `main`; do branch work in a worktree. Commit after each meaningful step. End messages with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- **Gate on ctest's summary line, not its exit code:** a pipe (`| tail`) reports 0 even when suites fail. Log to a file and check `100% tests passed`.
- **Never delete or recreate `build/`**: agents and tools share `build/iv_render`. Experiments use a scratch build dir or a worktree.
- **Audio system:** never run `auval -a`, kill `AudioComponentRegistrar` or `coreaudiod`, or reset AU caches (it once froze CoreAudio and Ableton for Resilio). Validate only our AU (`auval -v aumf ImVs Ombr`), and only with Ableton closed.
- **Plugin in Ableton changes only on purpose:** dev builds don't install (`IV_INSTALL_PLUGIN` off). Install a milestone with `tools/install_plugin.sh <commit>`, then update the "Plugin installed" line in `docs/TASKS.md`.
- **Agent briefs** state: owned files, "don't commit" (or commit in its worktree branch with explicit paths), never delete `build/`, never run system-wide AU commands, and musical targets in the owner's words. Review every agent's diff and re-run the gates yourself before committing.
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
```

## Where things are
`core/` DSP + params (`dsp/Engine.h` is the Core's one entry point) · `host/render`, `host/common`, `host/tests` · `plugin/` (+ `plugin/tests`) · `firmware/` (`README.md` for the variants and flash techniques) · `tools/` · `docs/` (ADRs, M0 check, prototypes, handoff) · `renders/` (gitignored).
