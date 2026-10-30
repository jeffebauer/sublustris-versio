// TAIL: the reverb's decay drawn like a spectrogram (time left to right, pitch bottom to top).
#if V == 0
// A · Warm decay: a note strikes at the left edge; its partials stretch to the right
// and fade, the high ones first and the low ones last (what "warm" means in a reverb).
vec2 field(vec2 p) {
  float ar = u_res.x / u_res.y;
  float x = p.x - (-0.5 * ar + 0.05);
  float f = p.y + 0.5;
  float bands = 34.0;
  float bi = floor(f * bands);
  float bf = fract(f * bands);
  float line = exp(-pow((bf - 0.5) / 0.2, 2.0));
  float tau = mix(1.0, 0.11, pow(clamp(f, 0.0, 1.0), 0.8));
  float dec = exp(-max(x, 0.0) / tau);
  float onset = smoothstep(-0.015, 0.01, x);
  float n = vn(vec3(x * 6.0, bi * 0.37, LZ(3.0)), 3.0);
  float lev = 0.4 + 0.6 * h1(vec2(bi, 3.1));
  float e = (line * lev * (0.5 + 0.7 * n) + 0.12) * dec * onset;
  float strike = exp(-x * x / 0.00006) * 0.5 * smoothstep(1.05, 0.2, f);
  return vec2(e * 1.0 + strike, 0.25 + 0.7 * exp(-3.0 * max(x, 0.0)));
}
#else
// B · Freeze: the same partials held forever; nothing decays, the field only breathes.
// Freeze blue, as on the module's LED.
vec2 field(vec2 p) {
  float ar = u_res.x / u_res.y;
  float f = p.y + 0.5;
  float bands = 34.0;
  float bi = floor(f * bands);
  float bf = fract(f * bands);
  float line = exp(-pow((bf - 0.5) / 0.2, 2.0));
  float n = vn(vec3(p.x * 2.5, bi * 0.37, LZ(2.0)), 2.0);
  float lev = (0.35 + 0.65 * h1(vec2(bi, 3.1))) * exp(-1.6 * f);
  float breathe = 0.85 + 0.15 * sin(u_ph + bi * 0.9);
  float edges = smoothstep(0.5 * ar, 0.5 * ar - 0.5, abs(p.x - 0.1 * ar));
  float e = (line * lev * (0.55 + 0.6 * n) + 0.08 * lev) * breathe * edges;
  return vec2(e * 0.9, 0.9);
}
#endif
