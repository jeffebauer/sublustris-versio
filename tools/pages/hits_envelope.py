#!/usr/bin/env python3
"""How evenly a wet tail builds after each hit (M8, the owner's "MASSIVE medium and large
sounding a little uneven on Hits"; docs/research/knob-feel-audit.md).

Renders test_audio/stimulus/02_hits.wav (snare and rim at -6 / -12 / -18 dBFS, 6 s apart)
100 % wet through each setting and, per hit, measures the wet's level envelope (20 ms RMS,
5 ms hop) over the first 2.5 s:
  - ripple: how far the envelope wanders round its own smooth trend (a 250 ms moving
    median of the dB envelope), as the RMS in dB from 30 ms after the wet arrives;
    a smooth build or decay reads near 0, a tail that comes in lumps reads higher;
  - lumps: the biggest bumps over the trend (time after the hit, dB);
  - level: the wet's peak against the hit's own level, so hit-to-hit consistency shows.

Usage: .venv/bin/python tools/pages/hits_envelope.py [--bin build-wt/iv_render] [--set k=v ...]
       (each --case "name:k=v,k=v" is one setting; defaults to the grainy_start settings)
Environment variables (the renderer's prototype hooks) pass through.
"""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf

SR = 48000
ROOT = Path(__file__).resolve().parents[2]
HITS = ROOT / "test_audio" / "stimulus" / "02_hits.wav"
ONSETS = [1.0 + 6.0 * k for k in range(6)]
LEVELS = ["snare -6", "snare -12", "snare -18", "rim -6", "rim -12", "rim -18"]
DEFAULT_CASES = [
    "HALL medium:space=HALL,era=0,size=0.5,decay=0.4",
    "MASSIVE small:space=MASSIVE,era=0,size=0,decay=0.4",
    "MASSIVE medium:space=MASSIVE,era=0,size=0.5,decay=0.4",
    "MASSIVE large:space=MASSIVE,era=0,size=1,decay=0.4",
]


def envelope_db(x: np.ndarray) -> np.ndarray:
    win, hop = int(0.02 * SR), int(0.005 * SR)
    p = np.convolve(x ** 2, np.ones(win) / win, "same")[::hop]
    return 10 * np.log10(p + 1e-14)


def moving_median(v: np.ndarray, n: int) -> np.ndarray:
    pad = np.pad(v, n // 2, mode="edge")
    return np.array([np.median(pad[i:i + n]) for i in range(len(v))])


def analyse(y: np.ndarray, x: np.ndarray) -> list[dict]:
    m = y.mean(axis=1)
    out = []
    for t0 in ONSETS:
        i0 = int(t0 * SR)
        seg = m[i0: i0 + int(2.5 * SR)]
        inp = x[i0: i0 + int(0.2 * SR)]
        env = envelope_db(seg)
        hop = 0.005
        peak = env.max()
        arrive = int(np.argmax(env > peak - 20))
        start = arrive + int(0.03 / hop)
        trend = moving_median(env, int(0.25 / hop))
        live = slice(start, len(env))
        dev = (env - trend)[live]
        ok = env[live] > peak - 40  # ignore the noise floor
        ripple = float(np.sqrt(np.mean(dev[ok] ** 2))) if ok.any() else 0.0
        order = np.argsort(dev)[::-1]
        lumps = []
        for j in order:
            if len(lumps) == 3 or dev[j] < 1.0:
                break
            if all(abs(j - k) > int(0.08 / hop) for k, _ in lumps):
                lumps.append((j, dev[j]))
        in_db = 20 * np.log10(np.abs(inp).max() + 1e-12)
        out.append({"ripple": ripple, "peak_vs_hit": float(peak - in_db),
                    "lumps": [((start + j) * hop, float(d)) for j, d in lumps], "arrive": arrive * hop})
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.environ.get("IV_RENDER", str(ROOT / "build" / "iv_render")))
    ap.add_argument("--case", action="append", help='"name:k=v,k=v" (repeatable)')
    args = ap.parse_args()
    cases = args.case or DEFAULT_CASES
    x, _ = sf.read(str(HITS), always_2d=True)
    x = x.mean(axis=1)
    with tempfile.TemporaryDirectory() as t:
        stim = Path(t) / "hits.wav"
        sf.write(str(stim), np.pad(x, (0, 4 * SR)).astype(np.float32), SR, subtype="FLOAT")
        for case in cases:
            name, sets = case.split(":", 1)
            o = Path(t) / "o.wav"
            cmd = [args.bin, str(stim), str(o), "--set", "mix=1"]
            for kv in sets.split(","):
                cmd += ["--set", kv]
            subprocess.run(cmd, check=True, capture_output=True)
            y, _ = sf.read(str(o), always_2d=True)
            res = analyse(y, x)
            rip = [r["ripple"] for r in res]
            lv = [r["peak_vs_hit"] for r in res]
            print(f"{name:18s} ripple dB per hit {' '.join(f'{v:4.2f}' for v in rip)}  (mean {np.mean(rip):.2f})"
                  f"  wet peak vs hit {' '.join(f'{v:+5.1f}' for v in lv)}")
            for lab, r in zip(LEVELS, res):
                ls = ", ".join(f"{tt:.2f} s +{d:.1f} dB" for tt, d in r["lumps"]) or "none over 1 dB"
                print(f"    {lab:10s} arrives {r['arrive']:.2f} s; lumps over trend: {ls}")


if __name__ == "__main__":
    main()
