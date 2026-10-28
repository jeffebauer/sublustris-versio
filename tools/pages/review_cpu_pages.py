#!/usr/bin/env python3
# Renders at its original commit: build iv_render at d74c785 (tape_os: IV_TAPE_OS is ignored since v1.0.42; still_lows_rate still renders today).
"""PROTOTYPE pick pages from the overnight review (26 Oct 2026): two CPU savings that change the sound a
little, for the owner to hear before deciding.

Usage: <main checkout>/.venv/bin/python tools/pages/review_cpu_pages.py tape_os|still_lows_rate [...]
           [--render build-agent/iv_render] [--material <main checkout>/renders/_material]

  tape_os          ERA's tape out (the cassette) with its curve at 4x (as built) or 2x (renderer hook
                   IV_TAPE_OS=2): ~4-6 % of the module's CPU while the cassette is in, and ~800 B of flash.
  still_lows_rate  The still lows' own tank at 1/4 of the rate (12 kHz, as built) or 1/8 (6 kHz, hook
                   IV_LOW_DECIMATE=8): ~5-10 % of the module's CPU in HALL and MASSIVE.

Each page: renders 100 % wet (the page's MIX fader blends the dry in: --wet-only --dry), level-matched,
a pick page (no --showcase) with make_review's hidden twin (one row repeated elsewhere). The material is
the main checkout's (renders/_material), trimmed here to ~12 s of playing plus a tail so each page stays
small; the trimmed copies go to renders/_material_review (the page's dry). Every render's stderr line from
the hook is kept in renders/<page>/_hooks.txt, and renders/<page>/_verify.txt measures A against B in
every card (the memory "verify voicing labels": a page whose A and B don't differ is mislabelled).
"""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[2]


def main_checkout() -> Path:
    common = subprocess.run(["git", "rev-parse", "--path-format=absolute", "--git-common-dir"], cwd=ROOT,
                            capture_output=True, text=True, check=True).stdout.strip()
    return Path(common).parent


def trim(src: Path, dst: Path, play_s: float, tail_s: float):
    """The first play_s seconds of the material (a 80 ms fade at the cut), then tail_s of silence."""
    if dst.exists():
        return
    x, sr = sf.read(src, always_2d=True)
    n = int(play_s * sr)
    x = x[:n].copy()
    f = int(0.08 * sr)
    x[-f:] *= np.linspace(1.0, 0.0, f)[:, None]
    x = np.concatenate([x, np.zeros((int(tail_s * sr), x.shape[1]))])
    dst.parent.mkdir(parents=True, exist_ok=True)
    sf.write(dst, x, sr, subtype="PCM_24")


def render(binary: Path, src: Path, out: Path, sets: list, env_extra: dict, hooks_log):
    out.parent.mkdir(parents=True, exist_ok=True)
    env = {k: v for k, v in os.environ.items() if not k.startswith("IV_")}
    env.update(env_extra)
    args = [str(binary), str(src), str(out), "--set", "mix=1", "--sidecar"]
    for s in sets:
        args += ["--set", s]
    p = subprocess.run(args, env=env, capture_output=True, text=True)
    if p.returncode != 0:
        sys.exit("render failed: {}\n{}".format(" ".join(args), p.stderr))
    hooks_log.write("{}  env {}  ->  {}\n".format(out.relative_to(ROOT), env_extra or "{}",
                                                  p.stderr.strip().replace("\n", " | ") or "(no hook line)"))


def bandpass(x, sr, lo, hi):
    X = np.fft.rfft(x, axis=0)
    f = np.fft.rfftfreq(x.shape[0], 1.0 / sr)
    X[(f < lo) | (f >= hi)] = 0
    return np.fft.irfft(X, x.shape[0], axis=0)


def db(v):
    return 10.0 * np.log10(v + 1e-30)


def third_octaves(x, sr):
    X = np.abs(np.fft.rfft(x.mean(axis=1))) ** 2
    f = np.fft.rfftfreq(x.shape[0], 1.0 / sr)
    out = []
    fc = 31.25
    while fc < 20000:
        sel = (f >= fc / 2 ** (1 / 6)) & (f < fc * 2 ** (1 / 6))
        out.append((fc, db(X[sel].sum())))
        fc *= 2 ** (1 / 3)
    return out


def verify_pair(a_path: Path, b_path: Path, kind: str) -> str:
    a, sr = sf.read(a_path, always_2d=True)
    b, _ = sf.read(b_path, always_2d=True)
    n = min(len(a), len(b))
    a, b = a[:n], b[:n]
    if np.array_equal(a, b):
        return "IDENTICAL (mislabelled!)"
    pa = np.mean(a ** 2)
    null = db(np.mean((a - b) ** 2) / pa)
    ta, tb = third_octaves(a, sr), third_octaves(b, sr)
    loud = [i for i, (fc, v) in enumerate(ta) if v > max(t[1] for t in ta) - 50]
    worst = max(loud, key=lambda i: abs(tb[i][1] - ta[i][1]))
    s = "null {:.1f} dB re A; max third-octave change {:+.2f} dB at {:.0f} Hz".format(
        null, tb[worst][1] - ta[worst][1], ta[worst][0])
    if kind == "tape":
        top_a = db(np.mean(bandpass(a, sr, 12000, 20000) ** 2))
        top_b = db(np.mean(bandpass(b, sr, 12000, 20000) ** 2))
        s += "; 12-20 kHz B-A {:+.2f} dB".format(top_b - top_a)
    else:
        for lo, hi in ((20, 300), (2000, 8000)):
            la, lb = bandpass(a, sr, lo, hi), bandpass(b, sr, lo, hi)
            s += "; {}-{} Hz: null {:.1f} dB re that band, level B-A {:+.2f} dB".format(
                lo, hi, db(np.mean((la - lb) ** 2) / (np.mean(la ** 2) + 1e-30)), db(np.mean(lb ** 2)) - db(np.mean(la ** 2)))
    return s


PAGES = {
    "tape_os": dict(
        title="ERA's cassette: tape at 4x or 2x",
        materials={"drums": (12, 8), "lead": (12, 8), "skank": (12, 8), "pad": (12, 10)},
        cols=[("1_plate_cassette", ["space=PLATE", "era=0.7", "decay=0.6", "size=0.6", "attack=NORMAL", "time=0"]),
              ("2_hall_cassette", ["space=HALL", "era=0.7", "decay=0.6", "size=0.6", "attack=NORMAL", "time=0"])],
        versions=[("A_4x_as_built", {}), ("B_2x", {"IV_TAPE_OS": "2"})],
        kind="tape",
        readme="""ERA's cassette: the tape at 4x or 2x. When ERA is at the cassette, the wet goes through a tape stage (drive, a little hysteresis, the soft curve). That curve makes overtones, and overtones above the top of the audio band fold back down as a fizz unless the curve runs at a higher rate: today it runs at 4x. At 2x it costs about half (roughly 4 to 6 % of the module's CPU while the cassette is in) and frees ~800 bytes of flash. Measured: every third-octave band of a cassette render within about 0.2 dB, but with very hot, very bright material (the top octave) 2x lets a little more fizz through, most in PLATE, whose wet reaches the top octave. Can you hear a difference? If not, B saves the CPU. ERA at 0.7 (the cassette fully in; 0.8 renders exactly the same). DECAY 0.6, SIZE 0.6, ATTACK NORMAL. Level-matched; renders are 100 % wet, so use the MIX fader for the dry.

1_plate_cassette   PLATE, ERA at the cassette
2_hall_cassette    HALL, ERA at the cassette

A_4x_as_built      the tape at 4x (today)
B_2x               the tape at 2x (saves ~4-6 % CPU with the cassette in)""",
    ),
    "still_lows_rate": dict(
        title="Still lows: their tank at 12 kHz or 6 kHz",
        materials={"bass": (12, 10), "voicelow": (11, 10), "pad": (12, 10)},
        cols=[("1_hall_size06_decay06", ["space=HALL", "size=0.6", "decay=0.6", "drift=1", "era=0", "attack=NORMAL"]),
              ("2_hall_size1_infinite", ["space=HALL", "size=1", "decay=1", "drift=1", "era=0", "attack=NORMAL"]),
              ("3_massive_size06_decay06", ["space=MASSIVE", "size=0.6", "decay=0.6", "drift=1", "era=0", "attack=NORMAL"]),
              ("4_massive_size1_infinite", ["space=MASSIVE", "size=1", "decay=1", "drift=1", "era=0", "attack=NORMAL"])],
        versions=[("A_12kHz_as_built", {}), ("B_6kHz", {"IV_LOW_DECIMATE": "8"})],
        kind="lows",
        readme="""Still lows: their own tank at 12 kHz or 6 kHz. In HALL and MASSIVE the bass below ~300 Hz rings in its own still reverb (your pick on still_lows). That tank runs at 12 kHz, far more than a 300 Hz band needs; at 6 kHz it does half the work: roughly 5 to 10 % of the module's CPU. The catch: its eight lines snap to a different grid of lengths, so the bass's resonances land on slightly different notes (like a slightly different room): the overall bass level stays within about 1 dB, but a single bass note can sit a few dB louder or quieter (up to ~4 dB on the pad here). Measured on a click, its bass decay times stay within ~2 %; infinite holds hold the same. Does the bass sound as good? DRIFT fully CW, where the still lows matter most; ERA clean. Level-matched; renders are 100 % wet, so use the MIX fader for the dry.

1_hall_size06_decay06      HALL, SIZE 0.6, DECAY 0.6
2_hall_size1_infinite      HALL, SIZE full, DECAY full (infinite)
3_massive_size06_decay06   MASSIVE, SIZE 0.6, DECAY 0.6
4_massive_size1_infinite   MASSIVE, SIZE full, DECAY full (infinite)

A_12kHz_as_built   the still lows' tank at 12 kHz (today)
B_6kHz             at 6 kHz (saves ~5-10 % CPU)""",
    ),
}


def build(page: str, binary: Path, material: Path, py: Path):
    spec = PAGES[page]
    out = ROOT / "renders" / page
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    dry = ROOT / "renders" / "_material_review"
    for m, (play, tail) in spec["materials"].items():
        trim(material / (m + ".wav"), dry / (m + ".wav"), play, tail)
    click = dry / "_click.wav"
    trim(material / "_click.wav", click, 40, 0)
    with open(out / "_hooks.txt", "w") as hooks:
        for col, sets in spec["cols"]:
            for tag, env in spec["versions"]:
                for m in spec["materials"]:
                    render(binary, dry / (m + ".wav"), out / col / "{}_{}.wav".format(m, tag), sets, env, hooks)
                render(binary, click, out / "_measure" / col / "{}.wav".format(tag), sets, env, hooks)
    with open(out / "_verify.txt", "w") as v:
        v.write("A vs B in every card ({}):\n".format(page))
        for col, _ in spec["cols"]:
            for m in list(spec["materials"]) + ["_click"]:
                a_tag, b_tag = spec["versions"][0][0], spec["versions"][1][0]
                if m == "_click":
                    a, b = out / "_measure" / col / (a_tag + ".wav"), out / "_measure" / col / (b_tag + ".wav")
                else:
                    a, b = out / col / "{}_{}.wav".format(m, a_tag), out / col / "{}_{}.wav".format(m, b_tag)
                v.write("{:28s} {:9s} {}\n".format(col, m, verify_pair(a, b, spec["kind"])))
    (out / "README.txt").write_text(spec["readme"] + "\n")
    subprocess.run([str(py), str(ROOT / "tools/review/make_review.py"), str(out), "--title", spec["title"],
                    "--level-match", "--dry", str(dry), "--wet-only"], check=True, stdout=subprocess.DEVNULL)
    print(out / "index.html")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("pages", nargs="+", choices=sorted(PAGES))
    ap.add_argument("--render", type=Path, default=ROOT / "build-agent" / "iv_render")
    ap.add_argument("--material", type=Path, default=None, help="default: <main checkout>/renders/_material")
    a = ap.parse_args()
    main_root = main_checkout()
    material = a.material or main_root / "renders" / "_material"
    py = Path(sys.executable)
    for p in a.pages:
        build(p, a.render.resolve(), material, py)


if __name__ == "__main__":
    main()
