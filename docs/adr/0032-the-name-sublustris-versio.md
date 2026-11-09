# 0032 — The name: Sublustris Versio

**Status:** Accepted, 27 Oct 2026 (owner). SPEC v1.0.48. The name only: no sound change.

**Context:**
- **The old name didn't hold.** "Immensio" (Latin *immensus*, "boundless") kept turning into "immersio" when people read or said it, and it rhymed with Versio ("Immensio Versio").
- **The shortlist** (the owner's): Nubila, Caligo, Obscurus, Sublustris.
- **What a rename can break.** Saved sets and automation must keep working:
  - Ableton (and every AU host) finds the Audio Unit by its type, subtype and manufacturer (`aumf ImVs Ombr`), not by its name.
  - The VST3's class UID is derived from the plugin and manufacturer codes, so a VST3 set finds the plugin by those too.
  - VST3 (and AU) parameters are saved by a hash of each ParamSpec key, and the plugin's own state is a tree named `params` holding the keys, never the product's or the parameters' display names.
  - The macOS bundle ID is hidden, and changing it can leave a stale entry in the AU cache (the cache must never be reset here: CLAUDE.md).

**Decision:**
- **The product is Sublustris Versio:** Latin *sublustris*, "dimly lit, faintly glowing", the half-light of a clouded night.
- **Renamed (what people see):**
  - the plugin's name, so the bundles are `Sublustris Versio.vst3` and `Sublustris Versio.component` (`IV_PLUGIN_NAME`), and the editor's title;
  - the firmware's files: `sublustris_versio.bin`, `sublustris_versio_m0test.bin`, `sublustris_versio_profile.bin` and their build-option variants (`firmware/Makefile`);
  - the friends' package (`tools/make_release.sh`: `SublustrisVersio_<tag>.zip`, `releases/README.md`), `tools/install_plugin.sh`, the review pages' titles;
  - SPEC.md, CONTEXT.md, README.md, NOTICE, CLAUDE.md and `firmware/README.md`;
  - the owner's session commands, now `/sublustris-start` and `/sublustris-wrap` (the owner's choice).
- **Kept on purpose (what saved sets and the tools depend on):**
  - the plugin code `ImVs`, the manufacturer code `Ombr` and the bundle ID `com.OmbraAudio.ImmensioVersio` (and a candidate's `ImV<label>` / `…ImmensioVersio<label>`);
  - every ParamSpec key;
  - internal identifiers: the `iv::` namespace, `IV_*` macros and options, the CMake project and target names (`immensio_versio`, `ImmensioVersio`), file names in `core/`;
  - the repo and folder name (`immensio-versio`, which the tools' paths use);
  - history: `IMMENSIO_BRIEF.md`, `docs/spec-session.md`, the bodies of ADRs 0001-0031, `docs/research/`, the round logs, the handoff's past sessions.

**Rejected:**
- **Changing the codes or the bundle ID with the name:** every saved set would lose the plugin, and the AU cache could keep a stale entry.
- **Renaming the internal identifiers and the repo:** hundreds of edits, every worktree's and agent's paths, for nothing anyone hears or sees.

**Consequences:**
- **Sound:** none. `tools/same_render.py` against v1.0.47: every render the same, sample for sample.
- **Installing:** the new bundles have a new file name, so they don't replace the old ones. Where "Immensio Versio.vst3" / ".component" are installed (the owner's Ableton, any friend's), they must be removed: the two copies are the same plugin with the same codes, and two copies confuse the host. `tools/install_plugin.sh` warns when it finds an old-named copy; the read-me tells friends to delete theirs.
- **The module:** Firmware Swap shows the new file name; nothing on the module itself shows the name.
- **Flash:** release unchanged (no name string in the firmware).

**Amendment, 29 Oct 2026 (owner): the repo and the folder take the name too.** Near v1.0, the owner asked for the GitHub repository and the local folder to match the product. The repository is now `jeffebauer/sublustris-versio` (GitHub redirects the old URL; `origin` points at the new one; `NOTICE` and `tools/make_release.sh` use it). The local folder moves from `immensio-versio` to `sublustris-versio` at a wrap-up, as the session's last step, because it ends the session running in it: the tools' absolute paths, `build/`'s CMake cache, the `.venv` and Claude's memory folder all depend on the path (checklist in `docs/handoff/HANDOFF.md`). Still kept: the plugin codes, the bundle ID, ParamSpec keys, `iv::` / `IV_*` and the CMake names, for the reasons above.

**Amendment, 8 Nov 2026 (owner): the session commands are the shared ones.** `/sublustris-start` and `/sublustris-wrap` gave way to the shared `/session-start`, `/session-checkpoint` and `/session-wrap` (`~/Documents/Sites/session-skills`, used by all the owner's projects). This project's facts for them are in `.claude/session.md`; the orchestrator guide moved to `docs/orchestrator.md`, the handoff's shape to `docs/handoff/TEMPLATE.md`.
