#!/usr/bin/env python3
"""SIZE's ends, measured (no page): how smooth the echoes are and how metallic the tail is, per SPACE and SIZE, at DECAY
noon, MOTION noon, WEAR 0, TIME 0, ONSET NORMAL, 100 % wet, on a single click (an impulse of 0.5 at 0.5 s).

- NED: Abel & Huang's normalised echo density (AES 2006): in 20 ms windows, the share of samples more than one standard
  deviation from zero over a Gaussian's share (0.3173). 1 = noise-like (smooth), well under 1 = separate echoes.
  Reported: the mean over the first 300 ms after the wet arrives, and when it first reaches 0.9 (the mixing time).
- kurt: the excess kurtosis in 10 ms windows over the same 300 ms, median (0 = Gaussian; large = spiky, grainy).
- flutter: the early envelope (the first 300 ms, 2 ms RMS in dB, its decay trend removed by a quadratic fit),
  normalised autocorrelation: its highest local peak at 15-150 ms lags, and that lag (a high peak = a repeating
  echo, flutter; independent noise reads ~0.1-0.2).
- ring: narrow peaks in the tail (1-4 s after the click, 200 Hz-10 kHz, 8192-point frames averaged): the share of bins
  (per mille) over +10 dB against the spectrum's running median (+-1/6 octave), and the highest peak (dB).
Usage: <venv>/bin/python tools/pages/size_texture.py [out.txt]   (IV_RENDER: the iv_render, default build/)
"""
import os
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]
R = os.environ.get("IV_RENDER", str(ROOT / "build" / "iv_render"))
SR = 48000
SIZES = (0.0, 0.25, 0.5, 0.75, 1.0)
SPACES = ("PLATE", "HALL", "CANYON")


def render(tmp, space, size):
    o = tmp / f"{space}_{size}.wav"
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    subprocess.run([R, str(tmp / "click.wav"), str(o), "--set", "mix=1", "--set", f"space={space}", "--set", f"size={size}",
                    "--set", "decay=0.5", "--set", "drift=0.5", "--set", "era=0", "--set", "time=0", "--set", "attack=NORMAL"],
                   check=True, capture_output=True, env=env)
    y = sf.read(str(o), always_2d=True)[0]
    o.unlink()
    return y


def measure(y):
    x = y[:, 0]
    a = int(0.5 * SR)
    env = np.abs(x[a:])
    arr = a + int(np.argmax(env > env.max() * 0.05))  # the wet's arrival
    seg = x[arr:arr + int(0.3 * SR)]
    w = int(0.02 * SR)
    ned = []
    for i in range(0, len(seg) - w, w // 2):
        s = seg[i:i + w]
        sd = s.std()
        ned.append(np.mean(np.abs(s) > sd) / 0.3173 if sd > 0 else 0.0)
    ned = np.array(ned)
    mix = next((i * (w // 2) / SR for i, v in enumerate(ned) if v >= 0.9), float("nan"))
    k = int(0.01 * SR)
    kurt = [((s - s.mean()) ** 4).mean() / (s.var() ** 2 + 1e-30) - 3 for s in (seg[i:i + k] for i in range(0, len(seg) - k, k))]
    r = int(0.002 * SR)
    e = 10 * np.log10((seg[: len(seg) // r * r].reshape(-1, r) ** 2).mean(1) + 1e-20)
    tt = np.arange(len(e))
    e = e - np.polyval(np.polyfit(tt, e, 2), tt)  # the decay's own trend out: what's left is the echoes' pattern
    ac = np.correlate(e, e, "full")[len(e) - 1:]
    ac /= ac[0] + 1e-30
    lo, hi = int(0.015 / 0.002), int(0.15 / 0.002)
    peaks = [i for i in range(lo, hi) if ac[i] > ac[i - 1] and ac[i] >= ac[i + 1]]
    j = max(peaks, key=lambda i: ac[i]) if peaks else lo
    tail = y[int(1.5 * SR):int(4.5 * SR)].mean(1)
    n = 8192
    P = np.mean([np.abs(np.fft.rfft(tail[i:i + n] * np.hanning(n))) ** 2 for i in range(0, len(tail) - n, n // 2)], axis=0)
    f = np.fft.rfftfreq(n, 1 / SR)
    db = 10 * np.log10(P + 1e-30)
    band = np.nonzero((f >= 200) & (f <= 10000))[0]
    over = []
    for b in band:
        lo_, hi_ = np.searchsorted(f, f[b] / 2 ** (1 / 6)), np.searchsorted(f, f[b] * 2 ** (1 / 6))
        over.append(db[b] - np.median(db[lo_:hi_ + 1]))
    over = np.array(over)
    return {"arrive_ms": (arr - a) / SR * 1000, "ned": float(ned.mean()), "mix_ms": mix * 1000, "kurt": float(np.median(kurt)),
            "flutter": float(ac[j]), "flutter_ms": j * 2.0, "ring_pm": float(np.mean(over > 10) * 1000), "ring_max": float(over.max())}


if __name__ == "__main__":
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else None
    with tempfile.TemporaryDirectory() as t:
        tmp = Path(t)
        x = np.zeros((int(6 * SR), 2))
        x[int(0.5 * SR)] = 0.5
        sf.write(str(tmp / "click.wav"), x, SR, subtype="FLOAT")
        jobs = [(s, z) for s in SPACES for z in SIZES]
        with ThreadPoolExecutor(8) as ex:
            res = list(ex.map(lambda j: measure(render(tmp, *j)), jobs))
    lines = ["SIZE texture (click, DECAY noon, MOTION noon, WEAR 0, TIME 0, NORMAL; renderer " + R + ")",
             f"{'SPACE':7s}{'SIZE':>5s} {'arrive':>7s} {'NED':>5s} {'mix@0.9':>8s} {'kurt':>6s} {'flutter':>8s} {'at':>6s} {'ring‰':>6s} {'ringmax':>8s}"]
    for (s, z), m in zip(jobs, res):
        lines.append(f"{s:7s}{z:5.2f} {m['arrive_ms']:6.0f}ms {m['ned']:5.2f} {m['mix_ms']:6.0f}ms {m['kurt']:6.2f} {m['flutter']:8.2f} "
                     f"{m['flutter_ms']:4.0f}ms {m['ring_pm']:6.1f} {m['ring_max']:6.1f}dB")
    print("\n".join(lines))
    if out:
        out.write_text("\n".join(lines) + "\n")
