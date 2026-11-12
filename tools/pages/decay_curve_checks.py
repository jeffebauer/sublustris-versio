#!/usr/bin/env python3
"""renders/decay_curve's checks (IV_DECAY_CURVE A-D, core/params/Mappings.h decayCurveKnob), every SPACE, SIZE 0.6:
1. clicks (the worst 1 ms frame above 3 kHz over its neighbours, motion_radio_checks.py's measure) on a held pad, MOTION
   noon: DECAY from 0.1 under the version's finite top to fully CW over 10 s and back over 10 s; and a flick across the
   crossover (0.02 under the top -> 0.01 past endless in 50 ms, back after 3 s);
2. holds: the pad 8 s then 72 s of silence at the version's endless start, 0.005 past it and 0.005 under it (the
   crossover, where the tail holds once the input is quiet), with MOTION noon, full left and full right; and frozen
   (a tap at 7.5 s, the pad held to the end) at the endless start. Six 10 s windows from 10 s after the pad: endless
   passes when no window is above the first + 1 dB and nothing is non-finite;
3. endless follows the playing (v1.0.61): while the pad plays (2-8 s) the endless start's level against the finite
   top's (dB, should be ~0), then 60 s after it stops still within 3 dB of where it stood 10 s after.
Usage: <venv>/bin/python tools/pages/decay_curve_checks.py <out.txt>  (IV_RENDER: the prototype's iv_render)
"""
import json
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import decay_curve_table as dt
import motion_radio_checks as mc

REPO_ = os.path.abspath(os.path.join(HERE, "../.."))
R = os.environ.get("IV_RENDER", REPO_ + "/build/iv_render")
SR = mc.SR
SPACES = ("PLATE", "HALL", "CANYON")
VERS = "ABCD"


def render(ver, src, out, sets, auto=None, buttons=None):
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    env["IV_DECAY_CURVE"] = ver
    args = [R, str(src), str(out), "--set", "mix=1"]
    for s in sets:
        args += ["--set", s]
    if auto or buttons:
        af = out.with_name(out.stem + "_auto.json")
        d = {"breakpoints": [{"t": t, "key": k, "value": v} for t, k, v in (auto or [])]}
        if buttons:
            d["buttons"] = buttons
        af.write_text(json.dumps(d))
        args += ["--auto", str(af)]
    p = subprocess.run(args, capture_output=True, text=True, env=env)
    if p.returncode != 0:
        sys.exit(p.stderr)
    if f"IV_DECAY_CURVE={ver}" not in p.stderr:
        sys.exit(f"hook line missing for {ver}: {p.stderr}")
    if auto or buttons:
        af.unlink()
    y, _ = sf.read(str(out), always_2d=True)
    out.unlink()
    return y


def win(y, t0, t1):
    return 10 * np.log10(np.mean(y[int(t0 * SR):int(t1 * SR)] ** 2) + 1e-30)


if __name__ == "__main__":
    out = Path(sys.argv[1]); tmp = out.parent / "_dc_checks"; tmp.mkdir(exist_ok=True)
    src = tmp / "pad.wav"; sf.write(str(src), mc.pad(30.0, 4.0), SR, subtype="FLOAT")
    hold = tmp / "hold.wav"; sf.write(str(hold), mc.pad(8.0, 72.0), SR, subtype="FLOAT")
    base = ["size=0.6", "era=0.3"]
    jobs = []
    for sp in SPACES:
        for v in VERS:
            f, e, _ = dt.VERSIONS[v]
            sweep = [(0, "decay", f - 0.1), (4, "decay", f - 0.1), (14, "decay", 1.0), (24, "decay", f - 0.1)]
            flick = [(0, "decay", f - 0.02), (6, "decay", f - 0.02), (6.05, "decay", min(1.0, e + 0.01)),
                     (9, "decay", min(1.0, e + 0.01)), (9.05, "decay", f - 0.02)]
            jobs.append(("click", sp, v, "sweep", (src, [f"space={sp}", "drift=0.5"] + base, sweep, None)))
            jobs.append(("click", sp, v, "flick", (src, [f"space={sp}", "drift=0.5"] + base, flick, None)))
            for name, d in (("endless start", e), ("endless +0.005", min(1.0, e + 0.005)), ("crossover -0.005", e - 0.005)):
                for drift in ("0.5", "0", "1"):
                    jobs.append(("hold", sp, v, f"{name} {d:.3f} MOTION {drift}", (hold, [f"space={sp}", f"decay={d}", f"drift={drift}"] + base, None, None)))
            jobs.append(("hold", sp, v, f"frozen at endless start {e:.3f}", (hold, [f"space={sp}", f"decay={e}", "drift=0.5"] + base, None, [[7.5, 7.6]])))
            jobs.append(("follow", sp, v, "top", (hold, [f"space={sp}", f"decay={f}", "drift=0.5"] + base, None, None)))
            jobs.append(("follow", sp, v, "endless", (hold, [f"space={sp}", f"decay={e}", "drift=0.5"] + base, None, None)))

    def run(i_job):
        i, (kind, sp, v, name, (s, sets, auto, buttons)) = i_job
        return render(v, s, tmp / f"r{i}.wav", sets, auto, buttons)

    with ThreadPoolExecutor(max_workers=8) as ex:
        ys = list(ex.map(run, enumerate(jobs)))
    lines = ["renders/decay_curve checks (SIZE 0.6, WEAR 0.3), renderer " + R, ""]
    clicks, fails = {}, []
    lines.append("1. clicks, dB (worst 1 ms frame > 3 kHz over its neighbours; the music's own read ~15-20):")
    for (kind, sp, v, name, _), y in zip(jobs, ys):
        if kind == "click":
            clicks[(sp, v, name)] = mc.clicks(y)
    for sp in SPACES:
        for name in ("sweep", "flick"):
            lines.append(f"  {sp:6s} DECAY {name:5s} " + "  ".join(f"{v} {clicks[(sp, v, name)]:5.1f}" for v in VERS))
    lines += ["", "2. holds, six 10 s windows from 18 s (dB); endless passes with none above the first + 1 dB:"]
    for (kind, sp, v, name, _), y in zip(jobs, ys):
        if kind != "hold":
            continue
        w = [win(y, t, t + 10) for t in range(18, 78, 10)]
        endless = "crossover" not in name
        ok = np.isfinite(y).all() and max(w) <= w[0] + 1.0
        verdict = ("holds, no growth" if ok else "GROWS") if endless else (
            "no growth" if ok else "GROWS") + f", {w[-1] - w[0]:+.1f} dB over 50 s"
        if not ok:
            fails.append(f"{sp} {v} {name}")
        lines.append(f"  {sp:6s} {v} {name:34s} " + " ".join(f"{x:6.1f}" for x in w) + f"  -> {verdict}")
    lines += ["", "3. endless follows the playing: playing (2-8 s) endless start vs finite top; held 18-28 s vs 68-78 s:"]
    fol = {}
    for (kind, sp, v, name, _), y in zip(jobs, ys):
        if kind == "follow":
            fol[(sp, v, name)] = y
    for sp in SPACES:
        for v in VERS:
            top, end = fol[(sp, v, "top")], fol[(sp, v, "endless")]
            play = win(end, 2, 8) - win(top, 2, 8)
            drift = win(end, 68, 78) - win(end, 18, 28)
            ok = abs(play) < 1.0 and abs(drift) < 3.0
            if not ok:
                fails.append(f"{sp} {v} follows")
            lines.append(f"  {sp:6s} {v} playing {play:+5.2f} dB re the top; held {drift:+5.2f} dB over 50 s  -> {'ok' if ok else 'CHECK'}")
    lines += ["", "result: " + ("all pass" if not fails else "check: " + "; ".join(fails))]
    for p in tmp.glob("*"):
        p.unlink()
    tmp.rmdir()
    out.write_text("\n".join(lines) + "\n")
    print("\n".join(lines))
