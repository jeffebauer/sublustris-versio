#!/usr/bin/env python3
"""PROTOTYPE page: what happens to the tail when SPACE flips (owner, 25 Oct 2026).

Usage: .venv/bin/python tools/pages/space_flip.py [--render BIN] [--out renders/space_flip]

Emulates the hardware designs with two renders per flip, no Core change: the old SPACE stops
getting input at the flip (its tail goes on), the new SPACE starts fresh from the flip and takes
the input, and the two are mixed per version (Eventide H90 / Strymon BigSky "spillover": the
previous program's tail decays naturally, or over a set time). The engine's own flip (ADR 0013:
the old tail fades out over ~100 ms, the tank restarts) is rendered by the real engine moving SPACE.

Versions:
  A_today        the engine's own flip (automation of SPACE)
  B_ring_out     the old tail rings out fully
  C_fade_3s      the old tail fades out over ~3 s (60 dB, on top of its own decay)
  D_morph_3s     both spaces hear the music for 3 s while the old one's sound crossfades into the
                 new one's (equal power): PASP / US 10559295, crossfade rather than glide the delays
Columns are flips (HALL to MASSIVE, MASSIVE to HALL, PLATE to MASSIVE), rows the music. The flip
lands halfway through each piece's playing. 100 % wet, for the page's MIX fader (--wet-only --dry).
"""
import argparse
import json
import os
import subprocess
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]
MAIN = Path("/Users/jesse/Documents/Sites/sublustris-versio")  # pages live in the main checkout's renders/
SRC = MAIN / "renders" / "_material"
MATERIALS = ["skank", "vibes", "stabs", "pad", "keys", "voice"]
SPACE = {"PLATE": 0.0, "HALL": 0.5, "MASSIVE": 1.0}
FLIPS = [("1_hall_to_massive", "HALL", "MASSIVE"), ("2_massive_to_hall", "MASSIVE", "HALL"),
         ("3_plate_to_massive", "PLATE", "MASSIVE")]
COMMON = ["--set", "mix=1", "--set", "decay=0.5", "--set", "size=0.6"]
FADE_S, MORPH_S, EDGE_S = 3.0, 3.0, 0.01


def render(binary, src, dst, args):
    subprocess.run([binary, str(src), str(dst)] + COMMON + args, check=True, stdout=subprocess.DEVNULL)
    x, sr = sf.read(dst, always_2d=True)
    return x, sr


def flip_time(x, sr):
    """Halfway through the piece's playing (the material ends in 12 s of silence)."""
    m = np.abs(x).max(axis=1)
    on = np.nonzero(m > 1e-3)[0]
    return (on[0] + 0.5 * (on[-1] - on[0])) / sr


def ramp(n_total, sr, start_s, length_s, rising):
    """0 → 1 (or 1 → 0) raised-cosine ramp starting at start_s."""
    t = (np.arange(n_total) / sr - start_s) / length_s
    g = 0.5 - 0.5 * np.cos(np.pi * np.clip(t, 0.0, 1.0))
    return g if rising else 1.0 - g


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", default=str(ROOT / "build-scratch" / "iv_render"))
    ap.add_argument("--out", default=str(MAIN / "renders" / "space_flip"))
    a = ap.parse_args()
    out = Path(a.out)
    tmp = Path(tempfile.mkdtemp(prefix="space_flip_"))
    for m in MATERIALS:
        src = SRC / f"{m}.wav"
        x, sr = sf.read(src, always_2d=True)
        n = len(x)
        tf = flip_time(x, sr)
        cut = ramp(n, sr, tf, EDGE_S, rising=False)[:, None]        # the old space's input stops
        start = ramp(n, sr, tf, EDGE_S, rising=True)[:, None]       # the new space's input starts
        sf.write(tmp / "cut.wav", x * cut, sr, subtype="FLOAT")
        sf.write(tmp / "start.wav", x * start, sr, subtype="FLOAT")
        for col, old, new in FLIPS:
            d = out / col
            d.mkdir(parents=True, exist_ok=True)
            auto = tmp / "flip.json"
            auto.write_text(json.dumps({"breakpoints": [
                {"t": 0, "key": "space", "value": SPACE[old]}, {"t": tf - 0.001, "key": "space", "value": SPACE[old]},
                {"t": tf, "key": "space", "value": SPACE[new]}]}))
            today, _ = render(a.render, src, tmp / "today.wav", ["--set", f"space={old}", "--auto", str(auto)])
            old_full, _ = render(a.render, src, tmp / "old_full.wav", ["--set", f"space={old}"])
            old_cut, _ = render(a.render, tmp / "cut.wav", tmp / "old_cut.wav", ["--set", f"space={old}"])
            new_wet, _ = render(a.render, tmp / "start.wav", tmp / "new.wav", ["--set", f"space={new}"])
            # The new space hears nothing before the flip; its render starts from silence there.
            t = np.arange(n) / sr
            fade = np.where(t < tf, 1.0, 10.0 ** (-3.0 * np.clip(t - tf, 0.0, None) / FADE_S))[:, None]
            xo = ramp(n, sr, tf, MORPH_S, rising=True)                 # 0 → 1 across the morph
            g_old, g_new = np.cos(0.5 * np.pi * xo)[:, None], np.sin(0.5 * np.pi * xo)[:, None]
            # Morph: the old space keeps hearing the music until its sound has crossfaded out.
            versions = {
                "A_today": today,
                "B_ring_out": old_cut + new_wet,
                "C_fade_3s": old_cut * fade + new_wet,
                "D_morph_3s": old_full * g_old + new_wet * g_new,
            }
            for tag, y in versions.items():
                sf.write(d / f"{m}_{tag}.wav", y.astype(np.float32), sr, subtype="PCM_24")
            print(f"{col} {m}: flip at {tf:.2f} s")
    print(out / "index.html")


if __name__ == "__main__":
    main()
