// WAVES: sound as pressure moving out through the room, losing its edge as it goes.
#if V == 0
// A · Ripples: rings leave the source crisp; the further they travel the wider, softer and
// more broken they get, until far out they have scattered into loose points of light,
// the way a reverb's echoes smear into a diffuse wash.
const float K = 34.0;   // rings per unit distance (x TAU)
const float NW = 3.0;   // whole waves per loop: seamless
float crest(float r, float jitter) {
  float ph = r * K - u_ph * NW + jitter;
  return fract(ph / TAU - 0.25 + 0.5) - 0.5;   // distance to the nearest crest, in wavelengths
}
float rings(vec2 p, vec2 src, float m1, float m2) {
  float r = length(p - src);
  float diff = smoothstep(0.12, 1.6, r);                 // 0 at the source, 1 far away
  float u = crest(r, diff * diff * (m1 - 0.5) * 7.0);     // far rings wobble out of round
  float w = mix(0.022, 0.32, diff);                      // crest width grows a lot
  float ring = exp(-u * u / (w * w)) * mix(1.0, 0.42, diff);
  float frag = mix(1.0, 1.5 * smoothstep(0.2, 0.9, m2), diff);   // and breaks into fragments
  return ring * frag * exp(-0.95 * r);
}
vec2 field(vec2 p) {
  float ar = u_res.x / u_res.y;
  vec2 src = vec2(-0.32 * ar, -0.78);
  vec2 img = vec2(src.x + 0.15, 2.0 * 0.62 - src.y);     // the source mirrored in a wall above
  float m1 = vn2(p * 3.0 + 3.1);                          // a still "medium" the waves pass through
  float m2 = vn2(p * 10.0 + 7.7);
  float e = rings(p, src, m1, m2) + 0.4 * rings(p, img, m1, m2);
  // scattered points: each lights up as a ring passes it, only far from the source
  float NG = 46.0;
  vec2 q = p * NG;
  vec2 c0 = floor(q);
  float dots = 0.0;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      vec2 c = c0 + vec2(float(i), float(j));
      if (h1(c + 0.7) > 0.5) continue;
      vec2 o = 0.15 + 0.7 * h2(c);
      vec2 dp = (c + o) / NG;
      float rd = length(dp - src);
      float dd = smoothstep(0.35, 1.7, rd);
      float ud = crest(rd, 0.0);
      float wd = mix(0.022, 0.32, dd) * 1.4;
      float lit = exp(-ud * ud / (wd * wd)) * dd * exp(-0.8 * rd) * (0.5 + 0.5 * h1(c + 3.3));
      vec2 d = q - c - o;
      dots += lit * exp(-dot(d, d) / 0.03);
    }
  }
  float glow = 0.25 * exp(-2.2 * length(p - src));
  return vec2(e * 0.95 + dots * 0.9 + glow, 0.75 * exp(-0.9 * length(p - src)));
}
#else
// B · Contour lines: thin level lines lifted by a slow swell; crisp at the left, and toward
// the right they widen, fray, jitter and fall apart, like the tail of a sound.
vec2 field(vec2 p) {
  float ar = u_res.x / u_res.y;
  vec2 tp = tpath(0.5);
  float px = 1.0 / u_res.y;
  float sp = 1.0 / 26.0;
  float dx = smoothstep(-0.42 * ar, 0.5 * ar, p.x);       // 0 at the left edge, 1 at the right
  float i0 = floor(p.y / sp);
  float acc = 0.0;
  float env0 = 0.0;
  for (int k = -2; k <= 2; k++) {
    float i = i0 + float(k);
    float base = (i + 0.5) * sp;
    float n1 = vn4(vec4(p.x * 1.4, i * 0.21, tp));
    float n2 = vn4(vec4(p.x * 3.1 + 4.0, i * 0.37, tp * 1.6 + 2.0));
    float n3 = vn4(vec4(p.x * 9.0 + 1.0, i * 0.83, tp * 2.2 + 6.0));
    float env = exp(-pow((base + 0.04) / 0.30, 2.0)) * (0.55 + 0.45 * (1.0 - dx));
    float off = (0.7 * n1 + 0.3 * n2 - 0.5) * 0.11 * env + dx * dx * (n3 - 0.5) * 0.045;
    float d = abs(p.y - base - off);
    float wdt = px * (0.5 + 1.2 * env) + dx * dx * sp * 0.15;
    float line = 1.0 - smoothstep(wdt - px, wdt + px + dx * sp * 0.25, d);
    float frag = mix(1.0, smoothstep(0.25, 0.75, n3) * 1.3, dx);
    acc += line * frag * (0.22 + 0.78 * env) * mix(1.0, 0.4, dx);
    env0 = max(env0, env);
  }
  return vec2(acc * 0.75, 0.25 + 0.6 * env0);
}
#endif
