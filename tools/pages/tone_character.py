#!/usr/bin/env python3
"""PROTOTYPE pick page (27 Oct 2026; owner: "add more character to the tone control - maybe some soft, sweet resonance
... a balance between performance and quality so we don't reintroduce cpu overloads or run out of flash";
docs/research/tone-character.md). The loop keeps today's TONE; the wet's tilt (dsp/Output.h Tilt) is replaced by a
filter with a soft resonance (dsp/Output.h ToneCharacter), renderer hook IV_TONE_CHAR=A|B|C:

    T  today's tilt
    A  resonant tilt: today's tilt plus a bell riding its bend (Pultec / 1073 idea)
    B  SEM sweep (Oberheim 12 dB low-pass CCW, high-pass + air CW), in place of the tilt
    C  today's tilt plus a ladder low-pass CCW (ZDF 4-pole, soft clip in its feedback), a resonant air shelf CW

Subcommands:
    calibrate   the level make-up per version against today's tilt (music, LUFS), for ToneCharacter::kMakeupDb
    page        renders/tone_character in the main checkout (100 % wet, the dry for the page's MIX fader)
    checks      level, metallic, holds, freeze holds and click checks -> renders/tone_character/_checks.txt

Usage: <main checkout>/.venv/bin/python tools/pages/tone_character.py page [--render build-tone/iv_render]
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from review_cpu_pages import main_checkout, trim  # noqa: E402

SR = 48000
VERSIONS = [("T_today", None), ("A_resonant_tilt", "A"), ("B_sem_sweep", "B"), ("C_ladder_air", "C")]
HOOK = {v: h for v, h in VERSIONS}
COMMON = ["size=0.7", "decay=0.75", "drift=0.5", "time=0.1", "era=0", "attack=NORMAL"]
SPACES = {"hall": ["space=HALL"], "massive": ["space=MASSIVE"], "plate": ["space=PLATE"]}
JOBS = max(1, (os.cpu_count() or 2) - 2)


def db(v):
    return 10.0 * np.log10(v + 1e-30)


# ---------------------------------------------------------------- loudness (ITU-R BS.1770-4)
def _biquad_mag(b, a, f):
    z = np.exp(-2j * np.pi * f / SR)
    return np.abs((b[0] + b[1] * z + b[2] * z * z) / (a[0] + a[1] * z + a[2] * z * z))


def lufs(x):
    """Integrated loudness: K-weighting (applied as its magnitude, in the frequency domain), 400 ms blocks with 75 %
    overlap, the -70 LUFS and -10 LU gates."""
    x = np.atleast_2d(x.T).T
    f = np.fft.rfftfreq(len(x), 1.0 / SR)
    k = _biquad_mag([1.53512485958697, -2.69169618940638, 1.19839281085285], [1, -1.69065929318241, 0.73248077421585], f) \
        * _biquad_mag([1.0, -2.0, 1.0], [1, -1.99004745483398, 0.99007225036621], f)
    y = np.fft.irfft(np.fft.rfft(x, axis=0) * k[:, None], len(x), axis=0)
    n, hop = int(0.4 * SR), int(0.1 * SR)
    c = np.cumsum(np.concatenate([np.zeros((1, y.shape[1])), y ** 2]), axis=0)
    starts = np.arange(0, len(y) - n + 1, hop)
    p = ((c[starts + n] - c[starts]) / n).sum(axis=1)
    p = p[-0.691 + db(p) > -70.0]
    if len(p) == 0:
        return -99.0
    p = p[-0.691 + db(p) > -0.691 + db(p.mean()) - 10.0]
    return -0.691 + db(p.mean())


# ---------------------------------------------------------------- rendering
def render(binary, src, out, sets, version, auto=None, keep=True):
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    if HOOK[version]:
        env["IV_TONE_CHAR"] = HOOK[version]
    args = [str(binary), str(src), str(out), "--set", "mix=1"]
    if auto is not None:
        a = Path(str(out) + ".auto.json")
        a.write_text(json.dumps(auto))
        args += ["--auto", str(a)]
    for s in sets:
        args += ["--set", s]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    if auto is not None:
        a.unlink()
    if p.returncode != 0:
        sys.exit("render failed: {}\n{}".format(" ".join(args), p.stderr))
    line = [l for l in p.stderr.splitlines() if l.startswith("iv_render: IV_TONE_CHAR")]
    y, _ = sf.read(str(out), always_2d=True)
    if not keep:
        os.unlink(out)
    summary = p.stdout.strip().splitlines()[-1] if p.stdout.strip() else ""
    return y, (line[0] if line else "iv_render: (no IV_TONE_CHAR: today)"), summary


def sweep_auto(points, buttons=None):
    """[(t, tone)] breakpoints (linear between them)."""
    a = {"breakpoints": [{"t": float(t), "key": "tone", "value": float(v)} for t, v in points]}
    if buttons:
        a["buttons"] = buttons
    return a


def cv_auto(hz, seconds, start=1.0):
    """A full-range triangle on TONE at hz, from start for seconds (noon before and after)."""
    pts, t, half = [(0.0, 0.5), (start, 0.5)], start, 0.5 / hz
    pts.append((t + half / 2, 1.0))
    t += half / 2
    v = 0.0
    while t + half < start + seconds:
        t += half
        pts.append((t, v))
        v = 1.0 - v
    pts.append((t + half / 2, 0.5))
    return sweep_auto(pts)


# ---------------------------------------------------------------- calibrate
TONES = [i / 8 for i in range(9)]


def level_grid(binary, material, tones, tmp):
    """LUFS of every version at each TONE: HALL and MASSIVE x skank, vibes, pad."""
    cases = [(v, sp, m, t) for v, _ in VERSIONS for sp in ("hall", "massive") for m in ("skank", "vibes", "pad") for t in tones]

    def job(c):
        v, sp, m, t = c
        out = tmp / "{}_{}_{}_{:.4f}.wav".format(v, sp, m, t)
        y, _, _ = render(binary, material / (m + ".wav"), out, SPACES[sp] + COMMON + ["tone={}".format(t)], v, keep=False)
        return c, lufs(y)

    with ThreadPoolExecutor(JOBS) as pool:
        return dict(pool.map(job, cases))


def calibrate(binary, material):
    with tempfile.TemporaryDirectory() as t:
        L = level_grid(binary, material, TONES, Path(t))
    for v, h in VERSIONS[1:]:
        diffs = [[L[("T_today", sp, m, t)] - L[(v, sp, m, t)] for sp in ("hall", "massive") for m in ("skank", "vibes", "pad")]
                 for t in TONES]
        print("{} add dB: {{{}}}".format(v, ", ".join("{:.2f}".format(np.mean(d)) for d in diffs)))
        print("   spread: " + " ".join("{:+.1f}..{:+.1f}".format(min(d), max(d)) for d in diffs))


# ---------------------------------------------------------------- page
README = """TONE with character: a soft, sweet resonance. Your note: "add more character to the tone control - maybe some soft, sweet resonance ... a balance between performance and quality so we don't reintroduce cpu overloads or run out of flash."

What changes: today TONE does two things, it darkens or brightens the reverb's own decay (inside the loop) and it tilts the reverb's output (more lows and fewer highs CCW, the opposite CW, around 600 Hz). The loop part stays exactly as it is, so long holds and frozen pads behave as today. Only the output tilt is replaced, by a filter with a gentle bump where it bends. Noon is flat and identical to today in every version.

T is today's tilt. A, the resonant tilt: today's EQ, made sweeter. It keeps today's tilt exactly and adds a soft bump (+4.5 dB at the ends) that rides the bend: turning CCW the bump slides down from 3 kHz to 600 Hz, a warm swell just under the darkening; turning CW it slides up from 1 kHz to 5 kHz, a sheen on top. B, the SEM sweep: the dub-desk filter you throw, in place of today's tilt. CCW a 12 dB low-pass closes from the top down to 350 Hz, with a soft, vowel-like bump at the corner (+3.5 dB): the reverb sinks underwater. CW a high-pass opens from 20 Hz up to 280 Hz (no further, so the reverb stays big) with a little air on top (+3 dB): it thins to airy wisps. C, the ladder: today's tilt, then CCW a steep, creamy 4-pole low-pass like a Moog's (16 kHz down to 500 Hz), darker and smoother than B, its bump (+5 dB) rounding off gently on loud moments; CW a resonant air shelf (+3 dB, the bump on top climbing from 2 to 8 kHz). Each version is set to the same loudness as today at every knob position (measured on music), so none wins by being louder.

The question: should TONE's ends feel like a filter you throw (B, or C's darker CCW), or a sweeter version of today's EQ (A)? Listen for: the warmth CCW (does the swell feel sweet or boxy?), the sheen CW (sweet, or glassy?), how the frozen pad sounds as TONE moves across it, and the sweeps for how the knob travels (any whistle, any honk, any steps).

Cards: HALL and MASSIVE at TONE fully CCW, 9 o'clock, 3 o'clock and fully CW (skank, vibes, a pad). Sweep cards: TONE slowly from noon to fully CCW, across to fully CW and back to noon (0-24 s), on HALL, on PLATE, on a frozen MASSIVE pad (frozen at 4 s, so you hear only TONE moving), and on MASSIVE with WEAR at 0.9 (the radio band in: listen for honk or phasing where its band meets the bump). One SHIMMER card at TONE fully CW (glassy?). SIZE 0.7, DECAY 0.75, DRIFT noon, TIME near 0, WEAR clean unless the card says so. Renders are 100 % wet: use the MIX fader for the dry.

Level: each version is matched to T per card by its loudness (LUFS, the mean over the card's sounds), on top of the knob's own make-up.

01_hall_ccw          HALL, TONE fully CCW
02_hall_9oclock      HALL, TONE 9 o'clock
03_hall_3oclock      HALL, TONE 3 o'clock
04_hall_cw           HALL, TONE fully CW
05_massive_ccw       MASSIVE, TONE fully CCW
06_massive_9oclock   MASSIVE, TONE 9 o'clock
07_massive_3oclock   MASSIVE, TONE 3 o'clock
08_massive_cw        MASSIVE, TONE fully CW
09_sweep_hall        TONE swept slowly: noon, fully CCW (6 s), fully CW (18 s), noon (24 s)
10_sweep_plate       the same sweep on PLATE
11_sweep_frozen_pad  a MASSIVE pad (DECAY 0.9) frozen at 4 s; TONE to CCW by 10 s, CW by 20 s, noon at 26 s
12_sweep_wear        the sweep on MASSIVE with WEAR 0.9 (the radio band in)
13_shimmer_cw        HALL, ATTACK on SHIMMER, TONE fully CW

T_today              today's tilt
A_resonant_tilt      a sweeter version of today's EQ: today's tilt with a soft bump riding its bend
B_sem_sweep          a filter you throw: 12 dB low-pass CCW, high-pass + air CW, a soft bump at the corner
C_ladder_air         today's tilt plus a steep, creamy 4-pole low-pass CCW (like a Moog); a resonant air shelf CW"""

SWEEP = [(0.0, 0.5), (1.0, 0.5), (6.0, 0.0), (8.0, 0.0), (18.0, 1.0), (20.0, 1.0), (24.0, 0.5)]
FROZEN_SWEEP = [(0.0, 0.5), (5.0, 0.5), (10.0, 0.0), (12.0, 0.0), (20.0, 1.0), (22.0, 1.0), (26.0, 0.5)]


def cards():
    """(name, sets, materials, auto)."""
    c = []
    tones = [("ccw", 0.0), ("9oclock", 0.25), ("3oclock", 0.75), ("cw", 1.0)]
    i = 1
    for sp in ("hall", "massive"):
        for nm, t in tones:
            c.append(("{:02d}_{}_{}".format(i, sp, nm), SPACES[sp] + COMMON + ["tone={}".format(t)], ["skank", "vibes", "pad"], None))
            i += 1
    c.append(("09_sweep_hall", SPACES["hall"] + COMMON, ["skanklong", "vibes", "pad"], sweep_auto(SWEEP)))
    c.append(("10_sweep_plate", SPACES["plate"] + COMMON, ["skanklong", "vibes"], sweep_auto(SWEEP)))
    c.append(("11_sweep_frozen_pad", SPACES["massive"] + [s for s in COMMON if not s.startswith("decay")] + ["decay=0.9"],
              ["padfreeze"], sweep_auto(FROZEN_SWEEP, [[4.0, 4.1]])))
    c.append(("12_sweep_wear", SPACES["massive"] + [s for s in COMMON if not s.startswith("era")] + ["era=0.9"],
              ["skanklong", "vibes"], sweep_auto(SWEEP)))
    c.append(("13_shimmer_cw", SPACES["hall"] + [s for s in COMMON if not s.startswith("attack")] + ["attack=SHIMMER", "tone=1"],
              ["vibes", "pad"], None))
    return c


def make_dry(material, dry):
    trim(material / "skank.wav", dry / "skank.wav", 12.0, 6.0)
    for m, play, tail in (("vibes", 18.0, 8.0), ("pad", 18.0, 8.0), ("skanklong", 24.0, 6.0)):
        trim(material / (("skank" if m == "skanklong" else m) + ".wav"), dry / (m + ".wav"), play, tail)
    if not (dry / "padfreeze.wav").exists(): # the pad for 4.5 s (frozen at 4 s), then 26 s of nothing: the pad held
        trim(material / "pad.wav", dry / "padfreeze.wav", 4.5, 26.0)


def page(binary, material, out):
    root = out.parent.parent
    dry = root / "renders" / "_material_tone_character"
    for d in (out, dry):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
    make_dry(material, dry)
    todo = [(col, sets, m, auto, v) for col, sets, mats, auto in cards() for m in mats for v, _ in VERSIONS]
    for col, _, _, _ in cards():
        (out / col).mkdir()

    def job(j):
        col, sets, m, auto, v = j
        y, line, _ = render(binary, dry / (m + ".wav"), out / col / "{}_{}.wav".format(m, v), sets, v, auto)
        return j, y, line

    with ThreadPoolExecutor(JOBS) as pool:
        res = list(pool.map(job, todo))
    got = {(j[0], j[2], j[4]): y for j, y, _ in res}
    with open(out / "_verify.txt", "w") as vf:
        vf.write("tone_character: label check (each render's hook line from iv_render's stderr) and the per-card level "
                 "match (LUFS, the mean over the card's sounds, each version to T).\n")
        for (col, sets, m, auto, v), _, line in res:
            want = {"T_today": "(no IV_TONE_CHAR", "A_resonant_tilt": "A resonant tilt", "B_sem_sweep": "B SEM sweep",
                    "C_ladder_air": "C ladder"}[v]
            vf.write("{:22s} {:10s} {:16s} {} {}\n".format(col, m, v, "OK  " if want in line else "WRONG", line))
        for col, sets, mats, auto in cards():
            ref = np.mean([lufs(got[(col, m, "T_today")]) for m in mats])
            parts = []
            for v, _ in VERSIONS:
                gain = 0.0 if v == "T_today" else ref - np.mean([lufs(got[(col, m, v)]) for m in mats])
                g = 10 ** (gain / 20)
                for m in mats:
                    y = got[(col, m, v)] * g
                    sf.write(out / col / "{}_{}.wav".format(m, v), y, SR, subtype="PCM_16")
                parts.append("{} {:+.2f} dB (peak {:.1f} dBFS)".format(v, gain, 20 * np.log10(max(np.abs(got[(col, m, v)] * g).max() for m in mats) + 1e-12)))
            vf.write("{}: {}\n".format(col, "; ".join(parts)))
    (out / "README.txt").write_text(README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title", "TONE with character",
                    "--no-level-match", "--dry", str(dry), "--wet-only"], check=True, stdout=subprocess.DEVNULL)
    print(out / "index.html")


# ---------------------------------------------------------------- checks
def metallic(y, t0, t1):
    """The tail's largest spectral peak over its 1/3-octave neighbourhood (SPEC's metallic check), dB, 100 Hz-10 kHz:
    Welch (8192-point Hann, half overlap) on t0..t1, each bin against the median of the bins within +-1/6 octave."""
    s = y[int(t0 * SR):int(t1 * SR)].mean(axis=1)
    n = 8192
    w = np.hanning(n)
    segs = [s[i:i + n] * w for i in range(0, len(s) - n + 1, n // 2)]
    P = np.mean([np.abs(np.fft.rfft(g)) ** 2 for g in segs], axis=0)
    f = np.fft.rfftfreq(n, 1.0 / SR)
    L = db(P)
    best, at = -99.0, 0.0
    for i in np.where((f >= 100) & (f <= 10000))[0]:
        lo, hi = np.searchsorted(f, f[i] * 2 ** (-1 / 6)), np.searchsorted(f, f[i] * 2 ** (1 / 6))
        p = L[i] - np.median(L[lo:hi + 1])
        if p > best:
            best, at = p, f[i]
    return best, at


def clicks_1ms(y, t0, t1):
    """The loudest 1 ms frame of the highs (> 5 kHz) between t0 and t1 against the median of the 1 s around it, dB."""
    X = np.fft.rfft(y.mean(axis=1))
    f = np.fft.rfftfreq(len(y), 1.0 / SR)
    X[f < 5000] = 0
    h = np.fft.irfft(X, len(y))
    n = SR // 1000
    e = (h[:len(h) // n * n].reshape(-1, n) ** 2).mean(axis=1)
    worst, spike = -99.0, -99.0
    floor = max(e.max() * 1e-6, 1e-9) # frames under -90 dBFS, or 60 dB under the loudest: inaudible (a shut low-pass)
    for i in range(int(t0 * 1000), min(len(e), int(t1 * 1000))):
        if e[i] < floor:
            continue
        med = np.median(e[max(0, i - 500):i + 500]) + 1e-30
        worst = max(worst, db(e[i]) - db(med))
        # a click stands out of its own few milliseconds; a filter sweeping the highs away and back rises over several
        near = np.concatenate([e[max(0, i - 4):i - 1], e[i + 2:i + 5]])
        spike = max(spike, db(e[i]) - db(near.mean() + 1e-30))
    return worst, spike


def checks(binary, material, out, only):
    rep = []
    tmp = Path(tempfile.mkdtemp())
    pool = ThreadPoolExecutor(JOBS)

    def say(s):
        print(s, flush=True)
        rep.append(s)

    sections = set(only or "1234")
    # 1. Level against today across TONE (music, LUFS), at the table's points and between them.
    tones = [i / 16 for i in range(17)]
    L = {} if "1" not in sections else level_grid(binary, material, tones, tmp)
    if "1" in sections:
        say("1. Level vs today on music (LUFS; HALL and MASSIVE x skank, vibes, pad; TONE 0..1 in 1/16): worst case per version")
    for v, _ in (VERSIONS[1:] if "1" in sections else []):
        d = [(L[(v, sp, m, t)] - L[("T_today", sp, m, t)], sp, m, t) for sp in ("hall", "massive")
             for m in ("skank", "vibes", "pad") for t in tones]
        mean_t = [np.mean([x[0] for x in d if x[3] == t]) for t in tones]
        w = max(d, key=lambda x: abs(x[0]))
        say("   {}: mean per TONE {}; worst single {:+.2f} dB ({} {} TONE {:.3f}); {}".format(
            v, " ".join("{:+.1f}".format(x) for x in mean_t), w[0], w[1], w[2], w[3],
            "pass (means within 1 dB)" if max(abs(x) for x in mean_t) <= 1.0 else "FAIL"))
    # 2. Metallic: noise hits, the tail (0.3-3.3 s after the last hit), TONE's ends and quarters, normal, SHIMMER, WEAR 0.9.
    rng = np.random.default_rng(3)
    hit = np.zeros((int(9 * SR), 2))
    for k in range(3):
        a = k * SR
        hit[a:a + 1200, :] = (0.6 * rng.uniform(-1, 1, 1200) * np.exp(-np.arange(1200) / 200.0))[:, None]
    sf.write(str(tmp / "hits.wav"), hit.astype(np.float32), SR, subtype="FLOAT")
    variants = [("normal", []), ("shimmer", ["attack=SHIMMER"]), ("wear0.9", ["era=0.9"])]
    mcases = [(v, sp, var, t) for v, _ in VERSIONS for sp in ("plate", "hall", "massive") for var, _ in variants
              for t in (0.0, 0.25, 0.75, 1.0)]

    def mjob(c):
        v, sp, var, t = c
        extra = dict(variants)[var]
        sets = SPACES[sp] + [s for s in COMMON if s.split("=")[0] not in [e.split("=")[0] for e in extra]] + extra + ["tone={}".format(t)]
        y, _, summ = render(binary, tmp / "hits.wav", tmp / "m_{}_{}_{}_{}.wav".format(*c), sets, v, keep=False)
        ring = [p for p in summ.split() if p.startswith("ringing=")]
        return c, metallic(y, 2.3, 5.3), ring[0] if ring else ""

    say("2. Metallic (SPEC: no tail peak > 6 dB over its 1/3-octave neighbourhood; 3 noise hits, the tail 0.3-3.3 s after; "
        "PLATE / HALL / MASSIVE x normal / SHIMMER / WEAR 0.9 x TONE 0, 0.25, 0.75, 1). The tail's bins are noisy, so "
        "today's renders pass 6 dB nowhere on this raw measure: what counts is what a version adds over today in the same "
        "case, and the renderer's calibrated ringing metric (M6, Ringing at 15 dB):")
    M = {c: (p, r) for c, p, r in pool.map(mjob, mcases if "2" in sections else [])}

    def ringdb(r):
        try:
            return float(r.split("=")[1].split("dB")[0])
        except (IndexError, ValueError):
            return float("nan")

    for v, _ in (VERSIONS if M else []):
        mine = [c for c in M if c[0] == v]
        w = max(mine, key=lambda c: M[c][0][0])
        rings = [(ringdb(M[c][1]), c) for c in mine if np.isfinite(ringdb(M[c][1]))]
        rw = max(rings) if rings else (float("nan"), None)
        line = "   {}: worst peak {:.1f} dB at {:.0f} Hz ({} {} TONE {}); renderer ringing worst {:.1f} dB{}".format(
            v, M[w][0][0], M[w][0][1], w[1], w[2], w[3], rw[0], " ({} {} TONE {})".format(*rw[1][1:]) if rw[1] else "")
        if v != "T_today":
            adds = [(M[c][0][0] - M[("T_today",) + c[1:]][0][0], c) for c in mine]
            a = max(adds)
            line += "; most added over today {:+.1f} dB ({} {} TONE {}); {}".format(
                a[0], a[1][1], a[1][2], a[1][3], "pass" if a[0] <= 1.5 and not (rw[0] >= 15.0) else "LOOK")
        say(line)
    # 3. Holds: 10-minute infinite (DECAY 1) and 5-minute freeze, at TONE's ends and quarters.
    h = np.zeros((int(620 * SR), 2))
    h[:int(20 * SR)] = 0.3 * rng.uniform(-1, 1, (int(20 * SR), 1))
    sf.write(str(tmp / "hold.wav"), h.astype(np.float32), SR, subtype="FLOAT")
    fz = np.zeros((int(305 * SR), 2))
    fz[:int(4.5 * SR)] = 0.3 * rng.uniform(-1, 1, (int(4.5 * SR), 1))
    sf.write(str(tmp / "freeze.wav"), fz.astype(np.float32), SR, subtype="FLOAT")
    hcases = [(kind, v, sp, t) for kind in ("infinite", "freeze") for v, _ in VERSIONS for sp in ("plate", "hall", "massive")
              for t in (0.0, 0.25, 0.75, 1.0)]

    W = {}

    def hjob2(c):
        kind, v, sp, t = c
        if kind == "infinite":
            y, _, _ = render(binary, tmp / "hold.wav", tmp / "h_{}_{}_{}_{}.wav".format(*c),
                             SPACES[sp] + ["decay=1", "size=0.5", "time=0", "tone={}".format(t)], v, keep=False)
            first = 6
        else:
            y, _, _ = render(binary, tmp / "freeze.wav", tmp / "h_{}_{}_{}_{}.wav".format(*c),
                             SPACES[sp] + COMMON + ["tone={}".format(t)], v, {"buttons": [[4.0, 4.1]]}, keep=False)
            first = 2
        w = np.array([db(np.mean(y[i:i + 10 * SR] ** 2)) for i in range(0, len(y) - 10 * SR + 1, 10 * SR)])
        return c, w[first:], bool(np.isfinite(y).all())

    for c, w, ok in pool.map(hjob2, hcases if "3" in sections else []):
        W[c] = (w, ok)
    say("3. Holds (10 s windows; infinite: DECAY 1 SIZE 0.5, 20 s of noise then 10 min, judged from 60 s; freeze: 4.5 s of "
        "noise, tapped at 4 s, 5 min, judged from 20 s; PLATE / HALL / MASSIVE x TONE 0, 0.25, 0.75, 1). The loop is "
        "today's, bit for bit, so each version's hold must follow today's: 'follows' = the most its window-by-window "
        "difference from today wanders (dB). Own range = max - min of its windows.")
    for kind in (("infinite", "freeze") if W else ()):
        for v, _ in VERSIONS:
            rows = [c for c in W if c[0] == kind and c[1] == v]
            rng_ = max(W[c][0].max() - W[c][0].min() for c in rows)
            worst = max(rows, key=lambda c: W[c][0].max() - W[c][0].min())
            fin = all(W[c][1] for c in rows)
            grow = max(W[c][0].max() - W[c][0][0] for c in rows)
            line = "   {:8s} {:16s} own range worst {:.2f} dB ({} TONE {}); grows at most {:+.2f} dB over its first window; finite {}".format(
                kind, v, rng_, worst[2], worst[3], grow, fin)
            if v != "T_today":
                # where today's own hold is steady (range < 3 dB): the version must follow it
                dev = [((W[c][0] - W[(kind, "T_today") + c[2:]][0]), c) for c in rows
                       if np.ptp(W[(kind, "T_today") + c[2:]][0]) < 3.0]
                wd = max(dev, key=lambda d: d[0].max() - d[0].min())
                line += "; where today holds steady, follows it within {:.2f} dB (worst {} TONE {}); {}".format(
                    wd[0].max() - wd[0].min(), wd[1][2], wd[1][3],
                    "pass" if wd[0].max() - wd[0].min() < 1.0 and fin and grow <= 1.0 else "LOOK")
            say(line)
        drift = [c for c in W if c[0] == kind and c[1] == "T_today" and np.ptp(W[c][0]) >= 3.0]
        if drift:
            say("   {}: today's own hold drifts 3 dB or more at {} (the loop, as built: see the note below)".format(
                kind, ", ".join("{} TONE {}".format(c[2], c[3]) for c in drift)))
        lows = [(W[c][0].min(), c) for c in W if c[0] == kind]
        say("   {}: quietest window anywhere {:.1f} dBFS ({} {} TONE {})".format(kind, min(lows)[0], *min(lows)[1][1:]))
    # 4. Clicks: full-range TONE CV triangles at 0.1, 2 and 20 Hz on music.
    trim(material / "skank.wav", tmp / "cvskank.wav", 23.0, 2.0)
    ccases = [(v, sp, hz) for v, _ in VERSIONS for sp in ("hall", "massive") for hz in (0.1, 2.0, 20.0)]

    def cjob(c):
        v, sp, hz = c
        secs = 10.0 if hz == 0.1 else 20.0
        y, _, _ = render(binary, tmp / "cvskank.wav", tmp / "c_{}_{}_{}.wav".format(*c), SPACES[sp] + COMMON,
                         v, cv_auto(hz, secs, 2.0), keep=False)
        return c, clicks_1ms(y, 2.0, 2.0 + secs)

    C = dict(pool.map(cjob, ccases if "4" in sections else []))
    say("4. Clicks during a full-range TONE triangle at 0.1, 2 and 20 Hz on the skank (HALL, MASSIVE), the worst 1 ms frame "
        "of the highs > 5 kHz: 'vs 1 s' against the median of the second around it (the usual score; a low-pass swept "
        "fully shut and open raises it by itself, since the highs come and go), 'spike' against its own neighbours "
        "(2-4 ms each side: a click stands out of them, a sweep does not); frames under -90 dBFS or 60 dB under the "
        "file's loudest are left out (inaudible: the highs of a shut low-pass in the music's last, quiet seconds). Today's sweep is the reference:")
    for v, _ in (VERSIONS if C else []):
        line = "   {}: {}".format(v, " ".join("{}@{}Hz {:+.1f} / spike {:+.1f}".format(c[1], c[2], C[c][0], C[c][1])
                                             for c in C if c[0] == v))
        if v != "T_today":
            worst = max(C[c][1] - C[("T_today",) + c[1:]][1] for c in C if c[0] == v)
            line += "; spike at most {:+.1f} dB over today's {}".format(worst, "pass" if worst <= 3.0 else "LOOK")
        say(line)
    pool.shutdown()
    shutil.rmtree(tmp)
    out.mkdir(parents=True, exist_ok=True)
    with open(out / "_checks.txt", "a") as f:
        f.write("\n".join(rep) + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("what", choices=["calibrate", "page", "checks"])
    ap.add_argument("--only", help="checks: the sections to run, e.g. 34 (default all)")
    ap.add_argument("--render", type=Path, default=ROOT / "build-tone" / "iv_render")
    a = ap.parse_args()
    root = main_checkout()
    material = root / "renders" / "_material"
    out = root / "renders" / "tone_character"
    binary = a.render.resolve()
    if a.what == "calibrate":
        calibrate(binary, material)
    elif a.what == "page":
        page(binary, material, out)
    else:
        checks(binary, material, out, a.only)


if __name__ == "__main__":
    main()
