#!/usr/bin/env python3
"""Measures our spaces and the owner's reference recordings with one method, side by side.

Usage: .venv/bin/python tools/compare_reference.py [--work DIR] [--only hall,plate,massive]
                                                    [--json out.json] [--render BIN]

Read only: it never writes into the repo or test_audio. It renders Immensio through the
same kit files the reference units played (test_audio/reference_kit/music.wav and
measure.wav) into --work (default: the system temp dir), then measures both sides the
same way and prints Markdown tables (docs/research/valhalla-comparison.md was made from
them). Recordings are never committed (ADR 0006).

Both sides are "raw takes" on the kit's own timeline: the sync click at 1.0 s and every
part at cues.json's offset. (The ingested pieces in test_audio/reference/<take>/ are cut
from the sync click's *response*, which hides a unit's predelay; the raw takes keep it.)

What is measured (docs/research/reverb-testing-practices.md; definitions in each function):
  click response  music.wav's sync click (3 s before the first piece; Room and Plate have
                  nothing else) or, where a measure take exists, measure.wav's click (24 s):
                  first sound, ISO onset, when the tail peaks, rise time; band EDT/T60,
                  C80, IACC, echo-density grain via `iv_render --analyze` (host/common/Metrics)
  impulse         the ingested ir.wav (Supermassive) and ours deconvolved from our measure
                  render with tools/ingest_reference.py's own code, both through --analyze
  music tails     after each piece's last note: decay per octave band (a line fit from
                  -5 to -25 dB below the tail's peak, x3, as T60), how much darker the tail
                  gets from 0.25 to 2 s, L/R correlation and IACC of the tail
  music overall   the wet's octave-band balance against the dry's (normalised at 1 kHz),
                  and the wet's level against the dry's
  modulation      pitch wobble of the vibes and pad tails (tools/pitch_track.py's method with a
                  narrower filter, see wobble_cents); ours also with the tanks' modulation off
                  (IV_MOD_SCALE=0), the floor any tail shows without movement
  recording       clipping, noise floor, energy at the dry's instant (dry leak), the end of
                  the take still ringing (cut short)
To add a unit (H9, Mercury7): add its takes to REFS and a pair to PAIRS.
"""
import argparse
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np
import soundfile as sf

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import ingest_reference as ing  # noqa: E402  (load, octave: the same filters as the units' T60)

SR = 48000
KIT = ROOT / "test_audio" / "reference_kit"
REF = ROOT / "test_audio" / "reference"
INCOMING = REF / "incoming"
SYNC_AT = 1.0
BANDS = [125, 250, 500, 1000, 2000, 4000, 8000]
BAL_BANDS = [63, 125, 250, 500, 1000, 2000, 4000, 8000]
ORDER = ["skank", "vibes", "stabs", "pad", "lead", "drums", "bass", "keys", "voice"]

# The page renders/valhalla_compare: our settings at each decay target.
COMMON = {"era": 0, "time": 0.2, "tone": 0.5, "drift": 0.5, "mix": 1, "attack": "NORMAL"}
OURS = {
    "hall": {"space": "HALL", "size": 0.6, "decay": {"2s": 0.296, "5s": 0.509, "15s": 0.772}},
    "plate": {"space": "PLATE", "size": 0.6, "decay": {"2s": 0.398, "5s": 0.624, "15s": 0.816}},
    "massive": {"space": "MASSIVE", "size": 0.7, "decay": {"2s": 0.068, "5s": 0.198, "15s": 0.406}},
}
# Reference takes per target: incoming/<take>_music.wav (+ _measure.wav, and REF/<take>/ir.wav).
PAIRS = {
    "hall": {"title": "HALL vs Valhalla Room", "refs": {
        "Large Room": {"2s": "valhallaroom_largeroom_short", "5s": "valhallaroom_largeroom_medium",
                       "15s": "valhallaroom_largeroom_long"},
        "Dark Chamber": {"2s": "valhallaroom_darkchamber_short", "5s": "valhallaroom_darkchamber_medium",
                         "15s": "valhallaroom_darkchamber_long"}}},
    "plate": {"title": "PLATE vs Valhalla Plate", "refs": {
        "Chrome": {"2s": "valhallaplate_chrome_short", "5s": "valhallaplate_chrome_medium",
                   "15s": "valhallaplate_chrome_long"},
        "Lithium": {"2s": "valhallaplate_lithium_short", "5s": "valhallaplate_lithium_medium",
                    "15s": "valhallaplate_lithium_long"}}},
    "massive": {"title": "MASSIVE vs Valhalla Supermassive", "refs": {
        "Sagittarius": {"2s": "supermassive_sagittarius_fb007", "5s": "supermassive_sagittarius_fb030",
                        "15s": "supermassive_sagittarius_fb073"},
        "Andromeda": {"2s": "supermassive_andromeda_fb000", "5s": "supermassive_andromeda_fb012",
                      "15s": "supermassive_andromeda_fb054"}}},
}
# Takes only checked for recording issues (no counterpart on the page).
EXTRA_TAKES = ["supermassive_sagittarius_fb100", "supermassive_andromeda_fb100"]


# ---------------------------------------------------------------- helpers
def db(x, floor=1e-20):
    return 10 * np.log10(np.maximum(x, floor))


def rms_env(x, win_s):
    """Centred moving-average power over win_s, in dB."""
    n = max(1, int(win_s * SR))
    k = np.ones(n) / n
    return db(np.convolve(x * x, k, mode="same"))


def cues():
    return json.loads((KIT / "cues.json").read_text())


def at(name, kind="music"):
    """Sample index of a kit part on the kit timeline."""
    return int(round((SYNC_AT + cues()[kind][name]) * SR))


def piece_bounds(kit_len):
    c = cues()["music"]
    out = {}
    for i, m in enumerate(ORDER):
        a = at(m)
        b = at(ORDER[i + 1]) if i + 1 < len(ORDER) else kit_len
        out[m] = (a, b)
    return out


def iacc(l, r, max_lag_s=0.001):
    """Max |normalised cross-correlation| within +-1 ms (ISO 3382-1)."""
    el, er = float(np.dot(l, l)), float(np.dot(r, r))
    if el <= 0 or er <= 0:
        return float("nan")
    n = 1 << int(np.ceil(np.log2(2 * len(l))))
    xc = np.fft.irfft(np.fft.rfft(l, n) * np.conj(np.fft.rfft(r, n)), n)
    lag = int(max_lag_s * SR)
    v = np.concatenate([xc[-lag:], xc[: lag + 1]])
    return float(np.abs(v).max() / np.sqrt(el * er))


def analyze_cpp(render, seg, tmpdir, tag):
    """The Renderer's own metrics (host/common/Metrics) on a stereo segment."""
    wav = Path(tmpdir) / f"{tag}.wav"
    js = Path(tmpdir) / f"{tag}.json"
    sf.write(str(wav), seg.astype(np.float32), SR, subtype="FLOAT")
    subprocess.run([render, "--analyze", str(wav), "--sidecar-out", str(js)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    m = json.loads(js.read_text())["metrics"]
    wav.unlink()
    js.unlink()
    return m


# ---------------------------------------------------------------- click response
def click_response(seg, before=None):
    """seg starts at the click's instant. First sound: first sample within 40 dB of the
    loudest (and 4x above whatever rang in the 100 ms `before` it: measure.wav's sync
    click is only 3 s earlier, so a long tail is still sounding); onset: ISO 3382-1
    (within 20 dB); peak: the loudest 50 ms; rise: the 50 ms envelope's climb from 20 dB
    under its peak to 3 dB under it (the swell, or ~0 for a reverb that starts at full
    level)."""
    mono = seg.mean(axis=1)
    a = np.abs(seg).max(axis=1)
    pk = a.max()
    floor = 4 * np.abs(before).max() if before is not None and len(before) else 0.0
    first = int(np.argmax(a > max(pk * 10 ** (-40 / 20), floor)))
    onset = int(np.argmax(np.abs(mono) > np.abs(mono).max() * 0.1))
    env = rms_env(mono, 0.05)
    ip = int(np.argmax(env))
    i20 = int(np.argmax(env > env[ip] - 20))
    i3 = int(np.argmax(env > env[ip] - 3))
    # Dry leak: energy within +-1 ms of the dry click's instant, against the first 100 ms.
    e = mono ** 2
    w = int(0.001 * SR)
    early = e[: int(0.1 * SR)].sum()
    # (Nothing in the first 100 ms, e.g. a long predelay: nothing to leak, so no figure.)
    leak = db(e[:w].sum()) - db(early) if early > 1e-12 * max(e.sum(), 1e-30) and early > 1e-16 else float("nan")
    return {"first_ms": 1000 * first / SR, "onset_ms": 1000 * onset / SR, "peak_ms": 1000 * ip / SR,
            "rise_ms": 1000 * max(0, i3 - i20) / SR, "leak_db": float(leak),
            "truncated": bool(ip > len(mono) - int(0.1 * SR))}


# ---------------------------------------------------------------- music tails
def band_env(x, fc, pre):
    """Octave band (ingest_reference's filter) of mono x, 20 ms power envelope in dB, with
    the first `pre` samples (filter run-in) dropped."""
    return rms_env(ing.octave(x, fc), 0.02)[pre:]


def tail_decay(env):
    """T60 from a band envelope that starts where the input stopped: the line fit of dB
    against time from 5 to 25 dB below the tail's loudest point in its first second, x3
    (as tools/ingest_reference.py's -5..-25 dB). NaN if it never falls that far."""
    first = int(1.0 * SR)
    ip = int(np.argmax(env[:first]))
    ref = env[ip]
    rest = env[ip:]
    a = int(np.argmax(rest < ref - 5))
    b = int(np.argmax(rest < ref - 25))
    if a == 0 or b <= a:
        return float("nan")
    t = np.arange(a, b) / SR
    slope = np.polyfit(t, rest[a:b], 1)[0]
    return float(-60 / slope) if slope < 0 else float("nan")


def music_tails(wet, dry, kit_len):
    """Per piece: where the dry's last note ends (its 10 ms level falls for good below
    40 dB under the piece's loudest), then the wet after it."""
    res = {}
    pre = int(0.5 * SR)
    for m, (a, b) in piece_bounds(kit_len).items():
        d = dry[a:b].mean(axis=1)
        env = rms_env(d, 0.01)
        loud = np.nonzero(env > env.max() - 40)[0]
        if not len(loud):
            continue
        end = a + int(loud[-1])
        if b - end < 3 * SR or end + 3 * SR > len(wet):
            continue
        w = wet[end - pre: min(b, len(wet))]
        mono = w.mean(axis=1)
        r = {"t60": {}, "lvl": {}}
        for fc in BANDS:
            e = band_env(mono, fc, pre)
            r["t60"][fc] = tail_decay(e)
            r["lvl"][fc] = {t: float(e[int(t * SR)]) if int(t * SR) < len(e) else float("nan")
                            for t in (0.25, 2.0)}
        s0, s1 = pre + int(0.2 * SR), pre + int(3.2 * SR)
        L, R = w[s0:s1, 0], w[s0:s1, 1]
        r["corr"] = float(np.corrcoef(L, R)[0, 1]) if L.std() > 0 and R.std() > 0 else float("nan")
        r["iacc"] = iacc(L, R)
        # The whole piece: wet balance re dry, and the wet's level re the dry's.
        bb = min(b, len(wet), len(dry))
        wp, dp = wet[a:bb].mean(axis=1), dry[a:bb].mean(axis=1)
        W, D = np.abs(np.fft.rfft(wp)) ** 2, np.abs(np.fft.rfft(dp)) ** 2
        f = np.fft.rfftfreq(len(wp), 1 / SR)
        bal = {}
        for fc in BAL_BANDS:
            sel = (f >= fc / np.sqrt(2)) & (f < fc * np.sqrt(2))
            bal[fc] = float(db(W[sel].sum()) - db(D[sel].sum()))
        r["balance"] = {fc: bal[fc] - bal[1000] for fc in BAL_BANDS}
        r["wet_db"] = float(db((wp ** 2).sum()) - db((dp ** 2).sum()))
        r["end_s"] = (end - a) / SR
        res[m] = r
    return res


def strongest_partial(x, fmin=150.0, fmax=2000.0):
    X = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    f = np.fft.rfftfreq(len(x), 1 / SR)
    sel = (f > fmin) & (f < fmax)
    return float(f[sel][np.argmax(X[sel])])


def wobble_cents(seg, f0):
    """Pitch wobble of one partial, p95 |cents| about its median. tools/pitch_track.py's
    method (band-pass, analytic signal, instantaneous frequency, smoothed), but with a
    filter a quarter-tone wide (pitch_track's 30 % band spans several partials of a chord
    tail and read 100-200 cents) and the frequency weighted by the band's power over
    20 ms, so the beat nulls between a tail's modes don't read as pitch jumps."""
    n = len(seg)
    f = np.fft.rfftfreq(n, 1 / SR)
    w = np.exp(-0.5 * ((f - f0) / (f0 * (2 ** (1 / 24) - 1))) ** 2)
    S = np.fft.rfft(seg) * w
    h = np.zeros(n // 2 + 1)
    h[0] = 1
    h[1:] = 2
    z = np.fft.ifft(np.concatenate([S * h, np.zeros(n - len(S))]))
    amp = np.abs(z)[1:] ** 2
    inst = np.diff(np.unwrap(np.angle(z))) * SR / (2 * np.pi)
    k = np.ones(int(0.02 * SR))
    lvl = np.convolve(amp, k, "same")
    s = np.convolve(inst * amp, k, "same") / np.maximum(lvl, 1e-30)
    trim = int(0.15 * SR)
    s, lvl = s[trim:-trim], lvl[trim:-trim]
    keep = lvl > 0.1 * np.median(lvl)
    if keep.sum() < SR // 2:
        return float("nan")
    c = 1200 * np.log2(np.maximum(s[keep], 1.0) / np.median(s[keep]))
    return float(np.percentile(np.abs(c[:: SR // 100]), 95))


def wobble(wet, dry, kit_len, pieces=("vibes", "pad")):
    """Pitch wobble on 0.2-3.2 s of each piece's tail, at the dry's last note's strongest
    partial (the same frequency for every take). "dry_" entries: the dry's own last 2 s,
    the control (the pad's detuned voices beat by themselves)."""
    out = {}
    for m, (a, b) in piece_bounds(kit_len).items():
        if m not in pieces:
            continue
        d = dry[a:b].mean(axis=1)
        env = rms_env(d, 0.01)
        end = int(np.nonzero(env > env.max() - 40)[0][-1])
        last = d[max(0, end - int(2.2 * SR)): end - int(0.2 * SR)]
        f0 = strongest_partial(last)
        out["dry_" + m] = wobble_cents(last, f0)
        seg = wet[a + end + int(0.2 * SR): a + end + int(3.2 * SR)].mean(axis=1)
        out[m] = wobble_cents(seg, f0) if len(seg) >= 2 * SR else float("nan")
        out["f0_" + m] = f0
    return out


# ---------------------------------------------------------------- one take
def recording_checks(x, kit_len):
    a = np.abs(x)
    pre = x[: int(0.9 * SR)]
    tail = x[-int(0.5 * SR):]
    return {"peak_dbfs": float(20 * np.log10(max(a.max(), 1e-12))),
            "clipped": int((a >= 0.9999).sum()),
            "floor_dbfs": float(db(np.mean(pre ** 2))) if len(pre) else float("nan"),
            "end_db_re_peak": float(db(np.mean(tail ** 2)) - 20 * np.log10(max(a.max(), 1e-12))),
            "lr_identical": bool(np.allclose(x[:, 0], x[:, 1]))}


def measure_take(render, music, measure, ir, tmpdir, tag, nomod=None):
    kit = ing.load(KIT / "music.wav")
    x = ing.load(music)
    out = {"checks": recording_checks(x, len(kit))}
    # The sync click's response: 2.95 s before the first piece starts.
    s0 = int(SYNC_AT * SR)
    seg = x[s0: at(ORDER[0]) - int(0.05 * SR)]
    out["sync"] = click_response(seg)
    out["sync_cpp"] = analyze_cpp(render, seg, tmpdir, tag + "_sync")
    out["tails"] = music_tails(x, kit, len(kit))
    out["wobble"] = wobble(x, kit, len(kit))
    if nomod is not None:
        out["wobble_nomod"] = wobble(ing.load(nomod), kit, len(kit))
    if measure is not None and Path(measure).exists():
        y = ing.load(measure)
        c = int(round((SYNC_AT + cues()["measure"]["click"]) * SR))
        seg = y[c: c + 24 * SR]
        out["click"] = click_response(seg, y[c - SR // 10: c])
        # The Renderer's metrics on our own measure click only: on Supermassive's clicks its
        # event finder splits the delay's repeats into separate events (T60s of 0.1-0.3 s),
        # so their decay figures come from ir.wav instead.
        if tag.startswith("ours"):
            out["click_cpp"] = analyze_cpp(render, seg, tmpdir, tag + "_click")
        out["checks_measure"] = recording_checks(y, len(y))
    if ir is not None and Path(ir).exists():
        z, _ = sf.read(str(ir), always_2d=True)
        out["ir_cpp"] = analyze_cpp(render, z, tmpdir, tag + "_ir")
    return out


def deconvolve(measure_wav, out_ir):
    """Our impulse response, exactly as tools/ingest_reference.py makes ir.wav (including
    its alignment to the sync click's response)."""
    cu = cues()["measure"]
    x = ing.load(measure_wav)
    s0 = ing.sync_index(x)
    kit = ing.load(KIT / "measure.wav")
    k0 = ing.sync_index(kit)
    ref = ing.part(kit, k0, cu["sweep"], 10.0).mean(axis=1)
    rec = ing.part(x, s0, cu["sweep"], 35.0)
    n = 1 << int(np.ceil(np.log2(len(rec) + len(ref))))
    R = np.fft.rfft(ref, n)
    reg = 1e-3 * np.abs(R).max()
    ir = np.stack([np.fft.irfft(np.fft.rfft(rec[:, c], n) * np.conj(R) / (np.abs(R) ** 2 + reg ** 2), n)[: 25 * SR]
                   for c in range(2)], axis=1)
    ir /= max(1e-9, np.abs(ir).max())
    sf.write(str(out_ir), (0.5 * ir).astype(np.float32), SR, subtype="PCM_24")


def render_ours(render, work, space_key, target):
    o = OURS[space_key]
    sets = dict(COMMON, space=o["space"], size=o["size"], decay=o["decay"][target])
    args = sum((["--set", f"{k}={v}"] for k, v in sets.items()), [])
    base = Path(work) / f"ours_{space_key}_{target}"
    paths = {"music": base.with_name(base.name + "_music.wav"), "measure": base.with_name(base.name + "_measure.wav"),
             "nomod": base.with_name(base.name + "_music_nomod.wav"), "ir": base.with_name(base.name + "_ir.wav")}
    # The no-modulation control only needs the vibes and pad pieces: the kit up to "lead".
    short_kit = Path(work) / "kit_music_to_lead.wav"
    if not short_kit.exists():
        k, _ = sf.read(str(KIT / "music.wav"), always_2d=True)
        sf.write(str(short_kit), k[: at("lead")], SR, subtype="PCM_24")
    for key, src, env in (("music", KIT / "music.wav", {}), ("measure", KIT / "measure.wav", {}),
                          ("nomod", short_kit, {"IV_MOD_SCALE": "0"})):
        if not paths[key].exists():
            subprocess.run([render, str(src), str(paths[key])] + args, check=True, stdout=subprocess.DEVNULL,
                           env=dict(os.environ, **env))
    if not paths["ir"].exists():
        deconvolve(paths["measure"], paths["ir"])
    return paths


# ---------------------------------------------------------------- tables
def med(vals):
    v = [x for x in vals if x == x]
    return float(np.median(v)) if v else float("nan")


def summarize(t):
    """One column of numbers from a measured take."""
    tails = t["tails"].values()
    s = {}
    for fc in BANDS:
        s[f"t60_tail_{fc}"] = med([r["t60"][fc] for r in tails])
    s["t60_tail_mid"] = med([(r["t60"][500] + r["t60"][1000]) / 2 for r in tails])
    s["n_tails"] = sum(1 for r in tails if r["t60"][1000] == r["t60"][1000])
    s["darken_db"] = med([(r["lvl"][4000][2.0] - r["lvl"][250][2.0]) - (r["lvl"][4000][0.25] - r["lvl"][250][0.25])
                          for r in tails])
    s["tail_corr"] = med([r["corr"] for r in tails])
    s["tail_iacc"] = med([r["iacc"] for r in tails])
    for fc in BAL_BANDS:
        s[f"bal_{fc}"] = med([r["balance"][fc] for r in tails])
    s["wet_db"] = med([r["wet_db"] for r in tails])
    sc = t["sync_cpp"]
    s.update({"sync_" + k: v for k, v in t["sync"].items()})
    s["sync_edt_mid"] = (sc["band_edt_s"][2] + sc["band_edt_s"][3]) / 2 if sc["band_edt_s"][2] is not None else float("nan")
    s["sync_c80"] = sc["c80_db"]
    s["sync_iacc_early"] = sc["iacc_early"]
    s["sync_iacc_late"] = sc["iacc_late"]
    s["sync_grain"] = sc["echo_density_grain_s"]
    s["sync_late_density"] = sc["echo_density_late"]
    for k in ("vibes", "pad"):
        s[f"wobble_{k}"] = t["wobble"].get(k, float("nan"))
        s[f"wobble_dry_{k}"] = t["wobble"].get("dry_" + k, float("nan"))
        if "wobble_nomod" in t:
            s[f"wobble_nomod_{k}"] = t["wobble_nomod"].get(k, float("nan"))
    for src in ("click", "ir"):
        c = t.get(src + "_cpp")
        if c:
            s[f"{src}_t60"] = [x if x is not None else float("nan") for x in c["band_t60_s"]]
            s[f"{src}_edt"] = [x if x is not None else float("nan") for x in c["band_edt_s"]]
            s[f"{src}_c80"] = c["c80_db"]
            s[f"{src}_iacc_early"] = c["iacc_early"]
            s[f"{src}_iacc_late"] = c["iacc_late"]
            s[f"{src}_grain"] = c["echo_density_grain_s"]
            s[f"{src}_late_density"] = c["echo_density_late"]
    if "click" in t:
        s.update({"click_" + k: v for k, v in t["click"].items()})
    return s


def fmt(v, nd=2):
    if v is None or (isinstance(v, float) and v != v):
        return "–"
    if isinstance(v, bool):
        return "yes" if v else "no"
    if isinstance(v, int):
        return str(v)
    return f"{v:.{nd}f}"


ROWS = [
    ("Decay, mids (s), music tails", "t60_tail_mid", 1),
    ("Decay 125 Hz (s)", "t60_tail_125", 1), ("Decay 250 Hz (s)", "t60_tail_250", 1),
    ("Decay 500 Hz (s)", "t60_tail_500", 1), ("Decay 1 kHz (s)", "t60_tail_1000", 1),
    ("Decay 2 kHz (s)", "t60_tail_2000", 1), ("Decay 4 kHz (s)", "t60_tail_4000", 1),
    ("Decay 8 kHz (s)", "t60_tail_8000", 1),
    ("Tail darkens 0.25→2 s (dB, 4 kHz re 250 Hz)", "darken_db", 1),
    ("Click: first sound (ms)", "sync_first_ms", 0), ("Click: ISO onset (ms)", "sync_onset_ms", 0),
    ("Click: tail peaks at (ms)", "sync_peak_ms", 0), ("Click: rise −20→−3 dB (ms)", "sync_rise_ms", 0),
    ("Click: EDT mids (s, ≤3 s window)", "sync_edt_mid", 2),
    ("Click: C80 (dB)", "sync_c80", 1), ("Click: grain (s)", "sync_grain", 3),
    ("Click: IACC early", "sync_iacc_early", 2), ("Click: IACC late (to 3 s)", "sync_iacc_late", 2),
    ("Tail L/R correlation (music)", "tail_corr", 2), ("Tail IACC (music)", "tail_iacc", 2),
    ("Wet balance 63 Hz re 1 kHz (dB)", "bal_63", 1), ("Wet balance 125 Hz", "bal_125", 1),
    ("Wet balance 250 Hz", "bal_250", 1), ("Wet balance 4 kHz", "bal_4000", 1), ("Wet balance 8 kHz", "bal_8000", 1),
    ("Wet level re dry (dB)", "wet_db", 1),
    ("Pitch wobble, vibes tail (p95 cents)", "wobble_vibes", 1),
    ("Pitch wobble, pad tail (p95 cents)", "wobble_pad", 1),
    ("… ours with modulation off: vibes", "wobble_nomod_vibes", 1),
    ("… ours with modulation off: pad", "wobble_nomod_pad", 1),
    ("… the dry's last note itself: vibes", "wobble_dry_vibes", 1),
    ("… the dry's last note itself: pad", "wobble_dry_pad", 1),
    ("Music tails measured", "n_tails", 0),
]
IMPULSE_ROWS = [
    ("Measure click: T60 per band 125…8k (s)", "click_t60"), ("Measure click: EDT per band (s)", "click_edt"),
    ("IR: T60 per band (s)", "ir_t60"), ("IR: EDT per band (s)", "ir_edt"),
]
IMPULSE_SCALARS = [
    ("Measure click: first sound (ms)", "click_first_ms", 0), ("Measure click: tail peaks at (ms)", "click_peak_ms", 0),
    ("Measure click: rise −20→−3 dB (ms)", "click_rise_ms", 0),
    ("Measure click: C80 (dB)", "click_c80", 1), ("Measure click: grain (s)", "click_grain", 3),
    ("Measure click: IACC early", "click_iacc_early", 2), ("Measure click: IACC late", "click_iacc_late", 2),
    ("IR: C80 (dB)", "ir_c80", 1), ("IR: grain (s)", "ir_grain", 3), ("IR: IACC late", "ir_iacc_late", 2),
]


def table(cols):
    names = list(cols)
    lines = ["| | " + " | ".join(names) + " |", "|---|" + "---|" * len(names)]
    for label, key, nd in ROWS:
        if all(key not in cols[n] for n in names):
            continue
        lines.append(f"| {label} | " + " | ".join(fmt(cols[n].get(key), nd) for n in names) + " |")
    for label, key, nd in IMPULSE_SCALARS:
        if any(key in cols[n] for n in names):
            lines.append(f"| {label} | " + " | ".join(fmt(cols[n].get(key), nd) for n in names) + " |")
    for label, key in IMPULSE_ROWS:
        if any(key in cols[n] for n in names):
            lines.append(f"| {label} | " + " | ".join(
                " / ".join(fmt(v, 1) for v in cols[n][key]) if key in cols[n] else "–" for n in names) + " |")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--work", default=str(Path(tempfile.gettempdir()) / "iv_compare_reference"))
    ap.add_argument("--only", default="hall,plate,massive")
    ap.add_argument("--json")
    ap.add_argument("--render", default=os.environ.get("IV_RENDER", str(ROOT / "build" / "iv_render")))
    a = ap.parse_args()
    Path(a.work).mkdir(parents=True, exist_ok=True)
    tmp = tempfile.mkdtemp(dir=a.work)
    results = {}
    for key in a.only.split(","):
        pair = PAIRS[key]
        print(f"\n## {pair['title']}\n")
        for target in ("2s", "5s", "15s"):
            cols = {}
            p = render_ours(a.render, a.work, key, target)
            ours = measure_take(a.render, p["music"], p["measure"], p["ir"], tmp, f"ours_{key}_{target}", p["nomod"])
            cols[f"Ours ({OURS[key]['space']} {OURS[key]['decay'][target]})"] = summarize(ours)
            results[f"ours_{key}_{target}"] = ours
            for name, takes in pair["refs"].items():
                take = takes[target]
                music = INCOMING / f"{take}_music.wav"
                if not music.exists():
                    continue
                t = measure_take(a.render, music, INCOMING / f"{take}_measure.wav", REF / take / "ir.wav",
                                 tmp, take)
                cols[name] = summarize(t)
                results[take] = t
            print(f"### About {target[:-1]} s\n")
            print(table(cols))
            print()
    print("\n## Recording checks\n")
    print("| Take | peak dBFS | clipped samples | floor dBFS | end re peak (dB) | click: energy at dry instant (dB) | L = R |")
    print("|---|---|---|---|---|---|---|")
    for take in [k for k in results] + EXTRA_TAKES:
        if take in results:
            t = results[take]
        else:
            music = INCOMING / f"{take}_music.wav"
            if not music.exists():
                continue
            t = measure_take(a.render, music, INCOMING / f"{take}_measure.wav", None, tmp, take)
            results[take] = t
        for kind in ("checks", "checks_measure"):
            if kind not in t:
                continue
            c = t[kind]
            leak = t["sync"]["leak_db"] if kind == "checks" else t.get("click", {}).get("leak_db", float("nan"))
            label = take + (" (measure)" if kind == "checks_measure" else "")
            print(f"| {label} | {fmt(c['peak_dbfs'], 1)} | {c['clipped']} | {fmt(c['floor_dbfs'], 0)} | "
                  f"{fmt(c['end_db_re_peak'], 0)} | {fmt(leak, 0)} | {fmt(c['lr_identical'])} |")
    if a.json:
        Path(a.json).write_text(json.dumps(results, indent=1, default=str))
    os.rmdir(tmp)


if __name__ == "__main__":
    main()
