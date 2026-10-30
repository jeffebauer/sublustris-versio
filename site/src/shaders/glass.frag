// The hero study's glass wordmark (style/hero, version E; owner, 30 Oct 2026: "a more realistic
// glass like Apple's Liquid Glass"). Appended to contours.frag (with GLASS defined), so the glass
// looks through to the very contour lines behind it, at the same time, and bends them:
//   - each letter is a rounded glass tube: a height map (u_hmap, from the logo's distance to its
//     edge) gives a surface normal per pixel;
//   - refraction: the lines are sampled at a point pushed along that normal, strongly at the
//     edges where the surface is steep, not at all on the flat top; red and blue are pushed a
//     little more and less than green (dispersion), so the bent lines fringe;
//   - a fresnel rim (glass reflects more at a grazing angle), and a specular highlight from a
//     light above the module, breathing with its LEDs;
//   - everything is brightest at the centre, behind the module, and falls off toward the ends.
uniform sampler2D u_hmap;   // r: height (high byte), b: height (low byte), g: coverage
uniform vec2 u_gRes;        // this canvas, device px
uniform vec2 u_c;           // this canvas's centre in the band, CSS px from its bottom left
uniform float u_rot;        // 1: the wordmark is turned a quarter left (phones), so its +x runs up the screen
uniform float u_px2css;     // CSS px per device px of this canvas
uniform float u_cScale;     // the band canvas's px per CSS px
uniform float u_bevel;      // the tubes' bevel radius, device px of this canvas
uniform float u_refr;       // refraction at the steepest edge, CSS px
uniform float u_breath;     // the LEDs' breath, 0..1

float hAt(vec2 uv) {
  vec4 t = texture2D(u_hmap, uv);
  return (t.r * 255.0 * 256.0 + t.b * 255.0) / 65535.0;
}

void main() {
  vec2 uv = gl_FragCoord.xy / u_gRes;
  float m = texture2D(u_hmap, uv).g;
  if (m < 0.004) { gl_FragColor = vec4(0.0); return; }
  vec2 e = 1.0 / u_gRes;
  float hx = hAt(uv + vec2(e.x, 0.0)) - hAt(uv - vec2(e.x, 0.0));
  float hy = hAt(uv + vec2(0.0, e.y)) - hAt(uv - vec2(0.0, e.y));
  vec3 n = normalize(vec3(-hx, -hy, 2.0 / u_bevel));   // a slope of 1 where the height climbs 1 over the bevel
  float h = hAt(uv);

  float x = uv.x;
  float focus = mix(0.3, 1.0, exp(-pow((x - 0.5) / 0.26, 2.0)));

  vec2 o = n.xy * u_refr;
  vec2 lc = (gl_FragCoord.xy - 0.5 * u_gRes) * u_px2css + o;   // CSS px from the centre, in the wordmark's own frame
  vec2 css = u_c + (u_rot > 0.5 ? vec2(-lc.y, lc.x) : lc);
  vec3 col;
  vec2 ro = u_rot > 0.5 ? vec2(-o.y, o.x) : o;   // the refraction, in the band's frame
  col.r = bandColor((css + ro * 0.18) * u_cScale).r;
  col.g = bandColor(css * u_cScale).g;
  col.b = bandColor((css - ro * 0.16) * u_cScale).b;

  // the glass body: a touch brighter and cooler than what it looks through
  col = col * 1.45 + vec3(0.04, 0.05, 0.09) * (0.35 + 0.65 * focus);
  // a light frosted tint (owner, 30 Oct: a bit more contrast against the lines, not too much)
  col = mix(col, vec3(0.60, 0.66, 0.82), 0.11 * (0.55 + 0.45 * focus));
  // fresnel rim
  float fr = pow(1.0 - n.z, 2.5);
  col += fr * vec3(0.50, 0.56, 0.72) * (0.35 + 0.65 * focus);
  // specular: a light above the module (canvas centre, well above the top)
  vec3 P = vec3(gl_FragCoord.xy, h * u_bevel);
  vec3 L = normalize(vec3(0.5 * u_gRes.x, 2.6 * u_gRes.y, 1.4 * u_gRes.y) - P);
  vec3 H = normalize(L + vec3(0.0, 0.0, 1.0));
  float sp = pow(max(dot(n, H), 0.0), 45.0) * (0.75 + 0.5 * u_breath) * focus;
  // and a faint second one from below left, so the lower edges catch something too
  vec3 H2 = normalize(normalize(vec3(-0.4, -0.7, 0.6)) + vec3(0.0, 0.0, 1.0));
  float sp2 = pow(max(dot(n, H2), 0.0), 40.0) * 0.22;
  col += (sp * 2.4 + sp2) * vec3(0.93, 0.96, 1.0);

  gl_FragColor = vec4(col * m, m);
}
