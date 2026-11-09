# Working as the orchestrator

The main session is the **orchestrator**: it talks to the owner, decides, reviews, commits, and keeps
`docs/TASKS.md` and the handoff. Long, noisy work goes to **nested sessions** (`start_session`) or
**subagents** (the Agent tool), so the main thread's context lasts the day. Session 8 (26 Oct 2026)
ran hardware rounds, five prototypes and pages, flash hunts and a crackle hunt in one thread, and ran
out of context; most of that belonged elsewhere.

## What stays here
The owner's questions and picks (AskUserQuestion), reading a delegate's report and diff, re-running
the gates on its result, commits with explicit paths, merges (the owner's OK first), plugin installs,
the build paths to hand the owner, `docs/TASKS.md`, ADRs, SPEC lines and the handoff.

## What goes where
| Work | To | Why |
|---|---|---|
| A prototype behind a renderer hook plus its listening page (build, gates, click and energy checks, render, `_verify.txt`) | a nested session, in its own worktree off the working branch | self-contained; ends in a branch and a page; the owner may want to open it |
| A debug hunt (a click in a prototype, a crackle on the module) | a nested session, or a subagent if the answer is needed this turn | many builds and renders; only the cause and the fix matter here |
| A flash hunt (which symbols grew, what to trim, `nm -S` diffs) | subagent | returns a table and a recommendation |
| Reading a hardware round (the owner's `screenlog.0` against the last round's log) | subagent (a cheap model) | returns the comparison table, never the raw log |
| Research before designing (papers, established units) | subagent | returns sources and a recommendation |
| Disk clean-ups, render-page housekeeping | subagent | lists what to move to the Trash; the owner OKs it here |

A **nested session** suits a task of its own that ends in a branch or a page, which the owner might
watch, steer or open while you talk to them. A **subagent** suits a read, a measurement or a search
whose result this turn's answer needs. Independent pieces run in parallel, one delegate each; each
writer gets its own worktree, so no two writers share files.

## Every brief states
- The goal in the owner's musical words.
- The files it owns.
- The branch its worktree comes from: made by hand off the working branch, which is often a `beta/`
  branch rather than `main` (memory "agent worktrees off the right branch").
- Commit only in its worktree branch, explicit paths, never `-a`.
- Its own build dir; the shared `build/` stays untouched (never deleted or recreated).
- No system-wide Audio Unit commands.
- Firmware built with `LIBDAISY_DIR=<the main checkout's libs/libDaisy>`.
- `python3 tools/make_stimulus.py` first in a fresh worktree.
- Pages written to the main checkout's `renders/` (served from there), ~2 GB of disk at most.
- The gates to run and quote:
  - ctest's "100% tests passed out of N" line, from a log file;
  - `tools/same_render.py`'s `SAME n/n` against the previous commit's renderer (bit-identical when
    the hook is off);
  - `make -C firmware all-variants` sizes against the budget (prototypes are desktop-only, so the
    firmware stays byte-identical);
  - a 1 ms-frame click check at every move;
  - energy and infinite-hold checks for loop changes (memory "energy-check loop prototypes").
- What to send back: a short report (what changed, the gate numbers, paths, open questions, what it
  couldn't check), with no raw logs or file dumps.

## When a delegate reports
Read its diff and re-run the cheapest decisive gate yourself (SAME n/n, or the firmware size) before
telling the owner it's done. Session 8's stagger prototype passed its first checks and still had
three clicks.

## Hardware rounds
The owner flashes and logs (`screenlog.0` in the main checkout; logging on with Ctrl-A, Shift-H).
Here: wait for the run to finish with a background `until` loop. Then a subagent compares it with the
last round and files it as `docs/research/m3-round<N>.log`. Knob and switch checks need rack power and
the LEDs, never serial (memory "USB power: knobs unpowered").
