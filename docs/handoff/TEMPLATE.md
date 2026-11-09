<!--
  Template for docs/handoff/HANDOFF.md. /session-wrap writes from this file (named in
  .claude/session.md § Handoff); /session-checkpoint edits the result in place, by these section
  names (the shared skills' names: keep them).

  OVERWRITTEN every wrap, never appended. /session-start checks every claim against git, so a stale
  or guessed line becomes a contradiction. Write "unsure" where you are, "nothing" in an empty
  section, and delete every guidance comment.

  Where things do NOT go (each outlives a session, and two copies drift):
    · rules, commands, traps that always apply → CLAUDE.md
    · the owner's to-dos and picks            → docs/TASKS.md
    · decisions                                → docs/adr/
    · mechanics, preferences, history          → memory files
    · start/wrap facts                         → .claude/session.md
-->

# Handoff

**Written:** <d Mon yyyy>, end of session <N> (<dates>).

<!-- Two to six bullets: what this session did, for someone who wasn't here. -->

Start with `/session-start`.

## Where things stand

| Milestone | State |
|---|---|
| Spec | <!-- version on main; versions on beta/ branches and their PRs --> |
| M0 | |
| M1/M4-M8 | |
| M2 | <!-- plugin installed: commit, version, date --> |
| M3 | <!-- what's on the module, profiled or not --> |
| M9 | <!-- Latest release; betas published --> |

<!-- main HEAD sha; what's committed, pushed, merged, released: separate claims. A red gate first. -->

## Start here

<!-- The single most useful next action and why, concrete enough to begin (files, criteria, ADRs).
     A decision only the owner can make is still the action. Then the rest, ordered, one line each. -->

## Waiting on Jesse

<!-- Decisions, rack tasks, recordings; point at docs/TASKS.md items rather than restating them. -->

## In flight

<!-- Running or unfinished delegates, and what to check. Branches and worktrees kept on purpose, each
     with why. "Nothing running" is a real answer. -->

## Will bite

<!-- Thin margins (flash bytes left, CPU), tools that gave a plausible wrong number, environment
     surprises, anything that surprised this session. -->

## Gates

<!-- ctest's "tests passed" line, firmware bytes and % per variant, the installed plugin against
     main's HEAD; each with the sha it ran at. The same for a beta branch whose sound differs. -->

## Where to look

<!-- The 3-6 files the next session should read first for "Start here". -->
