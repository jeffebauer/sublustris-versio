#!/usr/bin/env python3
"""Sweet-spot check (SPEC §2.3, "every control move is smooth"): sweeps each knob in each
SPACE end to end and flags dead zones (a stretch where the knob audibly does nothing) and
cliffs (one step that jumps far harder than its neighbours).

Usage: .venv/bin/python tools/sweetspot.py [--steps 11] [--out docs/sweetspot.md] [--bin build-wt/iv_render]
       [--only "MASSIVE:decay,PLATE:time (NORMAL)"] [--json out.json] [--jobs 8]
Renders with build/iv_render (or --bin / $IV_RENDER) into a temp folder (nothing kept), 100 % wet, ERA clean
except on ERA's own sweep. Per step it measures, on a click then a few seconds of skank:
loudness, decay (T60 from the click), brightness (the tail's spectral centroid), when
the wet arrives and peaks, and width (L/R correlation). Each change is scaled by roughly
how much is just audible (1 dB, 10 % of T60, 1 semitone of centroid, 15 % of arrival or peak time (at least 20 ms),
0.1 of correlation), and the step's size is the largest of them.
Replaces Resilio's M8 tool (it pointed at rv_render and Resilio's knobs).
Not covered: pitch movement (DRIFT's wobble), which these measures can't see.
Environment variables (the renderer's prototype hooks, e.g. IV_KNOB_FEEL) pass through to it.
--json writes every step's raw measures, so a step's size can be traced to the measure behind it.
"""
import argparse
import json
import os
import subprocess
from concurrent.futures import ThreadPoolExecutor
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf

SR = 48000
ROOT = Path(__file__).resolve().parent.parent
BIN = Path(os.environ.get("IV_RENDER", ROOT / "build" / "iv_render"))
SKANK = ROOT / "test_audio" / "stimulus" / "04_skank.wav"
KNOBS = ["decay", "tone", "era", "size", "drift"]
JND = {"loud_db": 1.0, "log_t60": np.log(1.1), "centroid_st": 1.0, "log_arrive": np.log(1.15), "log_peak": np.log(1.15), "corr": 0.1}
CLICK_AT, SKANK_AT = 0.5, 12.0


def stimulus(path: Path):
    x = np.zeros((int(20.0 * SR), 2))
    x[int(CLICK_AT * SR)] = 0.5
    s, sr = sf.read(str(SKANK), always_2d=True)
    s = s[: int(4.0 * sr)]
    if s.shape[1] == 1:
        s = np.repeat(s, 2, axis=1)
    i = int(SKANK_AT * SR)
    x[i:i + len(s)] += s[:, :2]
    sf.write(str(path), x.astype(np.float32), SR, subtype="FLOAT")


def features(y: np.ndarray) -> dict:
    m = y.mean(axis=1)
    click = m[int(CLICK_AT * SR): int((SKANK_AT - 0.5) * SR)]
    e = click ** 2
    pk = int(np.argmax(np.abs(click)))
    edc = np.cumsum(e[pk:][::-1])[::-1]
    d = 10 * np.log10(edc / max(edc[0], 1e-30) + 1e-30)
    a, b = int(np.argmax(d < -5)), int(np.argmax(d < -25))
    t60 = 3 * (b - a) / SR if b > a else 30.0
    env = np.sqrt(np.convolve(e, np.ones(480) / 480, "same")) + 1e-12
    arrive = int(np.argmax(env > env.max() * 0.1)) / SR
    peak = int(np.argmax(env)) / SR
    sk = y[int(SKANK_AT * SR): int((SKANK_AT + 6.0) * SR)]
    loud = 10 * np.log10(np.mean(sk ** 2) + 1e-20)
    tail = sk[int(4.2 * SR):].mean(axis=1)
    P = np.abs(np.fft.rfft(tail * np.hanning(len(tail)))) ** 2
    f = np.fft.rfftfreq(len(tail), 1 / SR)
    cent = float((f * P).sum() / max(P.sum(), 1e-30))
    corr = float(np.corrcoef(sk[:, 0], sk[:, 1])[0, 1]) if sk[:, 0].std() > 0 else 1.0
    return {"loud_db": loud, "log_t60": np.log(max(t60, 0.05)), "centroid_st": 12 * np.log2(max(cent, 20.0)),
            "log_arrive": np.log(max(arrive, 0.02)), "log_peak": np.log(max(peak, 0.02)), "corr": corr, "t60": t60}


def render_one(tmp: Path, stim: Path, space: str, knob: str, v: float, fixed: list[str], tag: str) -> dict:
    o = tmp / f"{tag}.wav"
    subprocess.run([str(BIN), str(stim), str(o), "--set", "mix=1", "--set", f"space={space}", *fixed,
                    "--set", f"{knob}={v:.4f}"], check=True, capture_output=True)
    y, _ = sf.read(str(o), always_2d=True)
    o.unlink()
    return features(y)


def sweep(pool, tmp: Path, stim: Path, space: str, knob: str, steps: int, fixed: list[str]) -> list[dict]:
    base = "".join(ch for ch in f"{space}_{knob}_{'_'.join(fixed)}" if ch.isalnum() or ch == "_")
    futs = [pool.submit(render_one, tmp, stim, space, knob, k / (steps - 1), fixed, f"{base}_{k}") for k in range(steps)]
    return [f.result() for f in futs]


def judge(feats: list[dict]) -> tuple[list[float], list[str], list[str]]:
    steps = []
    for a, b in zip(feats, feats[1:]):
        steps.append(max(abs(b[k] - a[k]) / j for k, j in JND.items()))
    n = len(steps)
    dead, cliffs = [], []
    run = 0
    for i, s in enumerate(steps + [99.0]):
        if s < 0.25:
            run += 1
        else:
            if run >= max(2, round(0.2 * n)):
                dead.append(f"{(i - run) / n:.0%}–{i / n:.0%}")
            run = 0
    med = float(np.median(steps)) if steps else 0.0
    for i, s in enumerate(steps):
        if s > 3.0 and s > 3.0 * max(med, 0.3):
            cliffs.append(f"{i / n:.0%}→{(i + 1) / n:.0%} ({s:.1f}×)")
    return steps, dead, cliffs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--steps", type=int, default=11)
    ap.add_argument("--out", default=str(ROOT / "docs" / "sweetspot.md"))
    ap.add_argument("--bin", help="renderer to use (default $IV_RENDER or build/iv_render)")
    ap.add_argument("--only", help="comma list of SPACE:knob rows, e.g. MASSIVE:decay,PLATE:time (NORMAL)")
    ap.add_argument("--json", help="also write every step's raw measures here")
    ap.add_argument("--jobs", type=int, default=max(1, (os.cpu_count() or 2) - 2))
    args = ap.parse_args()
    global BIN
    if args.bin:
        BIN = Path(args.bin).resolve()
    only = set(args.only.split(",")) if args.only else None
    rows, raw = [], {}
    with tempfile.TemporaryDirectory() as t, ThreadPoolExecutor(args.jobs) as pool:
        tmp = Path(t)
        stim = tmp / "stim.wav"
        stimulus(stim)
        for space in ("PLATE", "HALL", "MASSIVE"):
            jobs = [(knob, knob, [] if knob == "era" else ["--set", "era=0"]) for knob in KNOBS]
            jobs += [(f"time ({a})", "time", ["--set", "era=0", "--set", f"attack={a}"]) for a in ("NORMAL", "BLOOM", "REVERSE")]
            for label, knob, fixed in jobs:
                if only is not None and f"{space}:{label}" not in only:
                    continue
                feats = sweep(pool, tmp, stim, space, knob, args.steps, fixed)
                raw[f"{space}:{label}"] = [{k: float(v) for k, v in f.items()} for f in feats]
                rows.append((space, label, *judge(feats)))
            print(f"{space} done")
    if args.json:
        Path(args.json).write_text(json.dumps(raw, indent=1))
    lines = ["# Sweet-spot check", "",
             "Generated by `tools/sweetspot.py`: every knob in every SPACE, end to end in "
             f"{args.steps - 1} steps. A step's size is its biggest change in units of roughly just-audible "
             "(1 dB loudness, 10 % decay, 1 semitone brightness, 15 % arrival or peak time, 0.1 width). "
             "**Dead zone:** 20 % or more of the travel where each step is under 0.25 of that. "
             "**Cliff:** one step over 3 and over 3× the knob's median step. DRIFT's pitch movement isn't measured. "
             "Decay is read from the click within 11 s, so DECAY's top (infinite) shows as one big step by design.", "",
             "| SPACE | Knob | Step sizes, CCW → CW | Dead zones | Cliffs |", "|---|---|---|---|---|"]
    for space, knob, steps, dead, cliffs in rows:
        lines.append(f"| {space} | {knob} | {' '.join(f'{s:.1f}' for s in steps)} | {', '.join(dead) or '–'} | {', '.join(cliffs) or '–'} |")
    Path(args.out).write_text("\n".join(lines) + "\n")
    print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
