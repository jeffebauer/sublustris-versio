// WAVES: sound as pressure rings moving out through the air.
#if V == 0
// A · Ripples: rings spread from a source below the frame, soften as they travel
// and cross the copy bounced off a wall above, making faint interference.
float rings(vec2 p, vec2 src, float k, float n) {
  float r = length(p - src);
  float w = 0.5 + 0.5 * sin(r * k - u_ph * n);           // n whole waves per loop: seamless
  float sharp = mix(26.0, 3.0, smoothstep(0.3, 2.2, r));  // crisp near, diffuse far
  return pow(w, sharp) * exp(-1.1 * r);
}
vec2 field(vec2 p) {
  float ar = u_res.x / u_res.y;
  vec2 src = vec2(-0.32 * ar, -0.78);
  vec2 img = vec2(src.x + 0.15, 2.0 * 0.62 - src.y);     // the same source mirrored in a wall above
  float e = rings(p, src, 34.0, 3.0) + 0.45 * rings(p, img, 34.0, 3.0);
  float glow = 0.25 * exp(-2.2 * length(p - src));
  return vec2(e * 0.9 + glow, 0.75 * exp(-0.9 * length(p - src)));
}
#else
// B · Contour lines: a stack of thin level lines, gently lifted by a slow swell
// that passes through the middle of the frame, like a waveform drawn in relief.
vec2 field(vec2 p) {
  float z = LZ(2.0);
  float px = 1.0 / u_res.y;
  float sp = 1.0 / 26.0;
  float i0 = floor(p.y / sp);
  float acc = 0.0;
  float env0 = 0.0;
  for (int k = -2; k <= 2; k++) {
    float i = i0 + float(k);
    float base = (i + 0.5) * sp;
    float n = 0.7 * vn(vec3(p.x * 1.4, i * 0.21, z), 2.0) + 0.3 * vn(vec3(p.x * 3.1 + 4.0, i * 0.37, z * 2.0), 4.0);
    float env = exp(-pow((base + 0.04) / 0.30, 2.0)) * (0.55 + 0.45 * smoothstep(0.9, -0.6, p.x));
    float off = (n - 0.5) * 0.11 * env;
    float d = abs(p.y - base - off);
    float wdt = px * (0.5 + 1.2 * env);
    acc += (1.0 - smoothstep(wdt - px, wdt + px, d)) * (0.22 + 0.78 * env);
    env0 = max(env0, env);
  }
  return vec2(acc * 0.75, 0.25 + 0.6 * env0);
}
#endif
