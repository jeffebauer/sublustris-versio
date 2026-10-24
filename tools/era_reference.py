#!/usr/bin/env python3
"""ERA's sonic signature, measured from the engine, for the owner's reference page.

Each ERA stage (clean, Neve, cassette, mu-law, Midiverb) renders test signals through iv_render
(HALL, SIZE 0.6, DECAY 0.6, wet only, as M6) and this writes one JSON of chart data:
  tone       the wet's spectrum against clean (pink noise in, Welch PSD, 1/6-octave smoothed)
  decay      T60 per octave band after a pink burst stops (envelope, -5 to -25 dB fit, x3)
  harmonics  a 220 Hz sine's 2nd..7th harmonics against its fundamental, at -24 / -18 / -12 dBFS
  alias      a 7 kHz sine's spectrum: harmonics and the false "mirror" tones the 24 kHz boxes add
  grain      SINAD of a 1 kHz sine against its level, the hiss cancelled (x and -x, odd part);
  hiss       the even part (hiss and crackle) against the note
  wow        a held 1 kHz sine's pitch over time, in cents
The tanks' own modulation is off (IV_MOD_SCALE=0) except for wow, so only ERA moves the pitch.
Extra stages (e.g. a prototype's hook) can be added with --variant NAME:ENV=VALUE.

usage: .venv/bin/python tools/era_reference.py --out data.json [--render build/iv_render]
"""
import argparse
import json
import os
import subprocess
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parent.parent
SR = 48000
STAGES = [("clean", 0.0), ("neve", 0.3), ("cassette", 0.55), ("mulaw", 0.8), ("midiverb", 1.0)]
BASE = ["--set", "space=HALL", "--set", "size=0.6", "--set", "decay=0.6", "--set", "mix=1",
        "--set", "drift=0.5", "--set", "tone=0.5", "--set", "time=0.2"]


def render(src, era, tmp, tag, env=None, extra=()):
    out = Path(tmp) / f"{tag}.wav"
    e = dict(os.environ, **(env or {}))
    subprocess.run([A.render, str(src), str(out), "--set", f"era={era}", *BASE, *extra],
                   check=True, stdout=subprocess.DEVNULL, env=e)
    x, _ = sf.read(str(out), always_2d=True)
    out.unlink()
    return x.mean(axis=1)


def write(tmp, name, x):
    p = Path(tmp) / f"{name}.wav"
    sf.write(str(p), np.stack([x, x], 1).astype(np.float32), SR, subtype="FLOAT")
    return p


def stimuli(tmp):
    rng = np.random.default_rng(7)
    t = lambda s: np.arange(int(s * SR)) / SR
    pad = lambda x, s: np.concatenate([x, np.zeros(int(s * SR))])
    # pink noise, 4 s, -20 dBFS RMS
    w = rng.standard_normal(4 * SR)
    f = np.fft.rfftfreq(w.size, 1 / SR)
    W = np.fft.rfft(w) / np.sqrt(np.maximum(f, 20.0))
    pink = np.fft.irfft(W, w.size)
    pink *= 10 ** (-20 / 20) / np.sqrt(np.mean(pink ** 2))
    fade = lambda x: x * np.minimum(1, np.minimum(np.arange(x.size), np.arange(x.size)[::-1]) / (0.01 * SR))
    s = {"pink": pad(fade(pink), 8)}
    click = np.zeros(int(14 * SR)); click[int(0.1 * SR)] = 0.5
    s["click"] = click
    for lv in (-24, -18, -12):
        s[f"sine220_{lv}"] = pad(fade(10 ** (lv / 20) * np.sin(2 * np.pi * 220 * t(4))), 1)
    a7 = pad(fade(10 ** (-12 / 20) * np.sin(2 * np.pi * 7000 * t(4))), 1)
    s["alias7k"], s["alias7k_inv"] = a7, -a7
    for lv in range(-66, -11, 6):
        x = pad(fade(10 ** (lv / 20) * np.sin(2 * np.pi * 1000 * t(4))), 1)
        s[f"sine1k_{lv}"], s[f"sine1k_{lv}_inv"] = x, -x
    s["held1k"] = pad(fade(10 ** (-12 / 20) * np.sin(2 * np.pi * 1000 * t(8))), 1)
    return {k: write(tmp, k, v) for k, v in s.items()}


def psd(x, n=8192):
    win = np.hanning(n)
    segs = [x[i:i + n] * win for i in range(0, x.size - n, n // 2)]
    return np.fft.rfftfreq(n, 1 / SR), np.mean([np.abs(np.fft.rfft(s)) ** 2 for s in segs], axis=0)


def smooth_oct(f, p, frac=6):
    centres = 50 * 2 ** (np.arange(0, np.log2(20000 / 50) * frac + 1) / frac)
    out = []
    for c in centres:
        m = (f >= c * 2 ** (-0.5 / frac)) & (f < c * 2 ** (0.5 / frac))
        out.append(np.mean(p[m]) if m.any() else np.nan)
    return centres, np.array(out)


def band(x, fc):
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(x.size, 1 / SR)
    X[(f < fc / np.sqrt(2)) | (f >= fc * np.sqrt(2))] = 0
    return np.fft.irfft(X, x.size)


def t60(x):
    """From the level envelope (50 ms RMS): a line through -5 to -25 dB below where it starts, x3. Not a
    Schroeder integral: the low bands sit on a -50 dB floor that would bend it."""
    k = SR // 20
    env = 10 * np.log10(np.array([np.mean(x[i:i + k] ** 2) for i in range(0, x.size - k, k)]) + 1e-30)
    env -= env[0]
    i5, i25 = np.argmax(env <= -5), np.argmax(env <= -25)
    if i25 <= i5:
        return None
    t = np.arange(i5, i25 + 1) / 20
    slope = np.polyfit(t, env[i5:i25 + 1], 1)[0]
    return round(float(-60 / slope), 2) if slope < 0 else None


def steady(x, a=1.5, b=3.5):
    return x[int(a * SR):int(b * SR)]


def spectrum_db(x):
    n = x.size
    X = np.abs(np.fft.rfft(x * np.blackman(n))) / (np.sum(np.blackman(n)) / 2)
    return np.fft.rfftfreq(n, 1 / SR), 20 * np.log10(X + 1e-12)


def peak_db(f, d, f0, bw=15):
    m = (f > f0 - bw) & (f < f0 + bw)
    return float(d[m].max())


def sinad(x, f0=1000, notch=40):
    X = np.abs(np.fft.rfft(x * np.blackman(x.size))) ** 2
    f = np.fft.rfftfreq(x.size, 1 / SR)
    sig = X[(f > f0 - notch) & (f < f0 + notch)].sum()
    rest = X[(f > 20) & (f < 20000) & ~((f > f0 - notch) & (f < f0 + notch))].sum()
    return float(10 * np.log10(sig / rest))


def pitch_cents(x, f0=1000):
    """Instantaneous pitch of a held tone (analytic signal of a +-150 Hz band), 100 Hz rate, in cents."""
    ff = np.fft.fftfreq(x.size, 1 / SR)
    h = np.where((ff > f0 - 150) & (ff < f0 + 150), 2.0, 0.0)
    z = np.fft.ifft(np.fft.fft(x) * h)
    hz = np.diff(np.unwrap(np.angle(z))) * SR / (2 * np.pi)
    k = SR // 100
    hz = np.convolve(hz, np.ones(k) / k, mode="same")[::k]
    return 1200 * np.log2(np.maximum(hz, 1) / np.median(hz))


def measure(name, era, env, stim, tmp):
    r = lambda k, e=env: render(stim[k], era, tmp, f"{name}_{k}", e)
    nomod = dict(env, IV_MOD_SCALE="0")
    out = {"name": name, "era": era}
    # tone
    f, p = psd(r("pink", nomod))
    out["_psd"] = smooth_oct(f, p)
    # decay per octave, after the pink burst stops (a click is too quiet for the Midiverb's 13 bits)
    x = r("pink", nomod)[int(4.0 * SR):]
    out["decay"] = {str(fc): t60(band(x, fc)) for fc in (125, 250, 500, 1000, 2000, 4000, 8000)}
    # harmonics of a 220 Hz sine at three levels (louder ones reach the output limiter even clean)
    out["harmonics"] = {}
    for lv in (-24, -18, -12):
        f, d = spectrum_db(steady(r(f"sine220_{lv}", nomod)))
        h1 = peak_db(f, d, 220)
        out["harmonics"][str(lv)] = {str(k): round(peak_db(f, d, 220 * k) - h1, 1) for k in range(2, 8)}
    # alias: the spectrum around a 7 kHz tone, relative to it, 100 Hz - 20 kHz, on a coarse grid. The
    # odd part (x and -x rendered, halved difference) cancels the hiss, which is the same in both.
    f, d = spectrum_db(steady((r("alias7k", nomod) - r("alias7k_inv", nomod)) / 2))
    d -= peak_db(f, d, 7000)
    grid = np.arange(100, 20001, 50)
    out["alias"] = [round(float(d[(f >= g - 25) & (f < g + 25)].max()), 1) for g in grid]
    # grain
    # grain: the odd part's SINAD (hiss cancelled); hiss: the even part against the note
    out["grain"], out["hiss"] = {}, {}
    for lv in range(-66, -11, 6):
        a, b = steady(r(f"sine1k_{lv}", nomod)), steady(r(f"sine1k_{lv}_inv", nomod))
        out["grain"][str(lv)] = round(sinad((a - b) / 2), 1)
        out["hiss"][str(lv)] = round(float(10 * np.log10(np.mean(((a + b) / 2) ** 2) / np.mean(((a - b) / 2) ** 2) + 1e-30)), 1)
    # wow (modulation on: DRIFT noon still runs the floor)
    c = pitch_cents(r("held1k", nomod)[int(2 * SR):int(7.5 * SR)])
    out["wow"] = [round(float(v), 2) for v in c[5:-5:5]]  # 20 per second
    return out


ap = argparse.ArgumentParser()
ap.add_argument("--out", required=True)
ap.add_argument("--render", default=str(ROOT / "build" / "iv_render"))
ap.add_argument("--variant", action="append", default=[], help="NAME:ERA:ENV=VAL[,ENV=VAL]")
A = ap.parse_args()
runs = [(n, e, {}) for n, e in STAGES]
for v in A.variant:
    n, e, envs = v.split(":", 2)
    runs.append((n, float(e), dict(kv.split("=", 1) for kv in envs.split(",") if kv)))
with tempfile.TemporaryDirectory() as tmp:
    stim = stimuli(tmp)
    with ThreadPoolExecutor(min(8, len(runs))) as ex:
        res = list(ex.map(lambda r: measure(*r, stim, tmp), runs))
clean = next(r for r in res if r["name"] == "clean")
fc, pc = clean["_psd"]
for r in res:
    c, p = r.pop("_psd")
    r["tone"] = [None if not np.isfinite(v) else round(float(10 * np.log10(v / w)), 2) for v, w in zip(p, pc)]
json.dump({"tone_hz": [round(float(v), 1) for v in fc], "alias_hz": list(range(100, 20001, 50)),
           "grain_levels": list(range(-66, -11, 6)), "wow_rate_hz": 20, "stages": res}, open(A.out, "w"))
print("wrote", A.out)
