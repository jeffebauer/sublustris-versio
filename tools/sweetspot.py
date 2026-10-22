#!/usr/bin/env python3
"""Sweet-spot check (SPEC §2.3, "every control move is smooth"): sweeps each knob in each
SPACE end to end and flags dead zones (a stretch where the knob audibly does nothing) and
cliffs (one step that jumps far harder than its neighbours).

Usage: .venv/bin/python tools/sweetspot.py [--steps 11] [--out docs/sweetspot.md]
Renders with build/iv_render into a temp folder (nothing kept), 100 % wet, ERA clean
except on ERA's own sweep. Per step it measures, on a click then a few seconds of skank:
loudness, decay (T60 from the click), brightness (the tail's spectral centroid), when
the wet arrives and peaks, and width (L/R correlation). Each change is scaled by roughly
how much is just audible (1 dB, 10 % of T60, 1 semitone of centroid, 15 % of arrival or peak time (at least 20 ms),
0.1 of correlation), and the step's size is the largest of them.
Replaces Resilio's M8 tool (it pointed at rv_render and Resilio's knobs).
Not covered: pitch movement (DRIFT's wobble), which these measures can't see.
"""
import argparse
import subprocess
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf

SR = 48000
ROOT = Path(__file__).resolve().parent.parent
BIN = ROOT / "build" / "iv_render"
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


def sweep(tmp: Path, stim: Path, space: str, knob: str, steps: int, fixed: list[str]) -> list[dict]:
    out = []
    for k in range(steps):
        v = k / (steps - 1)
        o = tmp / "o.wav"
        subprocess.run([str(BIN), str(stim), str(o), "--set", "mix=1", "--set", f"space={space}", *fixed,
                        "--set", f"{knob}={v:.4f}"], check=True, capture_output=True)
        y, _ = sf.read(str(o), always_2d=True)
        out.append(features(y))
    return out


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
    args = ap.parse_args()
    rows = []
    with tempfile.TemporaryDirectory() as t:
        tmp = Path(t)
        stim = tmp / "stim.wav"
        stimulus(stim)
        for space in ("PLATE", "HALL", "MASSIVE"):
            for knob in KNOBS:
                fixed = [] if knob == "era" else ["--set", "era=0"]
                rows.append((space, knob, *judge(sweep(tmp, stim, space, knob, args.steps, fixed))))
            for attack in ("NORMAL", "BLOOM", "REVERSE"):
                fixed = ["--set", "era=0", "--set", f"attack={attack}"]
                rows.append((space, f"time ({attack})", *judge(sweep(tmp, stim, space, "time", args.steps, fixed))))
            print(f"{space} done")
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
    print(f"wrote {Path(args.out).relative_to(ROOT)}")


if __name__ == "__main__":
    main()
