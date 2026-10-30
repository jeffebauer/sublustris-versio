// HAZE: the late tail, where the echoes are too dense to hear apart.
// Visibility: the cloud and mist bodies are lifted while the brightest spot is held down
// (a soft knee), so the haze reads more without the peak eating into text contrast.
float knee(float x) { return 1.0 - exp(-x); }
#if V == 0
// A · Moon behind cloud: a glow (never the disc) behind slow cloud that is lit across the
// sky by the moon, with a brighter rim where the cloud thins.
vec2 field(vec2 p) {
  float ar = u_res.x / u_res.y;
  vec2 m = vec2(0.28 * ar, 0.17);
  float c = fbm4(p * 1.7, tpath(0.55), 5);
  float cloud = smoothstep(0.34, 0.72, c);
  float r = length(p - m);
  float glow = 0.75 * exp(-3.2 * r) + 0.3 * exp(-11.0 * r);
  float sky = 0.30 + 0.55 * exp(-1.5 * r);                     // moonlight reaching the cloud
  float lit = glow * (1.0 - 0.7 * cloud) + cloud * sky * 0.95;
  float edge = smoothstep(0.40, 0.53, c) * (1.0 - smoothstep(0.53, 0.66, c)) * exp(-2.4 * r) * 0.7;
  return vec2(knee(1.35 * (lit + edge)), 0.6 * exp(-2.0 * r));
}
#else
// B · Ground mist: low banks of fog in layers, with clear gaps between them, thinning as they rise.
vec2 field(vec2 p) {
  vec2 tp = tpath(0.55);
  float e = 0.0;
  for (int k = 0; k < 3; k++) {
    float fk = float(k);
    float yk = -0.40 + 0.2 * fk;
    float n = fbm4(vec2(p.x * (1.0 + 0.35 * fk) + fk * 3.7, p.y * 3.2), tp + fk * 3.3, 4);
    float bank = smoothstep(0.36, 0.70, n) * exp(-pow((p.y - yk) / (0.11 + 0.035 * fk), 2.0));
    e += bank * 1.05 / (1.0 + 0.5 * fk);
  }
  float low = 0.28 * exp(-2.4 * (p.y + 0.5));
  return vec2(knee(1.3 * (e + low)), 0.3 + 0.4 * smoothstep(0.1, -0.5, p.y));
}
#endif
