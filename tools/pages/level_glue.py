#!/usr/bin/env python3
"""PROTOTYPE pick page (27 Oct 2026), renders/level_glue: the owner picked D on renders/level_duck (duck 2:1, floor -21)
and wrote: "there are some cases where there's still an audible dip once the dry signal stops, particularly in the
first example. What can we do to glue the dry and wet at points of transition so they sound more integrated rather
than two separate layers?" (HALL, infinite DECAY, the held washy chord, at MIX 50:50).

Measured on that card (D): while the chord is held, the duck keeps the wet 2.9 dB under the level its tail settles at
(MASSIVE 3.8). When the dry stops, the 0.4 s release brings the wet up over about a second: the 50:50 sum falls
below where it settles (1.7 dB, 0.3 s) and then swells back up. Without the duck the dip is 1.2 dB / 0.15 s (the
fresh input leaving the tank, at the edge of the tail's own motion).

Every version is D (IV_DUCK_RATIO=2, IV_FLOOR_DB=-21) plus:
    A  as picked
    B  a faster release: 0.15 s (IV_DUCK_RELEASE)
    C  a release that depends on the playing: 0.08 s after held sounds, 0.4 s after short ones (IV_DUCK_ADAPTIVE)
    D  less ducking on held sounds: 40 % of the reduction (IV_DUCK_SUSTAIN_CUT=0.6)
    E  C and D together
Cards: HALL and MASSIVE at infinite DECAY (the held chord, the pad, the stabs; a stop-start phrase: 4 s on, 3 s off),
MASSIVE at DECAY 0.75 (the control: no ducking there). Not level-matched: the tails after the stop are the same in
every version; only the reverb under the playing, and how it comes up when the playing stops, differ.

    level_glue.py [--render build-ld/iv_render]              the page (and its _verify.txt)
    level_glue.py --safety [--clicks-only] [--render ...]    the safety checks (level_duck's, on these versions)
    level_glue.py --safety --built --render build-glue/iv_render
        on v1.0.51 and later (E built in): A (adaptive release and sustain cut off) against E as built (no hooks)
"""
import argparse
import os
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from review_cpu_pages import main_checkout, db  # noqa: E402
import level_duck as ld  # noqa: E402

SR = 48000
BASE = {"IV_DUCK_RATIO": "2", "IV_FLOOR_DB": "-21"}
VERSIONS = [("A_as_picked", dict(BASE)),
            ("B_release_150ms", dict(BASE, IV_DUCK_RELEASE="0.15")),
            ("C_release_by_playing", dict(BASE, IV_DUCK_ADAPTIVE="0.08")),
            ("D_less_on_held", dict(BASE, IV_DUCK_SUSTAIN_CUT="0.6")),
            ("E_both", dict(BASE, IV_DUCK_ADAPTIVE="0.08", IV_DUCK_SUSTAIN_CUT="0.6"))]
COLS = [("1_hall_infinite", ["space=HALL", "size=0.8", "decay=1"], ["chord", "stopstart", "stabs"]),
        ("2_massive_infinite", ["space=MASSIVE", "size=0.8", "decay=1"], ["chord", "pad", "stopstart", "stabs"]),
        ("3_massive_decay075", ["space=MASSIVE", "size=0.8", "decay=0.75"], ["chord", "pad", "stabs"])]
STOPSTART_ON, STOPSTART_OFF, STOPSTART_N = 4.0, 3.0, 3
CHAPTERS = "Playing:0,Stop:12,Tail:13"

README = """The moment the playing stops. Your note on D: "there are some cases where there's still an audible dip once the dry signal stops, particularly in the first example. What can we do to glue the dry and wet at points of transition so they sound more integrated rather than two separate layers?"

What happens: while you hold a chord, the ducking keeps the reverb about 3 dB under the level it settles at afterwards (HALL 2.9 dB, MASSIVE 3.8). When you stop, the ducking lets go over about a second, so for a moment there's less of everything (the dry has gone, the reverb hasn't come up yet), and then the reverb swells back up on its own. That's the dip, and the swell after it is what makes the two sound like separate layers.

Every version here is D as you picked it (ducking 2:1, the endless tail at -21), with a different way of letting go:
A is D as picked. B lets go faster, in about a seventh of a second instead of 0.4 s. C lets go fast after held sounds (chords, pads) but keeps today's slower let-go after short ones (stabs), so stabs don't pump. D ducks held sounds less (40 % of the ducking), so there's less to come back when you stop; short sounds are ducked as before. E is C and D together.

Not level-matched: after the stop the tails are the same in every version, so the tail is your reference; what differs is how loud the reverb sits under the playing and how it arrives when you stop. The MIX fader starts at 50 %, where you heard the dip.

Each card: 12 s of playing, then the tail. The stop-start phrase plays a chord for 4 s and stops for 3, three times: listen to each stop. The stabs are there to check nothing pumps. Card 3 (MASSIVE at DECAY 0.75) has no ducking: all five are the same there.

1_hall_infinite        HALL, SIZE 0.8, DECAY full (your first example: the held chord)
2_massive_infinite     MASSIVE, SIZE 0.8, DECAY full
3_massive_decay075     MASSIVE, SIZE 0.8, DECAY 0.75: the control, nothing changes

A_as_picked            D from the last page: ducking 2:1, lets go over 0.4 s
B_release_150ms        lets go faster (0.15 s)
C_release_by_playing   lets go fast after held sounds, as today after short ones
D_less_on_held         held sounds ducked less (40 %), so less comes back at the stop
E_both                 C and D together"""


def stopstart():
    """The washy chord, 4 s on, 3 s off, three times (ld.washy_chord's sound), then silence to 24 s."""
    y = ld.washy_chord()
    x = np.zeros((int(ld.LEN * SR), 2))
    n_on = int(STOPSTART_ON * SR)
    seg = y[:n_on].copy()
    r = int(0.15 * SR)
    seg[-r:] *= np.linspace(1.0, 0.0, r)[:, None]
    for k in range(STOPSTART_N):
        a = int(k * (STOPSTART_ON + STOPSTART_OFF) * SR)
        x[a:a + n_on] = seg
    return x * (10 ** (ld.PEAK_DBFS / 20) / np.max(np.abs(x)))


def stimulus(material, name):
    return stopstart() if name == "stopstart" else ld.stimulus(material, name)


def env_db(x, smooth=3):
    """50 ms windows of the mean power, smoothed over 150 ms (the tail's own motion is ~±0.6 dB at 50 ms)."""
    m = (x ** 2).mean(axis=1)
    n = int(0.05 * SR)
    e = m[:len(m) // n * n].reshape(-1, n).mean(axis=1)
    return db(np.convolve(e, np.ones(smooth) / smooth, mode="same"))


def stops(d):
    """Times (s) the dry falls 10 dB under its playing level, one per stop."""
    e = env_db(d)
    t = np.arange(len(e)) * 0.05
    play = np.percentile(e[e > e.max() - 30], 50)
    below = e < play - 10
    out = []
    for i in range(1, len(e)):
        if below[i] and not below[i - 1] and t[i] > 1.0:
            out.append(t[i])
    return out


def hand_over(x, d, stop, settle):
    """At one stop: the 50:50 sum's dip under where it settles (the tail at `settle` seconds after the stop, 1 s long),
    how long it stays more than 1 dB under, the wet under the playing vs that tail, and how much the wet comes up
    after the stop (tail minus the wet's first 0.3 s)."""
    s = 0.7071 * (x + d)
    es, ew = env_db(s), env_db(x)
    t = np.arange(len(es)) * 0.05
    tail = (t >= stop + settle) & (t < stop + settle + 1.0)
    S, WS = es[tail].mean(), ew[tail].mean()
    post = es[(t >= stop) & (t < stop + settle)]
    playing = ew[(t >= stop - 2.0) & (t < stop - 0.5)].mean()
    return dict(dip=S - post.min(), dur=float((post < S - 1.0).sum() * 0.05), under=playing - WS,
                rise=WS - ew[(t >= stop) & (t < stop + 0.3)].mean())


def pumping(x):
    """How much the wet's level moves while the stabs play (2-12 s): the spread of its 50 ms level, p90 - p10, dB."""
    e = env_db(x, 1)
    seg = e[40:240]
    return float(np.percentile(seg, 90) - np.percentile(seg, 10))


def page(binary, root):
    out = root / "renders" / "level_glue"
    dry = root / "renders" / "_material_level_glue"
    material = root / "renders" / "_material"
    for d in (out, dry):
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
    names = sorted({m for _, _, ms in COLS for m in ms})
    for m in names:
        sf.write(dry / (m + ".wav"), stimulus(material, m), SR, subtype="PCM_24")
    log = []
    jobs = [(dry / (m + ".wav"), out / col / "{}_{}.wav".format(m, tag), sets + ld.KNOBS, env)
            for col, sets, ms in COLS for tag, env in VERSIONS for m in ms]
    with ThreadPoolExecutor(max(1, (os.cpu_count() or 2) - 2)) as pool:
        list(pool.map(lambda j: ld.render(binary, *j, log=log), jobs))
    (out / "_hooks.txt").write_text("\n".join(sorted(log)) + "\n")

    with open(out / "_verify.txt", "w") as v:
        v.write("level_glue. Not level-matched. At each stop of the playing (the dry 10 dB under its playing level), on the "
                "50:50 sum (equal gains, as MIX at noon): dip = how far the sum falls under the level it settles at "
                "(the tail 3-4 s after the stop; 2-3 s for the stop-start phrase), dur = how long it stays more than "
                "1 dB under; on the wet: under = the wet under the playing vs that tail (negative: held down), "
                "rise = how much the wet comes up after the stop (the tail minus its first 0.3 s). Stabs: pump = the "
                "wet's 50 ms level spread while playing (p90 - p10, dB). Click: the worst 1 ms frame of the highs from "
                "1 s on vs its second's median, dB. Every version is D (duck 2:1, floor -21).\n\n")
        for col, _, ms in COLS:
            for m in ms:
                d, _ = sf.read(dry / (m + ".wav"), always_2d=True)
                a, _ = sf.read(out / col / "{}_{}.wav".format(m, VERSIONS[0][0]), always_2d=True)
                st = stops(d)
                settle = 2.0 if m == "stopstart" else 3.0
                for tag, _ in VERSIONS:
                    x, _ = sf.read(out / col / "{}_{}.wav".format(m, tag), always_2d=True)
                    same = "" if tag == VERSIONS[0][0] else (
                        "  IDENTICAL to A" if np.array_equal(a, x) else "  null vs A {:+.1f} dB".format(
                            db(np.mean((a - x) ** 2) / np.mean(a ** 2))))
                    click = ld.worst_click(x, 1.0)
                    if m == "stabs":
                        v.write("{:20s} {:9s} {:22s} pump {:4.1f} dB  playing {:6.1f}  click {:4.1f}{}\n".format(
                            col, m, tag, pumping(x), ld.lv(x, 2, 12), click, same))
                        continue
                    hs = [hand_over(x, d, s, settle) for s in st]
                    v.write("{:20s} {:9s} {:22s} dip {:4.1f} dB  dur {:4.2f} s  under {:+5.1f}  rise {:+5.1f}  (stops {})  "
                            "click {:4.1f}{}\n".format(
                                col, m, tag, np.mean([h["dip"] for h in hs]), np.mean([h["dur"] for h in hs]),
                                np.mean([h["under"] for h in hs]), np.mean([h["rise"] for h in hs]),
                                ", ".join("{:.1f}".format(s) for s in st), click, same))
            v.write("\n")
    (out / "README.txt").write_text(README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "When the playing stops: gluing the reverb to the dry", "--no-level-match", "--dry", str(dry),
                    "--wet-only", "--chapters", CHAPTERS], check=True, stdout=subprocess.DEVNULL)
    return out / "index.html"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-ld" / "iv_render")
    ap.add_argument("--safety", action="store_true")
    ap.add_argument("--clicks-only", action="store_true")
    ap.add_argument("--built", action="store_true", help="v1.0.51+: E is built in; A turns it off")
    a = ap.parse_args()
    root = main_checkout()
    binary = a.render.resolve()
    if a.safety:
        vs = VERSIONS
        if a.built:
            vs = [("A_as_picked", dict(BASE, IV_DUCK_ADAPTIVE="0", IV_DUCK_SUSTAIN_CUT="0")), ("E_built", {})]
        print(ld.safety(binary, root, "clicks" if a.clicks_only else "all", safe=vs, clickv=vs))
    else:
        print(page(binary, root))


if __name__ == "__main__":
    main()
