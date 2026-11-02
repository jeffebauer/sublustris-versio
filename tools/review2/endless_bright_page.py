#!/usr/bin/env python3
"""renders/endless_bright_settle (v1.0.58 as built): "Still notes, settling only when bright: is this the one?"
B_still_notes: the owner's pick on review2_endless_final (review2/endless2 with main merged, IV_RUMBLE_FIX=19
IV_ENDLESS_CALM=10). D_settles_when_bright: v1.0.58 (no hooks): still notes, and the light settle in proportion to
how bright the playing was (voicing::kEndlessBright*).

Cards: the five of review2_endless_final (the pad chord on PLATE / HALL at TONE fully clockwise and noon; PLATE on a
noise swell over the pad) and two in-between bright sounds on PLATE (hats over the pad; a saw chord). Each card is one
10-minute render per version (32-bit float); excerpts 1_first25s_<material> (0-25 s, its own dry), 2_after5min
(5:00-5:20), 3_after10min (9:40-10:00), 16-bit with TPDF dither. Checks on the 32-bit excerpts: band envelopes
(<20 Hz, <500 Hz, >2 kHz), nulls (D against B), the largest 1 ms step, >3 kHz. Not level-matched.
Usage: endless_bright_page.py --proto R --v158 R --work DIR [--page DIR] [--versions TEXT]"""
import argparse, os, shutil, subprocess
from concurrent.futures import ThreadPoolExecutor
import numpy as np, soundfile as sf

REPO = '/Users/jesse/Documents/Sites/sublustris-versio'
ap = argparse.ArgumentParser()
ap.add_argument('--proto', required=True)
ap.add_argument('--v158', required=True)
ap.add_argument('--page', default=REPO + '/renders/endless_bright_settle')
ap.add_argument('--work', required=True)
ap.add_argument('--versions', default='')
a = ap.parse_args()
SR = 48000
T = 600

# Materials: 8.5 s, then silence to 10 minutes (the pad chord as on the last pages).
pad, sr = sf.read(REPO + '/renders/_material/pad.wav'); assert sr == SR
n = int(8.5 * SR); t = np.arange(n) / SR
fade = np.ones(n); fade[-SR // 2:] = np.linspace(1, 0, SR // 2)
p = pad[:n] * fade[:, None]


def hp(x, fc):
    X = np.fft.rfft(x, axis=0); f = np.fft.rfftfreq(len(x), 1 / SR); X[f < fc] = 0; return np.fft.irfft(X, len(x), axis=0)


MAT = {'chord': p,
       'noise': 0.5 * p + np.random.default_rng(7).uniform(-1, 1, (n, 2)) * (np.minimum(t / 7, 1) ** 2 * fade * 0.35)[:, None]}
rng = np.random.default_rng(11)
hats = np.zeros((n, 2))
for k in range(32):
    s = int(k * 0.25 * SR); L = int(0.08 * SR); e = np.exp(-np.arange(L) / (0.02 * SR)); hats[s:s + L] += rng.standard_normal((L, 2)) * e[:, None]
hats = hp(hats, 6000); hats *= 0.35 / np.abs(hats).max()
MAT['hats'] = 0.5 * p + hats * fade[:, None]
saw = np.zeros(n)
for f0 in (110, 164.8, 220, 277.2, 329.6):
    for d in (-0.15, 0, 0.15):
        ph = np.cumsum(np.full(n, f0 * 2 ** (d / 12) / SR)); saw += 2 * (ph % 1) - 1
saw = saw / np.abs(saw).max() * 0.5 * fade
MAT['saw'] = np.stack([saw, np.roll(saw, 7)], 1)

CARDS = [
    ('1_plate_tone_cw', 'chord', 'PLATE', 1, 'PLATE, TONE fully clockwise, the soft chord: D should equal B'),
    ('2_hall_tone_cw', 'chord', 'HALL', 1, 'HALL, TONE fully clockwise, the soft chord: D should equal B'),
    ('3_plate_tone_noon', 'chord', 'PLATE', 0.5, 'PLATE, TONE noon, the soft chord: D should equal B'),
    ('4_hall_tone_noon', 'chord', 'HALL', 0.5, 'HALL, TONE noon, the soft chord: D should equal B'),
    ('5_plate_noise_tone_cw', 'noise', 'PLATE', 1, 'PLATE, TONE fully clockwise, a noise swell over the chord: D settles fully'),
    ('6_plate_hats_tone_cw', 'hats', 'PLATE', 1, 'PLATE, TONE fully clockwise, hats over the chord: D settles about halfway'),
    ('7_plate_saw_tone_cw', 'saw', 'PLATE', 1, 'PLATE, TONE fully clockwise, a buzzy saw chord: D settles a touch'),
]
EXC = [('1_first25s', 0, 25), ('2_after5min', 300, 320), ('3_after10min', 580, 600)]
VERS = [('B_still_notes', a.proto, {'IV_RUMBLE_FIX': '19', 'IV_ENDLESS_CALM': '10'}), ('D_settles_when_bright', a.v158, None)]
FADE = int(0.02 * SR)
wrng = np.random.default_rng(29102026)


def brightness(x, N=32):  # the Engine's measure (Engine::process, voicing::kEndlessBright*), held at the end
    k = 1 - np.exp(-N / SR / 1.0); bd = bp = 0.0; pw = 0.0; a_ = x[:, 0] + x[:, 1]
    for s0 in range(0, len(a_) - N, N):
        if pw > 1e-7:
            c = a_[s0:s0 + N]; idx = np.arange(3, N, 4); d = c[idx] - c[idx - 1]
            bd += k * (np.sum(d * d) - bd); bp += k * (np.sum(c[idx] ** 2) - bp)
        for i in range(s0, s0 + N, 8): pw += 8 * 0.002 * (0.5 * (x[i, 0] ** 2 + x[i, 1] ** 2) - pw)  # ~the 10 ms follower
    hz = np.sqrt(bd / (bp + 1e-20)) * SR / (2 * np.pi)
    return hz, min(1.0, max(0.0, (hz - 1200) / 4800))


def write16(path, y):
    q = 1.0 / 32768.0
    d = (wrng.random(y.shape) - wrng.random(y.shape)) * q
    sf.write(path, np.clip(np.round((y + d) / q) * q, -1.0, 32767 / 32768), SR, subtype='PCM_16')


os.makedirs(a.work, exist_ok=True)
if os.path.isdir(a.page): shutil.rmtree(a.page)
os.makedirs(a.page + '/_dry')
STIM = {}
for m, x in MAT.items():
    full = np.concatenate([x, np.zeros((T * SR - n, 2))]).astype(np.float32)
    STIM[m] = f'{a.work}/{m}.wav'; sf.write(STIM[m], full, SR, subtype='FLOAT')
    write16(f'{a.page}/_dry/1_first25s_{m}.wav', full[:25 * SR].astype(np.float64))
for name, t0, t1 in EXC[1:]:
    write16(f'{a.page}/_dry/{name}.wav', np.zeros(((t1 - t0) * SR, 2)))


def render(job):
    renderer, env_add, stim, sp, tone, out = job
    e = {k: v for k, v in os.environ.items() if not k.startswith('IV_')}
    if env_add: e.update(env_add)
    pr = subprocess.run([renderer, stim, out, '--set', 'mix=1', '--set', f'space={sp}', '--set', 'decay=1', '--set', f'tone={tone}'],
                        env=e, capture_output=True, text=True, check=True)
    return ' | '.join(l for l in pr.stderr.splitlines() if 'IV_' in l) or '(no hook line)'


def cut(y, t0, t1):
    e = y[t0 * SR:t1 * SR].copy()
    if t0 > 0: e[:FADE] *= np.linspace(0, 1, FADE)[:, None]
    e[-FADE:] *= np.linspace(1, 0, FADE)[:, None]
    return e


def frames(m, k):
    j = len(m) // k; return np.mean(m[:j * k].reshape(j, k) ** 2, axis=1)


def hp3k(m):
    X = np.fft.rfft(m); fr = np.fft.rfftfreq(len(m), 1 / SR); X[fr < 3000] = 0; return np.fft.irfft(X, len(m))


def bands(y):
    P = (np.abs(np.fft.rfft(y, axis=0)) ** 2).mean(axis=1) / len(y) ** 2 * 2; fr = np.fft.rfftfreq(len(y), 1 / SR)
    return [10 * np.log10(P[(fr >= lo) & (fr < hi)].sum() + 1e-30) for lo, hi in ((1, 20), (1, 500), (2000, 24000), (0, 24000))]


V = ['renders/endless_bright_settle: one 10-minute render per card and version (32-bit float), measured here; the page\'s',
     'excerpts are 16-bit with TPDF dither, so they do not null bit for bit: the nulls below are on the 32-bit renders.',
     'Band envelopes: 1-20 Hz, 1-500 Hz, >2 kHz and total (dBFS). Per excerpt also the residual of D against B ("identical" =',
     'bit for bit), the largest 1 ms step (frames above -80 dBFS), >3 kHz against B (median of 50 ms frames) and its',
     'largest 50 ms rise.', a.versions]
for folder, m, sp, tone, what in CARDS:
    os.makedirs(f'{a.page}/{folder}')
    hz, share = brightness(MAT[m])
    V.append(f'\n== {folder}: {what}\n  the playing\'s brightness (the Engine\'s measure, worked out here): {hz:.0f} Hz -> settle share {share:.2f}')
    jobs = [(ren, env, STIM[m], sp, tone, f'{a.work}/{folder}_{tag}.wav') for tag, ren, env in VERS]
    with ThreadPoolExecutor(2) as ex:
        hooks = list(ex.map(render, jobs))
    exc = {}
    for (tag, ren, env), hook, job in zip(VERS, hooks, jobs):
        V.append(f'  {tag}: {ren}: {hook}')
        y, _ = sf.read(job[5], dtype='float64'); os.remove(job[5])
        for tt in (25, 300, 590):
            bb = bands(y[tt * SR:(tt + 10) * SR])
            V.append(f'      {tt:3d} s: <20 Hz {bb[0]:6.1f}  <500 Hz {bb[1]:6.1f}  >2 kHz {bb[2]:6.1f}  total {bb[3]:6.1f}')
        V.append(f'      peak over 10 min {20 * np.log10(np.abs(y).max() + 1e-30):.1f} dBFS')
        exc[tag] = {}
        for name, t0, t1 in EXC:
            e = cut(y, t0, t1); exc[tag][name] = e
            nm = f'{name}_{m}' if t0 == 0 else name
            write16(f'{a.page}/{folder}/{nm}_{tag}.wav', e)
        del y
    for name, t0, t1 in EXC:
        V.append(f'  {name} ({t0}-{t1} s), 32-bit excerpts:')
        B = exc['B_still_notes'][name]
        hB = 10 * np.log10(frames(hp3k(B.mean(1)), 2400) + 1e-14)
        for tag, _, _ in VERS:
            e = exc[tag][name]; mm = e.mean(1); bb = bands(e)
            res = 'identical' if np.array_equal(e, B) else f'{10 * np.log10(np.mean((e - B) ** 2) / np.mean(B ** 2) + 1e-30):+.1f} dB'
            f1 = 10 * np.log10(frames(mm, 48) + 1e-14); ok = f1[1:] > -80
            step = np.diff(f1)[ok].max() if ok.any() else 0.0
            h = 10 * np.log10(frames(hp3k(mm), 2400) + 1e-14); g0 = 2 if t0 > 0 else 0
            V.append(f'    {tag:22s} <20 Hz {bb[0]:6.1f}  <500 Hz {bb[1]:6.1f}  >2 kHz {bb[2]:6.1f}  total {bb[3]:6.1f}; vs B {res}; '
                     f'1 ms step {step:5.1f} dB; >3 kHz {np.median(h - hB):+5.1f} dB vs B; 50 ms rise >3 kHz {np.diff(h[g0:-2]).max():4.1f} dB')
open(f'{a.page}/_verify.txt', 'w').write('\n'.join(V) + '\n')
print('\n'.join(V))
for f_ in STIM.values(): os.remove(f_)
shutil.copy(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'endless_bright_readme.txt'), f'{a.page}/README.txt')
