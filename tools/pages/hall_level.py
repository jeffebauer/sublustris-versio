#!/usr/bin/env python3
"""PROTOTYPE pick page (28 Oct 2026), renders/hall_level. The owner on renders/level_glue (v1.0.51 = their pick E):
"Hall infinite examples feel slightly less level matched than massive, particularly the first example - reverb feels
a bit too quiet so when the set stops, there's a less seamless transition".

Measured before the page: on the held chord HALL's wet isn't quieter overall than MASSIVE's (both tails ~4.5 dB under
the dry), but HALL's tail is darker: above 2 kHz it sits 3-4 dB further under the dry than MASSIVE's.

Every version is v1.0.51 (ducking 2:1 that eases off on held playing, the endless tail at -21 dBFS), HALL, SIZE 0.8,
DECAY full, plus:
    A  as built
    B  HALL's endless tail +1.5 dB (IV_HALL_TAIL_DB=1.5: the floor governor's floor, HALL only)
    C  HALL's endless tail +3 dB   (IV_HALL_TAIL_DB=3)
    D  a slightly brighter HALL tail at the as-built level (IV_HALL_TAIL_TILT: TONE's tilt on the wet, HALL at
       infinite DECAY only: highs up, lows down, balanced so the level stays where A's is)
    M  MASSIVE as built: the reference the owner compared against (not an option)
Not level-matched (the point is the reverb's level against the dry); MIX starts at 50 %.

    hall_level.py [--render build-glue/iv_render] [--verify-only]    the page and its _verify.txt
"""
import argparse
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
import level_glue as lg  # noqa: E402
import frozen_twin as ft  # noqa: E402

SR = 48000
TILT = "-2,3.5"  # D: lows / highs round TONE's 600 Hz bend, dB at infinite (calibrated on the chord: its tail at A's level, its > 2 kHz ~3 dB up, near MASSIVE's)
HALL = ["space=HALL", "size=0.8", "decay=1"]
VERSIONS = [("A_as_built", HALL, {"IV_HALL_TAIL_DB": "0"}),  # 0: as built before v1.0.52 (C is built since)
            ("B_tail_plus_1_5dB", HALL, {"IV_HALL_TAIL_DB": "1.5"}),
            ("C_tail_plus_3dB", HALL, {"IV_HALL_TAIL_DB": "3"}),
            ("D_brighter_tail", HALL, {"IV_HALL_TAIL_TILT": TILT}),
            ("M_massive_reference", ["space=MASSIVE", "size=0.8", "decay=1"], {})]
MATERIALS = ["chord", "stopstart", "stabs"]
COL = "1_hall_infinite"
CHAPTERS = "Playing:0,Stop:12,Tail:13"

README = """HALL's level at the moment the playing stops. Your note on level_glue: "Hall infinite examples feel slightly less level matched than massive, particularly the first example - reverb feels a bit too quiet so when the set stops, there's a less seamless transition".

What I measured: HALL's reverb isn't quieter overall than MASSIVE's on the held chord (both tails sit about 4.5 dB under the dry), but HALL's tail is darker: its highs (above about 2 kHz) sit 3-4 dB further under the dry than MASSIVE's. So it may be the brightness you hear as "too quiet", or it may be the level. These versions try both.

Every version is what's built now (your D and E: ducking 2:1 that eases off on held playing, the endless tail 3 dB higher), HALL, SIZE 0.8, DECAY full:
A is HALL as built. B lifts HALL's endless tail 1.5 dB. C lifts it 3 dB. D keeps A's level but makes HALL's endless tail a little brighter (like turning TONE slightly clockwise, only for HALL at endless DECAY). M is MASSIVE as built, the one you said feels right: it's there to compare, not to pick. Measured on the held chord: B and C make HALL's tail louder but hardly brighter; D makes it brighter (its highs close to MASSIVE's) but no louder.

Not level-matched: the level against the dry is the point. The MIX fader starts at 50 %, where you heard it. Each card: 12 s of playing, then the tail. The stop-start phrase plays a chord for 4 s and stops for 3, three times: listen to each stop. The stabs are there to check nothing pumps or jumps.

1_hall_infinite        HALL, SIZE 0.8, DECAY full

A_as_built             HALL as built (v1.0.51)
B_tail_plus_1_5dB      HALL's endless tail 1.5 dB louder
C_tail_plus_3dB        HALL's endless tail 3 dB louder
D_brighter_tail        HALL's endless tail a little brighter, same level as A
M_massive_reference    MASSIVE as built, for comparison (not an option)"""


def band_db(x, lo, t0, t1):
    seg = x[int(t0 * SR):int(t1 * SR)]
    h = ft.highpass(seg, lo) if lo else seg
    return db(np.mean(h ** 2) + 1e-30)


def page(binary, root, verify_only=False):
    out = root / "renders" / "hall_level"
    dry = root / "renders" / "_material_level_glue"
    if not verify_only:
        if out.exists():
            shutil.rmtree(out)
        out.mkdir(parents=True)
        log = []
        jobs = [(dry / (m + ".wav"), out / COL / "{}_{}.wav".format(m, tag), sets + ld.KNOBS, env)
                for tag, sets, env in VERSIONS for m in MATERIALS]
        with ThreadPoolExecutor(max(1, (os.cpu_count() or 2) - 2)) as pool:
            list(pool.map(lambda j: ld.render(binary, *j, log=log), jobs))
        (out / "_hooks.txt").write_text("\n".join(sorted(log)) + "\n")

    with open(out / "_verify.txt", "w") as v:
        v.write("hall_level (v1.0.51 renderer). Not level-matched. Each version against A (identical / null in dB) and the "
                "renderer's hook line (_hooks.txt) confirm it rendered. Per stop of the playing (the dry 10 dB under its "
                "playing level), on the wet: tail = the wet 3-4 s after the stop (2-3 s for the stop-start phrase) "
                "against the dry while playing (dB), broadband and above 2 kHz; on the 50:50 sum: dip / dur as on "
                "level_glue. Click: the worst 1 ms frame of the highs (> 5 kHz) from 1 s to 23 s vs its second's median "
                "(level_glue's check; the render's last second is left out, where the FFT high-pass wraps its abrupt "
                "end), with the time it happens. HF burst: around each stop (-0.5..+1.5 s), 10 ms frames, the > 3 kHz "
                "energy's difference from A's minus the broadband difference: its peak above its own median in the "
                "window (a short burst of highs the click check misses; a steady tone change reads "
                "~0; 10 ms frames of a tail are noisy, so 1-3 dB is the measure's own spread: D, a steady tilt, reads "
                "1-3 dB; M is another reverb and isn't comparable).\n\n")
        for m in MATERIALS:
            d, _ = sf.read(dry / (m + ".wav"), always_2d=True)
            a, _ = sf.read(out / COL / "{}_{}.wav".format(m, VERSIONS[0][0]), always_2d=True)
            st = lg.stops(d)
            settle = 2.0 if m == "stopstart" else 3.0
            for tag, _, _ in VERSIONS:
                x, _ = sf.read(out / COL / "{}_{}.wav".format(m, tag), always_2d=True)
                same = "" if tag == VERSIONS[0][0] else (
                    "  IDENTICAL to A (FAIL: did not render)" if np.array_equal(a, x) else "  null vs A {:+.1f} dB".format(
                        db(np.mean((a - x) ** 2) / np.mean(a ** 2))))
                click, when = worst_click_at(x)
                burst = max(hf_excess(a, x, s) for s in st) if st else 0.0
                if m == "stabs":
                    v.write("{:9s} {:20s} pump {:4.1f} dB  playing {:6.1f}  click {:4.1f} ({:4.1f} s)  HF burst {:+5.2f} dB{}\n".format(
                        m, tag, lg.pumping(x), ld.lv(x, 2, 12), click, when, burst, same))
                    continue
                hs = [lg.hand_over(x, d, s, settle) for s in st]
                tl = [band_db(x, 0, s + settle, s + settle + 1) - band_db(d, 0, s - 2, s - 0.5) for s in st]
                th = [band_db(x, 2000.0, s + settle, s + settle + 1) - band_db(d, 2000.0, s - 2, s - 0.5) for s in st]
                v.write("{:9s} {:20s} tail vs dry {:+5.1f} dB, > 2 kHz {:+5.1f} dB  dip {:4.1f} dB  dur {:4.2f} s  "
                        "click {:4.1f} ({:4.1f} s)  HF burst {:+5.2f} dB{}\n".format(
                            m, tag, np.mean(tl), np.mean(th), np.mean([h["dip"] for h in hs]),
                            np.mean([h["dur"] for h in hs]), click, when, burst, same))
            v.write("\n")
        v.write(holds(binary, root))
    if verify_only:
        return out / "_verify.txt"
    (out / "README.txt").write_text(README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "HALL's level when the playing stops", "--no-level-match", "--dry", str(dry),
                    "--wet-only", "--chapters", CHAPTERS], check=True, stdout=subprocess.DEVNULL)
    return out / "index.html"


def hf_excess(a, x, stop):
    """Around a stop: the largest rise of x's > 3 kHz energy over a's, in 10 ms frames, beyond the broadband rise (dB)."""
    def frames(y, lo):
        seg = y[int((stop - 0.5) * SR):int((stop + 1.5) * SR)]
        seg = ft.highpass(seg, lo) if lo else seg
        seg = seg.mean(axis=1) if seg.ndim == 2 else seg
        n = int(0.01 * SR)
        seg = seg[:len(seg) // n * n].reshape(-1, n)
        return 10 * np.log10((seg ** 2).mean(axis=1) + 1e-30)
    d = (frames(x, 3000.0) - frames(a, 3000.0)) - (frames(x, 0) - frames(a, 0))
    return float(d.max() - np.median(d))


def worst_click_at(x, t0=1, t1=23):
    """ld.worst_click's measure (the loudest 1 ms frame of the highs > 5 kHz against its second's median), and when."""
    e = ld.frames_db(ft.highpass(x, 5000.0), 0.001)
    best = (-99.0, 0.0)
    for b in range(t0, min(t1, len(e) // 1000)):
        seg = e[b * 1000:(b + 1) * 1000]
        best = max(best, (float(seg.max() - np.median(seg)), b + int(np.argmax(seg)) / 1000.0))
    return best


def holds(binary, root):
    """Fed 10-minute infinite holds (level_duck's: hot material looped, then 1 min of tail) for every HALL version."""
    material = root / "renders" / "_material"
    lines = ["Fed 10-min infinite holds (hot material looped, HALL, DECAY full, SIZE 0.8): 1-min wet levels, growth = "
             "max(min 2-10) - min 1-2; tail 10 s windows after the input stops; peak. Fail: growth or tail growth >= 1 dB, "
             "or a peak at the limiter's ceiling."]
    with tempfile.TemporaryDirectory() as t, ThreadPoolExecutor(4) as pool:
        tmp = Path(t)
        hot = np.concatenate([ld.stimulus(material, n)[:int(ld.PLAY * SR)] for n in ("pad", "stabs", "chord")])
        fed = tmp / "fed.wav"
        sf.write(fed, np.concatenate([ld.loop_to(hot, 600), np.zeros((60 * SR, 2))]).astype(np.float32), SR,
                 subtype="FLOAT")

        def job(tag, sets, env):
            y = ld.read_gone(ld.render(binary, fed, tmp / "fed_{}.wav".format(tag), sets + ld.KNOBS, env))
            w = [ld.lv(y, s, s + 60) for s in range(0, 600, 60)]
            tail = [ld.lv(y, s, s + 10) for s in range(600, 660, 10)]
            pk = 20 * np.log10(np.max(np.abs(y)) + 1e-12)
            grow, tgrow = max(w[2:]) - w[1], max(tail[1:]) - tail[0]
            good = bool(np.isfinite(y).all()) and grow < 1.0 and tgrow < 1.0 and pk < 20 * np.log10(ld.CEILING)
            return "   {:20s} playing {:6.1f} .. {:6.1f} dBFS, growth {:+.2f} dB; tail {:6.1f} .. {:6.1f}, growth {:+.2f}; " \
                   "peak {:5.1f} dBFS  {}".format(tag, min(w[1:]), max(w[1:]), grow, min(tail), max(tail), tgrow, pk,
                                                  "pass" if good else "FAIL")
        lines += list(pool.map(lambda j: job(*j), VERSIONS))
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-glue" / "iv_render")
    ap.add_argument("--verify-only", action="store_true", help="re-measure the rendered page (no renders but the holds)")
    a = ap.parse_args()
    print(page(a.render.resolve(), main_checkout(), a.verify_only))


if __name__ == "__main__":
    main()
