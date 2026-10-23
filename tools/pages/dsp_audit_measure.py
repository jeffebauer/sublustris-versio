"""The DSP audit's per-item measurements (docs/research/dsp-audit.md, "Measured (24 Oct 2026)").

Usage: dsp_audit_measure.py <item> [ENV=VAL,...]
Items: e2 (box in the loop: band T60 by line length at ERA 1), r1 (REVERSE flicker), p4 (small PLATE ringing),
       p5 (PLATE T60 against DECAY's target), mi5 (wet level across DECAY), sat (how much the loop's soft clip does),
       p2 (Dattorro's decay diffusion 2), e3 (the mu-law layer's noise), e4 (halo around a held sine), e5 (head bump).
"""
import os, sys
import numpy as np
from dsp_audit_lib import render, SCRATCH, SR, ROOT, bandpass_fft, window_db, envelope_db

CLICK = os.path.join(ROOT, 'renders', '_material', '_click.wav')
MAT = os.path.join(ROOT, 'renders', '_material')
BANDS = (250, 500, 1000, 2000, 4000, 8000)


def band_t60(x, fc):
    y = bandpass_fft(x, fc / 1.414, fc * 1.414)
    e = np.cumsum((y ** 2)[::-1])[::-1]
    e = 10 * np.log10(e / e[0] + 1e-30)
    i5 = np.argmax(e < -5); i25 = np.argmax(e < -25)
    if i25 <= i5:
        return float('nan')
    return 3.0 * (i25 - i5) / SR


def t60s(sets, env=None):
    l, r = render(CLICK, dict(sets, mix=1), env)
    return [band_t60(0.5 * (l + r), f) for f in BANDS]


def fmt(v):
    return ' '.join(f'{b}:{t:5.2f}' for b, t in zip(BANDS, v))


def e2(env):
    for name, s in (('HALL size 0', dict(space='HALL', size=0)), ('HALL size 0.6', dict(space='HALL', size=0.6)),
                    ('MASSIVE size 1', dict(space='MASSIVE', size=1)), ('PLATE size 0.6', dict(space='PLATE', size=0.6))):
        a = t60s(dict(s, decay=0.6, era=0))
        b = t60s(dict(s, decay=0.6, era=1), env)
        print(f'{name:15s} ERA0 {fmt(a)}\n{"":15s} ERA1 {fmt(b)}\n{"":15s} ratio ' +
              ' '.join(f'{y / x:5.2f}' for x, y in zip(a, b)))


def flicker_db(l, r, t0, t1):
    """10 ms envelope's RMS deviation (dB) around its own 150 ms smoothing, between t0 and t1 s."""
    e = envelope_db(l, r, 480)
    k = np.ones(15) / 15
    sm = np.convolve(e, k, mode='same')
    a, b = int(t0 * 100), int(t1 * 100)
    return float(np.sqrt(np.mean((e[a:b] - sm[a:b]) ** 2)))


def r1(env):
    hit = os.path.join(SCRATCH, 'hit.wav')
    if not os.path.exists(hit):
        import soundfile as sf
        rng = np.random.default_rng(3)
        x = np.zeros(8 * SR); n = int(0.01 * SR)
        x[SR:SR + n] = 0.5 * rng.standard_normal(n) * np.linspace(1, 0, n)
        sf.write(hit, np.stack([x, x], 1), SR, subtype='FLOAT')
    # Flicker from the swell's onset to its peak (found on the envelope); BLOOM (smeared, v1.0.16) for scale.
    for sp in ('HALL', 'PLATE', 'MASSIVE'):
        for att, tm in (('BLOOM', 0.6), ('REVERSE', 0.6), ('REVERSE', 1.0)):
            l, r = render(hit, dict(space=sp, attack=att, time=tm, decay=0.45, size=0.5, era=0, mix=1), env)
            e = envelope_db(l, r, 480)
            on = int(np.argmax(e > e.max() - 40)); pk = int(np.argmax(e))
            print(f'{sp:8s} {att:7s} TIME {tm}: onset {on / 100:.2f} s, peak {pk / 100:.2f} s, '
                  f'flicker {flicker_db(l, r, on / 100 + 0.05, pk / 100 - 0.02):.1f} dB')
        ln, rn = render(hit, dict(space=sp, attack='NORMAL', time=0, decay=0.45, size=0.5, era=0, mix=1), env)
        print(f'{sp:8s} NORMAL tail flicker {flicker_db(ln, rn, 1.3, 2.5):.1f} dB')


def p4(env):
    import subprocess, re
    for sp in ('PLATE', 'HALL'):
        for sz in (0, 0.5, 1):
            for dc in (0.6, 0.75):
                out = os.path.join(SCRATCH, 'p4.wav')
                render(os.path.join(MAT, 'vibes.wav') if os.path.exists(os.path.join(MAT, 'vibes.wav')) else CLICK,
                       dict(space=sp, size=sz, decay=dc, era=0, mix=1), env, keep=out)
                s = subprocess.run([os.path.join(ROOT, 'build-wt', 'iv_render'), '--analyze', out], capture_output=True, text=True).stdout
                m = re.search(r'ringing=(\S+)', s)
                l, r = render(CLICK, dict(space=sp, size=sz, decay=dc, era=0, mix=1), env)
                print(f'{sp:6s} SIZE {sz} DECAY {dc}: ringing {m.group(1) if m else "?"}  T60(1k) {band_t60(0.5 * (l + r), 1000):.2f} s')
                os.remove(out)


def p5(env):
    sys.path.insert(0, ROOT)
    for sz in (0, 0.5, 1):
        for dc in (0.2, 0.5, 0.75):
            v = t60s(dict(space='PLATE', size=sz, decay=dc, era=0), env)
            print(f'PLATE SIZE {sz} DECAY {dc}: {fmt(v)}')


def mi5(env):
    pad = os.path.join(MAT, 'pad.wav')
    for sp in ('HALL', 'MASSIVE', 'PLATE'):
        row = []
        for dc in (0.2, 0.4, 0.6, 0.75, 0.95):
            l, r = render(pad, dict(space=sp, size=0.6, decay=dc, era=0.55, mix=1), env)
            row.append(f'{dc}:{window_db(l, r, 0, len(l) / SR - 0.1):6.1f}/{20 * np.log10(np.max(np.abs(l)) + 1e-9):5.1f}')
        print(f'{sp:8s} rms/peak dB by DECAY  ' + '  '.join(row))


def sat(env):
    """The soft clip's own share: today minus IV_SAT_BYPASS=1 (identical otherwise), on loud music."""
    for m in ('pad', 'fullmix', 'drums'):
        f = os.path.join(MAT, m + '_send.wav') if os.path.exists(os.path.join(MAT, m + '_send.wav')) else os.path.join(MAT, m + '.wav')
        for sp in ('HALL', 'MASSIVE', 'PLATE'):
            row = []
            for era in (0, 0.55):
                for dc in (0.4, 0.75, 0.95):
                    s = dict(space=sp, size=0.6, decay=dc, era=era, mix=1)
                    l, r = render(f, s)
                    lb, rb = render(f, s, {'IV_SAT_BYPASS': '1'})
                    d = 10 * np.log10((np.sum((l - lb) ** 2) + np.sum((r - rb) ** 2)) / (np.sum(l ** 2) + np.sum(r ** 2)) + 1e-30)
                    row.append(f'era{era}/d{dc}:{d:6.1f}')
            print(f'{m:8s} {sp:8s} clip share dB  ' + ' '.join(row))


def diff_db(a, b):
    return 10 * np.log10(np.sum((a - b) ** 2) / np.sum(a ** 2) + 1e-30)


def ab(sets_list, env, inp=CLICK, t0=0.0):
    for s in sets_list:
        l, r = render(inp, dict(s, mix=1))
        lb, rb = render(inp, dict(s, mix=1), env)
        a = int(t0 * SR)
        print(f'{s}: B-A difference {diff_db(np.r_[l[a:], r[a:]], np.r_[lb[a:], rb[a:]]):6.1f} dB re A;'
              f' T60 A {fmt([band_t60(0.5 * (l + r), f) for f in BANDS])}\n{"":60s} T60 B {fmt([band_t60(0.5 * (lb + rb), f) for f in BANDS])}')


def p2(env):
    ab([dict(space='PLATE', size=sz, decay=dc, era=0) for sz in (0, 0.5, 1) for dc in (0, 0.15, 0.3)], env or {'IV_PLATE_DD2': '1'})


def e2keep(env):
    env = env or {'IV_MIDI_KEEP': '1'}
    e2(env)
    for sp in ('HALL', 'MASSIVE', 'PLATE'):
        s = dict(space=sp, size=0.6, decay=0.6, era=1, mix=1)
        for e in ({}, env):
            l, r = render(CLICK, s, e)
            print(f'{sp:8s} {e or "today"}: last 5 s {window_db(l, r, 35, 40):.0f} dBFS; peak abs last 5 s {np.max(np.abs(l[35 * SR:])):.2e}')


def noise_share(sets, inp, env_b):
    """The mu layer's own noise: a render minus the same render with the layer off (IV_MU_MODE=1), dB re the signal."""
    l0, r0 = render(inp, sets, {'IV_MU_MODE': '3'})
    out = []
    for e in ({}, env_b):
        l, r = render(inp, sets, e)
        out.append(diff_db(np.r_[l0, r0], np.r_[l, r]))
    return out


def e3(env):
    env = env or {'IV_MU_MODE': '2'}
    for m in ('pad', 'drums', 'voice'):
        for era in (0.7, 0.8, 0.85):
            a, b = noise_share(dict(space='HALL', size=0.6, decay=0.6, era=era, mix=1), os.path.join(MAT, m + '.wav'), env)
            print(f'{m:6s} ERA {era}: noise re signal  mu-law {a:6.1f} dB   gain-ranging {b:6.1f} dB')
    l0, r0 = render(os.path.join(MAT, 'pad.wav'), dict(space='HALL', size=0.6, decay=0.6, era=0.8, mix=1), {'IV_MU_MODE': '3'})
    for name, e in (('mu-law', {}), ('gain-ranging', env)):
        l, r = render(os.path.join(MAT, 'pad.wav'), dict(space='HALL', size=0.6, decay=0.6, era=0.8, mix=1), e)
        n = len(l) // SR
        row = []
        for t in range(0, n - 1, 3):
            a, b = t * SR, (t + 1) * SR
            row.append(f'{t}s:{window_db(l0, r0, t, t + 1):5.0f}/{diff_db(np.r_[l0[a:b], r0[a:b]], np.r_[l[a:b], r[a:b]]):5.1f}')
        print(f'{name:12s} level/noise-re-signal by time  ' + ' '.join(row))


def halo(l, r, t0=5.0, t1=9.0, f0=1000.0):
    m = 0.5 * (l + r)[int(t0 * SR):int(t1 * SR)]
    X = np.abs(np.fft.rfft(m * np.hanning(len(m)))) ** 2
    f = np.fft.rfftfreq(len(m), 1 / SR)
    band = (f > 200) & (f < 10000)
    near = np.zeros_like(band)
    for h in (1, 2, 3):
        near |= np.abs(f - h * f0) < 40
    return 10 * np.log10(X[band & ~near].sum() / X[band & near].sum())


def e4(env):
    env = env or {'IV_VINTAGE_MOD': '1'}
    sine = os.path.join(SCRATCH, 'sine1k.wav')
    for sp in ('HALL', 'MASSIVE'):
        for era in (0.65, 0.8, 1.0):
            s = dict(space=sp, size=0.6, decay=0.65, era=era, mix=1, drift=0.5)
            l, r = render(sine, s); lb, rb = render(sine, s, env)
            print(f'{sp:8s} ERA {era}: halo (energy off the tone, 0.2-10 kHz, re the tone)  today {halo(l, r):6.1f} dB   B {halo(lb, rb):6.1f} dB')
        a = t60s(dict(space=sp, size=0.6, decay=0.65, era=0.8)); b = t60s(dict(space=sp, size=0.6, decay=0.65, era=0.8), env)
        print(f'{"":8s} T60 today {fmt(a)}\n{"":8s} T60 B     {fmt(b)}')


def e5(env):
    env = env or {'IV_TAPE_OUT': '1'}
    for m in ('pad', 'bass', 'drums', 'fullmix'):
        for sp in ('HALL', 'MASSIVE'):
            s = dict(space=sp, size=0.6, decay=0.6, era=0.55, mix=1)
            inp = os.path.join(MAT, m + '_send.wav') if os.path.exists(os.path.join(MAT, m + '_send.wav')) else os.path.join(MAT, m + '.wav')
            l, r = render(inp, s, {'IV_TAPE_OUT': '2'}); lb, rb = render(inp, s, env)
            print(f'{m:8s} {sp:8s}: B-A (A = the same 2x path, linear) {diff_db(np.r_[l, r], np.r_[lb, rb]):6.1f} dB re A; level A {window_db(l, r, 0, len(l) / SR - .1):6.1f}'
                  f' B {window_db(lb, rb, 0, len(l) / SR - .1):6.1f} dB; peak A {20 * np.log10(np.abs(l).max()):5.1f}')


def e2music(env):
    """Music-level tails at ERA 1: the level each second after the material stops, today / no truncation / B."""
    for m in ('pad', 'vibes', 'stabs'):
        inp = os.path.join(MAT, m + '.wav')
        import soundfile as sf
        x, _ = sf.read(inp)
        a = np.abs(x).max(1) if x.ndim > 1 else np.abs(x)
        end = (np.nonzero(a > 1e-4)[0][-1]) / SR
        for sp, sz in (('HALL', 0), ('HALL', 0.6), ('MASSIVE', 1), ('PLATE', 0.6)):
            for name, e in (('today', {}), ('no trunc', {'IV_MIDI_PART': '1'}), ('B keep', env or {'IV_MIDI_KEEP': '1'}),
                            ('ERA 0.95', None)):
                s = dict(space=sp, size=sz, decay=0.6, era=1 if e is not None else 0.95, mix=1)
                l, r = render(inp, s, e or {})
                lv = [window_db(l, r, end + t, end + t + 0.5) for t in range(0, 12, 1) if end + t + 0.5 < len(l) / SR]
                drop40 = next((t for t, v in enumerate(lv) if v < lv[0] - 40), None)
                print(f'{m:6s} {sp:7s} SIZE {sz:3}: {name:9s} after the end, dB/s ' + ' '.join(f'{v:5.0f}' for v in lv)
                      + f'   40 dB down after {drop40} s')


if __name__ == '__main__':
    item = sys.argv[1]
    env = dict(kv.split('=') for kv in (sys.argv[2] if len(sys.argv) > 2 else '').split(',') if kv)
    globals()[item](env)
