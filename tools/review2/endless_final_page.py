#!/usr/bin/env python3
"""renders/review2_endless_final (v1.0.58's confirm page): "Still notes with a light settle: is this the one?"
A_as_now: main (v1.0.57), the reference renderer. B_still_notes: last page's F on v1.0.57 (review2/endless2 with main
merged, IV_RUMBLE_FIX=19 IV_ENDLESS_CALM=10). C_still_notes_light_settle: v1.0.58 (F + E built in, no hooks).

Each card renders 10 minutes once per version (32-bit float, from a float stimulus: the pad's first 8.5 s, then
silence) and keeps three excerpts named to sort in time order: 1_first30s (0-30 s), 2_after5min (5:00-5:25),
3_after10min (9:35-10:00), written 16-bit with TPDF dither. The checks (nulls, 1 ms steps, >3 kHz) run on the 32-bit
excerpts in memory. Not level-matched. Writes the page folder (cards, _dry, README.txt, _verify.txt); then run
make_review.py on it (--wet-only --dry <page>/_dry --no-level-match).
Usage: endless_final_page.py --main R --proto R --v158 R [--page DIR] [--work DIR]"""
import argparse, os, shutil, subprocess
from concurrent.futures import ThreadPoolExecutor
import numpy as np, soundfile as sf

REPO = '/Users/jesse/Documents/Sites/immensio-versio'
ap = argparse.ArgumentParser()
ap.add_argument('--main', required=True)
ap.add_argument('--proto', required=True)
ap.add_argument('--v158', required=True)
ap.add_argument('--page', default=REPO + '/renders/review2_endless_final')
ap.add_argument('--work', required=True)
ap.add_argument('--versions', default='', help='text appended to _verify.txt (the renderers\' commits)')
a = ap.parse_args()
SR = 48000
CARDS = [
    ('1_plate_tone_cw', 'PLATE', {'decay': 1, 'tone': 1}, 'PLATE, DECAY fully up (endless), TONE fully clockwise'),
    ('2_hall_tone_cw', 'HALL', {'decay': 1, 'tone': 1}, 'HALL, DECAY fully up (endless), TONE fully clockwise'),
    ('3_plate_tone_noon', 'PLATE', {'decay': 1, 'tone': 0.5}, 'PLATE, DECAY fully up (endless), TONE noon'),
    ('4_hall_tone_noon', 'HALL', {'decay': 1, 'tone': 0.5}, 'HALL, DECAY fully up (endless), TONE noon'),
]
EXC = [('1_first30s', 0, 30), ('2_after5min', 300, 325), ('3_after10min', 575, 600)]
VERS = [('A_as_now', 'main', None), ('B_still_notes', 'proto', {'IV_RUMBLE_FIX': '19', 'IV_ENDLESS_CALM': '10'}),
        ('C_still_notes_light_settle', 'v158', None)]
REN = {'main': a.main, 'proto': a.proto, 'v158': a.v158}
FADE = int(0.02 * SR)
rng = np.random.default_rng(29102026)

os.makedirs(a.work, exist_ok=True)
x, sr = sf.read(REPO + '/renders/_material/pad.wav'); assert sr == SR
c = x[:int(8.5 * SR)].copy(); f = int(0.5 * SR); c[-f:] *= np.linspace(1, 0, f)[:, None]
stim = np.concatenate([c, np.zeros((600 * SR - len(c), 2))]).astype(np.float32)
STIM = a.work + '/chord600f.wav'
sf.write(STIM, stim, SR, subtype='FLOAT')


def write16(path, y):  # TPDF dither (+-1 LSB), 16-bit
    q = 1.0 / 32768.0
    d = (rng.random(y.shape) - rng.random(y.shape)) * q
    sf.write(path, np.clip(np.round((y + d) / q) * q, -1.0, 32767 / 32768), SR, subtype='PCM_16')


if os.path.isdir(a.page): shutil.rmtree(a.page)
os.makedirs(a.page + '/_dry')
for name, t0, t1 in EXC:
    write16(f'{a.page}/_dry/{name}.wav', stim[t0 * SR:t1 * SR].astype(np.float64))


def render(job):
    renderer, env_add, sp, knobs, out = job
    e = {k: v for k, v in os.environ.items() if not k.startswith('IV_')}
    if env_add: e.update(env_add)
    cmd = [renderer, STIM, out, '--set', 'mix=1', '--set', f'space={sp}'] + sum((['--set', f'{k}={v}'] for k, v in knobs.items()), [])
    p = subprocess.run(cmd, env=e, capture_output=True, text=True, check=True)
    hook = [l for l in p.stderr.splitlines() if 'IV_' in l]
    return ' | '.join(hook) if hook else '(no hook line)'


def cut(y, t0, t1):
    e = y[t0 * SR:t1 * SR].copy()
    if t0 > 0: e[:FADE] *= np.linspace(0, 1, FADE)[:, None]
    e[-FADE:] *= np.linspace(1, 0, FADE)[:, None]
    return e


def frames(m, n):
    k = len(m) // n; return np.mean(m[:k * n].reshape(k, n) ** 2, axis=1)


def hp3k(m):
    X = np.fft.rfft(m); fr = np.fft.rfftfreq(len(m), 1 / SR); X[fr < 3000] = 0; return np.fft.irfft(X, len(m))


OCT = [125, 500, 1000, 2000, 4000, 8000]
def octs(y):
    P = (np.abs(np.fft.rfft(y, axis=0)) ** 2).mean(axis=1) / len(y) ** 2 * 2; fr = np.fft.rfftfreq(len(y), 1 / SR)
    return [10 * np.log10(P[(fr >= o / 2 ** 0.5) & (fr < o * 2 ** 0.5)].sum() + 1e-30) for o in OCT]


V = ['renders/review2_endless_final: one 10-minute render per card and version (32-bit float), measured here; the page\'s',
     'excerpts are written 16-bit with TPDF dither, so they do not null bit for bit: the nulls below are on the 32-bit renders.',
     'Octave bands (dBFS) of 10 s windows at 25 s, 300 s, 590 s: ' + ', '.join(f'{o} Hz' for o in OCT) + '.',
     'Per excerpt: level; residual against A and against B (dB under the reference; "identical" = bit for bit); the largest',
     '1 ms step (dB, frames above -80 dBFS); >3 kHz against A (median of 50 ms frames) and its largest 50 ms rise.', a.versions]
for folder, sp, knobs, what in CARDS:
    os.makedirs(f'{a.page}/{folder}')
    V.append(f'\n== {folder}: {what}')
    jobs = [(REN[ren], env, sp, knobs, f'{a.work}/{folder}_{tag}.wav') for tag, ren, env in VERS]
    with ThreadPoolExecutor(len(VERS)) as ex:
        hooks = list(ex.map(render, jobs))
    exc = {}
    for (tag, ren, env), hook, job in zip(VERS, hooks, jobs):
        V.append(f'  {tag}: {ren} renderer {job[0]}: {hook}')
        y, _ = sf.read(job[4], dtype='float64'); os.remove(job[4])
        for t in (25, 300, 590):
            w = y[t * SR:(t + 10) * SR]
            V.append(f'      {t:3d} s: total {10 * np.log10(np.mean(w ** 2) + 1e-30):6.1f} | ' + ' '.join(f'{v:6.1f}' for v in octs(w)))
        V.append(f'      peak over 10 min {20 * np.log10(np.abs(y).max() + 1e-30):.1f} dBFS; wet DC at 9:50 {y[590 * SR:].mean():+.5f}')
        exc[tag] = {}
        for name, t0, t1 in EXC:
            e = cut(y, t0, t1); exc[tag][name] = e
            write16(f'{a.page}/{folder}/{name}_{tag}.wav', e)
        del y
    for name, t0, t1 in EXC:
        V.append(f'  {name} ({t0}-{t1} s), 32-bit excerpts:')
        A = exc['A_as_now'][name]; B = exc['B_still_notes'][name]
        hA = 10 * np.log10(frames(hp3k(A.mean(1)), 2400) + 1e-14)
        for tag, _, _ in VERS:
            e = exc[tag][name]; m = e.mean(1)
            lvl = 10 * np.log10(np.mean(e ** 2) + 1e-30)
            def res(r):
                if np.array_equal(e, r): return 'identical'
                return f'{10 * np.log10(np.mean((e - r) ** 2) / np.mean(r ** 2) + 1e-30):+6.1f} dB'
            f1 = 10 * np.log10(frames(m, 48) + 1e-14); ok = f1[1:] > -80
            step = np.diff(f1)[ok].max() if ok.any() else 0.0
            h = 10 * np.log10(frames(hp3k(m), 2400) + 1e-14); g0 = 2 if t0 > 0 else 0
            V.append(f'    {tag:27s} level {lvl:6.1f} dBFS; vs A {res(A):>10s}, vs B {res(B):>10s}; largest 1 ms step {step:5.1f} dB; '
                     f'>3 kHz {np.median(h - hA):+6.1f} dB vs A; largest 50 ms rise of >3 kHz {np.diff(h[g0:-2]).max():4.1f} dB')
open(f'{a.page}/_verify.txt', 'w').write('\n'.join(V) + '\n')
print('\n'.join(V))
os.remove(STIM)
shutil.copy(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'endless_final_readme.txt'), f'{a.page}/README.txt')
