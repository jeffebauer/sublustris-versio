"""renders/decay_swell's material (needs renders/_material/pad.wav from tools/make_material.py): playing.wav, the pad's
first 17 s looped with 1 s crossfades to ~39 s of playing (+4 s silence); tail.wav, its first 12 s then 28 s of silence."""
import os
import numpy as np, soundfile as sf
REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
S = os.environ.get("DS_WORK", REPO + "/renders/_decay_swell_work")
os.makedirs(S + "/mat", exist_ok=True)
x, sr = sf.read(REPO + "/renders/_material/pad.wav")
seg = x[:int(17 * sr)]; f = int(1.0 * sr); fi = np.linspace(0, 1, f)[:, None]
out = np.concatenate([seg[:-f], seg[-f:] * (1 - fi) + seg[:f] * fi, seg[f:]])
out = np.concatenate([out[:-f], out[-f:] * (1 - fi) + seg[:f] * fi, seg[f:int(8 * sr)]])
g = int(1.5 * sr); out[-g:] *= np.linspace(1, 0, g)[:, None]
sf.write(S + "/mat/playing.wav", np.concatenate([out, np.zeros((int(4 * sr), 2))]), sr, subtype="PCM_24")
t = x[:int(12 * sr)].copy(); f = int(0.5 * sr); t[-f:] *= np.linspace(1, 0, f)[:, None]
sf.write(S + "/mat/tail.wav", np.concatenate([t, np.zeros((int(28 * sr), 2))]), sr, subtype="PCM_24")
