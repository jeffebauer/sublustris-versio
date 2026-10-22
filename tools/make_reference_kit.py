#!/usr/bin/env python3
"""The files the owner plays through their reverbs (docs/reference-recording-guide.md).

Usage: .venv/bin/python tools/make_reference_kit.py
Writes test_audio/reference_kit/ (not in git; re-run to rebuild), 48 kHz stereo 24-bit:
  calibrate.wav  a sync click, then five clicks 22 s apart: turn DECAY to the next
                 marked position in each gap; tools/ingest_reference.py measures
                 each click's decay so knob-only units can be set to 2 / 5 / 15 s
  measure.wav    a sync click, a click, a pink-noise burst and a 10 s sine sweep,
                 each followed by 25 s of silence: decay per band and the response
  music.wav      a sync click, then every listening material (renders/_material,
                 built by tools/make_material.py) with its tail, 3 s apart
  cues.json      where each part starts, in seconds from the sync click
Every recording starts with the sync click at 1.0 s, so the ingest tool can find
the parts again whatever the recording's offset.
"""
import json
from pathlib import Path

import numpy as np
import soundfile as sf

SR = 48000
ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "test_audio" / "reference_kit"
MATERIAL = ROOT / "renders" / "_material"
ORDER = ["skank", "vibes", "stabs", "pad", "lead", "drums", "bass", "keys", "voice"]
SYNC_AT = 1.0


def click(level_db: float = -6.0) -> np.ndarray:
    """A 0.1 ms raised-cosine click: energy up to ~15 kHz."""
    n = max(3, int(0.0001 * SR))
    return 10 ** (level_db / 20) * 0.5 * (1 - np.cos(2 * np.pi * np.arange(n) / n))


def pink(seconds: float, level_db: float = -12.0) -> np.ndarray:
    rng = np.random.default_rng(1)
    n = int(seconds * SR)
    spec = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / SR)
    spec[1:] /= np.sqrt(f[1:])
    spec[0] = 0
    x = np.fft.irfft(spec, n)
    x *= 10 ** (level_db / 20) / np.abs(x).max()
    fade = int(0.01 * SR)
    x[:fade] *= np.linspace(0, 1, fade)
    x[-fade:] *= np.linspace(1, 0, fade)
    return x


def sweep(seconds: float = 10.0, f0: float = 20.0, f1: float = 20000.0, level_db: float = -12.0) -> np.ndarray:
    """Exponential sine sweep (Farina), faded at both ends."""
    t = np.arange(int(seconds * SR)) / SR
    k = np.log(f1 / f0)
    x = np.sin(2 * np.pi * f0 * seconds / k * (np.exp(t * k / seconds) - 1))
    fade = int(0.05 * SR)
    x[:fade] *= np.linspace(0, 1, fade)
    x[-fade:] *= np.linspace(1, 0, fade)
    return 10 ** (level_db / 20) * x


class Track:
    def __init__(self):
        self.parts: list[tuple[float, np.ndarray]] = []
        self.cues: dict[str, float] = {}
        self.t = SYNC_AT
        self.put("sync", click(), 3.0)

    def put(self, name: str, x: np.ndarray, gap: float):
        if x.ndim == 1:
            x = np.stack([x, x], axis=1)
        self.parts.append((self.t, x))
        self.cues[name] = round(self.t - SYNC_AT, 6)
        self.t += len(x) / SR + gap

    def write(self, name: str):
        y = np.zeros((int(self.t * SR) + SR, 2))
        for t, x in self.parts:
            i = int(round(t * SR))
            y[i:i + len(x)] += x
        sf.write(str(OUT / f"{name}.wav"), y.astype(np.float32), SR, subtype="PCM_24")
        print(f"wrote test_audio/reference_kit/{name}.wav: {len(y) / SR:.0f} s")
        return self.cues


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    cues = {}

    t = Track()
    for k in range(5):
        t.put(f"click{k + 1}", click(), 22.0)
    cues["calibrate"] = t.write("calibrate")

    t = Track()
    t.put("click", click(), 25.0)
    t.put("noise", pink(0.5), 25.0)
    t.put("sweep", sweep(), 25.0)
    cues["measure"] = t.write("measure")

    t = Track()
    for m in ORDER:
        p = MATERIAL / f"{m}.wav"
        if not p.exists():
            raise SystemExit(f"missing {p}: run tools/render_review_pages.sh or tools/make_material.py first")
        x, sr = sf.read(str(p), always_2d=True)
        assert sr == SR
        t.put(m, x, 3.0)
    cues["music"] = t.write("music")

    (OUT / "cues.json").write_text(json.dumps(cues, indent=2))


if __name__ == "__main__":
    main()
