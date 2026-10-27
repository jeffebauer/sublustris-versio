#!/usr/bin/env python3
"""PROTOTYPE pick page (27 Oct 2026), renders/level_duck: the owner, from the rack, "on hall and massive with Long
decays, I feel the limiter is keeping the level a bit too low. When sweeping the mix from 50:50 dry wet to 100% wet,
the reverb gets noticeably louder which makes it feel unbalanced and impacts performability", and "ducking on large
washy sounds still feels a bit heavy handed, I think we can reduce it from 4:1 to 2:1 or even 1.5:1".

Measured: the limiter barely acts. At infinite DECAY the wet sits 7-12 dB under the dry while playing, for two
reasons: the floor governor's floor (voicing::kFloorDbfs, -24 dBFS: HALL -4.3 dB, MASSIVE -7.5 dB) and the 4:1 duck
(voicing::kDuckRatio, ~5 dB on hot input, up to 9 dB on a held chord). Renderer hooks IV_DUCK_RATIO and IV_FLOOR_DB
(Engine::setDuckRatioPrototype, setFloorPrototype):
    A  today: floor -24, duck 4:1
    B  duck 2:1
    C  duck 1.5:1
    D  duck 2:1 + floor -21
    E  duck 1.5:1 + floor -18 (safety-checked only; held off the page: see PAGE_VERSIONS)
Cards: HALL and MASSIVE at infinite DECAY (1.0), MASSIVE at DECAY 0.75 (the control: no duck, no floor there).
Material at a hot input (peaks -4 dBFS): a large washy held chord, the pad, the dub chord stabs; 12 s playing, then
12 s of tail. Not level-matched: the level is the question.

    level_duck.py [--render build-ld/iv_render]            the page (and its _verify.txt)
    level_duck.py --safety [--render ...]                  the safety checks before the page (prints a summary)
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
sys.path.insert(0, str(ROOT / "tools"))
from review_cpu_pages import main_checkout, db  # noqa: E402
import frozen_twin as ft  # noqa: E402

SR = 48000
PLAY, LEN = 12.0, 24.0
PEAK_DBFS = -4.0
KNOBS = ["tone=0.5", "drift=0.5", "attack=NATURAL", "time=0", "era=0.1"]
COLS = [("1_hall_infinite", ["space=HALL", "size=0.8", "decay=1"], ["chord", "stabs"]),
        ("2_massive_infinite", ["space=MASSIVE", "size=0.8", "decay=1"], ["chord", "pad", "stabs"]),
        ("3_massive_decay075", ["space=MASSIVE", "size=0.8", "decay=0.75"], ["chord", "pad", "stabs"])]
VERSIONS = [("A_today", {}),
            ("B_duck_2to1", {"IV_DUCK_RATIO": "2"}),
            ("C_duck_1p5to1", {"IV_DUCK_RATIO": "1.5"}),
            ("D_duck_2to1_floor21", {"IV_DUCK_RATIO": "2", "IV_FLOOR_DB": "-21"}),
            ("E_duck_1p5to1_floor18", {"IV_DUCK_RATIO": "1.5", "IV_FLOOR_DB": "-18"})]
# E is held off the page: the safety check's 1 ms click scan failed it (MASSIVE, DECAY swept through infinite, +4.7 dB
# over A; HALL and PLATE +2.5-2.7; on the page's own renders HALL's held chord +10.8 dB in the tail). The ticks are gone
# at WEAR 0: the tail 6 dB hotter drives the Neve stage (WEAR 0.1) into making them. D passes everywhere.
PAGE_VERSIONS = VERSIONS[:4]
CHAPTERS = "Playing:0,Tail:12"
CEILING = 0.891  # voicing::kLimitCeiling, -1 dBFS

README = """How loud should the reverb sit at long DECAY? Your notes: "on hall and massive with Long decays, I feel the limiter is keeping the level a bit too low. When sweeping the mix from 50:50 dry wet to 100% wet, the reverb gets noticeably louder" and "ducking on large washy sounds still feels a bit heavy handed, I think we can reduce it from 4:1 to 2:1 or even 1.5:1".

What we found: it isn't the limiter (it hardly touches these). Two other things hold the reverb down while you play at infinite DECAY. The level an endless tail settles at is set low (-24 dBFS), which holds HALL about 4 dB and MASSIVE about 7.5 dB down; and the ducking squeezes the reverb under what you play, about 5 dB on a hot input and up to 9 dB under a held chord. Together the reverb sits 7-12 dB under your dry while you play.

A is today. B and C only soften the ducking (2:1, 1.5:1). D softens it to 2:1 and lets the endless tail settle 3 dB higher. Nothing else changes: the limiter still has room (the loudest peaks stay well under it).

NOT level-matched, on purpose: the level is the question. The MIX fader starts at 50 %, where you hear the imbalance; slide it to 100 % and back to hear how much the reverb jumps. Each card: 12 s of playing at a hot level (peaks -4 dBFS), then the tail on its own. Card 3 is MASSIVE at DECAY 0.75, where there's no ducking and no settling level: all four should sound the same there (the control).

1_hall_infinite        HALL, SIZE 0.8, DECAY full (infinite)
2_massive_infinite     MASSIVE, SIZE 0.8, DECAY full (infinite)
3_massive_decay075     MASSIVE, SIZE 0.8, DECAY 0.75: the control, little should change

A_today                today: ducking 4:1, the endless tail settles at -24
B_duck_2to1            ducking 2:1
C_duck_1p5to1          ducking 1.5:1
D_duck_2to1_floor21    ducking 2:1, the endless tail settles 3 dB higher (-21)"""


# ---- material

def washy_chord():
    """A large washy held chord: an open minor-ninth voicing of wide detuned saws, swelling in over 0.8 s,
    held to 11 s, released over 1 s (make_material's synth parts)."""
    import make_material as mm
    n = int(PLAY * SR)
    notes = ["A1", "E2", "C3", "G3", "B3", "E4", "G4"]
    x = sum(mm.detuned(mm.midi(c), n, 7, 18) for c in notes) / len(notes)
    t = np.arange(n) / SR
    cut = 2200 * (1 + 0.3 * np.sin(2 * np.pi * 0.12 * t))
    y = mm.svf_lp(x, cut, 0.7) * mm.adsr(n, 0.8, 0.5, 0.85, 1.0, PLAY - 1.0)
    return np.stack([y, np.roll(y, int(0.013 * SR))], axis=1)


def clip(material, name):
    a, _ = sf.read(material / (name + ".wav"), always_2d=True)
    return ft.fade(a[:int(PLAY * SR)], 0.0, 0.4)


def stimulus(material, name):
    y = washy_chord() if name == "chord" else clip(material, name)
    x = np.zeros((int(LEN * SR), 2))
    x[:len(y)] = y
    return x * (10 ** (PEAK_DBFS / 20) / np.max(np.abs(x)))


# ---- rendering

def render(binary, src, out, sets, env_extra, auto=None, log=None):
    out.parent.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    env.update(env_extra)
    args = [str(binary), str(src), str(out), "--set", "mix=1"]
    if auto is not None:
        a = out.with_suffix(".json")
        a.write_text(json.dumps(auto))
        args += ["--auto", str(a)]
    for s in sets:
        args += ["--set", s]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    if auto is not None:
        a.unlink()
    if p.returncode != 0:
        sys.exit("render failed: {}\n{}".format(" ".join(args), p.stderr))
    hook = " | ".join(l for l in p.stderr.splitlines() if l.startswith("iv_render:")) or "(no hook line)"
    if env_extra and hook == "(no hook line)":
        sys.exit("hook not confirmed by the renderer: {} {}".format(env_extra, out))
    if log is not None:
        log.append("{}/{}  env {}  ->  {}".format(out.parent.name, out.name, env_extra or "{}", hook))
    return out


def lv(x, t0, t1):
    seg = x[int(t0 * SR):int(t1 * SR)]
    return db(np.mean(seg ** 2) + 1e-30)


def frames_db(x, frame_s):
    n = int(frame_s * SR)
    m = x.mean(axis=1) if x.ndim == 2 else x
    m = m[:len(m) // n * n].reshape(-1, n)
    return db((m ** 2).mean(axis=1) + 1e-30)


def worst_click(x, t0=0.0, t1=None):
    """The loudest 1 ms frame of the highs (> 5 kHz) against the median of the second around it, dB, over t0..t1.
    The last 50 ms are left out: the FFT high-pass wraps the render's abrupt end (a ringing infinite tail) onto it."""
    h = ft.highpass(x, 5000.0)
    e = frames_db(h, 0.001)
    e = np.concatenate([e[:-50], np.full(50, np.median(e[-1000:]))])
    t1 = len(e) / 1000.0 if t1 is None else t1
    worst = -99.0
    blocks = len(e) // 1000  # median per 1 s block, compared with the frames in it
    for b in range(int(t0), min(blocks, int(t1))):
        seg = e[b * 1000:(b + 1) * 1000]
        worst = max(worst, float(seg.max() - np.median(seg)))
    return worst


# ---- the page

def page(binary, root):
    out = root / "renders" / "level_duck"
    dry = root / "renders" / "_material_level_duck"
    meas = root / "renders" / "_level_duck_noduck"  # measurement only (no ducking), removed at the end
    material = root / "renders" / "_material"
    for d in (out, dry, meas):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
    names = sorted({m for _, _, ms in COLS for m in ms})
    for m in names:
        sf.write(dry / (m + ".wav"), stimulus(material, m), SR, subtype="PCM_24")
    log = []
    jobs = []
    for col, sets, ms in COLS:
        for tag, env in PAGE_VERSIONS:
            for m in ms:
                jobs.append((dry / (m + ".wav"), out / col / "{}_{}.wav".format(m, tag), sets + KNOBS, env))
        for fl in ("-24", "-21"):
            for m in ms:
                jobs.append((dry / (m + ".wav"), meas / col / "{}_floor{}.wav".format(m, fl[1:]), sets + KNOBS,
                             {"IV_DUCK_RATIO": "1", "IV_FLOOR_DB": fl}))
    with ThreadPoolExecutor(max(1, (os.cpu_count() or 2) - 2)) as pool:
        list(pool.map(lambda j: render(binary, *j, log=log), jobs))
    (out / "_hooks.txt").write_text("\n".join(sorted(log)) + "\n")

    floors = {"A_today": "24", "B_duck_2to1": "24", "C_duck_1p5to1": "24", "D_duck_2to1_floor21": "21",
              "E_duck_1p5to1_floor18": "18"}
    rows = []
    with open(out / "_verify.txt", "w") as v:
        v.write("level_duck. Not level-matched. Per render: wet while playing (2-12 s, RMS dBFS), the wet vs the dry over "
                "the same window (at MIX 50 % the two have equal gains, so this is how the reverb sits under the dry), "
                "the duck's reduction (the same render with no ducking minus this one, 2-12 s), the tail (13-23 s), "
                "the wet's sample peak (the limiter's ceiling is -1 dBFS), the worst 1 ms click score from 1 s on "
                "(highs > 5 kHz, loudest 1 ms frame vs its second's median, dB; the first second is the input's onset "
                "out of silence) and the difference from A.\n\n")
        for col, _, ms in COLS:
            for m in ms:
                d, _ = sf.read(dry / (m + ".wav"), always_2d=True)
                a, _ = sf.read(out / col / "{}_{}.wav".format(m, PAGE_VERSIONS[0][0]), always_2d=True)
                for tag, _ in PAGE_VERSIONS:
                    x, _ = sf.read(out / col / "{}_{}.wav".format(m, tag), always_2d=True)
                    nd, _ = sf.read(meas / col / "{}_floor{}.wav".format(m, floors[tag]), always_2d=True)
                    same = "" if tag == PAGE_VERSIONS[0][0] else (
                        "  IDENTICAL to A" if np.array_equal(a, x) else "  null vs A {:+.1f} dB".format(
                            db(np.mean((a - x) ** 2) / np.mean(a ** 2))))
                    r = dict(col=col, m=m, tag=tag, play=lv(x, 2, PLAY), vsdry=lv(x, 2, PLAY) - lv(d, 2, PLAY),
                             duck=lv(nd, 2, PLAY) - lv(x, 2, PLAY), tail=lv(x, 13, 23),
                             peak=20 * np.log10(np.max(np.abs(x)) + 1e-12), click=worst_click(x, 1.0), same=same)
                    rows.append(r)
                    v.write("{col:20s} {m:6s} {tag:22s} playing {play:6.1f}  vs dry {vsdry:+5.1f}  duck -{duck:4.1f} dB  "
                            "tail {tail:6.1f}  peak {peak:5.1f} dBFS  click {click:4.1f}{same}\n".format(**r))
            v.write("\n")
        v.write("Means per card (playing vs A, dB; vs dry; duck reduction):\n")
        for col, _, ms in COLS:
            base = np.mean([r["play"] for r in rows if r["col"] == col and r["tag"] == PAGE_VERSIONS[0][0]])
            for tag, _ in PAGE_VERSIONS:
                rs = [r for r in rows if r["col"] == col and r["tag"] == tag]
                v.write("{:20s} {:22s} {:+5.1f} vs A   wet vs dry {:+5.1f}   duck -{:4.1f}   peak max {:5.1f}\n".format(
                    col, tag, np.mean([r["play"] for r in rs]) - base, np.mean([r["vsdry"] for r in rs]),
                    np.mean([r["duck"] for r in rs]), max(r["peak"] for r in rs)))
    shutil.rmtree(meas)
    (out / "README.txt").write_text(README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "The reverb's level at long DECAY: ducking and settling level", "--no-level-match", "--dry", str(dry),
                    "--wet-only", "--chapters", CHAPTERS], check=True, stdout=subprocess.DEVNULL)
    return out / "index.html"


# ---- the safety checks

SAFE = [("A_today", {}), VERSIONS[3], VERSIONS[4]]  # the raised floors, with A as the reference


def read_gone(path):
    """Reads a render and deletes it (the safety renders are long: keep the disk use to a few at a time)."""
    y, _ = sf.read(path, always_2d=True)
    os.unlink(path)
    return y


def loop_to(y, seconds):
    n = int(seconds * SR)
    reps = int(np.ceil(n / len(y)))
    return np.tile(y, (reps, 1))[:n]


def safety(binary, root, part="all", safe=None, clickv=None):
    safe = SAFE if safe is None else safe
    clickv = VERSIONS if clickv is None else clickv
    material = root / "renders" / "_material"
    fails = []
    lines = []
    with tempfile.TemporaryDirectory() as t, ThreadPoolExecutor(4) as pool:
        tmp = Path(t)
        hot = np.concatenate([stimulus(material, "pad")[:int(PLAY * SR)], stimulus(material, "stabs")[:int(PLAY * SR)],
                              stimulus(material, "chord")[:int(PLAY * SR)]])

        # 1. Fed 10-minute infinite holds: hot material for 10 min, then 1 min of tail.
        fed = tmp / "fed.wav"
        sf.write(fed, np.concatenate([loop_to(hot, 600), np.zeros((60 * SR, 2))]).astype(np.float32), SR, subtype="FLOAT")

        def fed_job(sp, tag, env):
            y = read_gone(render(binary, fed, tmp / "fed_{}_{}.wav".format(sp, tag), ["space=" + sp, "decay=1", "size=0.8"]
                                  + KNOBS, env))
            w = [lv(y, s, s + 60) for s in range(0, 600, 60)]
            tail = [lv(y, s, s + 10) for s in range(600, 660, 10)]
            return sp, tag, w, tail, 20 * np.log10(np.max(np.abs(y)) + 1e-12), bool(np.isfinite(y).all())

        # 2. A 5-minute freeze with a paint: the pad frozen at 3 s, keys played over 140-160 s with the button held
        #    (Paint) 145-150 s, the pad measured before (120-140 s), after (170-180 s) and at the end (290-300 s).
        fz = tmp / "freeze.wav"
        x = np.zeros((300 * SR, 2))
        x[:int(3.6 * SR)] = stimulus(material, "pad")[:int(3.6 * SR)]
        k = stimulus(material, "chord")[:int(PLAY * SR)]
        x[140 * SR:140 * SR + len(k)] += k
        sf.write(fz, x.astype(np.float32), SR, subtype="FLOAT")

        def fz_job(sp, decay, tag, env):
            y = read_gone(render(binary, fz, tmp / "fz_{}_{}_{}.wav".format(sp, decay, tag),
                                  ["space=" + sp, "decay=" + decay, "size=0.8"] + KNOBS, env,
                                  auto={"buttons": [[3.0, 3.1], [145.0, 150.0]]}))
            return (sp, decay, tag, lv(y, 20, 40), lv(y, 120, 140), lv(y, 170, 180), lv(y, 290, 300),
                    20 * np.log10(np.max(np.abs(y)) + 1e-12), bool(np.isfinite(y).all()))

        # 3. WEAR full + SHIMMER at infinite: 2 min of hot material, then 3 min of tail.
        sh = tmp / "shimmer.wav"
        sf.write(sh, np.concatenate([loop_to(hot, 120), np.zeros((180 * SR, 2))]).astype(np.float32), SR, subtype="FLOAT")

        def sh_job(sp, tag, env):
            y = read_gone(render(binary, sh, tmp / "sh_{}_{}.wav".format(sp, tag),
                                  ["space=" + sp, "decay=1", "size=0.8", "tone=0.5", "drift=0.5", "time=0", "era=1",
                                   "attack=SHIMMER"], env))
            w = [lv(y, s, s + 10) for s in range(120, 300, 10)]
            return sp, tag, lv(y, 100, 120), w, 20 * np.log10(np.max(np.abs(y)) + 1e-12), bool(np.isfinite(y).all())

        # 4. Click check: DECAY swept 0.7 -> 1 -> 0.7 (through the duck's and the floor's onset) while playing.
        ck = tmp / "click.wav"
        sf.write(ck, np.concatenate([loop_to(hot, 30), np.zeros((10 * SR, 2))]).astype(np.float32), SR, subtype="FLOAT")
        sweep = {"breakpoints": [{"t": 0, "key": "decay", "value": 0.7}, {"t": 15, "key": "decay", "value": 1.0},
                                 {"t": 30, "key": "decay", "value": 0.7}]}

        def ck_job(sp, tag, env, era):
            y = read_gone(render(binary, ck, tmp / "ck_{}_{}_{}.wav".format(sp, tag, era), ["space=" + sp, "size=0.8"]
                                 + KNOBS + ["era=" + era], env, auto=sweep))
            return sp, tag, era, worst_click(y, 1.0)

        long_ = part == "all"
        fed_f = [pool.submit(fed_job, sp, tag, env) for sp in ("HALL", "MASSIVE") for tag, env in safe] if long_ else []
        fz_f = [pool.submit(fz_job, sp, dc, tag, env) for sp in ("HALL", "MASSIVE") for dc in ("0.9", "1")
                for tag, env in safe] if long_ else []
        sh_f = [pool.submit(sh_job, sp, tag, env) for sp in ("HALL", "MASSIVE") for tag, env in safe] if long_ else []
        ck_f = [pool.submit(ck_job, sp, tag, env, era) for sp in ("HALL", "MASSIVE", "PLATE") for tag, env in clickv
                for era in ("0.1", "0")]
        fz_a = {}

        lines.append("1. Fed 10-min infinite holds (hot material looped, DECAY full, SIZE 0.8): 1-min wet levels, growth = "
                     "max(min 2-10) - min 1-2; tail 10 s windows after the input stops; peak")
        for f in fed_f:
            sp, tag, w, tail, pk, ok = f.result()
            grow = max(w[2:]) - w[1]
            tgrow = max(tail[1:]) - tail[0]
            good = ok and grow < 1.0 and tgrow < 1.0 and pk < 20 * np.log10(CEILING)
            if not good:
                fails.append("fed hold {} {}".format(sp, tag))
            lines.append("   {:8s} {:22s} playing {:6.1f} .. {:6.1f} dBFS, growth {:+.2f} dB; tail growth {:+.2f}; peak {:5.1f} "
                         "dBFS  {}".format(sp, tag, min(w[1:]), max(w[1:]), grow, tgrow, pk, "pass" if good else "FAIL"))
        lines.append("2. 5-min freeze + Paint (pad frozen at 3 s; chord over it 140-152 s, Paint held 145-150 s): pad level "
                     "20-40 s, 120-140 s (before), 170-180 s (after), 290-300 s (end); paint rise = after - before "
                     "(the ceiling: kPaintHeadroomDb 1.5 dB above the pad's level or the floor, on the tank; fail: more than "
                     "0.5 dB over A's rise, the pad moving > 1 dB while frozen, or a peak over -1 dBFS)")
        for f in fz_f:
            sp, dc, tag, early, before, after, end, pk, ok = f.result()
            rise = after - before
            drift = end - after
            fz_a.setdefault((sp, dc), rise)  # A comes first
            good = (ok and rise <= fz_a[(sp, dc)] + 0.5 and abs(drift) < 1.0 and abs(before - early) < 1.0
                    and pk < 20 * np.log10(CEILING))
            if not good:
                fails.append("freeze/paint {} {} {}".format(sp, dc, tag))
            lines.append("   {:8s} DECAY {:3s} {:22s} pad {:6.1f} / {:6.1f} -> {:6.1f} -> {:6.1f} dBFS; paint rise {:+.2f}; "
                         "after-paint drift {:+.2f}; peak {:5.1f}  {}".format(sp, dc, tag, early, before, after, end, rise,
                                                                              drift, pk, "pass" if good else "FAIL"))
        lines.append("3. WEAR full + SHIMMER, DECAY full: 2 min of hot material, 3 min of tail; growth = max of the tail's "
                     "10 s windows - the first; peak")
        for f in sh_f:
            sp, tag, playing, w, pk, ok = f.result()
            grow = max(w[1:]) - w[0]
            good = ok and grow < 1.0 and pk < 20 * np.log10(CEILING)
            if not good:
                fails.append("shimmer {} {}".format(sp, tag))
            lines.append("   {:8s} {:22s} playing {:6.1f}; tail {:6.1f} .. {:6.1f} dBFS, growth {:+.2f}; peak {:5.1f}  {}".format(
                sp, tag, playing, min(w), max(w), grow, pk, "pass" if good else "FAIL"))
        lines.append("4. 1 ms click check: DECAY swept 0.7 -> 1 -> 0.7 over 30 s while playing (from 1 s on: the first second "
                     "is the onset out of silence); worst 1 ms frame of the highs vs its second's median, dB, at WEAR 0.1 "
                     "(the page's) and WEAR 0 (fail: > 3 dB over A)")
        ck = {(sp, tag, era): s for sp, tag, era, s in (f.result() for f in ck_f)}
        for (sp, tag, era), s in ck.items():
            good = s <= ck[(sp, clickv[0][0], era)] + 3.0
            if not good:
                fails.append("click {} {} WEAR {}".format(sp, tag, era))
            lines.append("   {:8s} {:22s} WEAR {:3s} {:5.1f} dB  {}".format(sp, tag, era, s, "pass" if good else "FAIL"))
    lines.append("SAFETY: " + ("all pass" if not fails else "FAIL: " + ", ".join(fails)))
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-ld" / "iv_render")
    ap.add_argument("--safety", action="store_true")
    ap.add_argument("--clicks-only", action="store_true", help="with --safety: only the click check (seconds, not minutes)")
    a = ap.parse_args()
    root = main_checkout()
    binary = a.render.resolve()
    if a.safety:
        print(safety(binary, root, "clicks" if a.clicks_only else "all"))
    else:
        print(page(binary, root))


if __name__ == "__main__":
    main()
