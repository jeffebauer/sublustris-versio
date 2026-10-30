// SHIMMER: the octave-up sparkle that rises out of a long tail.
#if V == 0
// A · Rising glints: sparse points that twinkle as they drift upward and thin out.
float glints(vec2 p, float N, float rise, float seed) {
  vec2 q = p * N;
  vec2 c = floor(q);
  vec2 f = fract(q);
  vec2 r = h2(c + seed);
  if (r.x > 0.45) return 0.0;
  vec2 pos = 0.25 + 0.5 * h2(c + seed + 3.1);
  // each glint rises through its own cell `rise` whole times per loop (seamless),
  // fading in at the bottom of the cell and out at the top
  float y = fract(pos.y + u_ph / TAU * rise);
  float life = sin(3.14159265 * y);
  pos.y = 0.1 + 0.8 * y;
  pos.x += 0.06 * sin(u_ph + r.y * TAU);
  float tw = pow(0.5 + 0.5 * sin(u_ph * (1.0 + floor(r.y * 3.0)) + r.x * 40.0), 3.0);
  vec2 d = f - pos;
  float core = exp(-dot(d, d) / 0.002);
  float flare = exp(-d.y * d.y / 0.00025 - d.x * d.x / 0.02) * 0.35;
  return (core + flare) * tw * life;
}
vec2 field(vec2 p) {
  float env = smoothstep(0.62, -0.55, p.y);
  float e = glints(p, 8.0, 1.0, 0.0) + 0.65 * glints(p, 14.0, 2.0, 7.0) + 0.4 * glints(p, 23.0, 3.0, 13.0);
  float base = 0.22 * exp(-3.0 * (p.y + 0.55));
  return vec2(e * env * 1.6 + base, 0.8);
}
#else
// B · Caustic veil: a soft net of light, like sun on a pool floor, slowly re-knotting.
float ridge(vec3 p, float P) {
  float s = 0.0;
  float a = 0.6;
  for (int k = 0; k < 3; k++) {
    float n = 1.0 - abs(2.0 * vn(p, P) - 1.0);
    s += a * pow(n, 5.0);
    p = vec3(mat2(0.8, -0.6, 0.6, 0.8) * p.xy * 1.9 + 3.0, p.z * 2.0);
    P *= 2.0;
    a *= 0.45;
  }
  return s;
}
vec2 field(vec2 p) {
  float z = LZ(2.0);
  vec2 warp = 0.25 * vec2(vn(vec3(p * 1.5, z), 2.0), vn(vec3(p * 1.5 + 9.0, z), 2.0));
  float r = ridge(vec3(p * 3.2 + warp, z), 2.0);
  float env = 0.4 + 0.6 * exp(-1.2 * length(p - vec2(0.2, 0.1)));
  return vec2(pow(r, 1.6) * env * 0.8, 0.7);
}
#endif
