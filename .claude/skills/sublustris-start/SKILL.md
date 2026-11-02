---
name: sublustris-start
description: Start a Sublustris Versio working session from a cold context: read the handoff and rules, check git, worktrees, build and installed-plugin state, and propose the next step. Use at the beginning of a session, after clearing context, or when the owner says "start", "pick up where we left off" or "what's next".
---

# /sublustris-start: pick up cold

The previous session left a handoff. Read it before touching anything; don't re-derive decisions it records.

## Steps

1. **Read, in order:** `CLAUDE.md` (rules, commands), `docs/handoff/HANDOFF.md` (state, in flight, next steps, what will bite), `docs/TASKS.md` (the owner's list, installed plugin, waiting-on-Claude), then the "Where to look" files the handoff names. The memory index (`~/.claude/projects/-Users-jesse-Documents-Sites-sublustris-versio/memory/MEMORY.md`) loads automatically; open any memory whose hook matches the next step. Skim `docs/adr/` titles for decisions newer than you'd expect.

2. **Check reality against the handoff.**
   ```bash
   git status --short
   git log --oneline -10
   git worktree list
   git branch --no-merged main
   git fetch -q && git status -sb | head -1   # ahead/behind origin (private GitHub repo)
   cat dist/installed_plugin.txt 2>/dev/null || echo "no plugin installed yet"
   test -x build/iv_render || echo "rebuild needed"
   ```
   ⚠ Uncommitted changes that the handoff doesn't explain are probably an agent's unreviewed work from the last session. Read the diff and the handoff's "In flight" notes before committing or discarding anything. Never discard without asking.

3. **Rebuild if needed** (don't delete `build/`):
   ```bash
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
   ```
   Run `ctest --test-dir build` if the handoff says gates were red or the tree has changed since. It takes ~5 minutes; run it in the background while you read.

4. **Check new owner input.** Look for new reference recordings (`test_audio/reference/*.wav`; no ingest tool yet, so listen and note them in `docs/TASKS.md`), ticked boxes or notes in `docs/TASKS.md`, and M0/M2 results the owner may mention.

5. **Report to the owner** in a short, plain-language message: where things stand (one line per active milestone), anything that surprised you in step 2, what's waiting on them, and your proposed next step with a one-line reason. Say which parts of it you'd hand to other sessions or subagents (see "Working as the orchestrator"). Then wait for their go-ahead, unless the handoff says the next step was already agreed.

## Working as the orchestrator

This session is the **orchestrator**: it talks to the owner, decides, reviews, commits and keeps `docs/TASKS.md` and the handoff. The long, noisy work goes to **nested sessions** (`start_session`) or **subagents** (Agent tool), so this thread's context lasts. Session 8 (26 Oct 2026) ran everything in one thread (hardware rounds, five prototypes and pages, flash hunts, a crackle hunt) and ran out of context. Most of that could have gone elsewhere.

**Keep here:** the owner's questions and picks (AskUserQuestion), reading a delegate's report and diff, re-running the gates on its result, commits with explicit paths, merges (the owner's OK first), plugin installs, the build paths to hand the owner, `docs/TASKS.md`, ADRs, SPEC lines and the handoff.

**Hand off:**
| Work | To | Why |
|---|---|---|
| A prototype behind a renderer hook plus its listening page (build, gates, click and energy checks, render, `_verify.txt`) | a nested session, in its own worktree off the working branch | self-contained; ends in a branch and a page; the owner may want to open it |
| A debug hunt (a click in a prototype, a crackle on the module) | a nested session, or a subagent if the answer is needed this turn | many builds and renders; only the cause and the fix matter here |
| A flash hunt (which symbols grew, what to trim, `nm -S` diffs) | subagent | returns a table and a recommendation |
| Reading a hardware round (the owner's `screenlog.0` against the last round's log) | subagent (a cheap model) | returns the comparison table, never the raw log |
| Research before designing (papers, established units: CLAUDE.md "Read first") | subagent | returns sources and a recommendation |
| Disk clean-ups, render-page housekeeping | subagent | lists what to move to the Trash; the owner OKs it here |

**When a session vs a subagent:** a **nested session** for a task of its own that ends in a branch or a page and that the owner might watch, steer or open (it runs on while you talk to the owner). A **subagent** for a read, a measurement or a search whose result you need for this turn's answer. Several independent pieces at once: one per piece, in parallel. Never two writers on the same files; writers get their own worktree.

**Every brief states** (CLAUDE.md "Agent briefs"): the goal in the owner's musical words; the files it owns; the branch to make its worktree from (by hand, off the working branch, not `main`: memory "agent worktrees off the right branch"); "commit only in your worktree branch, explicit paths, never `-a`"; never delete or recreate `build/` (use its own build dir); no system-wide Audio Unit commands; firmware built with `LIBDAISY_DIR=<the main checkout's libs/libDaisy>`; `python3 tools/make_stimulus.py` first in a fresh worktree; pages written to the main checkout's `renders/` (served from there), ~2 GB of disk at most. The gates to run and quote: ctest's "100% tests passed out of N" line from a log file, `tools/same_render.py`'s `SAME n/n` against the previous commit's renderer (bit-identical when off), `make -C firmware all-variants` sizes against the 110 KB budget (prototypes desktop-only: firmware byte-identical), a 1 ms-frame click check at every move, energy and infinite-hold checks for loop changes (memory "energy-check loop prototypes"). **What to send back:** a short report (what changed, the gate numbers, paths, open questions, what it couldn't check). No raw logs or file dumps.

**When it reports:** read its diff and re-run the cheapest decisive gate yourself (SAME n/n, or the firmware size) before telling the owner it's done. A delegate's claim is a lead, not a result: session 8's stagger prototype passed its first checks and still had three clicks.

**Hardware rounds:** the owner flashes and logs (`screenlog.0` in the main checkout; logging on with Ctrl-A, Shift-H). Here: wait for the run to finish with a background `until` loop. Elsewhere: a subagent compares it with the last round and files it in `docs/research/m3-round<N>.log`. Knob and switch checks need rack power and the LEDs, never serial (memory "USB power: knobs unpowered").

## Rules that matter at the start
Everything in `CLAUDE.md`'s Hard rules applies from the first command, especially: explicit-path commits, never delete `build/`, no system-wide Audio Unit commands (the owner may have Ableton open), and install plugin builds only via `tools/install_plugin.sh` with a `docs/TASKS.md` note.
