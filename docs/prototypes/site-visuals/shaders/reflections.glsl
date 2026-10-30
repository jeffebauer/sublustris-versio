// REFLECTIONS: every echo is the sound bounced off a surface.
#if V == 0
// A · Mirror room: a small light in a room and all its mirror images in the walls,
// each bounce a little fainter and softer (how acousticians model a room's early echoes).
vec2 field(vec2 p) {
  float ar = u_res.x / u_res.y;
  vec2 ctr = vec2(0.16 * ar, 0.0);
  float R = 0.2;
  vec2 s = vec2(-0.03, 0.02) + vec2(0.08 * cos(u_ph), 0.05 * sin(2.0 * u_ph));
  vec2 q = p - ctr;
  float acc = 0.0;
  float blue = 0.0;
  for (int m = -5; m <= 5; m++) {
    for (int n = -3; n <= 3; n++) {
      float fm = float(m);
      float fn = float(n);
      vec2 img = vec2(2.0 * R * fm + (mod(fm, 2.0) == 0.0 ? s.x : -s.x),
                      2.0 * R * fn + (mod(fn, 2.0) == 0.0 ? s.y : -s.y));
      float order = abs(fm) + abs(fn);
      float lev = pow(0.66, order);
      float rad = 0.014 + 0.024 * order;
      float d = length(q - img);
      float g = lev * exp(-d * d / (rad * rad)) * (0.014 / rad + 0.35);
      acc += g;
      blue += g * exp(-0.8 * order);
    }
  }
  float px = 1.0 / u_res.y;
  float tx = q.x / (2.0 * R);
  float ty = q.y / (2.0 * R);
  float wx = abs(fract(tx) - 0.5) * 2.0 * R;
  float wy = abs(fract(ty) - 0.5) * 2.0 * R;
  float walls = (1.0 - smoothstep(0.0, 1.5 * px, wx)) + (1.0 - smoothstep(0.0, 1.5 * px, wy));
  acc += 0.10 * walls * exp(-1.6 * length(q));
  return vec2(acc, clamp(blue / max(acc, 1e-4), 0.0, 1.0));
}
#else
// B · Light on water: a faint light low in the sky and its broken reflection,
// a path of glints that spreads and dims toward you.
vec2 field(vec2 p) {
  float ar = u_res.x / u_res.y;
  float z = LZ(2.0);
  float hz = 0.26;
  float cx = 0.22 * ar;
  float below = hz - p.y;
  float glow = 0.55 * exp(-6.0 * length(p - vec2(cx, hz + 0.1))) + 0.12 * exp(-2.0 * length(p - vec2(cx, hz)));
  if (below <= 0.0) return vec2(glow, 0.5);
  float depth = 1.0 / (below + 0.05);
  vec2 w = vec2((p.x - cx) * depth, depth);
  float n = vn(vec3(w.x * 2.6, w.y * 7.0, z), 2.0) * 0.65 + vn(vec3(w.x * 6.0, w.y * 15.0, z * 2.0), 4.0) * 0.35;
  float glint = smoothstep(0.58, 0.86, n);
  float width = 0.05 + 0.45 * below;
  float path = exp(-pow((p.x - cx) / width, 2.0));
  float near = smoothstep(0.0, 0.03, below);
  float e = glint * near * (0.15 + 0.85 * path) * (0.35 + 0.65 * exp(-2.2 * below));
  return vec2(e + glow * 0.6, 0.35 + 0.5 * path);
}
#endif
