// DIFFUSION B · Ink in water: folded swirls that thicken and thin, very slowly.
// Motion: every noise layer moves forward along tpath (never back and forth). Live, the ink
// also drifts slowly upward, so it flows in one direction; the loop files morph in place.
vec2 field(vec2 p) {
  vec2 tp = tpath(0.5);
  vec2 q = p * 1.5 + vec2(0.0, -0.012 * liveT());
  float a = fbm4(q, tp, 4);
  float b = fbm4(q + vec2(5.2, 1.3), tp + vec2(2.9, 7.1), 4);
  vec2 w = q + 1.7 * vec2(a - 0.5, b - 0.5);
  float c = fbm4(w * 1.3 + vec2(a, b), tp * 1.3 + vec2(11.0, 4.0), 5);
  float ink = smoothstep(0.34, 0.78, c);
  float env = exp(-1.4 * length(p - vec2(-0.25, -0.15)));
  float e = ink * (0.35 + 0.65 * env);
  return vec2(e * 1.15, smoothstep(0.55, 0.85, c));
}
