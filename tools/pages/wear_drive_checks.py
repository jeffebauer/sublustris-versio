#!/usr/bin/env python3
"""renders/wear_drive's checks, each version (IV_WEAR_DRIVE A-D) on the prototype's iv_render:
1. clicks (1 ms frames above 3 kHz, motion_radio_checks.py's measure) on a held pad, HALL and CANYON, MOTION noon:
   WEAR 0 -> 1 over 10 s and back over 10 s (D's join at 0.75 crossed both ways), and a flick (0.5 -> 1 in 20 ms and back);
2. an infinite hold for C and D (and A for reference): CANYON, DECAY endless (0.9), WEAR full, MOTION noon and full left,
   the pad 8 s then 72 s of silence: six 10 s windows from 10 s after it stops never above the first + 1 dB.
Usage: <venv>/bin/python tools/pages/wear_drive_checks.py <out.txt>   (IV_RENDER default build/iv_render)
The IV_WEAR_DRIVE hook is gone since v1.0.70 (C is built, ADR 0045); rerun this from commit 88a5148.
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
    w = out.with_suffix(".sh")  # mc.render strips IV_* from the environment, so pass the hook through a wrapper
    w.write_text(f"#!/bin/sh\nIV_WEAR_DRIVE={v} exec {R} \"$@\"\n")
    w.chmod(0o755)
    y = mc.render(w, src, out, sets, auto)
    w.unlink()
    return y


if __name__ == "__main__":
    out = Path(sys.argv[1]); tmp = out.parent / "_checks"; tmp.mkdir(exist_ok=True)
    lines = ["clicks: the worst 1 ms frame above 3 kHz over its neighbours, dB, per version " + " ".join(VERSIONS)]
    src = tmp / "pad.wav"; sf.write(str(src), mc.pad(24.0, 4.0), SR, subtype="FLOAT")
    for space in ("HALL", "CANYON"):
        for name, auto in (("WEAR 0 -> 1 -> 0 (10 s each way)", [(0, "era", 0.0), (2, "era", 0.0), (12, "era", 1.0), (22, "era", 0.0)]),
                           ("WEAR flicks 0.5 -> 1 -> 0.5 (20 ms)", [(0, "era", 0.5), (6, "era", 0.5), (6.02, "era", 1.0),
                                                                     (12, "era", 1.0), (12.02, "era", 0.5), (18, "era", 0.5)])):
            r = [mc.clicks(render(v, src, tmp / "x.wav", [f"space={space}", "size=0.6", "decay=0.7", "drift=0.5"], auto)) for v in VERSIONS]
            lines.append(f"  {space:6s} {name:36s} " + "  ".join(f"{v} {c:5.1f}" for v, c in zip(VERSIONS, r)))
    hold = tmp / "hold.wav"; sf.write(str(hold), mc.pad(8.0, 72.0), SR, subtype="FLOAT")
    for drift, dname in (("0.5", "MOTION noon"), ("0", "MOTION full left")):
        for v in "ACD":
            y = render(v, hold, tmp / "h.wav", ["space=CANYON", "size=0.6", "decay=0.9", "era=1", f"drift={drift}"])
            w = [10 * np.log10(np.mean(y[int(t * SR):int((t + 10) * SR)] ** 2) + 1e-30) for t in range(18, 78, 10)]
            ok = max(w) <= w[0] + 1.0 and np.isfinite(y).all()
            lines.append(f"hold {v}: CANYON endless WEAR full {dname:16s}, 10 s windows from 10 s after the pad (dB) "
                         + " ".join(f"{x:6.1f}" for x in w) + f"  -> {'holds, no growth' if ok else 'GROWS'}")
    for p in tmp.glob("*"): p.unlink()
    tmp.rmdir()
    out.write_text("\n".join(lines) + "\n")
    print("\n".join(lines))
