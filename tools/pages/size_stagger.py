#!/usr/bin/env python3
"""PROTOTYPE pick page (26 Oct 2026; on the module a SIZE crossfade in MASSIVE reached ~110 % of the CPU
and the owner heard drop-outs right after moving SIZE): SIZE's crossfade as built (all eight lines fade
from their old lengths to the new over 0.3 s together) against staggered (lines 0, 2, 4, 6 fade first,
then 1, 3, 5, 7 straight after, 0.3 s each; renderer hook IV_SIZE_STAGGER=2). Staggered, only half the
lines read twice at once, about half the extra memory reads that tip the module over.

Usage: <main checkout>/.venv/bin/python tools/pages/size_stagger.py [--render build-scratch/iv_render]

SIZE: 0.3, jumps to 0.8 at 4 s, back to 0.4 at 9 s, then a slow turn to 0.7 over 13-16 s. Renders are
100 % wet (--wet-only --dry: the page's MIX fader blends the dry in), level-matched, a pick page. Written
to the main checkout's renders/size_stagger; _hooks.txt keeps every render's hook line and _verify.txt
measures A against B and looks for clicks at the moves.
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from review_cpu_pages import main_checkout, trim, third_octaves, db  # noqa: E402

MOVES = (4.0, 9.0, 13.0)  # seconds: SIZE jumps up, jumps down, then a slow turn to 16 s
MATERIALS = {"pad": (18, 9), "keys": (18, 9), "drums": (18, 8)}
# Each column's SIZE: a start, a jump up at 4 s, a jump down at 9 s, a slow turn over 13-16 s. HALL stays
# above noon: where its in-loop diffusion joins or leaves (below), the prototype fades all lines together.
COLS = [("1_massive", ["space=MASSIVE", "decay=0.6", "era=0.1", "attack=NORMAL", "time=0"], (0.3, 0.8, 0.4, 0.7)),
        ("2_hall", ["space=HALL", "decay=0.6", "era=0.1", "attack=NORMAL", "time=0"], (0.55, 0.95, 0.6, 0.85))]
VERSIONS = [("A_together_as_built", {}), ("B_staggered", {"IV_SIZE_STAGGER": "2"})]
README = """SIZE's crossfade: all lines together, or in two groups. When you move SIZE in HALL or MASSIVE, the reverb's eight delay lines swap to their new lengths through a 0.3 s crossfade (your pick C on knob_moves: no pitch bend). For those 0.3 s every line is read twice, and on the module that is what tips MASSIVE over the CPU limit right after a SIZE move: the drop-outs you hear.

A is today: all eight lines crossfade together. B staggers it: four lines crossfade, then the other four straight after, so the room changes size over about 0.6 s in two steps, and only half the lines are read twice at any moment (about half the extra work).

MASSIVE's SIZE jumps from 0.3 to 0.8 at 4 s, back down to 0.4 at 9 s, then turns slowly to 0.7 between 13 and 16 s; HALL's goes 0.55, 0.95, 0.6, then turns to 0.85 (in HALL below noon, where its in-loop diffusion comes and goes, B fades all lines together, as A). Does B's move sound as good? Listen at 4 s, 9 s and just after 16 s. DECAY 0.6, ERA at the Neve, ATTACK NORMAL. Level-matched; renders are 100 % wet, so use the MIX fader for the dry.

1_massive             MASSIVE
2_hall                HALL

A_together_as_built   today: all lines crossfade at once (0.3 s)
B_staggered           two groups of four lines, one after the other (0.3 s each)"""


def boundary_times():
    return [MOVES[0], MOVES[1], 16.0]


def size_moves(a, up, down, turn):
    return {"breakpoints": [
        {"t": 0.0, "key": "size", "value": a}, {"t": 4.0, "key": "size", "value": a},
        {"t": 4.01, "key": "size", "value": up}, {"t": 9.0, "key": "size", "value": up},
        {"t": 9.01, "key": "size", "value": down}, {"t": 13.0, "key": "size", "value": down},
        {"t": 16.0, "key": "size", "value": turn}]}


def render(binary, src, out, sets, env_extra, auto, hooks):
    out.parent.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    env.update(env_extra)
    args = [str(binary), str(src), str(out), "--set", "mix=1", "--sidecar", "--auto", str(auto)]
    for s in sets:
        args += ["--set", s]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    if p.returncode != 0:
        sys.exit("render failed: {}\n{}".format(" ".join(args), p.stderr))
    hooks.write("{}  env {}  ->  {}\n".format(out.name, env_extra or "{}",
                                              p.stderr.strip().replace("\n", " | ") or "(no hook line)"))


def hf_frames(x, sr, frame_s=0.005, lo=5000.0):
    """Energy above lo Hz in frame_s frames: a click shows as one frame far above its neighbours."""
    X = np.fft.rfft(x.mean(axis=1))
    f = np.fft.rfftfreq(x.shape[0], 1.0 / sr)
    X[f < lo] = 0
    h = np.fft.irfft(X, x.shape[0])
    n = int(frame_s * sr)
    m = len(h) // n
    return (h[:m * n].reshape(m, n) ** 2).mean(axis=1), frame_s


def click_score(x, sr, t):
    """Around boundary time t (+-0.4 s): the loudest 5 ms frame above 5 kHz against the median of the
    second around it, in dB. Music's own transients read ~6-12 dB; a click reads far more."""
    e, fs = hf_frames(x, sr)
    i0, i1 = int((t - 0.4) / fs), int((t + 0.4) / fs)
    j0, j1 = max(0, int((t - 1.0) / fs)), int((t + 1.0) / fs)
    return db(e[i0:i1].max()) - db(np.median(e[j0:j1]))


def verify(a_path, b_path):
    a, sr = sf.read(a_path, always_2d=True)
    b, _ = sf.read(b_path, always_2d=True)
    n = min(len(a), len(b))
    a, b = a[:n], b[:n]
    if np.array_equal(a, b):
        return "IDENTICAL (mislabelled!)"
    null = db(np.mean((a - b) ** 2) / np.mean(a ** 2))
    ta, tb = third_octaves(a, sr), third_octaves(b, sr)
    level = db(np.mean(b ** 2)) - db(np.mean(a ** 2))
    clicks = ["{:.1f}s A {:+.1f} B {:+.1f}".format(t, click_score(a, sr, t), click_score(b, sr, t))
              for t in boundary_times()]
    return "null {:.1f} dB; level B-A {:+.2f} dB; HF peak/median at the moves: {}".format(
        null, level, ", ".join(clicks))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-scratch" / "iv_render")
    a = ap.parse_args()
    main_root = main_checkout()
    material = main_root / "renders" / "_material"
    out = main_root / "renders" / "size_stagger"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    dry = main_root / "renders" / "_material_size_stagger"
    for m, (play, tail) in MATERIALS.items():
        trim(material / (m + ".wav"), dry / (m + ".wav"), play, tail)
    binary = a.render.resolve()
    with open(out / "_hooks.txt", "w") as hooks:
        for col, sets, moves in COLS:
            auto = out / "_size_moves_{}.json".format(col)
            auto.write_text(json.dumps(size_moves(*moves)))
            sets = sets + ["size={}".format(moves[0])]
            for tag, env in VERSIONS:
                for m in MATERIALS:
                    render(binary, dry / (m + ".wav"), out / col / "{}_{}.wav".format(m, tag), sets, env, auto, hooks)
    with open(out / "_verify.txt", "w") as v:
        v.write("A vs B in every card (size_stagger); moves at {}\n".format(
            ", ".join("{:.1f} s".format(t) for t in boundary_times())))
        for col, _, _ in COLS:
            for m in MATERIALS:
                v.write("{:10s} {:6s} {}\n".format(col, m, verify(
                    out / col / "{}_{}.wav".format(m, VERSIONS[0][0]), out / col / "{}_{}.wav".format(m, VERSIONS[1][0]))))
    (out / "README.txt").write_text(README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "SIZE: crossfade together or staggered", "--level-match", "--dry", str(dry), "--wet-only"],
                   check=True, stdout=subprocess.DEVNULL)
    print(out / "index.html")


if __name__ == "__main__":
    main()
