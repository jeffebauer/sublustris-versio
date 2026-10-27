# 0029 — The panel names: BLEND, WEAR, MOTION, DEPTH, ONSET, FREEZE / PAINT

**Status:** Accepted, 27 Oct 2026 (the owner's answers; ERA → WEAR with "Start Clean. We should rename this knob to reflect its new behavior: Wear"). SPEC v1.0.42. Amends the names in ADR 0012, 0015, 0017 and 0024. The positions (ADR 0017) don't change.

**Context:**
- Several controls had outgrown their names. ERA is no longer a journey through eras but one wear amount (ADR 0027). DRIFT carries a tape transport on one side and Wander on the other (ADR 0030). TIME means predelay, swell time or shimmer amount depending on ATTACK.
- The panel is printed, and the plugin shows the same names, so they were settled before the playing build and the friend's copy.
- Hosts save a device's parameters by their key (the VST3 ID is a hash of it). A renamed key would leave a dead slot in every saved Ableton set (`core/params/ParamSpec.h`).

**Decision:**

| Position | Was | Panel name | Key (unchanged) |
|---|---|---|---|
| P1 | MIX | **BLEND** | `mix` |
| P2 | DECAY | DECAY | `decay` |
| P3 | TONE | TONE | `tone` |
| P4 | ERA | **WEAR** | `era` |
| P5 | SIZE | SIZE | `size` |
| P6 | DRIFT | **MOTION** (the owner's preference) | `drift` |
| P7 | TIME | **DEPTH** | `time` |
| SW0 | SPACE: PLATE / HALL / MASSIVE | SPACE: PLATE / HALL / MASSIVE | `space` |
| SW1 | ATTACK: NORMAL / BLOOM / SHIMMER | **ONSET: NATURAL** / BLOOM / SHIMMER | `attack` |
| Button | (freeze / paint) | **FREEZE / PAINT** (tap / hold) | — |
| Gate | (gate) | **FREEZE** | — |

- **Names only.** `ParamSpec`'s `name` and its Switch3 labels change. The keys, the plugin's parameter IDs, the plugin's code identity (ImVs, ADR 0018) and every internal identifier (`ParamId::Drift`, `map::eraMix`, `kDriftDeadLo`, …) stay as they are. **Keys and plugin IDs never change**; a later rename changes `name` only.
- `"attack=NORMAL"` still reads in presets, sweeps and page scripts.
- The renderer's usage lists each key with its panel name and a switch's positions. The plugin's button reads FREEZE / PAINT with a tap / hold tooltip. The firmware's profile lines say WEAR (`wear10` for the old `neve`).

**Considered:**
- Renaming the keys too: every saved set, preset and page script would break.
- Keeping the old names: they describe controls that no longer exist (ERA's eras, DRIFT's sine side).

**Consequences:**
- Renders are unchanged (`tools/same_render.py` as before the rename).
- Older pages, ADRs and changelog lines keep the names of their day. `CONTEXT.md` lists each old name beside the new one.
- The docs use the panel names from here on. Code and the glossary's "was" notes keep the keys.
