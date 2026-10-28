#!/usr/bin/env python3
"""PROTOTYPE pick page (28 Oct 2026), renders/decay_range. The owner: "i often find myself wanting longer lush decays,
but the knob biases towards smaller decays until you hit the 3 oclock mark and it turns into perpetual tails. I'd like
more usable range so i can access longer decays without hitting the perpetual holds. I'd still like the held tails to
kick in at the 3 oclock position. This applies to plate and hall". Research: docs/research/decay-range.md.

DECAY for PLATE and HALL (renderer hook IV_DECAY_PLATE_MAX / IV_DECAY_HALL_MAX / IV_DECAY_TOP / IV_DECAY_GAMMA,
map::decayRatePrototype). Endless still starts at DECAY 0.85 (~3 o'clock on the module) in every version, and the
held tail's governor and the ducking keep 0.80-0.85. Versions:
    A  as built: exponential 0 -> 0.80 (PLATE 0.4 -> 12 s, HALL 0.8 -> 25 s), then a 0.05 crossover to endless
    B  longer tops on the same curve: PLATE -> 30 s, HALL -> 60 s at 0.80, the same short crossover
    C  as built up to ~1 o'clock, then a long gentle run-in: the finite top (12 / 25 s) reached at 0.70, and from
       there to 0.85 the tail lengthens gradually (1/T60 falls in a straight line, as A's crossover but 3x as wide)
    D  both: PLATE -> 20 s, HALL -> 40 s at 0.70, then C's gentle run-in
Knob positions are the module's (a 300 degree pot, 7 to 5 o'clock, with the firmware's 2.5 % end stops):
    9 0.184 / 10 0.289 / 11 0.395 / noon 0.5 / 1 0.605 / 2 0.711 / 2:30 0.763 / 3 0.816.

    decay_range.py [--render build-decay/iv_render] [--verify-only] [--skip-safety]
    decay_range.py --built <59ce205's iv_render> [--render build-decay/iv_render]   the build against the page

Built as SPEC v1.0.53 (ADR 0034, the owner's pick B). The IV_DECAY_* hook the page was rendered with (ed11f65)
is gone, so only --built (and --table) work on the build.
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
from review_cpu_pages import main_checkout, db  # noqa: E402
import level_duck as ld  # noqa: E402
import frozen_twin as ft  # noqa: E402

SR = 48000
PLAY, TAIL = 12.0, 30.0                 # the chord: 12 s, then 30 s of tail
PH_ON, PH_OFF, PH_N, PH_TAIL = 4.0, 3.0, 6, 8.0  # the phrase: 4 s on, 3 s off, six times, then 8 s
CLOCK = [("9", 0.184), ("10", 0.289), ("11", 0.395), ("noon", 0.5), ("1", 0.605), ("2", 0.711), ("2:30", 0.763),
         ("3", 0.816)]
MIN_S = {"PLATE": 0.4, "HALL": 0.8}
VERSIONS = [("A_today", {}, {"PLATE": 12.0, "HALL": 25.0, "top": 0.80}),
            ("B_longer_tops", {"IV_DECAY_PLATE_MAX": "30", "IV_DECAY_HALL_MAX": "60"},
             {"PLATE": 30.0, "HALL": 60.0, "top": 0.80}),
            ("C_gentle_run_in", {"IV_DECAY_TOP": "0.70"}, {"PLATE": 12.0, "HALL": 25.0, "top": 0.70}),
            ("D_both", {"IV_DECAY_PLATE_MAX": "20", "IV_DECAY_HALL_MAX": "40", "IV_DECAY_TOP": "0.70"},
             {"PLATE": 20.0, "HALL": 40.0, "top": 0.70})]
# HALL's T60 scale at SIZE 0.8 (map::hallT60Scale: 0.49 ^ (1 - SIZE)); PLATE's SIZE doesn't scale its T60.
SPACES = {"PLATE": (["space=PLATE", "size=0.5"], 1.0), "HALL": (["space=HALL", "size=0.8"], 0.49 ** 0.2)}
COLS = [("1_plate", "PLATE"), ("2_hall", "HALL")]
CARDS = [("p1_noon", 0.5), ("p2_one", 0.605), ("p3_two", 0.711), ("p4_half_past_two", 0.763)]
SWEEP = "p5_sweep"
CHAPTERS = "Playing:0,Stop:12,Tail:13"


def t60(v, space, cfg):
    """map::decayRatePrototype's T60 (s; inf = endless), SIZE full."""
    lo, hi, top = MIN_S[space], cfg[space], cfg["top"]
    if v <= top:
        return lo * (hi / lo) ** (v / top)
    if v >= 0.85:
        return float("inf")
    return hi / ((0.85 - v) / (0.85 - top))


def fmt(t):
    return "endless" if not np.isfinite(t) else ("{:.0f} s".format(t) if t >= 10 else "{:.1f} s".format(t))


def table(scale_note=True):
    rows = ["{:24s}".format("") + "".join("{:>9s}".format(h) for h, _ in CLOCK)]
    for sp in ("PLATE", "HALL"):
        for tag, _, cfg in VERSIONS:
            rows.append("{:24s}".format(sp + " " + tag.split("_")[0]) +
                        "".join("{:>9s}".format(fmt(t60(v, sp, cfg))) for _, v in CLOCK))
        rows.append("")
    return "\n".join(rows)


README = """Longer lush tails without reaching the endless hold too soon. Your note: "i often find myself wanting longer lush decays, but the knob biases towards smaller decays until you hit the 3 oclock mark and it turns into perpetual tails. I'd like more usable range so i can access longer decays without hitting the perpetual holds. I'd still like the held tails to kick in at the 3 oclock position."

Today DECAY doubles the tail length about every 1.5 hours of the knob, tops out at 12 s (PLATE) or 25 s (HALL) just before 3 o'clock, then jumps to endless within a few degrees. So the long tails you want only live in a sliver just before the hold. Big reverbs that let the knob reach endless do one of two things: a long finite top (Valhalla and Lexicon go to 70 s, Strymon's Cloud to 50 s), or a gentle approach where the last stretch keeps lengthening the tail gradually until it holds (Valley's Plateau, Make Noise's Erbe-Verb).

In every version the endless hold still starts just after 3 o'clock, where it does today, and the hold, the ducking and the level at endless are exactly as now. Short tails at the bottom of the knob stay short. The versions:

A_today            as built.
B_longer_tops      the same curve, aiming higher: PLATE up to 30 s, HALL up to 60 s just before 3 o'clock, then the same quick step to endless. Every position is longer, noon about 1.7x.
C_gentle_run_in    today's tops, reached at 2 o'clock instead of 3, then from 2 o'clock to 3 the tail keeps growing gradually (HALL 27 s at 2, 43 s at half past, ~110 s at 3) and slides into the hold rather than stepping into it. A little longer through the middle (noon about 1.4x), the short end almost as today.
D_both             longer all the way up (noon about 2x today) plus C's gentle run-in: the most long-tail range, the least short-to-medium range.

How long the tail is at each knob position (the time to fade by 60 dB; PLATE at any SIZE, HALL at SIZE full; the HALL cards here are at SIZE 0.8, about 13 % shorter):

{table}

What to listen for: the chord plays 12 s and stops; listen to how long the tail rings out at noon, 1, 2 and half past 2. The last card on each side ("p5_sweep") turns DECAY slowly from 9 o'clock to 3 o'clock while a chord phrase plays with gaps: listen to how each gap's tail grows, and whether the long tails arrive gradually or all at the end.

Level: matched while playing. Each version's reverb is set to A's level over the chord's 2-11 s (unmatched, B-D were up to 1-2.5 dB louder at their long settings, see _verify.txt), so what differs is how long it rings, not how loud it is. The sweep cards aren't matched (the knob moves). MIX starts at 50 %.

p1_noon            DECAY at noon (0.5)
p2_one             DECAY at 1 o'clock (0.605)
p3_two             DECAY at 2 o'clock (0.711)
p4_half_past_two   DECAY at half past 2 (0.763)
p5_sweep           DECAY turned slowly from 9 to 3 o'clock (0.184 -> 0.816) over 42 s while the phrase plays"""


# ---- material

def chord():
    y = ld.washy_chord()[:int(PLAY * SR)]
    y = ft.fade(y, 0.0, 0.4)
    x = np.zeros((int((PLAY + TAIL) * SR), 2))
    x[:len(y)] = y
    return x * (10 ** (ld.PEAK_DBFS / 20) / np.max(np.abs(x)))


def phrase():
    y = ld.washy_chord()
    n_on = int(PH_ON * SR)
    seg = y[:n_on].copy()
    r = int(0.15 * SR)
    seg[-r:] *= np.linspace(1.0, 0.0, r)[:, None]
    x = np.zeros((int((PH_N * (PH_ON + PH_OFF) + PH_TAIL) * SR), 2))
    for k in range(PH_N):
        a = int(k * (PH_ON + PH_OFF) * SR)
        x[a:a + n_on] = seg
    return x * (10 ** (ld.PEAK_DBFS / 20) / np.max(np.abs(x)))


def sweep_auto():
    end = PH_N * (PH_ON + PH_OFF)
    return {"breakpoints": [{"t": 0.0, "key": "decay", "value": 0.184},
                            {"t": end, "key": "decay", "value": 0.816}]}


# ---- measures

def band(x, lo, hi):
    X = np.fft.rfft(x.mean(axis=1) if x.ndim == 2 else x)
    f = np.fft.rfftfreq(x.shape[0], 1.0 / SR)
    X[(f < lo) | (f > hi)] = 0
    return np.fft.irfft(X, x.shape[0])


def measured_t60(x, stop=PLAY):
    """The tail's T60 in the 200-800 Hz band (below the highs' extra air loss): a straight-line fit of its 20 ms level
    (smoothed over 100 ms) from 5 dB to 35 dB under its level at the stop (T30), within 25 s of the stop."""
    e = ld.frames_db(band(x, 200.0, 800.0), 0.02)
    e = np.convolve(e, np.ones(5) / 5, mode="same")
    t = np.arange(len(e)) * 0.02
    w = (t >= stop + 0.1) & (t < stop + 25.0)
    e0 = e[w][0]
    k = w & (e <= e0 - 5.0) & (e >= e0 - 35.0)
    first_below = np.argmax(w & (e < e0 - 35.0)) if np.any(w & (e < e0 - 35.0)) else len(e)
    k &= np.arange(len(e)) < first_below
    p = np.polyfit(t[k], e[k], 1)
    return -60.0 / p[0] if p[0] < 0 else float("inf")


def worst_click_at(x, t0, t1):
    e = ld.frames_db(ft.highpass(x, 5000.0), 0.001)
    best = (-99.0, 0.0)
    for b in range(int(t0), min(int(t1), len(e) // 1000 - 1)):
        seg = e[b * 1000:(b + 1) * 1000]
        best = max(best, (float(seg.max() - np.median(seg)), b + int(np.argmax(seg)) / 1000.0))
    return best


def hf_frames(y):
    """10 ms frame levels (dB) of the whole render: broadband and above 3 kHz (high-passed whole, so no segment edges)."""
    def fr(z):
        n = int(0.01 * SR)
        z = z[:len(z) // n * n].reshape(-1, n)
        return 10 * np.log10((z ** 2).mean(axis=1) + 1e-30)
    return fr(y.mean(axis=1)), fr(ft.highpass(y, 3000.0))


def hf_excess(ref, x, t0, t1):
    """x against ref (the same version, DECAY unswitched; both hf_frames) over t0..t1: the largest rise of the > 3 kHz
    energy in 10 ms frames beyond the broadband rise, above its own median (dB): a short burst of highs from the move."""
    a, b = int(t0 / 0.01), int(t1 / 0.01)
    d = (x[1][a:b] - ref[1][a:b]) - (x[0][a:b] - ref[0][a:b])
    return float(d.max() - np.median(d))


# ---- the page

def page(binary, root, verify_only=False, skip_safety=False):
    out = root / "renders" / "decay_range"
    dry = root / "renders" / "_material_decay_range"
    cards = [(m, v) for m, v in CARDS]
    if not verify_only:
        if out.exists():
            shutil.rmtree(out)
        out.mkdir(parents=True)
        dry.mkdir(parents=True, exist_ok=True)
        np.random.seed(1)  # the chord's detuned saws have random phases
        ch = chord().astype(np.float32)
        for m, _ in cards:
            sf.write(dry / (m + ".wav"), ch, SR, subtype="FLOAT")
        sf.write(dry / (SWEEP + ".wav"), phrase().astype(np.float32), SR, subtype="FLOAT")
        log = []
        jobs = []
        for col, sp in COLS:
            sets = SPACES[sp][0] + ld.KNOBS
            for tag, env, _ in VERSIONS:
                for m, v in cards:
                    jobs.append((dry / (m + ".wav"), out / col / "{}_{}.wav".format(m, tag), sets + ["decay={}".format(v)],
                                 env, None))
                jobs.append((dry / (SWEEP + ".wav"), out / col / "{}_{}.wav".format(SWEEP, tag), sets + ["decay=0.184"],
                             env, sweep_auto()))
        with ThreadPoolExecutor(max(1, (os.cpu_count() or 2) - 2)) as pool:
            list(pool.map(lambda j: ld.render(binary, j[0], j[1], j[2], j[3], auto=j[4], log=log), jobs))
        (out / "_hooks.txt").write_text("\n".join(sorted(log)) + "\n")

    v = ["decay_range (v1.0.52 renderer + the IV_DECAY_* hook). Each version against A: identical (FAIL: did not "
         "render) or the null in dB; the renderer's hook line per file is in _hooks.txt.",
         "T60: the tail after the chord stops, 200-800 Hz, a straight-line fit of its level from 5 to 35 dB under the "
         "stop (T30), within 25 s; target = the version's mapping x HALL's SIZE 0.8 scale (0.867). Long tails read 5-10 % "
         "short (A's too: 25 s of a tail and the early sound weigh in). Playing: the wet's level over 2-11 s against A's; the page "
         "file is then scaled by minus that (level-matched while playing). Click: the worst 1 ms frame above 5 kHz "
         "against its second's median (1 s to the end - 1 s), and when.", ""]
    gains = {}
    for col, sp in COLS:
        scale = SPACES[sp][1]
        for m, dv in cards + [(SWEEP, None)]:
            a = None
            for tag, _, cfg in VERSIONS:
                p = out / col / "{}_{}.wav".format(m, tag)
                x, _ = sf.read(p, always_2d=True)
                raw_key = (col, m, tag)
                if verify_only and (out / "_gains.json").exists():
                    g = json.loads((out / "_gains.json").read_text()).get("/".join(raw_key), 0.0)
                    x = x * 10 ** (-g / 20)  # back to the raw render
                if a is None:
                    a = x
                same = "" if tag == VERSIONS[0][0] else (
                    "  IDENTICAL to A (FAIL: did not render)" if np.array_equal(a, x) else
                    "  null vs A {:+.1f} dB".format(db(np.mean((a - x) ** 2) / np.mean(a ** 2))))
                click, when = worst_click_at(x, 1, x.shape[0] / SR - 1)
                if m == SWEEP:
                    v.append("{:7s} {:17s} {:16s} click {:4.1f} dB ({:4.1f} s){}".format(sp, m, tag, click, when, same))
                    continue
                play = ld.lv(x, 2, 11) - ld.lv(a, 2, 11)
                gains["/".join(raw_key)] = play
                want = t60(dv, sp, cfg) * scale
                got = measured_t60(x)
                v.append("{:7s} {:17s} {:16s} T60 {:6.1f} s (target {:6.1f}, {:+4.0f} %)  playing {:+5.2f} dB vs A  "
                         "click {:4.1f} dB ({:4.1f} s){}".format(sp, m, tag, got, want, 100 * (got / want - 1), play,
                                                                 click, when, same))
                if not verify_only and abs(play) > 0.0:
                    sf.write(p, (x * 10 ** (-play / 20)).astype(np.float32), SR, subtype="FLOAT")
            v.append("")
    if not verify_only:
        (out / "_gains.json").write_text(json.dumps(gains, indent=1))
    if not skip_safety:
        v += safety(binary, root)
    (out / "_verify.txt").write_text("\n".join(v) + "\n")
    if verify_only:
        return out / "_verify.txt"
    (out / "README.txt").write_text(README.format(table=table()) + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "DECAY's range: longer tails before the hold", "--no-level-match", "--dry", str(dry),
                    "--wet-only", "--chapters", CHAPTERS], check=True, stdout=subprocess.DEVNULL)
    return out / "index.html"


# ---- safety: energy, clicks and bursts on DECAY moves

LEVELS = (-24.0, -12.0, -4.0)   # the fed material's peak, dBFS
HOLD_DECAYS = (0.80, 0.835)     # the longest finite setting before the governor (0.80) and inside the crossover


# Clicks and bursts: the phrase with DECAY flicked across the range (noon -> 0.84 in 0.2 s at 9.5 s, back to 0.4 in
# 0.2 s at 16.5 s, held -> 0.84 -> 0.6 at 23.5 s), against the same version with DECAY left at noon.
FLICK = {"breakpoints": [{"t": 9.5, "key": "decay", "value": 0.5}, {"t": 9.7, "key": "decay", "value": 0.84},
                         {"t": 16.5, "key": "decay", "value": 0.84}, {"t": 16.7, "key": "decay", "value": 0.4},
                         {"t": 23.5, "key": "decay", "value": 0.4}, {"t": 23.7, "key": "decay", "value": 0.84},
                         {"t": 24.0, "key": "decay", "value": 0.6}]}
MOVES = (9.5, 16.5, 23.5)


SEEDS = (1, 2, 3)


def flicks(binary, ph, tmp, job, k=0):
    sp, tag, env = job
    f = ld.read_gone(ld.render(binary, ph, tmp / "f_{}_{}_{}.wav".format(sp, tag, k), SPACES[sp][0] + ld.KNOBS + ["decay=0.5"],
                               env, auto=FLICK))
    s = ld.read_gone(ld.render(binary, ph, tmp / "s_{}_{}_{}.wav".format(sp, tag, k), SPACES[sp][0] + ld.KNOBS + ["decay=0.5"],
                               env))
    click, when = worst_click_at(f, 1, f.shape[0] / SR - 1)
    sclick, _ = worst_click_at(s, 1, s.shape[0] / SR - 1)
    fs, ss = hf_frames(f), hf_frames(s)
    burst = max(hf_excess(ss, fs, m - 0.2, m + 1.5) for m in MOVES)
    return click, when, sclick, burst


def safety(binary, root, versions=None):
    """versions: [(tag, env, binary)]; default the page's, all through `binary`."""
    vers = versions or [(tag, env, binary) for tag, env, _ in VERSIONS]
    bins = {tag: b for tag, _, b in vers}
    material = root / "renders" / "_material"
    lines = ["", "SAFETY", "Energy: fed 3-min holds (pad, stabs, chord looped at three input levels), then 30 s of tail, "
             "at DECAY 0.80 (each version's longest setting before the held tail's governor starts) and 0.835 (inside "
             "the crossover, where B-D ring 3-10x longer than A). 30 s wet levels: growth = the loudest window from 60 s "
             "on minus the 30-60 s window; tail growth = any rise after the input stops; peak vs the limiter's ceiling "
             "(-1 dBFS). Fail: growth >= 3 dB past the first minute, a rising tail, a non-finite sample or a peak at the "
             "ceiling. (A finite tail fed for minutes rises until loss equals input; a 3 dB rise in the last 2 min with "
             "T60s of minutes is the build-up this check looks for.)"]
    with tempfile.TemporaryDirectory() as t, ThreadPoolExecutor(max(1, (os.cpu_count() or 2) - 2)) as pool:
        tmp = Path(t)
        hot = np.concatenate([ld.stimulus(material, n)[:int(ld.PLAY * SR)] for n in ("pad", "stabs", "chord")])
        hot = hot / np.max(np.abs(hot))
        feds = {}
        for L in LEVELS:
            p = tmp / "fed_{:+.0f}.wav".format(L)
            sf.write(p, (np.concatenate([ld.loop_to(hot, 180), np.zeros((30 * SR, 2))]) * 10 ** (L / 20)).astype(
                np.float32), SR, subtype="FLOAT")
            feds[L] = p

        def hold(job):
            sp, tag, env, dv, L = job
            o = tmp / "h_{}_{}_{}_{:+.0f}.wav".format(sp, tag, dv, L)
            y = ld.read_gone(ld.render(bins[tag], feds[L], o, SPACES[sp][0] + ld.KNOBS + ["decay={}".format(dv)], env))
            w = [ld.lv(y, s, s + 30) for s in range(0, 180, 30)]
            tail = [ld.lv(y, s, s + 10) for s in range(180, 210, 10)]
            pk = 20 * np.log10(np.max(np.abs(y)) + 1e-12)
            grow, tgrow = max(w[2:]) - w[1], max(tail[1:]) - tail[0]
            good = bool(np.isfinite(y).all()) and grow < 3.0 and tgrow < 0.5 and pk < 20 * np.log10(ld.CEILING) - 0.05
            return (sp, tag, dv, L), (min(w[1:]), max(w[1:]), grow, tgrow, pk, good)
        jobs = [(sp, tag, env, dv, L) for sp in ("PLATE", "HALL") for tag, env, _ in vers for dv in HOLD_DECAYS
                for L in LEVELS]
        res = dict(pool.map(hold, jobs))
        fails = 0
        for sp in ("PLATE", "HALL"):
            for tag, env, _ in vers:
                for dv in HOLD_DECAYS:
                    cells = []
                    for L in LEVELS:
                        lo, hi, g, tg, pk, good = res[(sp, tag, dv, L)]
                        fails += not good
                        cells.append("in {:+3.0f}: {:6.1f}..{:6.1f} dBFS grow {:+5.2f} tail {:+5.2f} peak {:5.1f} {}".format(
                            L, lo, hi, g, tg, pk, "pass" if good else "FAIL"))
                    lines.append("  {:5s} {:16s} DECAY {:5.3f}  ".format(sp, tag, dv) + " | ".join(cells))
        lines.append("  energy: {} of {} holds fail".format(fails, len(jobs)))

        # Clicks and bursts: the phrase with DECAY flicked across the range (noon -> 0.84 in 0.2 s at 9.5 s, back to
        # 0.4 in 0.2 s at 16.5 s, held -> 0.84 -> 0.6 at 23.5 s), against the same version with DECAY left at noon.
        phs = []
        for seed in SEEDS:  # the chord's detuned saws have random phases: three takes of the phrase
            np.random.seed(seed)
            phs.append(tmp / "phrase_{}.wav".format(seed))
            sf.write(phs[-1], phrase().astype(np.float32), SR, subtype="FLOAT")
        lines += ["", "Clicks / bursts: the phrase with DECAY flicked noon -> 0.84 (0.2 s, at 9.5 s), 0.84 -> 0.4 (16.5 "
                  "s), 0.4 -> 0.84 -> 0.6 (23.5 s), and the slow 9 -> 3 o'clock sweep (the page's p5 card). Click: worst "
                  "1 ms frame above 5 kHz vs its second's median over the whole render (A, today, is the yardstick: the "
                  "music's own grain reads the same). Burst: around each flick (-0.2..+1.5 s), the > 3 kHz energy's rise "
                  "beyond the broadband rise against the same version with DECAY held at noon, above its median "
                  "(10 ms frames of a tail spread 1-3 dB on their own; three takes of the phrase, the chord's random phases differing, show that spread)."]

        fjobs = [(sp, tag, env) for sp in ("PLATE", "HALL") for tag, env, _ in vers]
        runs = [(j, k) for j in fjobs for k in range(len(SEEDS))]
        fres = list(pool.map(lambda r: flicks(bins[r[0][1]], phs[r[1]], tmp, r[0], r[1]), runs))
        for i, (sp, tag, _) in enumerate(fjobs):
            rs = fres[i * len(SEEDS):(i + 1) * len(SEEDS)]
            lines.append("  {:5s} {:16s} flicks, takes {}: click {} dB (held at noon {})  burst {} dB".format(
                sp, tag, "/".join(str(k) for k in SEEDS), " / ".join("{:4.1f}".format(r[0]) for r in rs),
                " / ".join("{:4.1f}".format(r[2]) for r in rs), " / ".join("{:+4.1f}".format(r[3]) for r in rs)))
    return lines


def built(ref, new, root):
    """The build (v1.0.53, B) against the page: each card rendered by the new renderer (no hooks) against the
    page's B (as heard: scaled by its _gains.json) and A; endless (DECAY >= 0.85) against the reference renderer
    (59ce205) bit for bit; then the safety checks with A = the reference renderer, B = the build."""
    out = root / "renders" / "decay_range"
    dry = root / "renders" / "_material_decay_range"
    gains = json.loads((out / "_gains.json").read_text())
    v = ["decay_range, the build (B: PLATE 30 s, HALL 60 s, the long tails' trim) vs the page. Null = the build against "
         "the page's B as the owner heard it (scaled to A's playing level); playing = the build's wet over 2-11 s "
         "against A's (target within +-0.5 dB); T60 as on the page.", ""]
    with tempfile.TemporaryDirectory() as t:
        tmp = Path(t)
        for col, sp in COLS:
            for m, dv in CARDS:
                y = ld.read_gone(ld.render(new, dry / (m + ".wav"), tmp / "b.wav",
                                           SPACES[sp][0] + ld.KNOBS + ["decay={}".format(dv)], {}))
                a, _ = sf.read(out / col / "{}_A_today.wav".format(m), always_2d=True)
                b, _ = sf.read(out / col / "{}_B_longer_tops.wav".format(m), always_2d=True)
                null = db(np.mean((y - b) ** 2) / np.mean(b ** 2))
                g = gains["/".join((col, m, "B_longer_tops"))]
                v.append("{:6s} {:17s} null vs the page's B (as heard, {:+.2f} dB) {:+6.1f} dB  playing vs A {:+5.2f} dB  "
                         "T60 {:5.1f} s (page's B {:5.1f})".format(sp, m, -g, null, ld.lv(y, 2, 11) - ld.lv(a, 2, 11),
                                                                   measured_t60(y), measured_t60(b)))
        v += ["", "Endless and the crossover's end, bit for bit against 59ce205 (the chord, 42 s):"]
        for sp in ("PLATE", "HALL"):
            for dv in (0.85, 0.9, 1.0):
                sets = SPACES[sp][0] + ld.KNOBS + ["decay={}".format(dv)]
                x = ld.read_gone(ld.render(ref, dry / "p1_noon.wav", tmp / "r.wav", sets, {}))
                y = ld.read_gone(ld.render(new, dry / "p1_noon.wav", tmp / "n.wav", sets, {}))
                v.append("  {:5s} DECAY {:.2f}: {}".format(sp, dv, "identical" if np.array_equal(x, y) else
                                                         "DIFFER (max {:.3g})".format(float(np.max(np.abs(x - y))))))
    v += safety(new, root, [("A_59ce205", {}, ref), ("B_built", {}, new)])
    (out / "_verify_built.txt").write_text("\n".join(v) + "\n")
    return out / "_verify_built.txt"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-decay" / "iv_render")
    ap.add_argument("--verify-only", action="store_true", help="re-measure the rendered page (and redo the safety renders)")
    ap.add_argument("--skip-safety", action="store_true")
    ap.add_argument("--table", action="store_true", help="print the tail-length table and stop")
    ap.add_argument("--built", type=Path, help="the reference renderer (59ce205): check the build (--render) against the page")
    a = ap.parse_args()
    if a.table:
        print(table())
        return
    if a.built:
        print(built(a.built.resolve(), a.render.resolve(), main_checkout()))
        return
    print(page(a.render.resolve(), main_checkout(), a.verify_only, a.skip_safety))


if __name__ == "__main__":
    main()
