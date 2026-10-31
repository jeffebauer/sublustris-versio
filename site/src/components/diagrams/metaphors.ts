// Alternative metaphors for the eight Sublustris diagrams (stop 3, the owner's ask: same line
// style, a different picture of each idea). Same 160 × 64 grid as shapes.ts, same roles:
// `ink` is what you play (and plain objects), `hot` the reverb in the signal colour, `faint`
// guides. Each kind is a different idea of what to picture, not a restyle. The owner picked one per
// concept on 30 Oct 2026 (`picked` in CONCEPTS); the tiles and demos use those kinds.
import type { DiagramKind, Shape } from './shapes';

export const METAPHOR_KINDS = [
  'endless-infinity', 'endless-tie', 'endless-bars',
  'longtail-rulers', 'longtail-trail',
  'freeze-frame', 'freeze-repeat', 'freeze-paint',
  'swell-hairpin', 'swell-bud', 'swell-light',
  'swell-measure', 'swell-family', 'swell-apart', 'swell-natural',
  'octave-string', 'octave-keys', 'octave-staff',
  'tape-drift', 'tape-head', 'tape-ticks',
  'spaces-paths', 'spaces-nested', 'spaces-forms',
  'spaces-plans', 'spaces-figure', 'spaces-depth', 'spaces-rays',
  'cv-sum', 'cv-scale', 'cv-plug',
  'wear-grain', 'wear-roughen', 'wear-floor', 'wear-turn',
  'wear-sweep', 'wear-layers', 'wear-stages', 'wear-roundtrip',
] as const;
export type MetaphorKind = (typeof METAPHOR_KINDS)[number];

const MID = 34;
const f = (n: number) => +n.toFixed(1);
type Pt = [number, number];
const line = (pts: Pt[]) => pts.map(([x, y], i) => `${i ? 'L' : 'M'}${f(x)} ${f(y)}`).join('');
/** y = fn(x) sampled from x0 to x1. */
const plot = (x0: number, x1: number, fn: (x: number) => number, step = 0.5) => {
  const pts: Pt[] = [];
  for (let x = x0; x <= x1 + 1e-6; x += step) pts.push([x, fn(x)]);
  return line(pts);
};
const circle = (cx: number, cy: number, r: number) =>
  `M${f(cx - r)} ${f(cy)}a${r} ${r} 0 1 0 ${f(2 * r)} 0a${r} ${r} 0 1 0 ${f(-2 * r)} 0`;
/** A small solid-looking dot (two rings, so it reads filled at any size). */
const dot = (x: number, y: number) => circle(x, y, 1.2) + circle(x, y, 0.45);
const rect = (x0: number, y0: number, x1: number, y1: number) => `M${f(x0)} ${f(y0)}H${f(x1)}V${f(y1)}H${f(x0)}Z`;
/** A tilted ellipse (a whole-note head). */
const noteHead = (cx: number, cy: number, rx = 7.6, ry = 5.4, tilt = -22) => {
  const t = (tilt * Math.PI) / 180;
  const pts: Pt[] = [];
  for (let i = 0; i <= 48; i++) {
    const a = (2 * Math.PI * i) / 48;
    const x = rx * Math.cos(a), y = ry * Math.sin(a);
    pts.push([cx + x * Math.cos(t) - y * Math.sin(t), cy + x * Math.sin(t) + y * Math.cos(t)]);
  }
  return line(pts) + 'Z';
};
/** One cycle of a chord-like wave, -1..1-ish, for u in 0..1 (the same cycle freeze.ts draws). */
const chord = (u: number) =>
  0.6 * Math.sin(2 * Math.PI * u) + 0.3 * Math.sin(2 * Math.PI * 2 * u + 0.8) + 0.18 * Math.sin(2 * Math.PI * 3 * u + 2.1);
const staff = (x0 = 4, x1 = 156, top = 18) => [0, 1, 2, 3, 4].map((i) => `M${x0} ${top + 8 * i}H${x1}`);
/** A knob's arc, seven o'clock (p = 0) to five o'clock (p = 1). */
const knob = (cx: number, cy: number, r: number) => {
  const at = (p: number, rr = r): Pt => {
    const a = ((-135 + 270 * p) * Math.PI) / 180;
    return [cx + rr * Math.sin(a), cy - rr * Math.cos(a)];
  };
  const arc = (p0: number, p1: number) => {
    const [x0, y0] = at(p0);
    const [x1, y1] = at(p1);
    return `M${f(x0)} ${f(y0)}A${r} ${r} 0 ${(p1 - p0) * 270 > 180 ? 1 : 0} 1 ${f(x1)} ${f(y1)}`;
  };
  const hand = (p: number, r0: number, r1: number) => line([at(p, r0), at(p, r1)]);
  return { at, arc, hand };
};


/** Deterministic noise in -1..1 (the same picture on every build). */
const noise = (i: number) => {
  const v = Math.sin(i * 12.9898 + 78.233) * 43758.5453;
  return 2 * (v - Math.floor(v)) - 1;
};
/** A line along y(x) with grain of height g(x) on it, a jitter every `step`. */
const rough = (x0: number, x1: number, y: (x: number) => number, g: (x: number) => number, step = 1.6, seed = 0) => {
  const pts: Pt[] = [];
  let i = 0;
  for (let x = x0; x <= x1 + 1e-6; x += step, i++) pts.push([x, y(x) + g(x) * noise(i + seed)]);
  return line(pts);
};

export function metaphor(kind: MetaphorKind): Shape {
  switch (kind) {
    // ---- The endless hold -------------------------------------------------------------
    case 'endless-infinity': {
      // The tail's level as one line: it falls from the hit, settles on a level and runs into
      // an ∞. Faint: where an ordinary tail would have fallen to.
      const base = 56, L = 26, P = 48;
      const lvl = (x: number) => base - (L + (P - L) * Math.exp(-(x - 14) / 11));
      const cx = 138, cy = base - L, a = 16;
      const inf: Pt[] = [];
      for (let i = 0; i <= 96; i++) {
        const t = Math.PI + (2 * Math.PI * i) / 96;
        const d = 1 + Math.sin(t) ** 2;
        inf.push([cx + (a * Math.cos(t)) / d, cy + (1.35 * a * Math.sin(t) * Math.cos(t)) / d]);
      }
      return {
        label: 'The endless hold: the tail falls a little from the hit, then runs on at a steady level into infinity.',
        faint: [`M10 ${base}H156`, plot(14, 156, (x) => base - P * Math.exp(-(x - 14) / 24))],
        ink: [`M10 ${base}V8`],
        hot: [plot(14, cx - a, lvl), line(inf)],
      };
    }
    case 'endless-tie': {
      // Notation: one note, its tie running off the edge of the page: held, never released.
      return {
        label: 'The endless hold: one note on a staff, its tie running off the edge of the page, held and never let go.',
        faint: staff(),
        ink: [noteHead(30, 34)],
        hot: ['M37 40C64 53 124 53 162 42'],
      };
    }
    case 'endless-bars': {
      // A level meter over time: the hit, then readings that fall and then stay up, to the edge.
      const bars: string[] = [];
      for (let x = 17; x <= 155; x += 5.75) {
        const h = 20 + 24 * Math.exp(-(x - 17) / 13);
        bars.push(`M${f(x)} 58V${f(58 - h)}`);
      }
      return {
        label: 'The endless hold as a level meter: after the hit the readings fall a little, then stay at one height to the edge.',
        faint: ['M6 58H156', 'M60 38H156'],
        ink: ['M10 58V8'],
        hot: bars,
      };
    }

    // ---- A long tail ---------------------------------------------------------------
    case 'longtail-rulers': {
      // Three rulers to one scale: about 30 s (PLATE), 60 s (HALL), 120 s (CANYON). Ticks every
      // 10 s, a taller one every 30 s; faint lines mark 30 s and 60 s down all three.
      const k = 1.2, x0 = 8;
      const ruler = (y: number, secs: number) => {
        let d = `M${x0} ${y}H${f(x0 + secs * k)}`;
        for (let s = 0; s <= secs; s += 10) d += `M${f(x0 + s * k)} ${y}V${y - (s % 30 === 0 ? 7 : 3.5)}`;
        return d;
      };
      return {
        label: 'Long tails to one scale: about 30 seconds in PLATE, 60 in HALL and 120 in CANYON.',
        faint: [`M${x0 + 30 * k} 4V60`, `M${x0 + 60 * k} 4V60`],
        ink: [ruler(20, 30), ruler(39, 60)],
        hot: [ruler(58, 120)],
      };
    }
    case 'longtail-trail': {
      // One hit, then a trail of dashes that slowly thins out but is still there at the edge.
      const dashes: string[] = [];
      let x = 17, len = 18, gap = 2.4;
      while (x < 156) {
        dashes.push(`M${f(x)} ${MID}H${f(Math.min(156, x + len))}`);
        x += len + gap;
        len = Math.max(1, len * 0.79);
        gap *= 1.2;
      }
      return {
        label: 'A long tail as a trail: one hit, then a line that slowly thins out but is still there at the edge.',
        ink: [`M10 ${MID - 26}V${MID + 26}`],
        hot: dashes,
      };
    }

    // ---- Freeze --------------------------------------------------------------------
    case 'freeze-frame': {
      // A still: what you play runs into a frame; inside, one stretch of it is caught, and the
      // same still carries on, unchanged, after the frame.
      const P = 22, xa = 60, xb = 108;
      const played = plot(4, xa - 4, (x) => MID - 16 * (0.75 + 0.25 * Math.sin(x / 9)) * chord((x - 4) / P + 0.1 * Math.sin(x / 13)));
      const still = (x0: number, x1: number) => plot(x0, x1, (x) => MID - 16 * chord((x - xa) / P));
      const c = 7;
      const corners = [
        `M${xa} ${12 + c}V12H${xa + c}`, `M${xb - c} 12H${xb}V${12 + c}`,
        `M${xa} ${56 - c}V56H${xa + c}`, `M${xb - c} 56H${xb}V${56 - c}`,
      ];
      return {
        label: 'Freeze as a still: what you play is caught in a frame at the moment you tap, and holds unchanged after it.',
        faint: [still(xb + 4, 156)],
        ink: [played, ...corners],
        hot: [still(xa + 4, xb - 4)],
      };
    }
    case 'freeze-repeat': {
      // Repeat signs: the tap starts a repeat; between the signs, the same cycle again and again.
      const P = 26, x0 = 52, x1 = 52 + 3 * P;
      const sign = (x: number, dir: 1 | -1) =>
        [`M${x} 12V56`, `M${x + 3.5 * dir} 12V56`, dot(x + 8.5 * dir, 28), dot(x + 8.5 * dir, 40)];
      return {
        label: 'Freeze as repeat signs: from the tap, one moment of the sound repeats identically, again and again.',
        faint: [],
        ink: [plot(4, 34, (x) => MID - 14 * (0.7 + 0.3 * Math.sin(x / 5)) * chord((x - 4) / 15)), ...sign(39, 1), ...sign(x1 + 12, -1)],
        hot: [plot(x0, x1, (x) => MID - 16 * chord((x - x0) / P))],
      };
    }
    case 'freeze-paint': {
      // The held pad runs across; while the button is held, a phrase above is painted in, and
      // after it the pad's wave has changed a little, at the same level.
      const P = 20, a = 64, b = 110;
      const pad = (x: number) => {
        const u = (x - 4) / P;
        const mix = x < a ? 0 : x > b ? 1 : (x - a) / (b - a);
        return 42 - 13 * (chord(u) + mix * 0.32 * Math.sin(2 * Math.PI * 4 * u + 0.6));
      };
      return {
        label: 'Paint: the frozen pad holds; while you hold the button, a little of what you play blends in, and the pad keeps it.',
        faint: [`M${a} 4V60`, `M${b} 4V60`],
        ink: [plot(a + 4, b - 4, (x) => 13 - 5 * Math.sin((Math.PI * (x - a - 4)) / (b - a - 8)) * Math.sin(2 * Math.PI * (x - a - 4) / 6.5))],
        hot: [plot(4, 156, pad)],
      };
    }

    // ---- BLOOM ---------------------------------------------------------------------
    case 'swell-hairpin': {
      // Dynamics marks: the note, then a long crescendo opening behind it, then a longer,
      // fainter diminuendo.
      return {
        label: 'BLOOM as dynamics: after the note, a crescendo as the reverb swells in, then a long diminuendo as it fades.',
        faint: ['M100 20L156 32M100 48L156 36'],
        ink: [`M10 ${MID - 26}V${MID + 26}`],
        hot: ['M94 20L20 34L94 48'],
      };
    }
    case 'swell-bud': {
      // Three moments of one opening: closed, half open, open. Abstract: arcs from one base.
      const bud = (x: number, w: number, h: number, inner = 0) => {
        const y = 58;
        const out = [`M${x} ${y}Q${f(x - w * 1.1)} ${f(y - h * 0.45)} ${f(x - w)} ${f(y - h)}`, `M${x} ${y}Q${f(x + w * 1.1)} ${f(y - h * 0.45)} ${f(x + w)} ${f(y - h)}`];
        return inner ? [...out, `M${x} ${y}Q${f(x - inner)} ${f(y - h * 0.6)} ${x} ${f(y - h * 1.08)}`, `M${x} ${y}Q${f(x + inner)} ${f(y - h * 0.6)} ${x} ${f(y - h * 1.08)}`] : out;
      };
      return {
        label: 'BLOOM as a bud opening: after the note, the reverb opens out slowly, from closed to half open to wide.',
        faint: ['M20 58H156'],
        ink: [`M10 ${MID - 26}V${MID + 26}`],
        hot: [
          `M38 58Q31 47 38 36Q45 47 38 58`,
          ...bud(80, 11, 34),
          ...bud(132, 21, 50, 10),
        ],
      };
    }
    case 'swell-light': {
      // A light coming up: after the note, a point that glows brighter, its rays growing.
      const sun = (cx: number, cy: number, core: number, ray: number, n = 8) => {
        const rays: string[] = [];
        for (let i = 0; i < n; i++) {
          const a = (2 * Math.PI * i) / n + Math.PI / 8;
          const r0 = core + 2.6, r1 = r0 + ray;
          rays.push(line([[cx + r0 * Math.cos(a), cy + r0 * Math.sin(a)], [cx + r1 * Math.cos(a), cy + r1 * Math.sin(a)]]));
        }
        return [circle(cx, cy, core), ...(ray > 0 ? rays : [])];
      };
      return {
        label: 'BLOOM as a light coming up: after the note, the reverb glows brighter and brighter.',
        ink: [`M10 ${MID - 26}V${MID + 26}`],
        hot: [...sun(30, MID, 1.6, 0), ...sun(56, MID, 2.2, 3.2), ...sun(90, MID, 3, 6.5), ...sun(136, MID, 3.8, 10.5)],
      };
    }

    // ---- SHIMMER -------------------------------------------------------------------
    case 'octave-string': {
      // One string: its plain note (one arc, ink) and the octave (two halves, accent) that
      // sounds when the string is touched at the middle. Faint: the string at rest, the node.
      return {
        label: 'SHIMMER as a string: the note you play, and the same string sounding in two halves, an octave up.',
        faint: ['M8 34H152', 'M80 8V60', 'M8 26V42', 'M152 26V42'],
        ink: [plot(8, 152, (x) => MID - 22 * Math.sin((Math.PI * (x - 8)) / 144))],
        hot: [plot(8, 152, (x) => MID - 13 * Math.sin((2 * Math.PI * (x - 8)) / 144))],
      };
    }
    case 'octave-keys': {
      // A keyboard from C to C: the played C marked in ink, the C an octave up in the accent.
      const x0 = 8, w = 18, top = 8, bot = 58;
      const whites = [rect(x0, top, x0 + 8 * w, bot), ...Array.from({ length: 7 }, (_, i) => `M${x0 + (i + 1) * w} ${top}V${bot}`)];
      // A solid bar at the foot of a key (close lines read as one filled mark).
      const bar = (xa: number, xb: number) => [45, 46, 47, 48, 49, 50, 51, 52, 53].map((y) => `M${xa} ${y}H${xb}`).join('');
      const blacks = [1, 2, 4, 5, 6].map((i) => rect(x0 + i * w - 5, top, x0 + i * w + 5, 38));
      return {
        label: 'SHIMMER on a keyboard: the key you play, and the same note an octave up, added to the reverb.',
        faint: [...whites, ...blacks],
        ink: [bar(x0 + 4, x0 + w - 4)],
        hot: [bar(x0 + 7 * w + 4, x0 + 8 * w - 4)],
      };
    }
    case 'octave-staff': {
      // On a staff: the note alone, then the same note with its octave above it, sounding at once.
      return {
        label: 'SHIMMER on a staff: the note you play alone, then with the same note an octave above it, sounding together.',
        faint: staff(),
        ink: [noteHead(46, 50), noteHead(112, 50)],
        hot: [noteHead(112, 22)],
      };
    }

    // ---- MOTION, tape ----------------------------------------------------------------
    case 'tape-drift': {
      // Pitch over time: a slow, uneven wow around true pitch, with a quick flutter on top.
      const wow = (x: number) => {
        const u = (x - 6) / 150;
        return 9 * Math.sin(2 * Math.PI * 1.15 * u + 0.4) + 3.5 * Math.sin(2 * Math.PI * 2.9 * u + 1.7) + 0.8 * Math.sin(2 * Math.PI * 21 * u);
      };
      return {
        label: 'MOTION to the left as a pitch line: the pitch drifts slowly and unevenly around true, with a quick flutter on top.',
        faint: [`M4 ${MID}H156`, `M4 ${MID - 16}H156`, `M4 ${MID + 16}H156`],
        ink: [],
        hot: [plot(6, 156, (x) => MID - wow(x))],
      };
    }
    case 'tape-head': {
      // A strip of tape running over a playback head: it lies flat where it touches the head
      // and wavers either side, unevenly.
      const hx = 80;
      const tape = (x: number) => {
        const d = Math.min(1, Math.abs(x - hx) / 34);
        const u = x / 156;
        return 30 - 8 * d * d * Math.sin(2 * Math.PI * (4.2 * u + 0.25 * Math.sin(2 * Math.PI * 1.1 * u)));
      };
      return {
        label: 'MOTION to the left as tape over a head: the tape runs flat across the head and wavers unevenly either side.',
        faint: ['M58 56H102'],
        ink: [`M${hx - 13} 56V42Q${hx - 13} 31.5 ${hx} 31.5Q${hx + 13} 31.5 ${hx + 13} 42V56`],
        hot: [plot(4, 156, tape)],
      };
    }
    case 'tape-ticks': {
      // Time marks on tape: evenly spaced above (faint, as recorded), crowding and stretching
      // below (as played back on a wavering transport).
      const n = 25, x0 = 8, step = 6;
      const even = Array.from({ length: n }, (_, i) => `M${x0 + i * step} 10V20`);
      const warped = Array.from({ length: n }, (_, i) => {
        const u = i / (n - 1);
        const x = x0 + i * step + 5.5 * Math.sin(2 * Math.PI * 1.3 * u + 0.5) + 1.2 * Math.sin(2 * Math.PI * 7 * u);
        return `M${f(x)} 40V56`;
      });
      return {
        label: 'MOTION to the left as time marks: even as recorded, then crowding and stretching as the tape speed wavers.',
        faint: [...even, 'M4 15H156'],
        ink: [],
        hot: [...warped, 'M4 48H156'],
      };
    }

    // ---- Three spaces --------------------------------------------------------------------
    case 'spaces-paths': {
      // A sound's path in three spaces: bouncing fast between close walls (PLATE), slower
      // between far ones (HALL), and in CANYON spreading out with nothing to stop it.
      const zig = (x0: number, x1: number, yA: number, yB: number, n: number) => {
        const pts: Pt[] = [];
        for (let i = 0; i <= n; i++) pts.push([x0 + ((x1 - x0) * i) / n, i % 2 ? yA : yB]);
        return line(pts);
      };
      return {
        label: 'Three spaces as sound paths: dense bounces between close walls in PLATE, slower ones in HALL, and in CANYON no walls at all.',
        ink: ['M6 25H38M6 43H38', 'M46 10H98M46 58H98', 'M104 58H156'],
        hot: [zig(6, 38, 25, 43, 8), zig(46, 98, 10, 58, 4), 'M106 58L156 6M106 58L156 26M106 58L156 44'],
      };
    }
    case 'spaces-nested': {
      // Three rooms around one floor: a small one inside a bigger one, inside CANYON, whose
      // walls rise past the frame and fade out with no roof.
      return {
        label: 'Three spaces nested: a small PLATE inside a bigger HALL, inside CANYON, whose walls rise and fade with no roof.',
        faint: ['M10 26V2', 'M150 26V2'],
        ink: ['M68 58V44H92V58', 'M40 58V22H120V58'],
        hot: ['M2 58H158', 'M10 58V26', 'M150 58V26'],
      };
    }
    case 'spaces-forms': {
      // The three names as things: a hung plate, a vaulted hall, an open horizon.
      return {
        label: 'Three spaces as places: a hanging metal plate, a vaulted hall, and an open horizon with no end.',
        faint: ['M108 58L134 30M126 58L134 30M144 58L134 30M160 54L134 30', 'M48 58H98'],
        ink: ['M10 24L24 8L38 24', rect(6, 24, 42, 30), 'M54 58V32A19 19 0 0 1 92 32V58'],
        hot: ['M106 30H160'],
      };
    }

    // ---- BLOOM, second round: the late arrival as the subject (owner, 29 Oct) ----------
    // BLOOM is a swell time (about 0.1 to 4 s): the reverb starts near nothing and rises on
    // a smooth curve to a peak well after the note, then falls for a long time. It is not a
    // silent gap; that is NATURAL's predelay (0 to 250 ms), drawn only in swell-natural.
    case 'swell-measure': {
      // A dimension line from the hit to the peak: the swell time, measured like a drawing.
      return {
        label: 'BLOOM measured: after the note the reverb rises from almost nothing to a peak well after it; the swell time, about 0.1 to 4 seconds, runs from the note to the peak.',
        faint: ['M10 56H156', 'M80 9V14'],
        ink: ['M10 56V16', 'M10 2V9M80 2V9', 'M10 5.5H80', 'M15 3L10 5.5L15 8', 'M75 3L80 5.5L75 8'],
        hot: ['M12 55.6C44 55.6 54 14 80 14C106 14 124 41 156 50'],
      };
    }
    case 'swell-family': {
      // Three swell times on one note: short, middle and long (DEPTH turned up), the longest
      // in the accent. Each rises from nothing, peaks later, then falls.
      const env = (tp: number) =>
        `M12 55.6C${f(12 + (tp - 12) * 0.55)} 55.6 ${f(tp - (tp - 12) * 0.4)} 14 ${tp} 14C${f(tp + (156 - tp) * 0.3)} 14 ${f(tp + (156 - tp) * 0.55)} 44 156 51`;
      return {
        label: 'BLOOM from short to long: the same note with three swell times; the longer DEPTH is set, the later the reverb peaks.',
        faint: ['M10 56H156', env(26), env(52)],
        ink: ['M10 56V8'],
        hot: [env(104)],
      };
    }
    case 'swell-apart': {
      // The note as one stroke; the swell as a soft curve of its own, starting low beside it.
      return {
        label: 'BLOOM: the note, then, beside it, a soft swell that lifts slowly, peaks late and fades for a long time.',
        faint: [],
        ink: ['M12 58V10'],
        hot: ['M22 57C50 57 62 18 90 18C116 18 132 42 156 48'],
      };
    }
    case 'swell-natural': {
      // For contrast, NATURAL: a silent gap (the predelay, bracketed), then the reverb at once.
      return {
        label: 'For contrast, NATURAL’s predelay: a silent gap of up to a quarter of a second, then the reverb arrives at once and fades.',
        faint: ['M10 56H156'],
        ink: ['M10 56V16', 'M10 2V9M40 2V9', 'M10 5.5H40', 'M14 3L10 5.5L14 8', 'M36 3L40 5.5L36 8'],
        hot: ['M40 56V14C70 16 112 42 156 51'],
      };
    }

    // ---- Spaces, second round: real spaces at growing scale (owner, 29 Oct) ------------
    case 'spaces-plans': {
      // Floor plans to one scale: a small chamber, a long hall with a stage at its end, and
      // CANYON, whose walls open out and run off the drawing.
      return {
        label: 'Three spaces as floor plans to one scale: a small chamber for PLATE, a long hall with a stage for HALL, and CANYON, whose walls open out and run off the page.',
        faint: ['M4 61H24M4 59V63M24 59V63'],
        ink: [rect(6, 40, 22, 56), rect(30, 28, 104, 56), 'M92 34H101V50H92Z'],
        hot: ['M160 4L112 22V46L160 60'],
      };
    }
    case 'spaces-figure': {
      // Sections with the same small listener in each: a low chamber, a vaulted hall, and
      // CANYON: a floor to the edge, one wall fading up into open air.
      const person = (x: number, b = 58) =>
        [circle(x, b - 11.2, 1.8), `M${x} ${b - 9.2}V${b - 4}`, `M${x - 2.6} ${b}L${x} ${b - 4}L${x + 2.6} ${b}`, `M${x - 2.6} ${b - 5.2}L${x} ${b - 7.8}L${x + 2.6} ${b - 5.2}`].join('');
      return {
        label: 'Three spaces in section, each with the same small listener: a low chamber, a tall vaulted hall, and CANYON, open to the sky with no far wall.',
        faint: ['M108 30V2'],
        ink: ['M4 58V40H30V58H4', 'M38 58V30A40 40 0 0 1 98 30V58H38', person(17), person(68), person(132)],
        hot: ['M104 58H160', 'M108 58V30'],
      };
    }
    case 'spaces-depth': {
      // Rooms in one-point perspective: a shallow one, a deep one, and CANYON with no walls:
      // open ground running to a horizon (an open box would draw an X).
      const box = (x0: number, x1: number, y0: number, y1: number, d: number) => {
        const cx = (x0 + x1) / 2, cy = (y0 + y1) / 2;
        const k = 1 - d;
        const bx0 = cx + (x0 - cx) * k, bx1 = cx + (x1 - cx) * k, by0 = cy + (y0 - cy) * k, by1 = cy + (y1 - cy) * k;
        return [rect(bx0, by0, bx1, by1), line([[x0, y0], [bx0, by0]]), line([[x1, y0], [bx1, by0]]), line([[x0, y1], [bx0, by1]]), line([[x1, y1], [bx1, by1]])];
      };
      return {
        label: 'Three spaces in perspective: a shallow room for PLATE, a deep one for HALL, and CANYON, no walls at all, open ground running to the horizon.',
        faint: [rect(4, 22, 38, 56), rect(46, 10, 100, 58), 'M106 62L129.8 33.8M120 62L131.4 33.8M146 62L134.6 33.8M160 62L136.2 33.8', 'M106.0 62.0H160.0M118.2 47.6H147.8M124.9 39.6H141.1'],
        ink: [...box(4, 38, 22, 56, 0.3), ...box(46, 100, 10, 58, 0.72)],
        hot: ['M106 30H160'],
      };
    }
    case 'spaces-rays': {
      // Plans with one sound ray each, from a source: many short bounces in the chamber,
      // fewer, longer ones in the hall; in CANYON the ray leaves through walls that fade.
      const bounce = (x0: number, y0: number, x1: number, y1: number, sx: number, sy: number, dx: number, dy: number, len: number) => {
        const pts: Pt[] = [[sx, sy]];
        let x = sx, y = sy, left = len;
        const n = Math.hypot(dx, dy); dx /= n; dy /= n;
        while (left > 0) {
          const tx = dx > 0 ? (x1 - x) / dx : (x0 - x) / dx;
          const ty = dy > 0 ? (y1 - y) / dy : (y0 - y) / dy;
          const t = Math.min(tx, ty, left);
          x += dx * t; y += dy * t; left -= t;
          pts.push([x, y]);
          if (t === tx) dx = -dx; else if (t === ty) dy = -dy;
        }
        return line(pts);
      };
      return {
        label: 'Three spaces as a sound ray in each: many short bounces in a small PLATE, fewer, longer ones in HALL, and in CANYON it travels off through walls that fade.',
        faint: ['M112 6H136', 'M112 58H136', 'M112 6V58'],
        ink: [rect(4, 36, 26, 58), rect(34, 18, 96, 58), dot(9, 52), dot(42, 50), dot(120, 46)],
        hot: [bounce(4, 36, 26, 58, 9, 52, 1, -1.7, 120), bounce(34, 18, 96, 58, 42, 50, 1, -0.42, 230), 'M120 46L160 22'],
      };
    }

    // ---- CV --------------------------------------------------------------------------
    case 'cv-sum': {
      // A sum: the knob where your hand set it, plus a voltage (0 to 5 V), equals the knob
      // turned further.
      const k1 = knob(20, 36, 13), k2 = knob(140, 36, 13);
      const p = 0.38, q = 0.76;
      return {
        label: 'CV as a sum: the knob where your hand set it, plus a voltage of 0 to 5 V, makes the knob turned further.',
        faint: [k1.arc(0, 1), k2.arc(0, 1)],
        ink: [k1.arc(0, p), k1.hand(p, 3, 9), 'M43 36H53M48 31V41', 'M97 33H107M97 39H107', k2.arc(0, p)],
        hot: ['M60 50H68V22H86', k2.arc(p, q), k2.hand(q, 3, 9)],
      };
    }
    case 'cv-scale': {
      // A scale: the marker where the hand set it, and the voltage pushing it along the scale.
      const x0 = 8, x1 = 152, y = 44, hand = 52, push = 112;
      const ticks = Array.from({ length: 11 }, (_, i) => `M${f(x0 + ((x1 - x0) * i) / 10)} ${y}V${y + (i % 5 ? 4 : 8)}`);
      return {
        label: 'CV as a push along a scale: the voltage moves the setting on from where your hand left it.',
        faint: [`M${x0} ${y}H${x1}`, ...ticks, `M${hand - 5} 22L${hand} 30L${hand + 5} 22Z`],
        ink: [`M${hand} ${y}V34`],
        hot: [`M${hand} ${y}H${push}`, `M${push - 5} 22L${push} 30L${push + 5} 22Z`, `M${push} ${y}V34`],
      };
    }
    case 'cv-plug': {
      // A patch cable plugged into a panel, and behind the panel the knob it turns further.
      const k = knob(122, 36, 20);
      const p = 0.38, q = 0.78;
      return {
        label: 'CV as a patch cable: a voltage plugged in beside a knob turns it on past where your hand set it.',
        faint: ['M72 6V62', k.arc(0, 1)],
        ink: [rect(40, 30, 58, 42), 'M58 33H66V39H58', 'M66 34.5H72', k.arc(0, p)],
        hot: ['M2 58C18 58 22 36 40 36', k.arc(p, q), k.hand(q, 6, 15)],
      };
    }

    // ---- WEAR (the worn guitar and the dark drone, 1 Nov 2026) --------------------------
    case 'wear-grain': {
      // Strummed chords (ink), each with its tail (blue); grain specks gather on the tails as
      // WEAR comes up across the clip.
      const hits = [8, 46, 84, 122];
      const tails = hits.map((x) => plot(x + 1, x + 34, (t) => 34 - 18 * Math.exp(-(t - x) / 11) * Math.sin((t - x) * 0.9), 0.5));
      const specks: string[] = [];
      hits.forEach((x, k) => {
        const n = [0, 4, 9, 15][k];
        for (let j = 0; j < n; j++) {
          const sx = x + 6 + ((j * 7.3 + k * 3) % 28), sy = 34 + 14 * noise(j + 10 * k);
          specks.push(`M${f(sx)} ${f(sy)}h0.6`);
        }
      });
      return {
        label: 'WEAR turned up across four chords: the first tail is clean, and each one after carries more grain.',
        ink: hits.map((x) => `M${x} 54V14`),
        hot: [...tails, ...specks],
      };
    }
    case 'wear-roughen': {
      // One long tail, fading as tails do, its line growing rough from clean on the left to
      // grainy on the right as WEAR comes up.
      return {
        label: 'WEAR turned up: one long tail whose line goes from clean to rough and grainy.',
        faint: ['M10 54H156'],
        ink: ['M10 54V10'],
        hot: [rough(12, 156, (x) => 54 - 38 * Math.exp(-(x - 12) / 70), (x) => 5.5 * ((x - 12) / 144) ** 1.6, 1.4)],
      };
    }
    case 'wear-floor': {
      // Chords as strokes over a hiss floor that rises from nothing as WEAR comes up.
      const hits = [16, 52, 88, 124];
      return {
        label: 'WEAR turned up under the chords: a floor of hiss rises from nothing to a band.',
        faint: ['M6 54H156'],
        ink: hits.map((x) => `M${x} 54V12`),
        hot: [rough(6, 156, (x) => 54 - 7 * ((x - 6) / 150), (x) => 6 * ((x - 6) / 150), 1.2, 3)],
      };
    }
    case 'wear-turn': {
      // The WEAR knob turned from fully left to 2 o'clock; beside it the tail that comes out,
      // clean, then gaining grain.
      const k = knob(24, 34, 16);
      const p = 0.78;
      return {
        label: 'The WEAR knob turned from fully left to 2 o\'clock, and beside it a tail going from clean to grainy.',
        faint: [k.arc(p, 1)],
        ink: [k.hand(p, 5, 13)],
        hot: [k.arc(0, p), rough(54, 156, (x) => 34 - 20 * Math.sin((x - 54) * 0.45) * Math.exp(-(x - 54) / 34), (x) => 4 * ((x - 54) / 102) ** 1.4, 1.3, 7)],
      };
    }
    case 'wear-sweep': {
      // Above, faint: WEAR's position over the clip (up, held, back down). Below, the endless
      // tail (blue) as a level line that roughens with it and comes clean again.
      const pos = (x: number) => (x < 30 ? 0 : x < 76 ? (x - 30) / 46 : x < 100 ? 1 : x < 140 ? 1 - (x - 100) / 40 : 0);
      return {
        label: 'WEAR over the whole clip: turned up, held, turned back down, and the endless tail grows rough with it, then comes clean.',
        faint: [plot(6, 156, (x) => 20 - 12 * pos(x), 1), 'M6 20H156'],
        ink: [],
        hot: [rough(6, 156, () => 44, (x) => 7 * pos(x), 1.2, 11)],
      };
    }
    case 'wear-layers': {
      // The drone's lows (ink, a slow wave) stay the same throughout; above them the wear (blue)
      // thickens into a band of grain and thins away again.
      const env = (x: number) => Math.sin(Math.PI * Math.min(1, Math.max(0, (x - 14) / 132))) ** 1.5;
      return {
        label: 'The drone\'s lows stay the same throughout while the wear above them thickens into grain and thins away again.',
        ink: [plot(6, 156, (x) => 48 + 4 * Math.sin(x * 0.09), 0.5)],
        hot: [rough(6, 156, () => 22, (x) => 10 * env(x), 1.1, 5), rough(6, 156, () => 22, (x) => 6 * env(x), 1.7, 41)],
      };
    }
    case 'wear-stages': {
      // Along the tail, what WEAR adds as it goes: clean, tape's slow wobble, grit, an old
      // radio's narrow band, then clean again on the way back.
      const y = 34;
      return {
        label: 'WEAR\'s stages along the tail: clean, tape wobble, grit, an old radio\'s narrow band, then clean again.',
        faint: ['M100 29H128', 'M100 39H128'],
        ink: [],
        hot: [
          `M6 ${y}H26`,
          plot(26, 62, (x) => y + 5 * Math.sin((x - 26) * 0.35), 0.5),
          rough(62, 100, () => y, () => 7, 1.2, 17),
          plot(100, 128, (x) => y + 3.5 * Math.sin((x - 100) * 1.3), 0.25),
          `M128 ${y}H156`,
        ],
      };
    }
    case 'wear-roundtrip': {
      // The WEAR knob's whole range, there and back (two arcs with arrowheads), beside an
      // endless level line that is rough in the middle.
      const k = knob(28, 34, 19);
      const k2 = knob(28, 34, 13);
      const [ex, ey] = k.at(1), [sx, sy] = k2.at(0);
      return {
        label: 'WEAR turned through its whole range and back, beside an endless tail that is rough in the middle and clean at both ends.',
        faint: [],
        ink: [k.hand(0, 3, 9)],
        hot: [k.arc(0, 1), `M${f(ex - 4)} ${f(ey - 2)}L${f(ex)} ${f(ey)}L${f(ex + 1)} ${f(ey - 4.5)}`, k2.arc(0, 1),
          `M${f(sx - 1)} ${f(sy - 4.5)}L${f(sx)} ${f(sy)}L${f(sx + 4)} ${f(sy - 2)}`,
          rough(62, 156, () => 34, (x) => 6 * Math.sin(Math.PI * (x - 62) / 94) ** 2, 1.2, 23)],
      };
    }
  }
}

/** The eight concepts, each with today's drawing as option 1 and the alternatives after. */
export interface Option { kind: DiagramKind; caption: string; risk?: string }
/** `picked`: the owner's choice, 1-based (30 Oct 2026). */
export interface Concept { id: string; name: string; picked: number; options: Option[] }
export const CONCEPTS: Concept[] = [
  { id: 'endless', name: 'The endless hold', picked: 2, options: [
    { kind: 'endless', caption: 'Today: a tail that levels off' },
    { kind: 'endless-infinity', caption: 'A level line into ∞' },
    { kind: 'endless-tie', caption: 'A note tied off the page', risk: 'at audio-tile size the note head shrinks to a small oval on a swoosh; without the staff it could pass for a magnifier on a cord.' },
    { kind: 'endless-bars', caption: 'A meter that stays up' },
  ] },
  { id: 'longtail', name: 'A long tail', picked: 1, options: [
    { kind: 'longtail', caption: 'Today: a slow-fading wave' },
    { kind: 'longtail-rulers', caption: 'Rulers: 30, 60, 120 s' },
    { kind: 'longtail-trail', caption: 'A thinning trail' },
  ] },
  { id: 'freeze', name: 'Freeze', picked: 1, options: [
    { kind: 'freeze', caption: 'Today: one cycle held at a line' },
    { kind: 'freeze-frame', caption: 'A still in a frame' },
    { kind: 'freeze-repeat', caption: 'Repeat signs' },
    { kind: 'freeze-paint', caption: 'A pad, painted into' },
  ] },
  { id: 'swell', name: 'BLOOM: a swell', picked: 6, options: [
    { kind: 'swell', caption: 'Today: a wave swelling in' },
    { kind: 'swell-hairpin', caption: 'Crescendo, then diminuendo', risk: 'the two hairpins together can close into a flat diamond or a lens, which edges towards an eye.' },
    { kind: 'swell-bud', caption: 'A bud opening', risk: 'the open stage can read as a tulip, a flame or a fork.' },
    { kind: 'swell-light', caption: 'A light coming up', risk: 'reads as the screen-brightness icon, or a row of suns from a weather app.' },
    { kind: 'swell-measure', caption: 'Note to peak, measured' },
    { kind: 'swell-family', caption: 'Short, middle, long swell' },
    { kind: 'swell-apart', caption: 'A note, then a soft rise' },
    { kind: 'swell-natural', caption: 'Contrast: NATURAL’s predelay gap' },
  ] },
  { id: 'octave', name: 'SHIMMER: an octave up', picked: 1, options: [
    { kind: 'octave', caption: 'Today: a wave at twice the speed' },
    { kind: 'octave-string', caption: 'A string in halves' },
    { kind: 'octave-keys', caption: 'Two keys, C to C' },
    { kind: 'octave-staff', caption: 'Two notes on a staff' },
  ] },
  { id: 'tape', name: 'MOTION: tape wobble', picked: 3, options: [
    { kind: 'tape', caption: 'Today: a wavering tone' },
    { kind: 'tape-drift', caption: 'Pitch drifting round true' },
    { kind: 'tape-head', caption: 'Tape over a head', risk: 'the head is an arched block, which can read as a door, a tombstone or a letterbox.' },
    { kind: 'tape-ticks', caption: 'Time marks stretching', risk: 'two rows of ticks can read as a barcode or a comb.' },
  ] },
  { id: 'spaces', name: 'PLATE · HALL · CANYON', picked: 1, options: [
    { kind: 'spaces', caption: 'Today: three rooms in a row' },
    { kind: 'spaces-paths', caption: 'Sound paths bouncing' },
    { kind: 'spaces-nested', caption: 'Rooms inside rooms' },
    { kind: 'spaces-forms', caption: 'Plate, hall, horizon', risk: 'the hung plate can read as a picture on a wall, the hall as a doorway.' },
    { kind: 'spaces-plans', caption: 'Floor plans, one scale', risk: 'CANYON’s fan-shaped plan can read as a megaphone or a funnel.' },
    { kind: 'spaces-figure', caption: 'Sections with a listener' },
    { kind: 'spaces-depth', caption: 'Rooms in perspective', risk: 'PLATE’s shallow box can read as a picture frame, and CANYON’s open ground as a road.' },
    { kind: 'spaces-rays', caption: 'A sound ray in each' },
  ] },
  { id: 'cv', name: 'CV inputs', picked: 3, options: [
    { kind: 'cv', caption: 'Today: a voltage turning the arc' },
    { kind: 'cv-sum', caption: 'Knob + voltage = further' },
    { kind: 'cv-scale', caption: 'A push along a scale' },
    { kind: 'cv-plug', caption: 'A cable beside a knob', risk: 'the plug, side on, can read as a battery or a USB stick.' },
  ] },
  { id: 'wear-guitar', name: 'WEAR turned up (Worn guitar)', picked: 2, options: [
    { kind: 'wear-grain', caption: 'Grain gathering on each chord’s tail' },
    { kind: 'wear-roughen', caption: 'One tail, clean to rough' },
    { kind: 'wear-floor', caption: 'A hiss floor rising under the chords' },
    { kind: 'wear-turn', caption: 'The knob turned, a tail gaining grain' },
  ] },
  { id: 'wear-drone', name: 'WEAR up and back (Dark drone)', picked: 3, options: [
    { kind: 'wear-sweep', caption: 'The knob’s path, the tail roughening with it' },
    { kind: 'wear-layers', caption: 'Lows steady, wear thickening and thinning' },
    { kind: 'wear-stages', caption: 'Clean, tape, grit, radio, clean' },
    { kind: 'wear-roundtrip', caption: 'The whole range and back', risk: 'the double arc with arrows can read as a refresh or sync icon.' },
  ] },
];
