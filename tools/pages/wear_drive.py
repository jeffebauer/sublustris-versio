#!/usr/bin/env python3
"""renders/wear_drive: how hard the tape is driven on v1.0.69's taper (a pick page, A/B/C/D).

The owner (5 Nov 2026): "increase the saturation ceiling as well, returning it to the strength it was before we reduced
it", i.e. tex::kTapeDriveDb back to v1.0.65's 16.25 dB (ADR 0040 -> 0041). On v1.0.69's taper the drive in dB is
kTapeDriveDb x w, w = 1 - (1 - WEAR)^2.2, so 16.25 puts noon at 12.7 dB and 3 o'clock at 15.8, where v1.0.65's 13 dB
at 3 o'clock was "folding back too much" (ADR 0041). The Renderer's IV_WEAR_DRIVE (dsp/EraTexture.h):

  A_now          v1.0.69 as built: 11.25 dB at full (8.8 at noon, 10.9 at 3 o'clock)
  B_13_75        13.75 dB at full (10.8 at noon, 13.4 at 3 o'clock)                                IV_WEAR_DRIVE=B
  C_16_25        16.25 dB at full, v1.0.65's (12.7 at noon, 15.8 at 3 o'clock)                     IV_WEAR_DRIVE=C
  D_far_end      as built to WEAR 0.75, then up to 16.25 dB at full on a smoothstep (8.8 at noon, 11.4 at 3 o'clock)
                                                                                                   IV_WEAR_DRIVE=D

Per card: the four versions level-matched to A where one differs by more than 0.5 dB (K-weighted loudness); on the pad,
the clip's products re the tape's output (wear_drive_lib.products, ADR 0041's measure: the tape alone in numpy on the
card's WEAR-0 wet, 8x oversampled) and the tail's crest. Usage: <venv>/bin/python tools/pages/wear_drive.py
(IV_RENDER: the prototype's iv_render, default build/) -> renders/wear_drive in the main checkout.
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
import wear_drive_lib as L

REPO_ = os.path.abspath(os.path.join(HERE, "../.."))
MAIN = REPO_.split("/.claude/worktrees/")[0]
R = os.environ.get("IV_RENDER", REPO_ + "/build/iv_render")
PAGE = os.environ.get("WD_PAGE", MAIN + "/renders/wear_drive")
SRC = MAIN + "/renders/_material"
SR = 48000
MATERIALS = [m for m in ("pad", "stabs", "piano", "drums") if os.path.exists(f"{SRC}/{m}.wav")]
VERS = [("A_now", "A"), ("B_13_75", "B"), ("C_16_25", "C"), ("D_far_end", "D")]
BASE = ["size=0.6", "time=0.6", "drift=0.5"]
SETTINGS = [  # knob clock positions: noon = 0.5, 3 o'clock = 0.8, 5 o'clock = 1
    ("1_hall_wear_noon", "HALL", 0.6, 0.5),
    ("2_hall_wear_3oclock", "HALL", 0.6, 0.8),
    ("3_hall_wear_full", "HALL", 0.6, 1.0),
    ("4_canyon_wear_noon", "CANYON", 0.6, 0.5),
    ("5_canyon_wear_3oclock", "CANYON", 0.6, 0.8),
    ("6_canyon_wear_full", "CANYON", 0.6, 1.0),
    ("7_canyon_endless_wear_full", "CANYON", 0.9, 1.0),
]


def render(src, out, sets, ver, sidecar=True):
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    if ver != "A":
        env["IV_WEAR_DRIVE"] = ver
    args = [R, src, out, "--set", "mix=1"] + (["--sidecar"] if sidecar else [])
    for s in BASE + sets:
        args += ["--set", s]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    if p.returncode != 0:
        sys.exit("render failed: {}\n{}".format(" ".join(args), p.stderr))
    return p.stderr


def sha(x):
    return hashlib.sha1(np.ascontiguousarray(x, dtype=np.float32).tobytes()).hexdigest()[:12]


if __name__ == "__main__":
    if os.path.isdir(PAGE):
        shutil.rmtree(PAGE)
    os.makedirs(PAGE)
    tmp = PAGE + "/_tmp.wav"
    lines = ["renders/wear_drive: verification (the page's renders, before any level match unless said).",
             "Per render: sha1 of the 32-bit samples; peak dBFS; loudness (K-weighted, ungated, LUFS); crest (99.99th-percentile",
             "peak over RMS, dB, mono); the difference against A (dB re A); the gain the page applies (when the loudness differs",
             "from A's by more than 0.5 dB); the worst 1 ms click frame (> 3 kHz, dB over its neighbours; music's own reads up",
             "to ~15-20). Per card, on the pad: the tape's drive (dB) and the clip's products re the tape's output (dB; the",
             "tape alone in numpy on the card's WEAR-0 wet, 8x oversampled: ADR 0041's measure).", ""]
    hooks, table = set(), []
    for col, space, decay, era in SETTINGS:
        sets = [f"space={space}", f"decay={decay}", f"era={era}"]
        os.makedirs(f"{PAGE}/{col}")
        lines.append(f"{col}  ({', '.join(sets)}; {', '.join(BASE)})")
        render(f"{SRC}/pad.wav", tmp, [s for s in sets if not s.startswith("era=")] + ["era=0"], "A", sidecar=False)
        x0 = sf.read(tmp, always_2d=True, dtype="float64")[0][:, 0]
        prod = {v: L.products(x0, L.w_of(era), L.drive_db(v, era)) for _, v in VERS}
        lines.append("  pad, the clip: " + "  ".join(f"{v} drive {L.drive_db(v, era):5.2f} dB products {prod[v]:+6.1f} dB" for _, v in VERS))
        card_gains = []
        padcrest = {}
        for m in MATERIALS:
            outs, gains = {}, {}
            for tag, v in VERS:
                path = f"{PAGE}/{col}/{m}_{tag}.wav"
                err = render(f"{SRC}/{m}.wav", path, sets, v)
                hooks.add(" ".join(l for l in err.splitlines() if l.startswith("iv_render: IV_WEAR_DRIVE")) or "(as built)")
                outs[tag] = sf.read(path, always_2d=True, dtype="float32")[0]
            a = outs["A_now"]
            la = L.lufs(a.astype(np.float64))
            for tag, _ in VERS[1:]:
                d = la - L.lufs(outs[tag].astype(np.float64))
                gains[tag] = d if abs(d) > 0.5 else 0.0
                if gains[tag]:
                    card_gains.append(f"{m} {tag} {gains[tag]:+.2f} dB")
            for tag, _ in VERS:
                y = outs[tag].astype(np.float64)
                x = y * 10 ** (gains.get(tag, 0.0) / 20)
                sf.write(f"{PAGE}/{col}/{m}_{tag}.wav", x, SR, subtype="PCM_24")
                lv, cr = L.lufs(y), L.crest(y)
                if m == "pad":
                    padcrest[tag] = cr
                pk = 20 * np.log10(np.max(np.abs(y)) + 1e-30)
                dd = "" if tag == "A_now" else (" diff vs A IDENTICAL" if np.array_equal(outs[tag], a) else
                                                f" diff vs A {L.db(np.mean((y - a) ** 2)) - L.db(np.mean(a.astype(np.float64) ** 2)):+6.1f} dB")
                lines.append(f"  {m:6s} {tag:10s} sha1 {sha(outs[tag])}  peak {pk:6.1f}  {lv:6.1f} LUFS ({lv - la:+5.2f} re A)  crest {cr:5.1f}"
                             f"  gain {gains.get(tag, 0.0):+5.2f}{dd}  click {mc.clicks(x):5.1f}")
            tags = [t for t, _ in VERS]
            same = [(p, q) for i, p in enumerate(tags) for q in tags[i + 1:] if np.array_equal(outs[p], outs[q])]
            lines.append(f"  {m:6s} " + ("every pair of versions differs" if not same else f"identical pairs: {same}"))
        lines.append("  level-matched: " + ("; ".join(card_gains) if card_gains else "no gain needed (all within 0.5 dB of A)"))
        lines.append("")
        table.append((col, {v: (L.drive_db(v, era), prod[v], padcrest[t]) for t, v in VERS}))
        print(*lines[-(len(MATERIALS) * 5 + 4):], sep="\n", flush=True)
    if os.path.exists(tmp):
        os.unlink(tmp)
    lines.append("Summary on the pad: drive dB / the clip's products dB re output / the tail's crest dB")
    for col, d in table:
        lines.append(f"  {col:28s} " + "  ".join(f"{v} {a:5.2f} {b:+6.1f} {c:5.1f}" for v, (a, b, c) in d.items()))
    lines.append("")
    lines.append("renderer: " + R)
    lines.append("hook lines: " + " / ".join(sorted(hooks)))
    open(f"{PAGE}/_verify.txt", "w").write("\n".join(lines) + "\n")
    shutil.copy(os.path.join(HERE, "wear_drive_README.txt"), PAGE + "/README.txt")
    subprocess.run([sys.executable, os.path.join(REPO_, "tools/review/make_review.py"), PAGE, "--title", "WEAR drive",
                    "--no-level-match", "--dry", SRC, "--wet-only"], check=True)
    print("Wrote", PAGE + "/index.html")
