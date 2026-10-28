"""Print a level trace (dB, 0.5 s steps of 200 ms-smoothed 50 ms RMS) for one case: held, moved, per renderer/env.
Usage: trace.py mat space size d0 d1 dur  label=renderer[:ENV=v,ENV=v] ...
"""
# renders/decay_swell (28 Oct 2026; docs/research/decay-swell.md). Work files (material, scratch renders) live in
# renders/_decay_swell_work (DS_WORK); make the material first: python3 tools/pages/decay_swell/material.py

import json, os, subprocess, sys, tempfile
import numpy as np, soundfile as sf
REPO_ = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
S = os.environ.get("DS_WORK", REPO_ + "/renders/_decay_swell_work")
os.makedirs(S + "/out", exist_ok=True)
mat, sp, size, d0, d1, dur = sys.argv[1], sys.argv[2], sys.argv[3], float(sys.argv[4]), float(sys.argv[5]), float(sys.argv[6])
t0 = {"playing": 18.0, "tail": 15.0}[mat]


def lev(R, env, auto):
    e = dict(os.environ); e.update(env)
    with tempfile.TemporaryDirectory(dir=S + "/out") as td:
        json.dump(auto, open(td + "/a.json", "w"))
        subprocess.run([R, f"{S}/mat/{mat}.wav", td + "/o.wav", "--set", "mix=1", "--set", f"space={sp}", "--set", f"size={size}", "--set", f"decay={d0}",
                        "--auto", td + "/a.json"], env=e, check=True, capture_output=True)
        x, sr = sf.read(td + "/o.wav")
    m = x.mean(1); n = sr // 20; k = len(m) // n
    p = np.convolve(np.mean(m[:k * n].reshape(k, n) ** 2, 1), np.ones(4) / 4, "same")
    return 10 * np.log10(p + 1e-14)


ts = np.arange(t0 - 1, t0 + 12, 0.5)
idx = (ts / 0.05).astype(int)
print("t-t0   " + " ".join("%5.1f" % (t - t0) for t in ts))
for spec in sys.argv[7:]:
    lab, rest = spec.split("=", 1)
    R, _, envs = rest.partition(":")
    env = dict(kv.split("=") for kv in envs.split(",") if kv)
    h = lev(R, env, {"breakpoints": [{"t": 0, "key": "decay", "value": d0}]})
    mv = lev(R, env, {"breakpoints": [{"t": 0, "key": "decay", "value": d0}, {"t": t0, "key": "decay", "value": d0},
                                      {"t": t0 + dur, "key": "decay", "value": d1}]})
    print("%-6s held " % lab[:6] + " ".join("%5.1f" % v for v in h[idx]))
    print("%-6s move " % lab[:6] + " ".join("%5.1f" % v for v in mv[idx]))
