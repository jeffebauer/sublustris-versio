// DIFFUSION: one sound becomes many echoes that blur into a cloud.
#if V == 0
// A · Echo cloud: points near the source are crisp; the further an echo travels,
// the wider, softer and dimmer it gets, until the points merge into mist.
vec2 field(vec2 p) {
  float ar = u_res.x / u_res.y;
  vec2 src = vec2(-0.5 * ar - 0.04, 0.06);   // the note, just off the left edge
  float N = 14.0;
  vec2 q = p * N;
  vec2 cell = floor(q);
  float acc = 0.0;
  for (int j = -1; j <= 1; j++) {
    for (int i = -1; i <= 1; i++) {
      vec2 c = cell + vec2(float(i), float(j));
      vec2 r = h2(c);
      float turns = floor(r.x * 2.0) + 1.0;   // one or two slow orbits per loop
      vec2 off = 0.2 + 0.6 * h2(c + 2.7) + 0.12 * vec2(cos(u_ph * turns + r.y * TAU), sin(u_ph * turns + r.x * TAU));
      vec2 dp = c + off;
      float s = length(dp / N - src);
      float rad = mix(0.05, 0.7, smoothstep(0.05, 1.5, s));
      float lev = exp(-1.1 * s) * (0.35 + 0.65 * h1(c + 7.3));
      float d = length(q - dp);
      acc += lev * exp(-d * d / (rad * rad)) * (0.05 / rad + 0.22);
    }
  }
  float s0 = length(p - src);
  vec2 drift = 0.05 * vec2(cos(u_ph), sin(u_ph));
  float mist = fbm(vec3(p * 2.2 + drift, LZ(2.0)), 2.0);
  float cloud = smoothstep(0.25, 1.4, s0) * exp(-1.0 * s0) * smoothstep(0.3, 0.8, mist);
  return vec2(acc * 1.3 + cloud * 1.4, exp(-1.3 * s0));
}
#else
// B · Ink in water: folded, slowly turning swirls that thicken and thin.
vec2 field(vec2 p) {
  float z = LZ(2.0);
  vec2 q = p * 1.5;
  float a = fbm(vec3(q, z), 2.0);
  float b = fbm(vec3(q + vec2(5.2, 1.3), z + 1.0), 2.0);
  vec2 w = q + 1.7 * vec2(a - 0.5, b - 0.5);
  float c = fbm(vec3(w * 1.3 + vec2(a, b), z), 2.0);
  float ink = smoothstep(0.34, 0.78, c);
  float env = exp(-1.4 * length(p - vec2(-0.25, -0.15)));
  float e = ink * (0.35 + 0.65 * env);
  return vec2(e * 1.15, smoothstep(0.55, 0.85, c));
}
#endif
