# 0046 — Sound changes go out as betas from their own branch, each with a pull request; branch and tag names

**Status:** Accepted, 5 Nov 2026 (owner: "If we published from branches, then it would mean we could throw away a branch instead of committing to main and potentially removing it again if we wanted to double back on any changes"). Amends the release process in `tools/make_release.sh` (go-live, 3 Nov 2026). Amended the same day (owner, "I agree"): each beta has a pull request.

**Context:**
- v1.0.68 is public. The site's download buttons fetch `releases/latest/download/<name>`, and every push to `main` redeploys the site, so `main` is what players get and what its docs describe.
- The first changes from players' feedback (lfad on Modwiggler, 4 Nov 2026: WEAR's taper, the tape's drive; the loudness and "thin/reedy" investigations) may need more tuning with those players before they're final.
- GitHub has no alpha or beta release types, but any release can be a **pre-release**: it's badged, never marked Latest, and `/latest/` skips it.
- Until now a published release needed its commit on `origin/main`, and `--candidate` (a renamed plugin for side-by-side A/B, no firmware) was the only pre-release.

**Decision:**
- **A sound change goes out as a beta before a release.** Once the owner has picked it on a page and checked it at the rack, it lives on a `beta/<topic>` branch (e.g. `beta/wear`), which is pushed to origin. `tools/make_release.sh --beta <ref> <n> --publish` builds the firmware and the normal plugin from it and publishes `<sound>-beta.<n>` (e.g. `v1.0.70-beta.1`) as a pre-release with notes for testers.
- **Tags name the sound they carry.** `<n>` counts betas of one SPEC version. A tuning change is a new SPEC version, so its beta tag starts again at 1 (e.g. `v1.0.70-beta.2` repackages the same sound, `v1.0.71-beta.1` is the next tuning).
- **Each beta has a pull request** from its `beta/<topic>` branch into `main`, opened when beta 1 is published. Its description is short and links rather than repeats: what changes in the sound, the feedback that started it (issues, forum posts), the ADRs, the pre-release(s) to try and what testers should listen for. Testers' feedback goes on the PR or in issues linked to it. A later beta adds a comment with its link. Merging the PR is the graduation below; closing it unmerged is the drop.
- **When the beta settles,** the owner OKs merging its branch into `main` (merging the PR), then a full release from `main` (`tools/make_release.sh --publish`, tag `<sound>`, Latest), which moves the site's downloads.
- **If it doesn't,** the PR is closed unmerged and the branch dropped. Its SPEC versions stay used, and the next sound takes the next number; `SPEC.md`'s changelog notes the dropped beta when the next version lands. `main` never carries a sound that wasn't released.
- **Tags are never deleted,** release or beta, even when their branch is. A published tag holds the source of what was given out (the plugin is AGPLv3 because of JUCE). The ruleset on `main` doesn't cover tags, so this is a rule of practice.
- **Branch names:**

  | Prefix | For | Pushed? | Ends |
  |---|---|---|---|
  | `main` | what's released; always releasable | yes | — |
  | `beta/<topic>` | a picked sound change on its way out (ports, its ADRs and SPEC lines, rack builds) | yes, to publish betas | merged into `main` after the beta, or dropped |
  | `proto/<topic>` | a prototype behind a renderer hook and its page | no | its pick is ported to a `beta/` branch; the prototype branch is deleted with the owner's OK |
  | `claude/<topic>` | everything else: docs, tools, the site, housekeeping (the desktop app adds a suffix to session branches) | no | merged into `main` with the owner's OK |

  Prototype sessions started by the app get `claude/<topic>-<suffix>` branches; that's fine while they're prototypes, and the port moves to `beta/<topic>`.
- **Docs and site changes** (`claude/` branches) still go straight to `main` with the owner's OK: they change no sound and don't need a beta or a PR. A visible site change may get a PR anyway when a Vercel preview link helps the owner review it on the phone.
- **Prototypes get no PR:** they aren't pushed (their pages use reference recordings that are never published), and their result shows in the beta's PR.

**Rejected:**
- **A PR for every change:** a solo project's review is the owner listening to local pages; PRs for docs, task-list and prototype work would be ceremony with no other reviewer, and prototypes can't be pushed.
- **Requiring PRs on `main` (a ruleset) now:** the docs and site merges above go direct; revisit with CI.
- **Betas from `main`:** `main`'s README, SPEC and manual would describe a sound most people can't download, and backing a beta out means reverting `main` with a version that never shipped in its history.
- **`--candidate` for betas:** it leaves the firmware out and renames the plugin; testers on the module need the firmware and want the normal plugin updated in place.
- **Branches named by version (`beta/v1.0.70`):** a beta's tuning bumps the SPEC version, so the name would go stale; the topic stays true.

**Consequences:**
- `tools/make_release.sh --beta <ref> <n>`: the normal plugin (same name, code and bundle ID, so it replaces the release one), the firmware, no stable-name assets, pre-release and never Latest; it accepts a commit on any pushed branch.
- `site/src/data/release.json` and the site's downloads change only with a full release.
- **No CI yet** (no `.github/workflows`), so a beta's PR shows no automated checks; the gates are run locally and quoted in the PR. A workflow running the desktop tests on each PR is a possible later step (the firmware build needs the ARM toolchain and libDaisy on GitHub's runners).
- The first beta is WEAR (v1.0.70: the taper of ADR 0044 and the drive of ADR 0045), after the owner's rack check, on `beta/wear`, with the first PR.
