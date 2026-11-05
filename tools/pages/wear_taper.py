#!/usr/bin/env python3
"""renders/wear_taper: how fast WEAR's wear arrives on the knob (a pick page, A/B/C/D).

lfad on Modwiggler (4 Nov 2026): WEAR is "very subtle until you get past noon, and then still relatively mild until you
max it out, at which point it becomes what I would normally want to hear around the noon mark". Full WEAR stays exactly
as the owner settled it in v1.0.66 (ADR 0041, tape_fold's B); only the path there changes (core/params/Mappings.h
eraMix, the Renderer's IV_WEAR_TAPER): the tape's drive and emphasis, the breathing, the in-loop compression, the hiss and
the crackle follow w(v) in place of v. The grit (the grain) stays on the knob itself from 0.65 (the owner's v1.0.49 pick
D) in A-C; D moves its start to 0.5.

  A_now             as built: w = v (the tape's drive 5.6 dB at noon, 11.25 at full)
  B_gentle          w = 1-(1-v)^1.6 (7.5 dB at noon)                                          IV_WEAR_TAPER=B
  C_stronger        w = 1-(1-v)^2.2 (8.8 dB at noon)                                          IV_WEAR_TAPER=C
  D_stronger_grit   C, and the grit from 0.5 in place of 0.65 (touches the owner's v1.0.49 pick) IV_WEAR_TAPER=D

Per card: the four versions level-matched to A (K-weighted loudness, a gain where one differs by more than 0.5 dB), and the
wear measured as the difference against the same setting at WEAR 0 (dB re that clean render), so "how much wear" reads as
a number. Usage: <venv>/bin/python tools/pages/wear_taper.py   (IV_RENDER: the prototype's iv_render, default build/;
WT_REF: main's iv_render, to prove A is main to the bit) -> renders/wear_taper in the main checkout.
The IV_WEAR_TAPER hook is gone since v1.0.69 (D is built, ADR 0044); rerun this page from commit 1cdc109.
"""
import hashlib
import os
import shutil
import subprocess
import sys

import numpy as np
import soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import motion_radio_checks as mc

REPO_ = os.path.abspath(os.path.join(HERE, "../.."))
MAIN = REPO_.split("/.claude/worktrees/")[0]  # pages land in the main checkout's renders/
R = os.environ.get("IV_RENDER", REPO_ + "/build/iv_render")
REF = os.environ.get("WT_REF")
PAGE = os.environ.get("WT_PAGE", MAIN + "/renders/wear_taper")
SRC = MAIN + "/renders/_material"
SR = 48000
MATERIALS = ["pad", "piano", "drums"]
VERS = [("A_now", {}), ("B_gentle", {"IV_WEAR_TAPER": "B"}), ("C_stronger", {"IV_WEAR_TAPER": "C"}),
        ("D_stronger_grit", {"IV_WEAR_TAPER": "D"})]
BASE = ["size=0.6", "time=0.6", "drift=0.5"]
# Knob clock positions: 7 o'clock = 0, 9 o'clock = 0.2, noon = 0.5, 3 o'clock = 0.8, 5 o'clock = 1.
SETTINGS = []
for i, (sp, n) in enumerate((("HALL", "hall"), ("PLATE", "plate"))):
    for j, (era, clock) in enumerate((("0.2", "9oclock"), ("0.5", "noon"), ("0.8", "3oclock"), ("1", "full"))):
        SETTINGS.append((f"{i * 4 + j + 1:02d}_{n}_wear_{clock}", [f"space={sp}", "decay=0.6", f"era={era}"]))
SETTINGS += [("09_hall_endless_wear_noon", ["space=HALL", "decay=0.9", "era=0.5"]),
             ("10_plate_endless_wear_noon", ["space=PLATE", "decay=0.9", "era=0.5"])]


def render(binary, src, out, sets, env_extra, sidecar=True):
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    env.update(env_extra)
    args = [binary, src, out, "--set", "mix=1"] + (["--sidecar"] if sidecar else [])
    for s in BASE + sets:
        args += ["--set", s]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    if p.returncode != 0:
        sys.exit("render failed: {}\n{}".format(" ".join(args), p.stderr))
    return p.stderr


def db(p):
    return 10 * np.log10(p + 1e-30)


def lufs(x):
    """BS.1770 K-weighted loudness (ungated), from the spectrum (era_rethink.py's)."""
    X = np.fft.rfft(x, axis=0)
    z = np.exp(-1j * np.linspace(0, np.pi, X.shape[0]))
    H = lambda b, a: (b[0] + b[1] * z + b[2] * z * z) / (1 + a[1] * z + a[2] * z * z)
    h = H([1.53512485958697, -2.69169618940638, 1.19839281085285], [1, -1.69065929318241, 0.73248077421585]) \
        * H([1, -2, 1], [1, -1.99004745483398, 0.99007225036621])
    p = np.sum(np.abs(X * h[:, None]) ** 2) / len(x) ** 2 * 2 / x.shape[1]
    return db(p) - 0.691


def sha(x):
    return hashlib.sha1(np.ascontiguousarray(x, dtype=np.float32).tobytes()).hexdigest()[:12]


def era0(sets):
    return [s for s in sets if not s.startswith("era=")] + ["era=0"]


if __name__ == "__main__":
    if os.path.isdir(PAGE):
        shutil.rmtree(PAGE)
    os.makedirs(PAGE)
    tmp = PAGE + "/_tmp.wav"
    lines = ["renders/wear_taper: verification (the page's renders, before any level match unless said).",
             "Per render: sha1 of the 32-bit samples; peak dBFS; loudness (K-weighted, ungated, LUFS); wear (the difference",
             "against the same setting at WEAR 0, dB re that clean render: higher = more wear); the difference against A",
             "(dB re A); the gain the page applies (when the loudness differs from A's by more than 0.5 dB); the worst 1 ms",
             "click frame (> 3 kHz, dB over its neighbours; music's own read up to ~15-20).", ""]
    hooks = set()
    summary = {}
    for col, sets in SETTINGS:
        os.makedirs(f"{PAGE}/{col}")
        lines.append(f"{col}  ({', '.join(sets)}; {', '.join(BASE)})")
        card_gain = False
        for m in MATERIALS:
            outs, gains = {}, {}
            render(R, f"{SRC}/{m}.wav", tmp, era0(sets), {}, sidecar=False)
            clean = sf.read(tmp, always_2d=True, dtype="float64")[0]
            pc = np.mean(clean ** 2)
            for tag, env in VERS:
                path = f"{PAGE}/{col}/{m}_{tag}.wav"
                err = render(R, f"{SRC}/{m}.wav", path, sets, env)
                hooks.add(" ".join(l for l in err.splitlines() if l.startswith("iv_render: IV_WEAR_TAPER")) or "(as built)")
                outs[tag] = sf.read(path, always_2d=True, dtype="float32")[0]
            a = outs["A_now"]
            if REF:
                render(REF, f"{SRC}/{m}.wav", tmp, sets, {}, sidecar=False)
                ref = sf.read(tmp, always_2d=True, dtype="float32")[0]
                lines.append(f"  {m:6s} A against main's renderer: " + ("BIT-IDENTICAL" if np.array_equal(ref, a) else "DIFFERS"))
            la = lufs(a.astype(np.float64))
            for tag, _ in VERS[1:]:
                d = la - lufs(outs[tag].astype(np.float64))
                gains[tag] = d if abs(d) > 0.5 else 0.0
                card_gain |= gains[tag] != 0.0
            for tag, _ in VERS:
                y = outs[tag].astype(np.float64)
                x = y * 10 ** (gains.get(tag, 0.0) / 20)
                sf.write(f"{PAGE}/{col}/{m}_{tag}.wav", x, SR, subtype="PCM_24")
                lv = lufs(y)
                wear = db(np.mean((y - clean) ** 2)) - db(pc)
                summary.setdefault((col, tag), []).append(wear)
                pk = 20 * np.log10(np.max(np.abs(y)) + 1e-30)
                dd = "" if tag == "A_now" else (" diff vs A IDENTICAL" if np.array_equal(outs[tag], a) else
                                                f" diff vs A {db(np.mean((y - a) ** 2)) - db(np.mean(a.astype(np.float64) ** 2)):+6.1f} dB")
                lines.append(f"  {m:6s} {tag:16s} sha1 {sha(outs[tag])}  peak {pk:6.1f}  {lv:6.1f} LUFS ({lv - la:+5.2f} re A)"
                             f"  wear {wear:+6.1f} dB  gain {gains.get(tag, 0.0):+5.2f}{dd}  click {mc.clicks(x):5.1f}")
            tags = [t for t, _ in VERS]
            same = [(p, q) for i, p in enumerate(tags) for q in tags[i + 1:] if np.array_equal(outs[p], outs[q])]
            lines.append(f"  {m:6s} " + ("every pair of versions differs" if not same else f"identical pairs: {same}"))
        lines.append(f"  level-matched: {'yes (a gain on at least one version)' if card_gain else 'no gain needed (all within 0.5 dB of A)'}")
        lines.append("")
        print(*lines[-(len(MATERIALS) * 5 + 3):], sep="\n", flush=True)
    if os.path.exists(tmp):
        os.unlink(tmp)
    lines.append("Wear per card, mean over the materials (dB re the WEAR 0 render):")
    for col, _ in SETTINGS:
        lines.append(f"  {col:30s} " + "  ".join(f"{t.split('_')[0]} {np.mean(summary[(col, t)]):+6.1f}" for t, _ in VERS))
    lines.append("")
    lines.append("renderer: " + R + (f"; reference: {REF}" if REF else ""))
    lines.append("hook lines: " + " / ".join(sorted(hooks)))
    open(f"{PAGE}/_verify.txt", "w").write("\n".join(lines) + "\n")
    shutil.copy(os.path.join(HERE, "wear_taper_README.txt"), PAGE + "/README.txt")
    subprocess.run([sys.executable, os.path.join(REPO_, "tools/review/make_review.py"), PAGE, "--title", "WEAR taper",
                    "--no-level-match", "--dry", SRC, "--wet-only"], check=True)
    print("Wrote", PAGE + "/index.html")
