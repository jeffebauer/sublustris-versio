"""DECAY-down swell grid: render each case with a renderer, measure the wet's 50 ms RMS over the move.
Usage: grid.py <renderer> <label> [ENV=val ...]   -> <label>.csv
Metrics per case (dB):
  rise_at   : loudest 50 ms in the 12 s after the move starts, over the 50 ms level at its start
  excess    : loudest (moved - held) after the move, both 50 ms RMS power-smoothed over 200 ms; held =
              the same render with DECAY left where it was (a turned-down tail should never outgrow it)
  rise_late : as rise_at, but only from 1.5 s after the move ends (did the level end up above the start)
"""
# renders/decay_swell (28 Oct 2026; docs/research/decay-swell.md). Work files (material, scratch renders) live in
# renders/_decay_swell_work (DS_WORK); make the material first: python3 tools/pages/decay_swell/material.py

import itertools, json, os, subprocess, sys, tempfile
from concurrent.futures import ThreadPoolExecutor
import numpy as np, soundfile as sf

REPO_ = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
S = os.environ.get("DS_WORK", REPO_ + "/renders/_decay_swell_work")
os.makedirs(S + "/out", exist_ok=True)
R, label = sys.argv[1], sys.argv[2]
env = dict(os.environ)
for kv in sys.argv[3:]:
    k, v = kv.split("=", 1); env[k] = v
MOVE = {"playing": 18.0, "tail": 15.0}
SPACES = os.environ.get("GRID_SPACES", "HALL,MASSIVE").split(",")
SIZES = [float(s) for s in os.environ.get("GRID_SIZES", "0.5,0.8").split(",")]
STARTS = [float(s) for s in os.environ.get("GRID_STARTS", "0.78,0.83,0.92").split(",")]
ENDS = [float(s) for s in os.environ.get("GRID_ENDS", "0.5,0.3,0.2").split(",")]
SPEEDS = [float(s) for s in os.environ.get("GRID_SPEEDS", "0.2,1,4").split(",")]
MATS = os.environ.get("GRID_MATS", "playing,tail").split(",")
FR = 0.05


def levels(x, sr):
    m = x.mean(1)
    n = int(FR * sr)
    k = len(m) // n
    p = np.mean(m[: k * n].reshape(k, n) ** 2, axis=1)
    sm = np.convolve(p, np.ones(4) / 4, mode="same")
    return 10 * np.log10(p + 1e-14), 10 * np.log10(sm + 1e-14)


def render(mat, sp, size, auto):
    with tempfile.TemporaryDirectory(dir=S + "/out") as td:
        a = td + "/a.json"; json.dump(auto, open(a, "w"))
        w = td + "/o.wav"
        subprocess.run([R, f"{S}/mat/{mat}.wav", w, "--set", "mix=1", "--set", f"space={sp}", "--set", f"size={size}", "--set", f"decay={auto['breakpoints'][0]['value']}",
                        "--auto", a], env=env, check=True, capture_output=True)
        x, sr = sf.read(w)
    return levels(x, sr)


def held(args):
    mat, sp, size, d0 = args
    return args, render(mat, sp, size, {"breakpoints": [{"t": 0, "key": "decay", "value": d0}]})


def case(args):
    mat, sp, size, d0, d1, dur = args
    t0 = MOVE[mat]
    db, sm = render(mat, sp, size, {"breakpoints": [{"t": 0, "key": "decay", "value": d0},
                                                    {"t": t0, "key": "decay", "value": d0},
                                                    {"t": t0 + dur, "key": "decay", "value": d1}]})
    hdb, hsm = H[(mat, sp, size, d0)]
    i0 = int(t0 / FR); i1 = i0 + int(12 / FR); il = int((t0 + dur + 1.5) / FR)
    at = db[i0 - 2: i0].max()
    rise_at = db[i0:i1].max() - at
    exc = sm[i0:i1] - hsm[i0:i1]
    excess = exc.max(); t_exc = exc.argmax() * FR
    rise_late = db[il:i1].max() - at
    # over: above both held levels (where it was and where it's going): the move's own swell, not the two
    # settings' different steady levels
    over = (sm[i0:i1] - np.maximum(hsm[i0:i1], H[(mat, sp, size, d1)][1][i0:i1])).max()
    return f"{mat},{sp},{size},{d0},{d1},{dur},{at:.2f},{rise_at:.2f},{excess:.2f},{t_exc:.2f},{rise_late:.2f},{over:.2f}"


with ThreadPoolExecutor(8) as ex:
    H = dict(ex.map(held, list(itertools.product(MATS, SPACES, SIZES, sorted(set(STARTS + ENDS))))))
    rows = list(ex.map(case, list(itertools.product(MATS, SPACES, SIZES, STARTS, ENDS, SPEEDS))))
with open(f"{S}/{label}.csv", "w") as f:
    f.write("mat,space,size,d0,d1,dur,at,rise_at,excess,t_excess,rise_late,over\n")
    f.write("\n".join(rows) + "\n")
print(label, len(rows))
