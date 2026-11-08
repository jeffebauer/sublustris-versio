# Session file: Sublustris Versio

The facts `/session-start`, `/session-checkpoint` and `/session-wrap` run on. The procedure is the
shared skills (`~/Documents/Sites/session-skills`); this file wins where they conflict. Rules that
hold all session long are `CLAUDE.md`'s Hard rules, and they apply from the first command.

## Handoff
- Path: docs/handoff/HANDOFF.md
- Template: docs/handoff/TEMPLATE.md (open it and write from it)
- Owner list: docs/TASKS.md. The "Last updated" line stays one sentence: replace it each
  time (date, and where the owner's next step is), so the to-dos stay at the top of the page. The
  "Plugin installed" line and "Waiting on Claude" also change often; ticked items move to Done with
  the date.

## Read first
- `docs/TASKS.md`, then the handoff's "Where to look" files.
- `docs/adr/README.md`: skim the titles for decisions newer than the handoff mentions. ⚠ Picks on a
  `beta/` branch have ADRs there, not yet on `main`.

## Reality checks
```bash
cat dist/installed_plugin.txt 2>/dev/null || echo "no plugin installed yet"   # the commit in Ableton
test -x build/iv_render || echo "rebuild needed"
gh pr list --state open --json number,title,headRefName,mergeable              # one PR per beta (ADR 0046)
gh release list --limit 3                                                      # Latest vs pre-releases
```
- The installed plugin and `main`'s HEAD can differ on purpose (dev builds never install): report
  both, and a mismatch with `docs/TASKS.md`'s "Plugin installed" line.
- A PR's comments from players are owner input: read them in step 4.

## New input
- `docs/TASKS.md`: ticked boxes and notes.
- `test_audio/reference/` and `test_audio/reference/incoming/`: new reference recordings (no ingest
  tool for some: listen and note them in `docs/TASKS.md`).
- `screenlog.0` in the main checkout: a newer timestamp than the last round's log in
  `docs/research/` means the owner ran a hardware round. ⚠ It appends across flashes: copy it, never
  move it, and slice one lap START→DONE.
- The owner's Ableton set for reference takes (memory "reference takes from Ableton"); forum threads
  come as screenshots (reddit is blocked in the built-in browser).

## Environment checks
```bash
export PATH="$HOME/.local/arm-gnu-toolchain/bin:$PATH"; arm-none-eabi-gcc --version | head -1
test -d test_audio/stimulus || echo "stimulus missing"
test -x .venv/bin/python || echo ".venv missing"
lsof -nP -iTCP:8765 -sTCP:LISTEN | tail -1    # the listening-page server; note which checkout it serves
df -h . | tail -1                              # renders need room: ~2 GB per rendering agent
```
- Ableton may be open at any time, with the installed plugin in a set: leave it and the audio system
  alone.
- The page server must serve the main checkout's `renders/`. Started from a worktree that is now
  removed, it serves nothing: restart it from `main` before the next page.

## Safe repairs
```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build   # never delete build/
python3 tools/make_stimulus.py                                                   # WAVs are not in git
python3 -m venv .venv && .venv/bin/pip install numpy soundfile                   # only if missing
```
Run ctest (about 5 minutes, in the background, logged to `build/ctest.log`) when the handoff says
gates were red or the tree changed since its gate numbers.

## Ask first
- Installing a plugin build (`tools/install_plugin.sh <commit>`), and any AU validation (only
  `auval -v aumf ImVs Ombr`, only with Ableton closed).
- Merging into `main`, pushing, publishing a release or beta.
- Deleting worktrees, branches, renders or `dist/` builds; the owner OKs a clean-up list first.
- Anything the owner does at the rack: they flash and log; this session reads the log.

## Hazards
- ⚠ **Flash headroom is in bytes, not KB:** the handoff's Gates give what's left. A change that adds
  code brings a trim plan with it.
- ⚠ **`tools/make_release.sh` reads `releases/README.md` from the checkout it runs in**, so betas run
  from the beta worktree, and `--dry-run` needs the branch pushed.

## Gates
```bash
cmake --build build && ctest --test-dir build > build/ctest.log 2>&1; grep 'tests passed' build/ctest.log
export PATH="$HOME/.local/arm-gnu-toolchain/bin:$PATH"; make -C firmware all-variants
cat dist/installed_plugin.txt 2>/dev/null
```
- Record: ctest's "100% tests passed out of N" line (slow: ~5 minutes; run it in the background);
  each firmware variant's bytes and % (release against the 112,640 B budget, profile against
  the 131,072 B chip); the installed plugin's commit against `main`'s HEAD; the same for
  any `beta/` branch whose sound differs from `main`.
- There is no remote CI: these are the only gates.

## Docs to sync
- `docs/TASKS.md` (see Handoff), in the same commit as the change it reflects.
- Each decision this session: an ADR in `docs/adr/` with its line in `docs/adr/README.md`, a SPEC
  changelog line for a sound change, `CONTEXT.md` for new vocabulary.
- One golden update per SPEC version (`IV_GOLDEN_UPDATE="why" build/iv_test_golden`).
- A new hard-learned rule: a feedback memory, and ask whether it also belongs in `CLAUDE.md`'s Hard
  rules.

## Push
- Remote: origin (`github.com/jeffebauer/sublustris-versio`, public).
- What a push does: to `main`, it is the release line and Vercel rebuilds the site (only when
  `site/` or `docs/minisite/` changed); to a `beta/` branch, it updates that beta's PR.

## Branches
`CLAUDE.md`'s Git and "Releases and branches" rules. Worktrees go under `.claude/worktrees/`.

## Delegation
on. This session orchestrates: the owner, decisions, reviews, commits, `docs/TASKS.md` and the
handoff. Prototypes, pages, debug and flash hunts and hardware-log reads go to nested sessions or
subagents. What goes where, what every brief states and how to review the result:
`docs/orchestrator.md`. Read it before the first delegation of the session.

## Extensions
- **start step 6:** report one line per active milestone (M0-M9) and say which parts of the next step
  go to sessions or subagents.
- **wrap step 7:** the handoff's milestone table (in the template).
