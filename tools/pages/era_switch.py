#!/usr/bin/env python3
# Renders at its original commit: build iv_render at d827daa (IV_ERA_SWITCH is ignored since v1.0.42).
"""PROTOTYPE pick page (26 Oct 2026; the owner: "even with the overlap, on the desktop plugin I noticed a
hard transition between tape and radio ... it didn't sound like a smooth crossfade"): ERA as built (each
era fades in over the first third of its fifth while the one before fades out) against ERA without the
overlaps (each era alone across its whole fifth, a 50 ms crossfade as the knob crosses a boundary;
renderer hook IV_ERA_SWITCH=0.05). Without overlaps only one era's stage runs at a time, so the dearest
spot on the knob (the tape and the radio together) goes away.

Usage: <main checkout>/.venv/bin/python tools/pages/era_switch.py [--render build-scratch/iv_render]

ERA sweeps 0.12 -> 0.95 over 16 s while the material plays, crossing all four boundaries (Neve|Midiverb
at ~1.5 s, Midiverb|BBD ~5.4 s, BBD|cassette ~9.3 s, cassette|radio ~13.1 s). Renders are 100 % wet
(--wet-only --dry: the page's MIX fader blends the dry in), level-matched, a pick page. Written to the
main checkout's renders/era_switch (where the pages are served from); _hooks.txt keeps every render's
hook line and _verify.txt measures A against B and looks for clicks at the boundaries.
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

SWEEP = (0.12, 0.95, 16.0)  # ERA from, to, over seconds
BOUNDARIES = (0.2, 0.4, 0.6, 0.8)
MATERIALS = {"pad": (17, 9), "skank": (17, 9), "drums": (17, 8)}
COLS = [("1_massive", ["space=MASSIVE", "size=0.7", "decay=0.6", "attack=NORMAL", "time=0"]),
        ("2_hall", ["space=HALL", "size=0.6", "decay=0.6", "attack=NORMAL", "time=0"])]
VERSIONS = [("A_overlap_as_built", {}), ("B_switch_50ms", {"IV_ERA_SWITCH": "0.05"})]
README = """ERA: today's overlaps, or a clean switch at each boundary. ERA sweeps slowly from the Neve to the AM radio (0.12 to 0.95 over 16 s) while the music plays, crossing all four boundaries: Neve to Midiverb at about 1.5 s, Midiverb to bucket-brigade at about 5.4 s, bucket-brigade to cassette at about 9.3 s, cassette to radio at about 13.1 s.

A is today: each era fades in over the first third of its stretch while the one before fades out, so around each boundary you hear both. B has no overlaps: each era plays alone across its whole stretch, and crossing a boundary swaps to the next in 50 ms (a small sticky zone keeps a knob resting on a boundary from flipping). B also means only one era's stage runs at a time, which takes the module's dearest spot on the knob away (the cassette and the radio together).

Which transitions do you prefer? Listen most at the boundaries; between them A and B are the same. MASSIVE SIZE 0.7, HALL SIZE 0.6, DECAY 0.6, ATTACK NORMAL. Level-matched; renders are 100 % wet, so use the MIX fader for the dry.

1_massive           MASSIVE, ERA swept across all five eras
2_hall              HALL, the same sweep

A_overlap_as_built  today: neighbouring eras crossfade across the first third of each stretch
B_switch_50ms       no overlap: each era alone, a 50 ms swap at each boundary"""


def boundary_times():
    a, b, secs = SWEEP
    return [(x - a) / (b - a) * secs for x in BOUNDARIES]


def render(binary, src, out, sets, env_extra, auto, hooks):
    out.parent.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    env.update(env_extra)
    args = [str(binary), str(src), str(out), "--set", "mix=1", "--sidecar", "--auto", str(auto)]
    for s in sets + ["era={}".format(SWEEP[0])]:
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
    return "null {:.1f} dB; level B-A {:+.2f} dB; HF peak/median at boundaries: {}".format(
        null, level, ", ".join(clicks))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-scratch" / "iv_render")
    a = ap.parse_args()
    main_root = main_checkout()
    material = main_root / "renders" / "_material"
    out = main_root / "renders" / "era_switch"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    dry = main_root / "renders" / "_material_era_switch"
    for m, (play, tail) in MATERIALS.items():
        trim(material / (m + ".wav"), dry / (m + ".wav"), play, tail)
    auto = out / "_era_sweep.json"
    auto.write_text(json.dumps({"breakpoints": [
        {"t": 0.0, "key": "era", "value": SWEEP[0]}, {"t": SWEEP[2], "key": "era", "value": SWEEP[1]}]}))
    binary = a.render.resolve()
    with open(out / "_hooks.txt", "w") as hooks:
        for col, sets in COLS:
            for tag, env in VERSIONS:
                for m in MATERIALS:
                    render(binary, dry / (m + ".wav"), out / col / "{}_{}.wav".format(m, tag), sets, env, auto, hooks)
    with open(out / "_verify.txt", "w") as v:
        v.write("A vs B in every card (era_switch); boundaries at {}\n".format(
            ", ".join("{:.1f} s".format(t) for t in boundary_times())))
        for col, _ in COLS:
            for m in MATERIALS:
                v.write("{:10s} {:6s} {}\n".format(col, m, verify(
                    out / col / "{}_{}.wav".format(m, VERSIONS[0][0]), out / col / "{}_{}.wav".format(m, VERSIONS[1][0]))))
    (out / "README.txt").write_text(README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "ERA: overlaps or a clean switch", "--level-match", "--dry", str(dry), "--wet-only"],
                   check=True, stdout=subprocess.DEVNULL)
    print(out / "index.html")


if __name__ == "__main__":
    main()
