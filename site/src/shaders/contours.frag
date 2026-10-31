// Contour lines (waves B, "dissolving"), behind the hero (owner, 30 Oct 2026: in place of the
// haze; the same pick as Resilio's site). Thin lines lifted by a slow swell, crisp at the left,
// widening and fraying toward the right like a sound's tail. From the site-visuals study
// (docs/prototypes/site-visuals: common.glsl + waves.glsl, V 1). Live only; the still
// (public/visuals/contours.jpg) is rendered from this same file (u_live 0, phase STILL_PH).
#define V 1
#define STILL_PH 1.884956
// Shared prelude for every study. Written for this project (no third-party shader code).
// Each study defines `vec2 field(vec2 p)`: x = light energy (0..1), y = how much of it is
// freeze blue (0 = cool grey haze, 1 = blue). main() maps that onto the dark band palette.
precision highp float;
uniform vec2 u_res;     // render size in pixels
uniform float u_ph;     // loop phase: 0..TAU over one loop
uniform float u_t;      // seconds since start (live only; never wraps back)
uniform float u_loopT;  // loop length in seconds
uniform float u_live;   // 1 = live: time runs forward forever; 0 = seamless loop (files, stills)
uniform float u_amt;    // strength of the layer
#define TAU 6.28318530718

float h1(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
vec2 h2(vec2 p) {
  return fract(sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)))) * 43758.5453);
}
float h4(vec3 p, float w) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7)) + w * 113.31) * 43758.5453); }

// Static 2D value noise (for fixed textures).
float vn2(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  float a = h1(i), b = h1(i + vec2(1.0, 0.0)), c = h1(i + vec2(0.0, 1.0)), d = h1(i + vec2(1.0, 1.0));
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

// 4D value noise: xy is space, zw is time. Time travels through zw along a path
// (see tpath), so the texture morphs without ever running backwards.
float vn4(vec4 p) {
  vec4 i = floor(p);
  vec4 f = fract(p);
  vec4 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  float r[2];
  for (int k = 0; k < 2; k++) {
    float w = i.w + float(k);
    float a = h4(vec3(i.xy, i.z), w);
    float b = h4(vec3(i.xy + vec2(1.0, 0.0), i.z), w);
    float c = h4(vec3(i.xy + vec2(0.0, 1.0), i.z), w);
    float d = h4(vec3(i.xy + vec2(1.0, 1.0), i.z), w);
    float e = h4(vec3(i.xy, i.z + 1.0), w);
    float f2 = h4(vec3(i.xy + vec2(1.0, 0.0), i.z + 1.0), w);
    float g = h4(vec3(i.xy + vec2(0.0, 1.0), i.z + 1.0), w);
    float h = h4(vec3(i.xy + vec2(1.0, 1.0), i.z + 1.0), w);
    float lo = mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
    float hi = mix(mix(e, f2, u.x), mix(g, h, u.x), u.y);
    r[k] = mix(lo, hi, u.z);
  }
  return mix(r[0], r[1], u.w);
}

// Where "now" is in the zw time plane, for a path of radius R (in noise cells).
// Loop: once round a circle of radius R per loop, so the file closes seamlessly and never
// reverses (a big enough circle is not a back-and-forth). Live: a straight line at the same
// speed, so the live version never repeats at all.
// Live starts exactly at the still frame (phase STILL_PH, injected by build.py) and leaves
// along the circle's tangent, so the first live frame matches the fallback image.
float liveT() { return u_live * (u_t - STILL_PH / TAU * u_loopT); }
vec2 tpath(float R) {
  if (u_live > 0.5) {
    vec2 c = vec2(cos(STILL_PH), sin(STILL_PH));
    return R * c + vec2(-c.y, c.x) * (liveT() * TAU * R / u_loopT);
  }
  return R * vec2(cos(u_ph), sin(u_ph));
}

// Fractal sum of vn4; each octave doubles space and multiplies the time path by 1.6
// (finer detail drifts a little faster, as in real smoke). Octaves: 1..5.
float fbm4(vec2 p, vec2 tp, int oct) {
  float s = 0.0;
  float a = 0.5;
  float norm = 0.0;
  mat2 rm = mat2(0.8, -0.6, 0.6, 0.8);
  for (int k = 0; k < 5; k++) {
    if (k >= oct) break;
    s += a * vn4(vec4(p, tp + float(k) * 5.17));
    norm += a;
    p = rm * p * 2.03 + 17.0;
    tp *= 1.6;
    a *= 0.5;
  }
  // stretch around 0.5: each extra interpolation in 4D flattens value noise toward grey
  return clamp(0.5 + (s / norm - 0.5) * 1.3, 0.0, 1.0);
}

vec2 field(vec2 p);

// The band's colour at a pixel of the band's canvas (fc, from the bottom left). The hero study's
// glass wordmark (style/hero) samples it through its refraction, so it defines GLASS and its own main().
vec3 bandColor(vec2 fc) {
  vec2 p = (fc - 0.5 * u_res) / u_res.y;
  vec2 fe = field(p);
  float e = clamp(fe.x, 0.0, 1.2) * u_amt;
  float b = clamp(fe.y, 0.0, 1.0);
  vec3 dark = vec3(0.047, 0.051, 0.078);   // #0c0d14
  vec3 mist = vec3(0.26, 0.28, 0.36);      // cool grey haze
  vec3 blue = vec3(0.13, 0.33, 0.80);      // freeze blue glow (#2F6FE4 family)
  vec3 col = dark + e * mix(mist, blue, b) * 0.5;
  // static triangular dither (+-1 step) so dark gradients survive 8-bit and compression without banding
  col += (h1(fc) + h1(fc.yx + 17.0) - 1.0) / 255.0;
  return col;
}
#ifndef GLASS
void main() { gl_FragColor = vec4(bandColor(gl_FragCoord.xy), 1.0); }
#endif

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
