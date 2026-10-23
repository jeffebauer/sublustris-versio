#!/usr/bin/env python3
"""The M8 knob-feel audit's before / after measure (docs/research/knob-feel-audit.md).

Runs tools/sweetspot.py's measures (loudness, decay, brightness, arrival, peak, width, each
in just-audible units) along one knob at finer steps, at extra settings the sweet-spot table
doesn't cover (MASSIVE's DECAY at every SIZE, say), once as today (A) and once with a
prototype curve (B: IV_KNOB_FEEL=<names>, the renderer's hook). Per row it prints the step
sizes, the biggest step over the median (how much one notch jumps), and the dead stretch.

Usage: .venv/bin/python tools/pages/knob_feel_measure.py [--bin build-wt/iv_render] [--rows decay,era,...]
       [--steps 21] [--json out.json]
"""
import argparse
import json
import os
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import sweetspot as ss  # noqa: E402

# name: (B's IV_KNOB_FEEL, [(label, space, knob, fixed --set list)])
ROWS = {
    "decay": ("decay", [(f"MASSIVE decay, SIZE {s}", "MASSIVE", "decay", ["era=0", f"size={s}"]) for s in ("0", "0.5", "1")]),
    "era": ("era", [(f"{sp} era", sp, "era", []) for sp in ("PLATE", "HALL", "MASSIVE")]),
    "time": ("time", [(f"MASSIVE time NORMAL, SIZE {s}", "MASSIVE", "time", ["era=0", "attack=NORMAL", f"size={s}"]) for s in ("0", "0.5", "1")]),
    "bloom": ("bloom", [(f"{sp} time BLOOM", sp, "time", ["era=0", "attack=BLOOM"]) for sp in ("PLATE", "HALL", "MASSIVE")]),
    "tone": ("", [(f"{sp} tone", sp, "tone", ["era=0"]) for sp in ("PLATE", "HALL", "MASSIVE")]),
    "size": ("", [("PLATE size", "PLATE", "size", ["era=0"])]),
    "drift": ("", [(f"{sp} drift", sp, "drift", ["era=0"]) for sp in ("PLATE", "HALL", "MASSIVE")]),
}


def judge(feats):
    steps = [max(abs(b[k] - a[k]) / j for k, j in ss.JND.items()) for a, b in zip(feats, feats[1:])]
    med = float(np.median(steps))
    worst = int(np.argmax(steps))
    n = len(steps)
    run, best, at = 0, 0, 0
    for i, s in enumerate(steps + [99.0]):
        if s < 0.25 * 10 / n:  # the sweet-spot dead line, scaled to this step count
            run += 1
        else:
            if run > best:
                best, at = run, i - run
            run = 0
    return steps, med, worst, best / n, at / n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.environ.get("IV_RENDER", str(ss.ROOT / "build" / "iv_render")))
    ap.add_argument("--rows", default=",".join(ROWS))
    ap.add_argument("--steps", type=int, default=21)
    ap.add_argument("--json")
    ap.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) - 2))
    args = ap.parse_args()
    ss.BIN = Path(args.bin).resolve()
    out = {}
    with tempfile.TemporaryDirectory() as t, ThreadPoolExecutor(args.jobs) as pool:
        tmp = Path(t)
        stim = tmp / "stim.wav"
        ss.stimulus(stim)
        for name in args.rows.split(","):
            feel, rows = ROWS[name]
            for label, space, knob, fixed in rows:
                sets = sum((["--set", kv] for kv in fixed), [])
                for ver, env in (("A", ""), ("B", feel)):
                    if ver == "B" and not feel:
                        continue
                    os.environ["IV_KNOB_FEEL"] = env
                    feats = ss.sweep(pool, tmp, stim, space, knob, args.steps, sets)
                    steps, med, worst, dead, dead_at = judge(feats)
                    x = (args.steps - 1) / 10  # per 10 % of travel, as in docs/sweetspot.md
                    out[f"{label} {ver}"] = {"steps": steps, "feats": [{k: float(v) for k, v in f.items()} for f in feats]}
                    print(f"{label:30s} {ver}  worst {steps[worst] * x:5.1f} at {worst / (args.steps - 1):4.0%}"
                          f" ({steps[worst] / max(med, 1e-3):4.1f}x median)  median {med * x:4.1f}"
                          f"  dead {dead:4.0%}{f' from {dead_at:.0%}' if dead else ''}"
                          f"  t60 {feats[0]['t60']:.2f}->{feats[-1]['t60']:.2f} s"
                          f"  steps/10%: {' '.join(f'{s * x:.1f}' for s in steps)}", flush=True)
    os.environ.pop("IV_KNOB_FEEL", None)
    if args.json:
        Path(args.json).write_text(json.dumps(out, indent=1))


if __name__ == "__main__":
    main()
