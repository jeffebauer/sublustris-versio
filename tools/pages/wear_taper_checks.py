#!/usr/bin/env python3
"""renders/wear_taper's checks, each version (IV_WEAR_TAPER A-D) on the prototype's iv_render:
1. clicks (1 ms frames above 3 kHz, motion_radio_checks.py's measure) on a held pad, HALL and PLATE, MOTION noon:
   WEAR 0 -> 1 over 10 s (and back over 10 s), and a flick (WEAR 0.25 -> 0.75 in 20 ms and back);
2. an infinite hold: HALL and PLATE, DECAY endless (0.9), WEAR noon, the pad 8 s then 72 s of silence: six 10 s windows
   from 10 s after it stops never above the first + 1 dB (the wear is on the wet after the loop, save the compression).
Usage: /path/to/.venv/bin/python tools/pages/wear_taper_checks.py <out.txt>   (IV_RENDER default build/iv_render)
"""
import os
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import motion_radio_checks as mc

REPO_ = os.path.abspath(os.path.join(HERE, "../.."))
R = os.environ.get("IV_RENDER", REPO_ + "/build/iv_render")
SR = mc.SR
VERSIONS = "ABCD"


def render(v, src, out, sets, auto=None):
    # mc.render strips IV_* from the environment, so pass the hook through a wrapper.
    w = out.with_suffix(".sh")
    w.write_text(f"#!/bin/sh\nIV_WEAR_TAPER={v} exec {R} \"$@\"\n")
    w.chmod(0o755)
    y = mc.render(w, src, out, sets, auto)
    w.unlink()
    return y


if __name__ == "__main__":
    out = Path(sys.argv[1]); tmp = out.parent / "_checks"; tmp.mkdir(exist_ok=True)
    lines = ["clicks: the worst 1 ms frame above 3 kHz over its neighbours, dB, per version " + " ".join(VERSIONS)]
    src = tmp / "pad.wav"; sf.write(str(src), mc.pad(24.0, 4.0), SR, subtype="FLOAT")
    for space in ("HALL", "PLATE"):
        for name, auto in (("WEAR 0 -> 1 -> 0 (10 s each way)", [(0, "era", 0.0), (2, "era", 0.0), (12, "era", 1.0), (22, "era", 0.0)]),
                           ("WEAR flicks 0.25 -> 0.75 -> 0.25 (20 ms)", [(0, "era", 0.25), (6, "era", 0.25), (6.02, "era", 0.75),
                                                                         (12, "era", 0.75), (12.02, "era", 0.25), (18, "era", 0.25)])):
            r = [mc.clicks(render(v, src, tmp / "x.wav", [f"space={space}", "size=0.6", "decay=0.7", "drift=0.5"], auto)) for v in VERSIONS]
            lines.append(f"  {space:6s} {name:40s} " + "  ".join(f"{v} {c:5.1f}" for v, c in zip(VERSIONS, r)))
    hold = tmp / "hold.wav"; sf.write(str(hold), mc.pad(8.0, 72.0), SR, subtype="FLOAT")
    for space in ("HALL", "PLATE"):
        for v in VERSIONS:
            y = render(v, hold, tmp / "h.wav", [f"space={space}", "size=0.6", "decay=0.9", "era=0.5", "drift=0.5"])
            w = [10 * np.log10(np.mean(y[int(t * SR):int((t + 10) * SR)] ** 2) + 1e-30) for t in range(18, 78, 10)]
            ok = max(w) <= w[0] + 1.0 and np.isfinite(y).all()
            lines.append(f"hold {v}: {space} endless WEAR noon, 10 s windows from 10 s after the pad (dB) "
                         + " ".join(f"{x:6.1f}" for x in w) + f"  -> {'holds, no growth' if ok else 'GROWS'}")
    for p in (src, hold): p.unlink()
    for p in tmp.glob("*"): p.unlink()
    tmp.rmdir()
    out.write_text("\n".join(lines) + "\n")
    print("\n".join(lines))
