#!/usr/bin/env python3
"""History (v1.0.68, ADR 0043: the owner picked B, now the Core's behaviour; the IV_PLATE_XFADE hook is gone, so this
script only re-runs against facbf40's renderer). PROTOTYPE pick page (3 Nov 2026; the owner: PLATE's SIZE should crossfade like HALL's and CANYON's, with no pitch
bend): PLATE's SIZE as built (A: the Dattorro plate's delays glide, so the tail bends like tape) against the crossfade
(B: once SIZE rests 50 ms the plate jumps to its new size and the old one fades out over 0.3 s; renderer hook
IV_PLATE_XFADE=B). C, the same crossfade over 1.2 s (IV_PLATE_XFADE=C), is left off the page: at endless DECAY it dipped
1-2.6 dB through the fade (B: within 0.5 dB), so it isn't a fair second choice yet.

Usage: <main checkout>/.venv/bin/python tools/pages/plate_size_xfade.py [--render build-proto/iv_render] [--out NAME]

Round 2 (renders/plate_size_xfade2, the default --out): the owner picked B on the slow turn, flicks, LFO and endless
moves, A on the frozen card, and heard a drop in B at 5.5 s (slow turn, stabs) and at the unfreeze (20 s, frozen
card). Both were small SIZE moves crossfaded (the knob's last step, 0.2 %): two reads a few samples apart summed as a
comb in the loop and the tail kept it (1.2-4 kHz down 2.8 dB after the unfreeze). Now a move within 4 % of the scale
glides slowly (about 5 cents), a larger one crossfades, and frozen (and its unfreeze) SIZE glides as built. A sixth
card: a full shrink at DECAY 0.3 (the +1.4-1.6 dB case). _verify.txt adds the two drops, as band levels against A.

Five cards on a pad and on stabs, PLATE at DECAY 0.7 unless said: a slow turn small -> large, quick flicks, a slow
LFO on SIZE's CV, SIZE moved while frozen (playing on into the layer, then Paint), SIZE moved at endless DECAY. Hand
turns go through release's knob path (a 10 ms one-pole, the 0.003 deadband, firmware/main.cpp); the LFO is CV (every
millisecond). Renders are 100 % wet (--wet-only --dry), level-matched, a pick page. Written to the main checkout's
renders/plate_size_xfade; _hooks.txt keeps every render's hook line; _verify.txt shows A is v1.0.67 to the bit (with
--ref, a v1.0.67 renderer) and measures B against A on every card (null, level, clicks in 1 ms frames).
"""
import argparse
import json
import math
import os
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from review_cpu_pages import main_checkout, trim, db  # noqa: E402

SECONDS = 26.0
MATERIALS = {"pad": (17, 9), "stabs": (17, 9)}
VERSIONS = [("A_glide_as_built", {"IV_PLATE_XFADE": "A"}), ("B_crossfade", {"IV_PLATE_XFADE": "B"})]


def hand(pts):
    """A hand's knob: smoothstep between (t, value) points."""
    def at(t):
        for (t0, v0), (t1, v1) in zip(pts, pts[1:]):
            if t <= t1:
                u = (t - t0) / (t1 - t0) if t > t0 else 0.0
                return v0 + (v1 - v0) * u * u * (3.0 - 2.0 * u)
        return pts[-1][1]
    return at


def knob_steps(raw, seconds=SECONDS):
    """Release's knob path (firmware/main.cpp): 1 ms blocks, a 10 ms one-pole, sent past the 0.003 deadband."""
    k, smooth, sent, steps = 1.0 / 11.0, None, None, []
    for b in range(int(seconds * 1000)):
        r = raw(b / 1000.0)
        smooth = r if smooth is None else smooth + k * (r - smooth)
        if sent is None or abs(smooth - sent) > 0.003:
            sent = smooth
            steps.append((b / 1000.0, smooth))
    bp, prev = [{"t": 0.0, "key": "size", "value": steps[0][1]}], steps[0][1]
    for t, v in steps[1:]:
        bp += [{"t": t - 1e-6, "key": "size", "value": prev}, {"t": t, "key": "size", "value": v}]
        prev = v
    return bp


def cv(fn, t0, seconds=SECONDS):
    return [{"t": 0.0, "key": "size", "value": fn(0.0)}] + [
        {"t": b / 1000.0, "key": "size", "value": fn(b / 1000.0)} for b in range(int(t0 * 1000), int(seconds * 1000))]


LFO = lambda t: 0.5 - 0.25 * math.cos(2 * math.pi * 0.15 * max(0.0, t - 3.0))
# (column, sets, automation, moments for the click check: (t0, t1))
COLS = [
    ("1_slow_turn_up", ["decay=0.7"],
     {"breakpoints": knob_steps(hand([(0, 0.15), (5.0, 0.15), (9.0, 0.9)]))}, [(5.0, 10.5)]),
    ("2_quick_flicks", ["decay=0.7"],
     {"breakpoints": knob_steps(hand([(0, 0.3), (5.0, 0.3), (5.12, 0.85), (9.0, 0.85), (9.12, 0.25), (12.0, 0.25),
                                       (12.1, 0.7), (12.5, 0.7), (12.6, 0.35)]))}, [(5.0, 7.0), (9.0, 11.0), (12.0, 14.5)]),
    ("3_slow_lfo_cv", ["decay=0.7"], {"breakpoints": cv(LFO, 3.0)}, [(3.0, 20.0)]),
    ("4_frozen_moves", ["decay=0.7"],
     {"breakpoints": knob_steps(hand([(0, 0.3), (8.0, 0.3), (8.15, 0.85), (11.0, 0.85), (11.15, 0.3)])),
      "buttons": [[5.0, 5.1], [13.0, 15.0], [20.0, 20.1]]}, [(8.0, 10.0), (11.0, 13.0), (20.0, 22.0)]),
    ("5_endless_moves", ["decay=1.0"],
     {"breakpoints": knob_steps(hand([(0, 0.3), (8.0, 0.3), (8.12, 0.85), (12.0, 0.85), (12.12, 0.2), (16.0, 0.2),
                                       (16.12, 0.6)]))}, [(8.0, 10.0), (12.0, 14.0), (16.0, 18.0)]),
    ("6_full_shrink", ["decay=0.3"],
     {"breakpoints": knob_steps(hand([(0, 1.0), (6.0, 1.0), (6.15, 0.0), (12.0, 0.0), (12.15, 1.0)]))},
     [(6.0, 8.0), (12.0, 14.0)]),
]
# The owner's two drops (round 1): (column, moment, seconds after it measured) for _verify.txt's band levels.
DROPS = [("1_slow_turn_up", 5.2, 1.5), ("4_frozen_moves", 20.0, 3.0)]
README = """PLATE's SIZE: should it glide or crossfade? Today, turning SIZE in PLATE slides the plate's delay lengths, so the tail bends in pitch a little, like tape (A). In HALL and CANYON SIZE doesn't bend: once the knob comes to rest, the space crossfades to its new size. B does the same for PLATE: once SIZE rests for 50 ms the plate jumps to its new size and the old one fades out over 0.3 s, with no pitch bend, no dip or swell in level and no restart of the tail.

Each card is PLATE at DECAY 0.7 (card 5: endless), on a pad and on stabs:
1 a slow turn from small to large (5-9 s);
2 quick flicks (5 s up, 9 s down, 12 s a quick there-and-back);
3 a slow LFO on SIZE's CV from 3 s (0.15 Hz): B moves only near the LFO's turning points, where it rests, as HALL does;
4 frozen at 5 s, the playing goes on into the layer, SIZE moved at 8 and 11 s (it sizes the layer), Paint held 13-15 s, unfrozen at 20 s;
5 endless DECAY, SIZE moved at 8, 12 and 16 s;
6 DECAY 0.3, SIZE flicked fully down at 6 s and back up at 12 s (round 1's loudest bump in B: +1.4-1.6 dB).

Round 2: B now glides small moves (within 4 % of the size, slowly enough not to bend: about 5 cents) and crossfades
larger ones; frozen, and while the unfreeze glides back, SIZE glides as in A. That's the fix for the drops heard at
5.5 s (card 1) and 20 s (card 4): a tiny move crossfaded dulled the tail's upper mids.

Listen for the pitch of the tail just after each move (A bends, B shouldn't), and whether B's fade sounds smooth. Level-matched; renders are 100 % wet, so use the MIX fader for the dry.

1_slow_turn_up      a slow turn, small to large
2_quick_flicks      quick flicks
3_slow_lfo_cv       a slow LFO on SIZE
4_frozen_moves      SIZE moved while frozen (the layer), then Paint
5_endless_moves     SIZE moved at endless DECAY
6_full_shrink       a full shrink and back at DECAY 0.3

A_glide_as_built    today: SIZE glides (the tail bends)
B_crossfade         SIZE crossfades over 0.3 s once it rests (no bend); small moves glide slowly; frozen, it glides"""


def render(binary, src, out, sets, env_extra, auto, hooks):
    out.parent.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    env.update(env_extra)
    args = [str(binary), str(src), str(out), "--set", "mix=1", "--set", "space=PLATE", "--sidecar", "--auto", str(auto)]
    for s in sets:
        args += ["--set", s]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    if p.returncode != 0:
        sys.exit("render failed: {}\n{}".format(" ".join(args), p.stderr))
    hooks.write("{}  env {}  ->  {}\n".format(out.name, env_extra or "{}",
                                              p.stderr.strip().replace("\n", " | ") or "(no hook line)"))
    return args, env


def hf_frames(x, sr, lo=5000.0):
    X = np.fft.rfft(x.mean(axis=1))
    f = np.fft.rfftfreq(x.shape[0], 1.0 / sr)
    X[f < lo] = 0
    h = np.fft.irfft(X, x.shape[0])
    n = sr // 1000
    m = len(h) // n
    return (h[:m * n].reshape(m, n) ** 2).mean(axis=1)


def band_levels(x, sr):
    X = np.abs(np.fft.rfft(x.mean(axis=1))) ** 2
    f = np.fft.rfftfreq(x.shape[0], 1.0 / sr)
    return np.array([db(X[(f >= lo) & (f < hi)].sum()) for lo, hi in ((0, 200), (200, 1000), (1000, 4000), (4000, 24000))])


def click_score(e, t0, t1):
    """The loudest 1 ms frame above 5 kHz against the median of the second around it, dB (compare versions)."""
    worst = -1e9
    for i in range(int(t0 * 1000), min(int(t1 * 1000), len(e))):
        worst = max(worst, db(e[i]) - db(np.median(e[max(0, i - 500):i + 500])))
    return worst


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--render", type=Path, default=ROOT / "build-proto" / "iv_render")
    ap.add_argument("--ref", type=Path, help="a v1.0.67 iv_render: A must equal it to the bit")
    ap.add_argument("--out", default="plate_size_xfade2", help="the page's folder under renders/")
    a = ap.parse_args()
    main_root = main_checkout()
    material = main_root / "renders" / "_material"
    out = main_root / "renders" / a.out
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    dry = main_root / "renders" / "_material_plate_size_xfade"
    for m, (play, tail) in MATERIALS.items():
        trim(material / (m + ".wav"), dry / (m + ".wav"), play, tail)
    binary = a.render.resolve()
    with open(out / "_hooks.txt", "w") as hooks:
        for col, sets, auto, _ in COLS:
            ap_ = out / "_auto_{}.json".format(col)
            ap_.write_text(json.dumps(auto))
            for tag, env in VERSIONS:
                for m in MATERIALS:
                    render(binary, dry / (m + ".wav"), out / col / "{}_{}.wav".format(m, tag), sets, env, ap_, hooks)
            if a.ref:  # v1.0.67 itself, for _verify.txt (then removed)
                for m in MATERIALS:
                    args = [str(a.ref.resolve()), str(dry / (m + ".wav")), str(main_root / "renders" / "_ref_plate_size_xfade" / "{}_{}.wav".format(col, m)),
                            "--set", "mix=1", "--set", "space=PLATE", "--auto", str(ap_)] + sum([["--set", s] for s in sets], [])
                    (main_root / "renders" / "_ref_plate_size_xfade").mkdir(exist_ok=True)
                    subprocess.run(args, env={k: v for k, v in os.environ.items() if not k.startswith("IV_")},
                                   capture_output=True, check=True)
    with open(out / "_verify.txt", "w") as v:
        v.write("plate_size_xfade: A (IV_PLATE_XFADE=A, the glide) against v1.0.67's renderer, and B (the crossfade) against A\n")
        v.write("click: the loudest 1 ms frame above 5 kHz over the median of the second around it, dB, at each move\n\n")
        for col, _, _, moments in COLS:
            for m in MATERIALS:
                pa = out / col / "{}_{}.wav".format(m, VERSIONS[0][0])
                pb = out / col / "{}_{}.wav".format(m, VERSIONS[1][0])
                xa, sr = sf.read(pa, always_2d=True, dtype="float32")
                xb, _ = sf.read(pb, always_2d=True, dtype="float32")
                if a.ref:
                    xr, _ = sf.read(main_root / "renders" / "_ref_plate_size_xfade" / "{}_{}.wav".format(col, m), always_2d=True, dtype="float32")
                    same = "A == v1.0.67 (bit-identical)" if xr.shape == xa.shape and np.array_equal(xr, xa) else "A DIFFERS from v1.0.67!"
                else:
                    same = "(no --ref)"
                n = min(len(xa), len(xb))
                differs = not np.array_equal(xa[:n], xb[:n])
                null = db(np.mean((xa[:n] - xb[:n]) ** 2) / np.mean(xa[:n] ** 2))
                level = db(np.mean(xb[:n] ** 2)) - db(np.mean(xa[:n] ** 2))
                ea, eb = hf_frames(xa, sr), hf_frames(xb, sr)
                clicks = ", ".join("{:.0f}-{:.0f}s A {:+.1f} B {:+.1f}".format(t0, t1, click_score(ea, t0, t1), click_score(eb, t0, t1))
                                   for t0, t1 in moments)
                v.write("{:17s} {:6s} {}; B {} A (null {:.1f} dB, level {:+.2f} dB); click {}\n".format(
                    col, m, same, "differs from" if differs else "IDENTICAL to (mislabelled!)", null, level, clicks))
        v.write("\nThe owner's drops (round 1), band levels B - A (dB) after the moment, <200 / 200-1k / 1-4k / >4k Hz:\n")
        for col, t, span in DROPS:
            for m in MATERIALS:
                xa, sr = sf.read(out / col / "{}_{}.wav".format(m, VERSIONS[0][0]), always_2d=True)
                xb, _ = sf.read(out / col / "{}_{}.wav".format(m, VERSIONS[1][0]), always_2d=True)
                i, j = int(t * sr), int((t + span) * sr)
                d = band_levels(xb[i:j], sr) - band_levels(xa[i:j], sr)
                v.write("  {:17s} {:6s} {:.1f}-{:.1f}s: {}\n".format(col, m, t, t + span, " ".join("{:+.1f}".format(x) for x in d)))
    if (main_root / "renders" / "_ref_plate_size_xfade").exists():
        shutil.rmtree(main_root / "renders" / "_ref_plate_size_xfade")
    (out / "README.txt").write_text(README + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title",
                    "PLATE's SIZE: glide or crossfade? (round 2)", "--level-match", "--dry", str(dry), "--wet-only"],
                   check=True, stdout=subprocess.DEVNULL)
    print(out / "index.html")


if __name__ == "__main__":
    main()
