# 0017 — Panel layout: P1–P7 mirror Resilio's positions

**Status:** Accepted, 21 Oct 2026 (owner approved spec v1.0). Follows Resilio ADR 0028 (pots named by panel position, not libDaisy index).

**Context:** Until Immensio has its own panel, the owner plays it behind the stock printed Versio panel, the same as Resilio. Putting each knob where its closest Resilio cousin sits keeps the muscle memory.

**Decision:** pots in reading order:

| Pot | Immensio | Resilio cousin in that position |
|---|---|---|
| P1 | MIX | BLEND |
| P2 | DECAY | DECAY |
| P3 | TONE | TONE |
| P4 | ERA | SPLASH |
| P5 | SIZE | TENSION ("which tank") |
| P6 | DRIFT | WOBBLE (bipolar pitch movement) |
| P7 | TIME | DRIVE |

- The firmware keeps two tables, as Resilio: pot → libDaisy knob index (a hardware fact) and pot → function (this decision).
- Which toggle is SW0 (SPACE) and which is SW1 (ATTACK) follows the printed panel, **confirmed at M0**.
