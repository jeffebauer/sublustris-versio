#!/usr/bin/env python3
"""v1.0.75's gates (ADR 0051): the port of the owner's pick D on renders/wear_ceiling, against the page and v1.0.74.

1. the page: every card's D re-rendered on the port, against the sha1 _verify.txt recorded for D_climbing, and to the bit
   against the prototype's own iv_render with IV_WEAR_CEILING=D;
2. WEAR 0: to the bit against v1.0.74, every SPACE, four materials, DECAY 0.6 and endless;
3. the drive: 13.5 x (1 - (1 - WEAR)^1.3) at 9 o'clock, noon, 3 o'clock and full (the numbers the page was labelled with);
4. clicks (1 ms frames above 3 kHz) on a held pad, every SPACE: WEAR 0 -> 1 -> 0 over 10 s each way, and a flick
   0.2 -> 1 -> 0.2 in 20 ms, on v1.0.74 and on the port;
5. infinite holds: every SPACE at endless, WEAR noon and full, MOTION noon and full left, pad 8 s then 72 s of silence:
   six 10 s windows from 10 s after it stops never above the first + 1 dB, on the port.

tools/same_render.py (mid-to-full WEAR only) and tools/pages/energy_check.py (--bin port --ref-bin v1.0.74) are run alongside.
Usage: <venv>/bin/python tools/pages/wear_ceiling_gates.py --port <iv_render> --ref <v1.0.74 iv_render> --proto <prototype iv_render> --out <file>
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import hashlib
import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import motion_radio_checks as mc

REPO_ = os.path.abspath(os.path.join(HERE, "../.."))
MAIN = REPO_.split("/.claude/worktrees/")[0]
SRC = MAIN + "/renders/_material"
PAGE = MAIN + "/renders/wear_ceiling"
SR = 48000
SPACES = ("HALL", "PLATE", "CANYON")
BASE = ["size=0.6", "time=0.6", "drift=0.5"]


def run(binary, src, sets, env=None, auto=None, tmp=None):
    e = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    e.update(env or {})
    with tempfile.NamedTemporaryFile(suffix=".wav", dir=tmp, delete=False) as f:
        out = f.name
    args = [binary, str(src), out, "--set", "mix=1"]
    for s in sets:
        args += ["--set", s]
    af = None
    if auto:
        af = out + ".json"
        Path(af).write_text('{"breakpoints": [' + ", ".join('{"t": %s, "key": "%s", "value": %s}' % a for a in auto) + "]}")
        args += ["--auto", af]
    subprocess.run(args, check=True, capture_output=True, env=e)
    if af:
        os.unlink(af)
    y = sf.read(out, always_2d=True, dtype="float32")[0]
    os.unlink(out)
    return y


def sha(x):
    return hashlib.sha1(np.ascontiguousarray(x, dtype=np.float32).tobytes()).hexdigest()[:12]


def page_cards():
    """(card, sets, material, D's sha1) from the page's _verify.txt."""
    cards, sets, card = [], None, None
    for line in open(PAGE + "/_verify.txt"):
        m = re.match(r"(\d\d_\S+)\s+\((.*?); (.*?)\)", line)
        if m:
            card = m.group(1)
            sets = [s.strip() for s in (m.group(2) + ", " + m.group(3)).split(",")]
            continue
        m = re.match(r"\s+(\w+)\s+D_climbing\s+sha1 (\w+)", line)
        if m:
            cards.append((card, sets, m.group(1), m.group(2)))
    return cards


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", required=True)
    ap.add_argument("--ref", required=True)
    ap.add_argument("--proto", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    lines = []
    pr = lambda s="": (lines.append(s), print(s, flush=True))
    with tempfile.TemporaryDirectory(dir=os.environ.get("WC_TMP")) as tmp, ThreadPoolExecutor(10) as ex:
        # 1
        jobs = []
        for card, sets, m, want in page_cards():
            src = f"{SRC}/{m}.wav"
            jobs.append((card, m, want, ex.submit(run, a.port, src, sets, None, None, tmp),
                         ex.submit(run, a.proto, src, sets, {"IV_WEAR_CEILING": "D"}, None, tmp)))
        same_page = same_proto = 0
        worst = -999.0
        for card, m, want, p, q in jobs:
            y, z = p.result(), q.result()
            same_page += sha(y) == want
            same_proto += np.array_equal(y, z)
            if not np.array_equal(y, z):
                d = 10 * np.log10(np.mean((y.astype(np.float64) - z) ** 2) + 1e-300) - 10 * np.log10(np.mean(z.astype(np.float64) ** 2))
                worst = max(worst, d)
                pr(f"   DIFFERS {card} {m}: {d:.1f} dB")
            if sha(y) != want:
                pr(f"   sha1 differs from the page's: {card} {m} {sha(y)} vs {want}")
        n = len(jobs)
        pr(f"1. The page's D on the port: sha1 as _verify.txt {same_page}/{n}; to the bit against the prototype's IV_WEAR_CEILING=D {same_proto}/{n}"
           + ("" if same_proto == n else f" (worst {worst:.1f} dB)"))
        # 2
        jobs = []
        for sp in SPACES:
            for m in ("pad", "piano", "drums", "bass"):
                for dec in (0.6, 0.97):
                    s = BASE + [f"space={sp}", f"decay={dec}", "era=0"]
                    src = f"{SRC}/{m}.wav"
                    jobs.append((ex.submit(run, a.port, src, s, None, None, tmp), ex.submit(run, a.ref, src, s, None, None, tmp)))
        same = sum(np.array_equal(p.result(), q.result()) for p, q in jobs)
        pr(f"2. WEAR 0, to the bit against v1.0.74: {same}/{len(jobs)}.")
        # 3
        pr("3. Drive at 9 o'clock, noon, 3 o'clock, full: " + " / ".join(f"{13.5 * (1 - (1 - v) ** 1.3):.1f}" for v in (0.2, 0.5, 0.8, 1.0)) + " dB.")
        # 4
        pr("4. Clicks: the worst 1 ms frame above 3 kHz over its neighbours, dB, v1.0.74 / port.")
        src = Path(tmp) / "pad.wav"
        sf.write(str(src), mc.pad(24.0, 4.0), SR, subtype="FLOAT")
        for sp in SPACES:
            for name, auto in (("WEAR 0 -> 1 -> 0 (10 s each way)", [(0, "era", 0.0), (2, "era", 0.0), (12, "era", 1.0), (22, "era", 0.0)]),
                               ("WEAR flicks 0.2 -> 1 -> 0.2 (20 ms)", [(0, "era", 0.2), (6, "era", 0.2), (6.02, "era", 1.0), (12, "era", 1.0), (12.02, "era", 0.2), (18, "era", 0.2)])):
                fs = [ex.submit(run, b, src, [f"space={sp}", "size=0.6", "decay=0.7", "drift=0.5"], None, auto, tmp) for b in (a.ref, a.port)]
                pr(f"   {sp:6s} {name:36s} " + " / ".join(f"{mc.clicks(f.result().astype(np.float64)):5.1f}" for f in fs))
        # 5
        hold = Path(tmp) / "hold.wav"
        sf.write(str(hold), mc.pad(8.0, 72.0), SR, subtype="FLOAT")
        pr("5. Infinite holds on the port, 10 s windows from 10 s after the pad (dB):")
        jobs = []
        for sp in SPACES:
            for era in (0.5, 1):
                for drift in (0.5, 0):
                    jobs.append((sp, era, drift, ex.submit(run, a.port, hold, [f"space={sp}", "size=0.6", "decay=0.97", f"era={era}", f"drift={drift}"], None, None, tmp)))
        grows = 0
        for sp, era, drift, f in jobs:
            y = f.result().astype(np.float64)
            w = [10 * np.log10(np.mean(y[int(t * SR):int((t + 10) * SR)] ** 2) + 1e-30) for t in range(18, 78, 10)]
            good = max(w) <= w[0] + 1.0 and np.isfinite(y).all()
            grows += not good
            pr(f"   {sp:6s} WEAR {era:3.1f} MOTION {drift:3.1f}: " + " ".join(f"{x:6.1f}" for x in w) + ("" if good else "  GROWS"))
        pr(f"   {len(jobs) - grows}/{len(jobs)} hold, no growth")
    Path(a.out).write_text("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
