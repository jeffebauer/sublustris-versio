"""Desktop CPU cost of each prototype: best-of-5 render time of 60 s, today vs B (DSP audit, 24 Oct 2026).
Firmware CPU is unmeasured (M3); this is only a relative guide."""
import os, subprocess, time
from dsp_audit_lib import RENDER, SCRATCH, SR
import numpy as np, soundfile as sf

src = os.path.join(SCRATCH, 'cpu60.wav')
if not os.path.exists(src):
    x = 0.1 * np.random.default_rng(9).uniform(-1, 1, 60 * SR)
    sf.write(src, np.stack([x, x], 1), SR, subtype='FLOAT')
out = os.path.join(SCRATCH, 'cpu_out.wav')


def best(sets, env):
    e = {k: v for k, v in os.environ.items() if not k.startswith('IV_')}
    e.update(env)
    args = [RENDER, src, out] + sum((['--set', f'{k}={v}'] for k, v in sets.items()), [])
    ts = []
    for _ in range(5):
        t = time.perf_counter()
        subprocess.run(args, env=e, check=True, stdout=subprocess.DEVNULL)
        ts.append(time.perf_counter() - t)
    return min(ts)


for name, sets, env in (('E4 vintage reads (HALL, ERA 0.8)', dict(space='HALL', era=0.8, mix=1), {'IV_VINTAGE_MOD': '1'}),
                        ('E4 vintage reads (MASSIVE, ERA 0.8)', dict(space='MASSIVE', era=0.8, mix=1), {'IV_VINTAGE_MOD': '1'}),
                        ('E5 tape on the way out (HALL, ERA 0.55)', dict(space='HALL', era=0.55, mix=1), {'IV_TAPE_OUT': '1'})):
    a, b = best(sets, {}), best(sets, env)
    print(f'{name:42s} today {a:.2f} s, B {b:.2f} s ({100 * (b / a - 1):+.1f} % of the whole render)')
os.remove(out)
