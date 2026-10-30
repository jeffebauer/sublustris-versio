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

void main() {
  vec2 p = (gl_FragCoord.xy - 0.5 * u_res) / u_res.y;
  vec2 fe = field(p);
  float e = clamp(fe.x, 0.0, 1.2) * u_amt;
  float b = clamp(fe.y, 0.0, 1.0);
  vec3 dark = vec3(0.047, 0.051, 0.078);   // #0c0d14
  vec3 mist = vec3(0.26, 0.28, 0.36);      // cool grey haze
  vec3 blue = vec3(0.13, 0.33, 0.80);      // freeze blue glow (#2F6FE4 family)
  vec3 col = dark + e * mix(mist, blue, b) * 0.5;
  // static triangular dither (+-1 step) so dark gradients survive 8-bit and compression without banding
  col += (h1(gl_FragCoord.xy) + h1(gl_FragCoord.yx + 17.0) - 1.0) / 255.0;
  gl_FragColor = vec4(col, 1.0);
}
