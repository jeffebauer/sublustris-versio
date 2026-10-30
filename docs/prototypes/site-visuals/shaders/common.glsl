// Shared prelude for every study. Written for this project (no third-party shader code).
// Each study defines `vec2 field(vec2 p)`: x = light energy (0..1), y = how much of it is
// freeze blue (0 = cool grey haze, 1 = blue). main() maps that onto the site palette.
precision highp float;
uniform vec2 u_res;     // render size in pixels
uniform float u_ph;     // loop phase: 0..TAU over one loop, so every motion term repeats seamlessly
uniform float u_light;  // 0 = dark band (#0c0d14), 1 = white page
uniform float u_amt;    // strength of the layer
#define TAU 6.28318530718

float h1(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
vec2 h2(vec2 p) {
  return fract(sin(vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)))) * 43758.5453);
}
float h3(vec3 p) { return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453); }

// Value noise whose z axis repeats every P units: travelling z from 0 to P over the loop
// makes the texture morph and come back to where it started.
float vn(vec3 p, float P) {
  vec3 i = floor(p);
  vec3 f = fract(p);
  vec3 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
  float z0 = mod(i.z, P);
  float z1 = mod(i.z + 1.0, P);
  float a = h3(vec3(i.xy, z0));
  float b = h3(vec3(i.xy + vec2(1.0, 0.0), z0));
  float c = h3(vec3(i.xy + vec2(0.0, 1.0), z0));
  float d = h3(vec3(i.xy + vec2(1.0, 1.0), z0));
  float e = h3(vec3(i.xy, z1));
  float f2 = h3(vec3(i.xy + vec2(1.0, 0.0), z1));
  float g = h3(vec3(i.xy + vec2(0.0, 1.0), z1));
  float h = h3(vec3(i.xy + vec2(1.0, 1.0), z1));
  float lo = mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
  float hi = mix(mix(e, f2, u.x), mix(g, h, u.x), u.y);
  return mix(lo, hi, u.z);
}

// Five octaves; each octave doubles z and its period, so the loop still closes.
float fbm(vec3 p, float P) {
  float s = 0.0;
  float a = 0.5;
  mat2 r = mat2(0.8, -0.6, 0.6, 0.8);
  for (int k = 0; k < 5; k++) {
    s += a * vn(p, P);
    p = vec3(r * p.xy * 2.03 + 17.0, p.z * 2.0);
    P *= 2.0;
    a *= 0.5;
  }
  return s / 0.96875;
}

// z position for a noise whose period is P
float LZ(float P) { return u_ph / TAU * P; }

vec2 field(vec2 p);

void main() {
  vec2 p = (gl_FragCoord.xy - 0.5 * u_res) / u_res.y;
  vec2 fe = field(p);
  float e = clamp(fe.x, 0.0, 1.2) * u_amt;
  float b = clamp(fe.y, 0.0, 1.0);
  vec3 dark = vec3(0.047, 0.051, 0.078);   // #0c0d14
  vec3 mist = vec3(0.26, 0.28, 0.36);      // cool grey haze
  vec3 blue = vec3(0.13, 0.33, 0.80);      // freeze blue glow (#2F6FE4 family)
  vec3 colD = dark + e * mix(mist, blue, b) * 0.5;
  vec3 paleK = mix(vec3(0.16, 0.13, 0.06), vec3(0.62, 0.42, 0.10), b);  // what a full-strength glow takes away from white
  vec3 colL = vec3(1.0) - e * paleK * 0.5;
  vec3 col = mix(colD, colL, u_light);
  // static triangular dither (+-1 step) so dark gradients survive 8-bit and compression without banding
  col += (h1(gl_FragCoord.xy) + h1(gl_FragCoord.yx + 17.0) - 1.0) / 255.0;
  gl_FragColor = vec4(col, 1.0);
}
