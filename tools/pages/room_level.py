#!/usr/bin/env python3
# Renders at its original commit: build iv_render at 3fd6aa5 (IV_ROOM_LEVEL is ignored since v1.0.43).
"""PROTOTYPE decision page (27 Oct 2026): the small room's level under a frozen pad across SIZE and DECAY.

v1.0.43 made the small room (the owner's pick E on renders/small_room) HALL's and MASSIVE's freeze twin, at the gains
the page set for it at SIZE 0.9, DECAY 0.9. Measured away from there (music, K-weighted, against v1.0.41's twin tank):
the room sits 3-9 dB louder against the pad at small SIZEs and shorter DECAYs, in MASSIVE even above the pad. Renderer
hook IV_ROOM_LEVEL (Engine::setRoomLevelPrototype; desktop only, the default unchanged):

    A  as built: the page's gain everywhere
    B  the room's level follows the pad: v1.0.41's lift for a twin cut short, the twin tank's level trims (MASSIVE's
       DECAY, HALL's SIZE) and the room's line length against SIZE 0.9 (1)
    C  halfway between, in dB (2)

All three are the same at the page's setting (SIZE 0.9, DECAY 0.9): the anchor card. Cards: MASSIVE and HALL x SIZE
0 / 0.5 / 0.9 x DECAY 0.5 / 0.9, keys and vibes over an in-key pad (renders/twin_alternatives' material), Paint held
9-12 s. Not level-matched: the level is the question. WEAR 0.

Usage: <main checkout>/.venv/bin/python tools/pages/room_level.py [--render build-wear/iv_render]
"""
import argparse
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from review_cpu_pages import main_checkout  # noqa: E402
import frozen_twin as ft  # noqa: E402
from freeze_moment import render, lv, clicks  # noqa: E402
import twin_alternatives as ta  # noqa: E402

MATS = ["keys", "vibes"]
VERSIONS = [("A_as_built", {}), ("B_follows_pad", {"IV_ROOM_LEVEL": "1"}), ("C_halfway", {"IV_ROOM_LEVEL": "2"})]
LABELS = {"A_as_built": None, "B_follows_pad": "room level follows the pad (full)", "C_halfway": "room level follows the pad (halfway)"}
CARDS = []
for sp in ("MASSIVE", "HALL"):
    for size in ("0", "0.5", "0.9"):
        for decay in ("0.5", "0.9"):
            CARDS.append((sp, size, decay))
README = """The small room's level under a frozen pad. The room you picked (E) was set to sit about where today's twin did, at SIZE 0.9 and DECAY 0.9. Away from there it doesn't keep that distance: at small SIZEs and shorter DECAYs it comes up much closer to the pad (in MASSIVE at small SIZEs it can even be louder than the pad).

The question: which keeps the room at a steady distance under the pad as SIZE and DECAY change?

A is the room as it is now (the same level everywhere). B lets the room's level follow the pad: it comes down where the pad is shorter or smaller, the way the old twin did. C is halfway between A and B. All three are identical at SIZE 0.9 / DECAY 0.9 (the cards marked "anchor"); that's the level you already picked, for reference.

The versions are NOT level-matched on purpose: the level is what you're judging. The pad itself is the same in A, B and C until Paint (Paint copies a little of the room into the pad, so after 9 s the pad differs a little too).

Each card: the pad plays and is frozen at 4 s; keys or vibes play over it from 4.5 s; the button is held from 9 to 12 s (Paint); the playing stops by 13.5 s; a tap unfreezes at 24 s. TONE noon, DRIFT noon, WEAR 0. Renders are 100 % wet, so use the MIX fader for the dry (around 50 %).

Cards: SPACE, SIZE, DECAY:
""" + "\n".join("{:02d}_{}_S{}_D{}{}".format(i + 1, sp.lower(), s, d, "  (anchor: A = B = C)" if (s, d) == ("0.9", "0.9") else "")
                for i, (sp, s, d) in enumerate(CARDS)) + """

A_as_built      the room at the same level everywhere (as now)
B_follows_pad   the room's level follows the pad
C_halfway       halfway between A and B"""


def col(i, sp, s, d):
    return "{:02d}_{}_S{}_D{}".format(i + 1, sp.lower(), s, d)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-wear" / "iv_render")
    a = ap.parse_args()
    root = main_checkout()
    material = root / "renders" / "_material"
    out = root / "renders" / "room_level"
    dry = root / "renders" / "_material_room_level"
    work = root / "renders" / "_room_level_work"
    for d in (out, dry, work):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
    ref, _ = sf.read(material / "pad.wav", always_2d=True)
    ref_rms = float(np.sqrt(np.mean(ref[int(1.5 * ta.SR):int(4.0 * ta.SR)] ** 2)))
    for m in MATS:
        x, _ = ta.stimulus(material, m, ref_rms)
        sf.write(dry / (m + ".wav"), x, ta.SR, subtype="PCM_24")
    binary = a.render.resolve()
    paint = [list(ft.FREEZE), list(ft.PAINT), list(ft.UNFREEZE)]
    nopaint = [list(ft.FREEZE), list(ft.UNFREEZE)]
    events = [(k, t) for k, t in ft.EVENTS]
    rows = []
    with open(out / "_hooks.txt", "w") as hooks:
        for i, (sp, s, d) in enumerate(CARDS):
            sets = ["space=" + sp, "size=" + s, "decay=" + d, "era=0"]
            for tag, e in VERSIONS:
                for m in MATS:
                    path = out / col(i, sp, s, d) / "{}_{}.wav".format(m, tag)
                    render(binary, dry / (m + ".wav"), path, sets, e, paint, hooks)
                    x, _ = sf.read(path, always_2d=True)
                    # The room alone (the pad out of the wet) while playing, and the pad alone after the room has gone.
                    room = ta.measured(binary, dry / (m + ".wav"), work / "solo.wav", sets, dict(e, IV_LAYER_SOLO="1"),
                                       paint, hooks, lambda y: lv(y, 5.0, 13.0, True))
                    pad_np = ta.measured(binary, dry / (m + ".wav"), work / "np.wav", sets, e, nopaint, hooks,
                                         lambda y: lv(y, 22.0, 23.9, True))
                    rows.append((col(i, sp, s, d), m, tag, room - pad_np, lv(x, 22.0, 23.9, True) - pad_np,
                                 clicks(x, events), bool(np.all(np.isfinite(x)))))
    # The label check: every render's own hook line says what it is.
    bad = []
    for line in (out / "_hooks.txt").read_text().splitlines():
        name = line.split("  env ")[0]
        for tag, _ in VERSIONS:
            if tag in name and "solo" not in name and "np.wav" not in name:
                want = LABELS[tag]
                ok = (want in line) if want else ("IV_ROOM_LEVEL" not in line)
                if not ok:
                    bad.append(line)
    with open(out / "_verify.txt", "w") as v:
        v.write("room_level. Per render: the room alone while playing (5-13 s) against the pad alone after the room has "
                "gone (22-23.9 s, no Paint), dB(A); Paint (the pad at 22-23.9 s, Paint minus none, dB(A); at most +1.5 by "
                "design); 1 ms click scores (above 5 kHz against the median around) at " +
                ", ".join("{} {:.1f} s".format(k, t) for k, t in events) + ".\n")
        v.write("label check: {}\n".format("all {} renders as labelled".format(len(rows)) if not bad else "MISMATCH:\n" + "\n".join(bad)))
        for c, m, tag, rp, paint_db, ck, fin in rows:
            v.write("{:22s} {:5s} {:14s} room - pad {:+6.1f} dB  Paint {:+.2f} dB  finite {}  clicks {}\n".format(
                c, m, tag, rp, paint_db, fin, ck))
        v.write("\nroom - pad, mean of keys and vibes:\n")
        for i, (sp, s, d) in enumerate(CARDS):
            c = col(i, sp, s, d)
            v.write("  {:22s} ".format(c) + "  ".join("{} {:+5.1f}".format(tag[0], np.mean([r[3] for r in rows if r[0] == c and r[2] == tag]))
                                                      for tag, _ in VERSIONS) + "\n")
    shutil.rmtree(work)
    if bad:
        sys.exit("label check failed: see _verify.txt")
    (out / "README.txt").write_text(README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "The small room's level under the pad", "--no-level-match", "--dry", str(dry), "--wet-only",
                    "--chapters", ft.CHAPTERS], check=True, stdout=subprocess.DEVNULL)
    print(out / "index.html")


if __name__ == "__main__":
    main()
