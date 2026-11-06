#!/usr/bin/env python3
"""WEAR taper's checks (v1.0.69, ADR 0044), the built renderer against a reference one (main before the change):
1. clicks (1 ms frames above 3 kHz, motion_radio_checks.py's measure) on a held pad, HALL, PLATE and CANYON, MOTION noon:
   WEAR 0 -> 1 over 10 s and back over 10 s, and a flick (WEAR 0.25 -> 0.75 in 20 ms and back);
2. an infinite hold: each SPACE at DECAY endless (0.9), WEAR noon, 3 o'clock (0.8) and full at MOTION noon, and full at
   MOTION full left (v1.0.70's drive, ADR 0045), the pad 8 s then 72 s of silence: six 10 s windows from 10 s after it
   stops never above the first + 1 dB.
Usage: <venv>/bin/python tools/pages/wear_taper_checks.py <out.txt>
  (IV_RENDER: the built iv_render, default build/; WT_REF: the reference iv_render, optional, for its click numbers)
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
REF = os.environ.get("WT_REF")
SR = mc.SR
SPACES = ("HALL", "PLATE", "CANYON")

if __name__ == "__main__":
    out = Path(sys.argv[1]); tmp = out.parent / "_checks"; tmp.mkdir(exist_ok=True)
    bins = [("built", R)] + ([("ref", REF)] if REF else [])
    lines = ["clicks: the worst 1 ms frame above 3 kHz over its neighbours, dB, " + " / ".join(n for n, _ in bins)]
    src = tmp / "pad.wav"; sf.write(str(src), mc.pad(24.0, 4.0), SR, subtype="FLOAT")
    for space in SPACES:
        for name, auto in (("WEAR 0 -> 1 -> 0 (10 s each way)", [(0, "era", 0.0), (2, "era", 0.0), (12, "era", 1.0), (22, "era", 0.0)]),
                           ("WEAR flicks 0.25 -> 0.75 -> 0.25 (20 ms)", [(0, "era", 0.25), (6, "era", 0.25), (6.02, "era", 0.75),
                                                                         (12, "era", 0.75), (12.02, "era", 0.25), (18, "era", 0.25)])):
            r = [mc.clicks(mc.render(b, src, tmp / "x.wav", [f"space={space}", "size=0.6", "decay=0.7", "drift=0.5"], auto)) for _, b in bins]
            lines.append(f"  {space:6s} {name:40s} " + "  ".join(f"{n} {c:5.1f}" for (n, _), c in zip(bins, r)))
    hold = tmp / "hold.wav"; sf.write(str(hold), mc.pad(8.0, 72.0), SR, subtype="FLOAT")
    for space in SPACES:
        for era, clock, drift in (("0.5", "noon", "0.5"), ("0.8", "3 o'clock", "0.5"), ("1", "full", "0.5"), ("1", "full, MOTION left", "0")):
            y = mc.render(R, hold, tmp / "h.wav", [f"space={space}", "size=0.6", "decay=0.9", f"era={era}", f"drift={drift}"])
            w = [10 * np.log10(np.mean(y[int(t * SR):int((t + 10) * SR)] ** 2) + 1e-30) for t in range(18, 78, 10)]
            ok = max(w) <= w[0] + 1.0 and np.isfinite(y).all()
            lines.append(f"hold: {space:6s} endless WEAR {clock:17s}, 10 s windows from 10 s after the pad (dB) "
                         + " ".join(f"{x:6.1f}" for x in w) + f"  -> {'holds, no growth' if ok else 'GROWS'}")
    for p in tmp.glob("*"): p.unlink()
    tmp.rmdir()
    out.write_text("\n".join(lines) + "\n")
    print("\n".join(lines))
