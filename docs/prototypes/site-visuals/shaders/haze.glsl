// HAZE: the late tail, where the echoes are too dense to hear apart.
#if V == 0
// A · Moon behind cloud: a faint glow (never the disc) behind slow-moving cloud,
// with a thin lit edge where the cloud thins: the "half-light of a clouded night".
vec2 field(vec2 p) {
  float ar = u_res.x / u_res.y;
  vec2 m = vec2(0.28 * ar, 0.17);
  vec2 drift = 0.06 * vec2(cos(u_ph), sin(u_ph));
  float c = fbm(vec3(p * 1.7 + drift, LZ(2.0)), 2.0);
  float cloud = smoothstep(0.38, 0.76, c);
  float r = length(p - m);
  float glow = 0.85 * exp(-3.0 * r) + 0.35 * exp(-11.0 * r);
  float lit = glow * (1.0 - 0.72 * cloud) + 0.22 * cloud * exp(-1.6 * r);
  float edge = smoothstep(0.42, 0.55, c) * (1.0 - smoothstep(0.55, 0.68, c)) * exp(-3.5 * r) * 0.5;
  return vec2(lit + edge, 0.6 * exp(-2.2 * r));
}
#else
// B · Ground mist: low banks of fog in layers, thinning as they rise.
float fbm4(vec3 p, float P) {
  float s = 0.0;
  float a = 0.5;
  for (int k = 0; k < 4; k++) {
    s += a * vn(p, P);
    p = vec3(mat2(0.8, -0.6, 0.6, 0.8) * p.xy * 2.03 + 17.0, p.z * 2.0);
    P *= 2.0;
    a *= 0.5;
  }
  return s / 0.9375;
}
vec2 field(vec2 p) {
  float z = LZ(2.0);
  float e = 0.0;
  for (int k = 0; k < 3; k++) {
    float fk = float(k);
    float yk = -0.40 + 0.2 * fk;
    vec2 drift = (0.05 + 0.03 * fk) * vec2(cos(u_ph + fk), 0.3 * sin(u_ph + fk));
    float n = fbm4(vec3(p.x * (1.0 + 0.35 * fk) + drift.x + fk * 3.7, p.y * 3.2 + drift.y, z + fk), 2.0);
    float bank = smoothstep(0.32, 0.78, n) * exp(-pow((p.y - yk) / (0.10 + 0.03 * fk), 2.0));
    e += bank * 0.75 / (1.0 + 0.6 * fk);
  }
  float low = 0.22 * exp(-2.6 * (p.y + 0.5));
  return vec2(e + low, 0.3 + 0.4 * smoothstep(0.1, -0.5, p.y));
}
#endif
