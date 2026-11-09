#!/usr/bin/env python3
"""renders/size_blast: a fast SIZE move from big to small can blast the speaker (a pick page, A/B/C/D; issue 2).

The cause (docs/research/size-blast-2026-11-09.md): SIZE's crossfade boosts the new, nearer read by exp(lnGain) with no
limit. Right for a dying tail, real gain in the loop while you keep playing. The Renderer's IV_SIZE_BLAST:

  A_as_now         the boost as it is                                        IV_SIZE_BLAST=A
  B_limit_small    the boost limited (at LIMIT_DB[B]) after a shrink          IV_SIZE_BLAST=B
  C_limit_large    the boost limited (at LIMIT_DB[C]) after a shrink          IV_SIZE_BLAST=C
  D_limit_decay    C's limit, the correction's rate from DECAY's own loss     IV_SIZE_BLAST=D

B, C and D also restart a tank that goes invalid (Engine::process, the tanks' health check). A level-matched page would
hide the blast, so there's no level match; instead every file gets one fixed attenuation (ATTEN_DB). SIZE is turned the
way a hand turns it on the module: a smoothstep over the move's time through the release's knob path (firmware/main.cpp:
1 ms blocks, a 10 ms one-pole, the end stops, the 0.003 deadband).

Usage (from the worktree): <main>/.venv/bin/python tools/pages/size_blast.py  (IV_RENDER: the renderer, default
build-proto/iv_render) -> renders/size_blast in the main checkout; _hooks.txt keeps every render's hook line and
_verify.txt the checks. The hook is removed once a pick is built; rerun this from the prototype's commit.
"""
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
from review_cpu_pages import db, main_checkout, trim  # noqa: E402

SR = 48000
LIMIT_DB = {"B": 3.0, "C": 6.0}  # measured: see the README text below
VERS = [("A_as_now", "A"), ("B_limit_3dB", "B"), ("C_limit_6dB", "C"), ("D_limit_decay_only", "D")]
ATTEN_DB = -12.0
MOVE_T = 8.0  # the move's start, seconds into the render
TOTAL = 15.0

# name, material, the card's settings, SIZE from, to, the move's ms (a growth when to > from)
PLAYER = ["space=CANYON", "attack=BLOOM", "decay=0.85", "tone=0.5", "era=0.5", "drift=0.5", "time=0.5"]
CARDS = [
    ("1_player_canyon", "violin", PLAYER, 1.0, 0.05, 40),
    ("2_hall_bloom_short", "violin_soft", ["space=HALL", "attack=BLOOM", "decay=0.0", "tone=0.5", "era=0.5", "drift=0.5", "time=0.5"], 1.0, 0.05, 40),
    ("3_low_note_worst", "lownote", ["space=CANYON", "attack=NATURAL", "decay=0.1", "tone=0.8", "era=0.0", "drift=0.5", "time=0.0"], 1.0, 0.2, 200),
    ("4_tail_normal_move", "stabs_stop", ["space=CANYON", "attack=NATURAL", "decay=0.2", "tone=0.5", "era=0.5", "drift=0.5", "time=0.0"], 0.85, 0.55, 250),
    ("5_played_hall", "stabs", ["space=HALL", "attack=NATURAL", "decay=0.55", "tone=0.5", "era=0.1", "drift=0.5", "time=0.0"], 0.6, 0.35, 150),
    ("6_growth", "pad", ["space=CANYON", "attack=BLOOM", "decay=0.8", "tone=0.5", "era=0.5", "drift=0.5", "time=0.5"], 0.3, 0.9, 150),
]
SKIP = set(os.environ.get("SB_SKIP", "").split(","))
NOTES = {
    "1_player_canyon": "The player's situation: a violin-like line played hot into CANYON with BLOOM, DECAY about 3 o'clock, SIZE slammed from full to nearly nothing at 8 s. Listen just after 8 s: does the sound jump up at you for a second or two?",
    "2_hall_bloom_short": "The same fast shrink in HALL, BLOOM, DECAY all the way down, a violin-like line at a moderate level. This is where HALL jumps most.",
    "3_low_note_worst": "The worst the old version does anywhere: a steady low note into a short CANYON, a big fast shrink. B, C and D still rise here (measured: about +10 dB at most). That leftover isn't the boost at all (with it almost off the rise stays), it's the smaller room itself ringing up on a low note, so these versions can't remove it.",
    "4_tail_normal_move": "A normal move on a dying tail, the job the correction was built for (your 'some jumps in amplitude with the crossfades'): play, stop, turn SIZE from about 2 o'clock to 1 o'clock. Does the level still glide through the move? B changes here by up to about a dB; C and D are identical to A.",
    "5_played_hall": "A normal move while playing stabs in HALL: all four versions are identical here (nothing changes on moves like this).",
    "6_growth": "A growth, SIZE up from small to big while playing a pad. On a growth the correction only turns the level down, and that part stays as it is: all four versions are identical here.",
}


def violin(path, seconds):
    """The harness's violin-like material (size_blast_sb.cpp): bowed notes, vibrato, bow noise, peak 1.0."""
    n = int(seconds * SR)
    notes = [440.0, 587.33, 659.25, 392.0]
    t = np.arange(n) / SR
    rng = np.random.default_rng(1234)
    ph = 0.0
    x = np.zeros(n)
    for i in range(n):
        tt = t[i]
        k = int(tt // 2.5)
        nt = tt - k * 2.5
        f0 = notes[k % 4] * (1.0 + 0.006 * np.sin(2 * np.pi * 5.5 * tt))
        env = min(1.0, nt / 0.05) * min(1.0, max(0.0, (2.4 - nt) / 0.05))
        ph = (ph + f0 / SR) % 1.0
        h = 0.0
        for m in range(1, 17):
            a = (1.0 / m) * (1.0 + 1.5 * np.exp(-((m * f0 - 2800.0) ** 2) / 1.5e6))
            h += a * np.sin(2 * np.pi * m * ph)
        x[i] = env * (0.55 * h + 0.015 * rng.uniform(-1, 1))
    x /= np.max(np.abs(x))
    sf.write(path, np.stack([x, x], axis=1), SR, subtype="PCM_24")


def end_stops(raw):
    v = (raw - 0.025) / (1.0 - 2 * 0.025)
    return max(0.0, min(1.0, v))


def knob_auto(path, s0, s1, ms, seconds):
    """Release's knob path for a turn from s0 to s1 over ms at MOVE_T: a step at each send."""
    k, smooth, sent, steps = 1.0 / 11.0, None, None, []
    for b in range(int(seconds * 1000)):
        t = b / 1000.0
        u = max(0.0, min(1.0, (t - MOVE_T) / (ms / 1000.0)))
        raw = s0 + (s1 - s0) * (u * u * (3.0 - 2.0 * u))
        smooth = raw if smooth is None else smooth + k * (raw - smooth)
        v = end_stops(smooth)
        if sent is None or abs(v - sent) > 0.003 or (v in (0.0, 1.0) and v != sent):
            sent = v
            steps.append((t, v))
    bp, prev = [{"t": 0.0, "key": "size", "value": steps[0][1]}], steps[0][1]
    for t, v in steps[1:]:
        bp += [{"t": max(0.0, t - 1e-6), "key": "size", "value": prev}, {"t": t, "key": "size", "value": v}]
        prev = v
    path.write_text(json.dumps({"breakpoints": bp}))
    return steps[0][1]


def render(binary, src, out, sets, ver, auto, hooks):
    out.parent.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    env["IV_SIZE_BLAST"] = ver
    if ver in LIMIT_DB:
        env["IV_SIZE_BLAST_DB"] = str(LIMIT_DB[ver])
    elif ver == "D":
        env["IV_SIZE_BLAST_DB"] = str(LIMIT_DB["C"])
    args = [str(binary), str(src), str(out), "--set", "mix=1", "--sidecar", "--auto", str(auto)]
    for s in sets:
        args += ["--set", s]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    if p.returncode != 0:
        sys.exit("render failed: {}\n{}".format(" ".join(args), p.stderr))
    hooks.write("{}/{}  env IV_SIZE_BLAST={} -> {}\n".format(out.parent.name, out.name, ver, p.stderr.strip().replace("\n", " | ")))


def attenuate(path, gain_db):
    info = sf.info(str(path))
    x, sr = sf.read(str(path), always_2d=True)
    sf.write(str(path), x * 10 ** (gain_db / 20.0), sr, subtype=info.subtype)


def r50(x, sr, t0, t1):
    n, hop = int(0.05 * sr), int(0.025 * sr)
    out = []
    i = int(t0 * sr)
    z = min(int(t1 * sr), x.shape[0])
    while i + n <= z:
        out.append(10 * np.log10(np.mean(x[i:i + n] ** 2) + 1e-20))
        i += hop
    return np.array(out)


def hf_frames(x, sr, frame_s=0.001, lo=5000.0):
    X = np.fft.rfft(x.mean(axis=1))
    f = np.fft.rfftfreq(x.shape[0], 1.0 / sr)
    X[f < lo] = 0
    h = np.fft.irfft(X, x.shape[0])
    n = int(frame_s * sr)
    m = len(h) // n
    return (h[:m * n].reshape(m, n) ** 2).mean(axis=1), frame_s


def click_score(e, fs, t0, t1):
    i0, i1 = int(t0 / fs), int(t1 / fs)
    worst = -1e9
    for i in range(i0, min(i1, len(e))):
        j0, j1 = max(0, i - int(0.5 / fs)), i + int(0.5 / fs)
        worst = max(worst, db(e[i]) - db(np.median(e[j0:j1])))
    return worst


def verify_card(card_dir):
    """Per version vs A: identical?, the level rise over the move, the 1 ms HF peak over its median at the move."""
    lines, a = [], {}
    for tag, ver in VERS:
        f = card_dir / "{}_{}.wav".format(card_dir.material, tag)
        x, sr = sf.read(f, always_2d=True)
        a[ver] = x
    n = min(len(v) for v in a.values())
    base = a["A"][:n]
    for tag, ver in VERS:
        x = a[ver][:n]
        ident = "IDENTICAL to A" if ver != "A" and np.array_equal(x, base) else ""
        pre = r50(x, SR, MOVE_T - 3.0, MOVE_T)
        post = r50(x, SR, MOVE_T, MOVE_T + 5.0)
        e, fs = hf_frames(x, SR)
        click = click_score(e, fs, MOVE_T - 0.1, MOVE_T + 2.5)
        pk = 20 * np.log10(np.max(np.abs(x)) + 1e-12)
        diff = "null vs A {:.1f} dB".format(db(np.mean((x - base) ** 2) / (np.mean(base ** 2) + 1e-20))) if ver != "A" else "(reference)"
        lines.append("{:18s} {} {:s} pre {:6.1f} post-peak {:6.1f} (rise {:+.1f} dB, peak {:.1f} dBFS before the fixed {} dB), 1 ms HF peak/median {:+.1f} dB; {}".format(tag, "", ident, np.max(pre), np.max(post), np.max(post) - np.max(pre), pk, ATTEN_DB, click, diff))
    return lines


def readme():
    return """SIZE: stopping the blast. A fast SIZE move from big to small can shoot the level up for a second or two, because SIZE's crossfade turns the new, nearer sound up to make up for level it thinks it skipped. That's right for a tail that's dying away, and wrong while you keep playing: the player on the forum's violin into CANYON and BLOOM jumped up to the limiter.

A_as_now             v1.0.76 as it is.
B_limit_3dB          the boost limited to {b:.0f} dB after a shrink.
C_limit_6dB          the boost limited to {c:.0f} dB after a shrink.
D_limit_decay_only   C's limit, and the boost worked out from the DECAY knob's own loss, leaving out the extra loss when the reverb's being driven hot.

B, C and D also have a safety: if a tank's maths ever goes invalid (an extreme SIZE move could turn the wet silent until a restart), the reverb restarts itself cleanly instead. You won't hear that on this page; it's measured separately.

No level matching on this page: the jump in level is the point, so a louder B or C is real. To keep A's blast from startling you, every file on the page is turned down by the same fixed {a:.0f} dB. Turn your monitors up to a comfortable level on card 1 and leave them there.

The move happens at 8 seconds on every card. Cards 1 and 2 are the player's situation (a violin-like line, CANYON and HALL with BLOOM, a big fast shrink), 3 is the worst the old version does (a steady low note), 4 and 5 are normal moves (a dying tail, and playing), and 6 is a growth. The last three should be identical or nearly so, and they say so below.

Things to compare: on 1 to 3, how much the level still jumps in B, C and D. On 4, whether B's dip on the dying tail bothers you. On 5 and 6 you should hear no difference at all.

{cards}""".format(cards="\n".join("{}   {}".format(k, v) for k, v in NOTES.items() if k not in SKIP), b=LIMIT_DB["B"], c=LIMIT_DB["C"], a=ATTEN_DB)


class Card:
    def __init__(self, base, material):
        self.base, self.material = base, material

    def __truediv__(self, o):
        return self.base / o


def main():
    main_root = main_checkout()
    binary = Path(os.environ.get("IV_RENDER", ROOT / "build-proto" / "iv_render")).resolve()
    out = main_root / "renders" / "size_blast"
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    src = main_root / "renders" / "_material_size_blast"
    src.mkdir(parents=True, exist_ok=True)
    material = main_root / "renders" / "_material"
    if not (src / "violin.wav").exists():
        violin(src / "violin.wav", TOTAL)
    for m in ("stabs", "pad"):
        trim(material / (m + ".wav"), src / (m + ".wav"), TOTAL, 0.0)
    x, _ = sf.read(src / "violin.wav", always_2d=True)
    sf.write(src / "violin_soft.wav", 0.3 * x, SR, subtype="PCM_24")  # peak 0.3
    n = int(TOTAL * SR)
    y = 0.3 * np.sin(2 * np.pi * 70.0 * np.arange(n) / SR)  # the harness's worst row: 70 Hz at 0.3
    sf.write(src / "lownote.wav", np.stack([y, y], axis=1), SR, subtype="PCM_24")
    trim(material / "stabs.wav", src / "stabs_stop.wav", 7.0, TOTAL - 7.0)
    with open(out / "_hooks.txt", "w") as hooks:
        for name, mat, sets, s0, s1, ms in CARDS:
            if name in SKIP:
                continue
            auto = out / "_auto_{}.json".format(name)
            start = knob_auto(auto, s0, s1, ms, TOTAL)
            sets2 = sets + ["size={:.4f}".format(start)]
            for tag, ver in VERS:
                render(binary, src / (mat + ".wav"), out / name / "{}_{}.wav".format(mat, tag), sets2, ver, auto, hooks)
    with open(out / "_verify.txt", "w") as v:
        v.write("Each version on every card (size_blast). Move at {} s. Attenuation {} dB applied after these numbers are taken (the raw render is measured).\n\n".format(MOVE_T, ATTEN_DB))
        for name, mat, _s, _a, _b, _c in CARDS:
            if name in SKIP:
                continue
            v.write(name + "\n")
            for ln in verify_card(Card(out / name, mat)):
                v.write("  " + ln + "\n")
    for name, mat, _s, _a, _b, _c in CARDS:
        if name in SKIP:
            continue
        for tag, _ver in VERS:
            attenuate(out / name / "{}_{}.wav".format(mat, tag), ATTEN_DB)
    (out / "README.txt").write_text(readme() + "\n")
    (HERE / "size_blast_README.txt").write_text(readme() + "\n")
    subprocess.run([sys.executable, str(ROOT / "tools/review/make_review.py"), str(out), "--title", "SIZE: stopping the blast", "--no-level-match"], check=True, stdout=subprocess.DEVNULL)
    print(out / "index.html")


if __name__ == "__main__":
    main()
