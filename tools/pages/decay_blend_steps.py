"""renders/decay_blend's measurement: each crossing's own step, the moved render against a control where DECAY stays put
at that turn, in the wet alone and in the full mix at each BLEND. Usage: decay_blend_steps.py <iv_render> (after decay_blend.py).
"""
import json, os, subprocess, sys, tempfile, numpy as np, soundfile as sf
from concurrent.futures import ThreadPoolExecutor
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import decay_blend as db, decay_cross as dc
R = sys.argv[1]; P = db.PAGE + "/_dry"; S = os.environ.get("TMPDIR", "/tmp")
full = db.auto()["breakpoints"]
TURNS = [("into", db.T_IN), ("out 0.78", db.T_OUT), ("into", db.T_IN2), ("out 0.7", db.T_OUT2)]
def held_from(t0):  # the automation up to the turn, then DECAY stays where it was
    pts = [p for p in full if p["t"] <= t0 + 1e-9]
    return {"breakpoints": pts}
def rend(mat, mix, auto):
    with tempfile.TemporaryDirectory(dir=S) as td:
        json.dump(auto, open(td + "/a.json", "w"))
        subprocess.run([R, f"{P}/{mat}.wav", td + "/o.wav", "--set", f"mix={mix}", "--set", "space=HALL", "--set", "size=0.9", "--set", "decay=0.78", "--auto", td + "/a.json"], check=True, capture_output=True)
        return dc.levels(sf.read(td + "/o.wav")[0])
def job(c):
    mat, mix = c
    mv = rend(mat, mix, db.auto()); res = []
    for name, t0 in TURNS:
        h = rend(mat, mix, held_from(t0)); i0 = int(t0 / 0.05)
        d = (mv - h)[i0:i0 + 80]  # 4 s after the turn
        res.append((round(float(d[20:60].mean()), 1), round(float(d.min()), 1)))
    return c, res
cases = [(m, mix) for m in ["1_pad_quiet", "2_pad_hot", "3_keys_quiet", "4_keys_hot", "5_hits_quiet", "6_hits_hot"] for mix in [1.0, 0.75, 0.5, 0.3]]
with ThreadPoolExecutor(8) as ex: out = dict(ex.map(job, cases))
print("crossing's own step vs a control with DECAY held: mean over +1..+3 s (deepest within 4 s), dB")
print("%-14s %-6s " % ("card", "BLEND") + " | ".join("%-15s" % n for n, _ in TURNS))
for m in ["1_pad_quiet", "2_pad_hot", "3_keys_quiet", "4_keys_hot", "5_hits_quiet", "6_hits_hot"]:
    for mix in [1.0, 0.75, 0.5, 0.3]:
        print("%-14s %-6s " % (m, f"{int(mix*100)}%") + " | ".join("%+5.1f (%+5.1f)  " % r for r in out[(m, mix)]))
