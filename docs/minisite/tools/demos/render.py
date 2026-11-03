#!/usr/bin/env python3
"""Site demos: render each clip's dry/wet pair, match loudness, encode, write demos.json.

Usage (repo root): .venv/bin/python docs/minisite/tools/demos/render.py <src dir> <work dir> <audio out dir> [clip ids...]
Needs numpy, soundfile, ffmpeg (libmp3lame) and the Renderer (build/iv_render, or $IV_RENDER;
never delete build/). <src dir> is build_sources.py's output.

Each clip is rendered twice from one source with the same settings and automation: BLEND
fully left (the dry, sample-identical to the input) and BLEND fully right (100 % wet). A
clip with a send (03) feeds the reverb only <id>_send.wav; its dry is the whole groove.
Both files get one gain, chosen so the mix at the clip's own BLEND (the module's
equal-power law, cos/sin: core/params/Mappings.h, ADR 0012) sits at TARGET_LUFS, then
the pair is turned down together if any of dry, wet or mix would peak over the ceiling.
The last 1.5 s fade out on both, so a held tail never ends on a cut. Encoded with ffmpeg's
libmp3lame at 160 kbps CBR. The site's fader mixes the pair live with the same law.

Also writes <work dir>/<id>_wet.json (the Renderer's sidecar: the settings it really used)
and <work dir>/demos.json's entries into <audio out dir>/demos.json.
"""
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parents[4]
RENDERER = Path(os.environ.get("IV_RENDER", ROOT / "build" / "iv_render"))
TARGET_LUFS = -20.5
PEAK_CEILING_DB = -1.3  # -1 dBFS with room for the MP3 encoder's overshoot
SR = 48000
FADE_S = 1.5

# Panel names and the order they're listed in; renderer keys (`iv_render --help`).
NAMES = {"space": "SPACE", "attack": "ONSET", "mix": "BLEND", "decay": "DECAY", "tone": "TONE", "era": "WEAR",
         "size": "SIZE", "drift": "MOTION", "time": "DEPTH"}


def clock(v: float) -> str:
    """A knob's value as a clock position: 7 o'clock = 0, noon = 0.5, 5 o'clock = 1."""
    q = round((7 + 10 * v) * 4) / 4
    h = int(q)
    m = round((q - h) * 60)
    return f"{(h - 1) % 12 + 1}{f':{m:02d}' if m else ''}"


def c2v(s: str) -> float:
    """'2:30' -> 0.75, '7' -> 0, '5' -> 1."""
    h, _, m = s.partition(":")
    h = int(h)
    h = h + 12 if h <= 5 else h
    return round((h - 7 + (int(m) / 60 if m else 0)) / 10, 4)


ramp = lambda k, *pts: [{"t": t, "key": k, "value": v} for t, v in pts]

CLIPS = {
    "01": dict(
        slug="endless-pad", title="The endless pad", blend="3", dur=24.0,
        set=dict(space="CANYON", attack="NATURAL", decay="5", tone="11", era="7", size="2", drift="12", time="9"),
        auto=None,
        caption="One pad chord held into an endless tail, then nothing more is played.",
        transcript="A soft synth pad plays one A minor chord, with an added ninth, and holds it for about eight "
                   "seconds. The reverb swells in under the chord and never lets go. When the chord is released "
                   "nothing more is played, and the wash goes on at a steady level, still holding the chord's "
                   "notes, until the clip fades.",
        notes="Pad synthesised in code."),
    "02": dict(
        slug="freeze-paint", title="Freeze, then paint", blend="1", dur=28.0,
        set=dict(space="HALL", attack="NATURAL", decay="2:30", tone="12", era="8", size="12", drift="12", time="9"),
        auto={"buttons": [[1.6, 1.7], [15.4, 20.6], [22.5, 22.6]]},
        events={"freeze": 1.6, "paint": 15.4, "paint off": 20.6, "release": 22.5},
        caption="A piano chord is frozen into a still pad, a piano melody plays over it, and its last phrase is painted in.",
        transcript="A grand piano plays one D minor ninth chord. A moment later the button is tapped, and the hall freezes the chord into a still pad that holds its level instead of fading. The piano plays a slow melody in three phrases over the pad, and the pad doesn't take them in. During the last phrase the button is held, and the melody's long high A is painted into the pad: from then on the pad carries that note too. A last tap lets the pad go, and it fades away.",
        notes="Piano: Steinway B from the Versilian Community Sample Library (CC0). Played in code."),
    "03": dict(
        slug="dub-stabs", title="Dub techno stabs", blend="10:30", dur=21.0, send=True,
        set=dict(space="HALL", attack="NATURAL", decay="10:30", tone="11:30", era="9", size="10", drift="11", time="10"),
        auto=None,
        caption="Minor-ninth stabs on the send over a kick and rimshot, in a short, small hall that sits behind the groove.",
        transcript="A dub techno groove at 118 bpm: a kick on every beat, a rimshot, a quiet hat, and short C minor ninth chord stabs on the offbeats. Only the stabs go into the reverb, a short, small hall: each stab leaves a soft, warm echo of itself that fades in about a bar, behind the drums, the way a reverb send sits in a mix. After eight bars the groove stops and the last tail dies away.",
        notes="Drums: Virtuosity Drums by Versilian Studios and Karoryfer (CC0). Stabs synthesised in code."),
    "05": dict(
        slug="bloom", title="Bloom swell", blend="1", dur=27.0,
        set=dict(space="CANYON", attack="BLOOM", decay="2", tone="11:30", era="7", size="12", drift="12", time="2"),
        auto=None,
        caption="Single piano notes, each one swelling in slowly behind itself.",
        transcript="A grand piano plays six soft single notes in A minor, about four seconds apart. Each note is dry and close; its reverb starts almost silent and swells in over the next two or three seconds, so every note blooms in behind itself and joins a long, soft wash.",
        notes="Piano: Steinway B from the Versilian Community Sample Library (CC0). Played in code."),
    "06": dict(
        slug="shimmer", title="Shimmer melody", blend="1", dur=27.0,
        set=dict(space="HALL", attack="SHIMMER", decay="3", tone="12", era="7", size="2", drift="12", time="12"),
        auto=None,
        caption="A slow lead with one steady octave above it in the reverb.",
        transcript="A soft, round synth lead plays a slow melody in D minor, with long notes and space between the phrases. The reverb adds one steady octave above each note: a quiet, higher voice that follows the melody and rings on in the tail. The lead has no octave of its own; the octave is only in the reverb.",
        notes="Lead synthesised in code."),
    "07": dict(
        slug="worn-guitar", title="Worn guitar", blend="12", dur=28.0,
        set=dict(space="HALL", attack="NATURAL", decay="1", tone="10", era="7", size="11", drift="12", time="9"),
        auto={"breakpoints": ramp("era", (0.0, 0.0), (1.0, 0.0), (24.0, c2v("2")))},
        sweep=("era", 1.0, 24.0, "2"),
        sweep_text="fully left → 2 o'clock, turned steadily from 1 to 24 s",
        caption="Slow guitar chords while WEAR is turned up from fully left to 2 o'clock: hiss and grain come into the tail.",
        transcript="A clean electric guitar strums seven slow chords in E minor, one every few seconds, while WEAR is turned up steadily from fully left. At the start the reverb is clean. As WEAR comes up, a soft hiss and grain rise into the top of the tail, and its deepest lows thin a little.",
        notes="Guitar: Emilyguitar by Karoryfer (CC0). Played in code."),
    "08": dict(
        slug="tape-side", title="The tape side of MOTION", blend="1", dur=28.0,
        set=dict(space="HALL", attack="NATURAL", decay="2", tone="11", era="9", size="12", drift="7", time="9"),
        auto=None,
        caption="Held poly synth chords with MOTION fully left, the tape side: wow, flutter, crinkle and short dropouts every few seconds.",
        transcript="A warm analogue-style poly synth, saw waves through a resonant low-pass filter, plays held chords, D minor nine, G minor seven, C nine and F major seven, with a few single notes between, twice through; the last chord rings on. With MOTION fully left the whole tail wavers in pitch together, like a worn tape, with quick crinkles, and every few seconds a short dropout dips it and dulls it for a moment.",
        notes="Synth synthesised in code."),
    "09": dict(
        # Checked on v1.0.60's render (1 Nov 2026): from noon to the end the tail's top end falls ~11 dB
        # and its lows ~7 dB against the mids, and the notch moves between ~700 Hz and 2.9 kHz.
        slug="radio-side", title="The radio side of MOTION", blend="1", dur=28.0,
        set=dict(space="HALL", attack="NATURAL", decay="2:30", tone="12", era="7", size="1", drift="12", time="9"),
        auto={"breakpoints": ramp("drift", (0.0, 0.5), (2.0, 0.5), (16.0, 1.0))},
        sweep=("drift", 2.0, 16.0, "5"),
        sweep_text="noon → fully right, turned steadily from 2 to 16 s, then held",
        caption="Picked guitar chords while MOTION is turned from noon to fully right: the tail narrows into an old radio and starts to phase.",
        transcript="A clean electric guitar picks seven chords in D major, one every few seconds, each left to ring into a long hall. MOTION starts at noon, and the tail is full and open. As it turns right, the tail narrows, losing its deepest lows and its top, like the reverb is playing through an old AM radio, and a slow notch drifts through its middle so it swirls like a phaser. From about 16 seconds MOTION is fully right and the swirl is deepest.",
        notes="Guitar: Emilyguitar by Karoryfer (CC0). Played in code."),
    "10": dict(
        slug="dark-drone", title="Dark drone, WEAR's whole range", blend="4", dur=28.0,
        set=dict(space="CANYON", attack="NATURAL", decay="4:30", tone="10", era="7", size="12", drift="10", time="11"),
        # WEAR acts on the wet straight away (the wear chain sits after the tank), so a steady turn works,
        # unlike SIZE. The whole range and back: clean, aged to fully right, then clean again.
        auto={"breakpoints": ramp("era", (0.0, 0.0), (2.0, 0.0), (14.0, 1.0), (18.0, 1.0), (25.0, 0.0))},
        sweep=("era", 2.0, 25.0, "7"),
        sweep_text="fully left → fully right (2-14 s), held to 18 s, then back to fully left by 25 s",
        caption="A low drone into an endless tail while WEAR is turned all the way up and back down: the wash ages, then comes clean.",
        transcript="A low, dark drone in D minor swells in and holds, then fades out at about 24 seconds; the reverb is endless, so the wash goes on. WEAR starts fully left, and the tail is clean and smooth. Over twelve seconds it is turned all the way up, and the wash ages as it goes: a warm tape thickness and a soft breath first, then grit and grain over the top as it nears fully right. The lows stay where they are. It holds there for a few seconds, then WEAR is turned back down and the wash comes clean again.",
        notes="Drone synthesised in code."),
}
ORDER = ["01", "02", "03", "05", "06", "07", "08", "09", "10"]


def value(k: str, v: str):
    return v if k in ("space", "attack") else c2v(v)


def run(*a) -> str:
    p = subprocess.run([str(x) for x in a], check=True, capture_output=True, text=True)
    return p.stderr


def lufs_and_peak(x: np.ndarray) -> tuple[float, float]:
    """EBU R128 integrated loudness and true peak (dBTP), from ffmpeg's ebur128."""
    with tempfile.NamedTemporaryFile(suffix=".wav") as f:
        sf.write(f.name, x.astype(np.float32), SR, subtype="FLOAT")
        p = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", f.name, "-af", "ebur128=peak=true", "-f", "null", "-"],
                           capture_output=True, text=True)
    lines = p.stderr.splitlines()
    i = next(l for l in reversed(lines) if l.strip().startswith("I:"))
    pk = next(l for l in reversed(lines) if l.strip().startswith("Peak:"))
    return float(i.split()[1]), float(pk.split()[1])


def decode(mp3: Path) -> np.ndarray:
    with tempfile.NamedTemporaryFile(suffix=".wav") as f:
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(mp3), "-c:a", "pcm_f32le", f.name], check=True)
        x, sr = sf.read(f.name, always_2d=True)
    assert sr == SR
    return x


def mix_of(dry: np.ndarray, wet: np.ndarray, b: float) -> np.ndarray:
    return dry * np.cos(b * np.pi / 2) + wet * np.sin(b * np.pi / 2)


def render_clip(cid: str, src: Path, work: Path, out: Path) -> dict:
    c = CLIPS[cid]
    vals = {k: value(k, v) for k, v in c["set"].items()}
    sets = []
    for k, v in vals.items():
        sets += ["--set", f"{k}={v}"]
    auto = []
    if c["auto"]:
        a = work / f"{cid}_auto.json"
        a.write_text(json.dumps(c["auto"], indent=1))
        auto = ["--auto", a]
    files = {}
    log = []
    for side, mix in (("dry", 0), ("wet", 1)):
        w = work / f"{cid}_{side}.wav"
        inp = src / (f"{cid}_send.wav" if side == "wet" and c.get("send") else f"{cid}.wav")
        if side == "dry" and c.get("send"):  # the whole groove: the renderer's dry is the input, bit for bit
            x, sr = sf.read(str(inp), always_2d=True)
            sf.write(str(w), x.astype(np.float32), sr, subtype="FLOAT")
        else:
            log.append(run(RENDERER, inp, w, *sets, "--set", f"mix={mix}", *auto, "--sidecar"))
        x, sr = sf.read(str(w), always_2d=True)
        assert sr == SR, f"{w} is {sr} Hz"
        files[side] = x[: int(c["dur"] * SR)]
    (work / f"{cid}_render.log").write_text("".join(log))
    n = min(len(files["dry"]), len(files["wet"]))
    dry, wet = files["dry"][:n].copy(), files["wet"][:n].copy()
    fade = np.linspace(1, 0, int(FADE_S * SR))[:, None] ** 2
    dry[-len(fade):] *= fade
    wet[-len(fade):] *= fade
    b = c2v(c["blend"])
    lufs, _ = lufs_and_peak(mix_of(dry, wet, b))
    gain_db = TARGET_LUFS - lufs
    g = 10 ** (gain_db / 20)
    peaks = {k: lufs_and_peak(v * g)[1] for k, v in (("dry", dry), ("wet", wet), ("mix", mix_of(dry, wet, b)))}
    over = max(peaks.values()) - PEAK_CEILING_DB
    if over > 0:  # turn the pair down together rather than limit either side
        g *= 10 ** (-over / 20)
        gain_db -= over
    names = {f"{cid}-{c['slug']}-{side}.mp3": side for side in ("dry", "wet")}
    names = {v: k for k, v in names.items()}
    for attempt in range(3):
        for side, x in (("dry", dry), ("wet", wet)):
            g_wav = work / f"{cid}_{side}_final.wav"
            sf.write(str(g_wav), (x * g).astype(np.float32), SR, subtype="FLOAT")
            run("ffmpeg", "-y", "-loglevel", "error", "-i", g_wav, "-codec:a", "libmp3lame", "-b:a", "160k",
                out / names[side])
        # The encoder's low-pass takes a few tenths of a LU off; measure what ships and correct once or twice.
        dec = {side: decode(out / names[side])[:n] for side in ("dry", "wet")}
        final_lufs, final_tp = lufs_and_peak(mix_of(dec["dry"], dec["wet"], b))
        err = TARGET_LUFS - final_lufs
        pk = max(20 * np.log10(np.abs(v).max()) for v in (*dec.values(), mix_of(dec["dry"], dec["wet"], b)))
        if abs(err) < 0.1 or over > 0 or pk + err > -1.05:
            break
        g *= 10 ** (err / 20)
        gain_db += err
    print(f"{cid}: gain {gain_db:+.1f} dB, mix at BLEND {c['blend']} {final_lufs:.1f} LUFS (tp {final_tp:.1f})"
          + (f", turned down {over:.1f} dB for peaks" if over > 0 else ""))
    settings = {}
    for k in ("space", "attack", "mix", "decay", "tone", "era", "size", "drift", "time"):
        if k == "mix":
            settings["BLEND"] = {"12": "noon"}.get(c["blend"], f"{c['blend']} o'clock") + " (the fader's start)"
            continue
        v = c["set"][k]
        if k in ("space", "attack"):
            settings[NAMES[k]] = v
        elif c.get("sweep") and c["sweep"][0] == k:
            settings[NAMES[k]] = c["sweep_text"]
        else:
            settings[NAMES[k]] = {"7": "fully left", "5": "fully right", "12": "noon"}.get(v, f"{v} o'clock")
    if c["auto"] and c["auto"].get("buttons"):
        settings["FREEZE / PAINT"] = "tap at 1.6 s (freeze), held 15.4-20.6 s (paint), tap at 22.5 s (release)"
    return {"id": cid, "title": c["title"], "caption": c["caption"], "settings": settings,
            "transcript": c["transcript"], "duration_s": round(n / SR, 1), "blend": b,
            "blend_clock": c["blend"], "dry": names["dry"], "file": names["wet"],
            "lufs": round(final_lufs, 2), "true_peak_db": round(final_tp, 2), "note": c["notes"],
            "gain_db": round(gain_db, 2)}


def main():
    src, work, out = (Path(a) for a in sys.argv[1:4])
    want = sys.argv[4:] or ORDER
    work.mkdir(parents=True, exist_ok=True)
    out.mkdir(parents=True, exist_ok=True)
    path = out / "demos.json"
    have = {e["id"]: e for e in json.loads(path.read_text())["clips"]} if path.exists() else {}
    for cid in want:
        have[cid] = render_clip(cid, src, work, out)
    clips = [have[c] for c in ORDER if c in have]
    doc = {"target_lufs": TARGET_LUFS, "blend_law": "equal-power: dry cos(b·π/2), wet sin(b·π/2)",
           "encoder": "ffmpeg libmp3lame, 160 kbps CBR", "clips": clips}
    path.write_text(json.dumps(doc, indent=2, ensure_ascii=False) + "\n")
    print(f"wrote {len(clips)} clips to {path}")


if __name__ == "__main__":
    main()
