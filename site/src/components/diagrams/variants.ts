// Alternative drawing sets for the tiny diagrams (stop 3, for the owner to compare on
// /style/diagrams). The default set stays `line` (shapes.ts). Each alternative draws the same
// eight Sublustris diagrams on the same 160 × 64 grid, from the same envelopes, so every set
// tells the same story about the sound (docs/minisite/content/manual.md):
//   endless  falls a little, then settles at a steady level and holds
//   longtail one hit, still ringing at the edge
//   freeze   what you played, then one moment held as a still, repeating pattern
//   swell    BLOOM: the reverb swells in behind the hit, peaks late, then fades
//   octave   SHIMMER: the note and one steady octave above, nothing higher
//   tape     MOTION left: cycles crowd and stretch, one short dropout near the end
//   spaces   PLATE short, HALL longer, MASSIVE swelling in and running off the edge
//   cv       the knob's own position plus the voltage carrying it further
// Colours only through roles: `ink` is the text colour, `hot` the signal (accent), `faint` a
// see-through text colour. Diagram.astro turns roles into currentColor / var(--signal).
import type { DiagramKind } from './shapes';

export type Variant = 'line' | 'glow' | 'dust' | 'meter';
export type Role = 'ink' | 'hot' | 'faint';

export type El =
  /** A stroked line. */
  | { k: 'line'; d: string; role: Role; w?: number; dash?: string; cap?: 'round' | 'butt' }
  /** A filled shape, fading toward the floor (the role's vertical gradient). */
  | { k: 'area'; d: string; role: Role }
  /** A filled shape at one flat opacity. */
  | { k: 'tint'; d: string; role: Role; o: number }
  | { k: 'dot'; x: number; y: number; r: number; role: Role; o?: number }
  | { k: 'cell'; x: number; y: number; w: number; h: number; role: Role; o?: number };

export interface AltShape { label: string; els: El[] }

export const VARIANTS: Record<Variant, { name: string; line: string }> = {
  line: { name: 'Line', line: 'Today’s set: 1.5 px lines in the text colour, the reverb in blue, faint guides. The waveform itself.' },
  glow: { name: 'Glow', line: 'The tail as a soft filled envelope that brightens where it’s loud and fades toward the paper. Level, not waveform.' },
  dust: { name: 'Dust', line: 'The sound as specks of light: dense and large while loud, thinning to a faint scatter as it fades.' },
  meter: { name: 'Meter', line: 'Columns of lit cells, like the module’s LEDs: the level over time, step by step, on a ghost grid.' },
};

export const SV_KINDS = ['endless', 'longtail', 'freeze', 'swell', 'octave', 'tape', 'spaces', 'cv'] as const;
export type SvKind = (typeof SV_KINDS)[number];

const f = (n: number) => +n.toFixed(2);
const poly = (pts: [number, number][]) => pts.map(([x, y], i) => `${i ? 'L' : 'M'}${f(x)} ${f(y)}`).join('');
const sample = (x0: number, x1: number, n: number, fn: (u: number, x: number) => number): [number, number][] =>
  Array.from({ length: n + 1 }, (_, i) => { const u = i / n; const x = x0 + (x1 - x0) * u; return [x, fn(u, x)]; });

/** A small seeded random (mulberry32), so the dust is the same on every build. */
function rng(seed: number) {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

// ---- The envelopes (level 0..1 over u 0..1), shared by every set ---------------------------
const ENV = {
  endless: (u: number) => 0.34 + 0.66 * Math.exp(-u / 0.09),
  plain: (u: number) => Math.exp(-u / 0.14),
  longtail: (u: number) => Math.exp(-1.25 * u),
  swell: (u: number) => Math.pow(Math.sin((Math.min(1, u / 0.5) * Math.PI) / 2), 2) * (u > 0.5 ? Math.exp(-(u - 0.5) / 0.45) : 1),
  /** Three chords played before the freeze. */
  played: (u: number) => {
    let v = 0;
    for (const [t, a] of [[0, 0.95], [0.36, 0.8], [0.7, 0.72]] as const) {
      if (u >= t) v += a * Math.min(1, (u - t) / 0.04) * Math.exp(-(u - t) / 0.22);
    }
    return Math.min(1, v);
  },
  plate: (u: number) => 0.6 * Math.exp(-4.2 * u),
  hall: (u: number) => 0.82 * Math.exp(-1.7 * u),
  massive: (u: number) => 1 - Math.exp(-2.4 * u),
  tape: (u: number) => 0.62 * (1 - 0.78 * Math.exp(-(((u - 0.86) / 0.035) ** 2))),
};
/** The held pattern after a freeze: one short cycle repeating unchanged, around a steady level. */
const HELD = ENV.played(1);
const heldCycle = (x: number) => 0.06 * Math.sin((2 * Math.PI * x) / 14) + 0.03 * Math.sin((4 * Math.PI * x) / 14 + 1);
/** Tape: the phase wanders with wow (slow) and flutter (quick). */
const tapePhase = (u: number) => u + 0.035 * Math.sin(2 * Math.PI * 1.3 * u) + 0.006 * Math.sin(2 * Math.PI * 9 * u);

const FREEZE_X = 62;
const SPACES: [number, number, number, (u: number) => number][] = [
  // hit x, tail from, tail to, envelope
  [6, 10, 44, ENV.plate],
  [52, 56, 100, ENV.hall],
  [108, 112, 156, ENV.massive],
];

// CV: the knob arc (seven to five o'clock) and the voltage bump.
const CV = { cx: 30, cy: 35, r: 22, knob: 0.42, cv: 0.78 };
const cvAt = (p: number, rr = CV.r): [number, number] => {
  const a = ((-135 + 270 * p) * Math.PI) / 180;
  return [CV.cx + rr * Math.sin(a), CV.cy - rr * Math.cos(a)];
};
const cvArc = (p0: number, p1: number) => {
  const [x0, y0] = cvAt(p0);
  const [x1, y1] = cvAt(p1);
  return `M${f(x0)} ${f(y0)}A${CV.r} ${CV.r} 0 ${(p1 - p0) * 270 > 180 ? 1 : 0} 1 ${f(x1)} ${f(y1)}`;
};
const volts = (u: number) => 0.5 * (1 - Math.cos(2 * Math.PI * u));
const PLUS = 'M64 35H74M69 30V40';

// ---- Labels (screen readers; the visible caption on the comparison page) -------------------
const LABELS: Record<SvKind, string> = {
  endless: 'The endless hold: the tail falls a little, then settles at a steady level and never fades.',
  longtail: 'A long tail: one sound, ringing on and on, slowly fading.',
  freeze: 'Freeze: what you played, then the moment you tap held still, the same pattern repeating at one level.',
  swell: 'BLOOM: after the note, the reverb swells in slowly, then fades.',
  octave: 'SHIMMER: the note you play and the same note one octave up, steady; nothing climbs higher.',
  tape: 'MOTION to the left: the cycles crowd and stretch with tape wow and flutter, and a dropout dips the level for a moment.',
  spaces: 'Three spaces: a short PLATE, a longer HALL, and MASSIVE, which swells in and runs on past the edge.',
  cv: 'CV: a voltage from 0 to 5 V adds to where the knob is set, turning it further than your hand did.',
};

// =============================================================================================
// GLOW: filled envelopes over a floor line, brightest where loudest.
// =============================================================================================
const FY = 58; // floor
const GH = 50; // full level reaches y = 8
const gy = (l: number) => FY - GH * l;

function glowTail(x0: number, x1: number, env: (u: number, x: number) => number, role: Role = 'hot', n = 80): El[] {
  const crest = sample(x0, x1, n, (u, x) => gy(env(u, x)));
  return [
    { k: 'area', d: `${poly(crest)}L${x1} ${FY}L${x0} ${FY}Z`, role },
    { k: 'line', d: poly(crest), role, w: 1.75 },
  ];
}
const glowHit = (x: number, h = 1): El => ({ k: 'cell', x: x - 1.4, y: gy(h), w: 2.8, h: GH * h, role: 'ink' });
const glowFloor: El = { k: 'line', d: `M4 ${FY}H156`, role: 'faint' };
const mound = (c: number, h: number, s = 7) => (u: number, x: number) => h * Math.exp(-(((x - c) / s) ** 2));

function glow(kind: SvKind): El[] {
  switch (kind) {
    case 'endless':
      return [
        glowFloor,
        { k: 'line', d: poly(sample(30, 156, 40, (_u, x) => gy(ENV.plain((x - 12) / 144)))), role: 'faint', dash: '2 3' },
        glowHit(8),
        ...glowTail(12, 156, ENV.endless),
      ];
    case 'longtail':
      return [glowFloor, glowHit(8), ...glowTail(12, 156, ENV.longtail)];
    case 'swell':
      return [glowFloor, glowHit(8), ...glowTail(12, 156, ENV.swell)];
    case 'freeze':
      return [
        glowFloor,
        { k: 'line', d: `M${FREEZE_X} 4V${FY}`, role: 'faint' },
        ...glowTail(6, FREEZE_X, ENV.played, 'ink', 60),
        ...glowTail(FREEZE_X, 156, (_u, x) => HELD + heldCycle(x - FREEZE_X), 'hot', 120),
      ];
    case 'octave':
      // A spectrum: low to high across. The note, its octave, and the next octave left empty.
      return [
        glowFloor,
        ...[44, 92, 140].map((x): El => ({ k: 'line', d: `M${x} ${FY + 2}V${FY - 3}`, role: 'faint' })),
        ...glowTail(14, 74, mound(44, 0.92), 'ink', 60),
        ...glowTail(62, 122, mound(92, 0.6), 'hot', 60),
      ];
    case 'tape': {
      const env = (u: number) => 24 * ENV.tape(u) / 0.62;
      const top = sample(6, 154, 60, (u) => 34 - env(u));
      const bottom = sample(6, 154, 60, (u) => 34 + env(u)).reverse();
      const wave = sample(6, 154, 450, (u) => 34 - env(u) * Math.sin(2 * Math.PI * 9 * tapePhase(u)));
      return [
        { k: 'tint', d: `${poly(top)}L${poly(bottom).slice(1)}Z`, role: 'hot', o: 0.16 },
        { k: 'line', d: 'M4 34H156', role: 'faint' },
        { k: 'line', d: poly(wave), role: 'hot', w: 1.5 },
      ];
    }
    case 'spaces':
      return [
        glowFloor,
        ...SPACES.flatMap(([h, x0, x1, env]): El[] => [glowHit(h, 0.9), ...glowTail(x0, x1, env, 'hot', 40)]),
      ];
    case 'cv': {
      const [hx0, hy0] = cvAt(CV.knob, 6);
      const [hx1, hy1] = cvAt(CV.knob, 14);
      return [
        { k: 'line', d: cvArc(0, 1), role: 'faint', w: 4 },
        { k: 'line', d: cvArc(0, CV.knob), role: 'ink', w: 4 },
        { k: 'line', d: cvArc(CV.knob, CV.cv), role: 'hot', w: 4 },
        { k: 'line', d: `M${f(hx0)} ${f(hy0)}L${f(hx1)} ${f(hy1)}`, role: 'ink', w: 2 },
        { k: 'line', d: PLUS, role: 'ink', w: 1.75 },
        glowFloor,
        ...glowTail(84, 156, (u) => 0.72 * volts(u), 'hot', 60),
      ];
    }
  }
}

// =============================================================================================
// DUST: a halftone cloud around the centre line; specks large and close while loud, small,
// scattered and see-through as the level falls.
// =============================================================================================
const MID = 34;
const DA = 24; // full level spreads ±24
const PITCH = 3.3;

function cloud(x0: number, x1: number, env: (u: number, x: number) => number, role: Role, seed: number): El[] {
  const r = rng(seed);
  const out: El[] = [];
  const cols = Math.max(2, Math.round((x1 - x0) / PITCH));
  for (let c = 0; c <= cols; c++) {
    const u = c / cols;
    const x = x0 + (x1 - x0) * u;
    const l = env(u, x);
    if (l < 0.02) continue;
    const spread = DA * l;
    const rows = Math.ceil(spread / PITCH) + 1;
    for (let k = -rows; k <= rows; k++) {
      const y = MID + (k + (c % 2 ? 0.5 : 0)) * PITCH + (r() - 0.5) * 1.2;
      const t = Math.abs(y - MID) / Math.max(spread, 1.5);
      if (t > 1.1) continue;
      if (r() > 0.97 - 0.4 * t) continue; // thinner toward the edge of the cloud
      const rad = (0.6 + 0.85 * Math.sqrt(l)) * (1 - 0.45 * Math.min(1, t));
      if (rad < 0.35) continue;
      out.push({ k: 'dot', x: f(x + (r() - 0.5) * 1.2), y: f(y), r: f(rad), role, o: f(0.45 + 0.55 * (1 - Math.min(1, t))) });
    }
  }
  return out;
}
const dustHit = (x: number, h = 1): El[] =>
  Array.from({ length: Math.round((DA * 2 * h) / PITCH) + 1 }, (_, i): El => ({ k: 'dot', x, y: f(MID - DA * h + i * PITCH), r: 1.15, role: 'ink' }));
const dotted = (pts: [number, number][], role: Role, r: number, o?: number): El[] => pts.map(([x, y]) => ({ k: 'dot', x: f(x), y: f(y), r, role, o }));

function dust(kind: SvKind): El[] {
  switch (kind) {
    case 'endless':
      return [
        ...dotted(sample(34, 156, 30, (u, x) => MID - DA * ENV.plain((x - 12) / 144)), 'faint', 0.6),
        ...dustHit(8),
        ...cloud(13, 156, ENV.endless, 'hot', 11),
      ];
    case 'longtail':
      return [...dustHit(8), ...cloud(13, 156, ENV.longtail, 'hot', 12)];
    case 'swell':
      return [...dustHit(8), ...cloud(13, 156, ENV.swell, 'hot', 13)];
    case 'freeze': {
      // The held part repeats the very same specks every cycle: a still pattern.
      const cyc = 14;
      const one = cloud(0, cyc - PITCH / 2, (_u, x) => HELD + heldCycle(x), 'hot', 14);
      const held: El[] = [];
      for (let x0 = FREEZE_X + 2; x0 < 156; x0 += cyc) {
        for (const e of one) if (e.k === 'dot' && x0 + e.x <= 157) held.push({ ...e, x: f(x0 + e.x) });
      }
      return [
        ...dotted(sample(4, 60, 14, () => FREEZE_X).map(([y, x]) => [x, y]), 'faint', 0.6),
        ...cloud(6, FREEZE_X - 4, ENV.played, 'ink', 15),
        ...held,
      ];
    }
    case 'octave': {
      const wave = (y: number, cycles: number, n: number) => sample(8, 152, n, (u) => y - 9 * Math.sin(2 * Math.PI * cycles * u));
      return [...dotted(wave(46, 3, 48), 'ink', 1.05), ...dotted(wave(18, 6, 72), 'hot', 1.05)];
    }
    case 'tape': {
      const pts = sample(6, 154, 190, (u) => MID - 22 * (ENV.tape(u) / 0.62) * Math.sin(2 * Math.PI * 9 * tapePhase(u)));
      return [
        ...dotted(sample(4, 156, 38, () => MID), 'faint', 0.55),
        ...pts.map(([x, y], i): El => {
          const l = ENV.tape(i / 190) / 0.62;
          return { k: 'dot', x: f(x), y: f(y), r: f(0.4 + 0.55 * l), role: 'hot', o: f(0.35 + 0.65 * l) };
        }),
      ];
    }
    case 'spaces':
      return SPACES.flatMap(([h, x0, x1, env], i) => [...dustHit(h, 0.9), ...cloud(x0, x1, env, 'hot', 20 + i)]);
    case 'cv': {
      const ring: El[] = [];
      for (let i = 0; i <= 18; i++) {
        const p = i / 18;
        const [x, y] = cvAt(p);
        const role: Role = p <= CV.knob + 1e-6 ? 'ink' : p <= CV.cv + 1e-6 ? 'hot' : 'faint';
        ring.push({ k: 'dot', x: f(x), y: f(y), r: role === 'faint' ? 0.8 : 1.5, role });
      }
      const [hx, hy] = cvAt(CV.knob, 10);
      const bars: El[] = [];
      for (let x = 84; x <= 156; x += PITCH) {
        const top = 50 - 34 * volts((x - 84) / 72);
        for (let y = 50; y >= top - 0.01; y -= PITCH) {
          const t = (50 - y) / 34;
          bars.push({ k: 'dot', x: f(x), y: f(y), r: f(0.5 + 0.7 * t), role: 'hot', o: f(0.35 + 0.65 * t) });
        }
      }
      return [...ring, { k: 'dot', x: f(hx), y: f(hy), r: 1.5, role: 'ink' }, { k: 'line', d: PLUS, role: 'ink', w: 1.75 }, ...bars];
    }
  }
}

// =============================================================================================
// METER: columns of cells on a ghost grid, like the module's LED ladders.
// =============================================================================================
const CW = 5; // cell width
const CP = 7; // column pitch: 22 columns, x 4..156
const RH = 3.6; // cell height
const RP = 5.5; // row pitch: 9 rows, floor 58
const ROWS = 9;
const colX = (i: number) => 4 + i * CP;

function column(x: number, lit: number, role: Role, ghost = true, w = CW): El[] {
  const out: El[] = [];
  for (let k = 0; k < ROWS; k++) {
    const y = FY - (k + 1) * RP + (RP - RH);
    if (k < lit) out.push({ k: 'cell', x, y: f(y), w, h: RH, role });
    else if (ghost) out.push({ k: 'cell', x, y: f(y), w, h: RH, role: 'faint' });
  }
  return out;
}
/** Columns `from`..`to` (inclusive), lit from an envelope over u 0..1. */
function columns(from: number, to: number, env: (u: number) => number, role: Role = 'hot'): El[] {
  const out: El[] = [];
  for (let i = from; i <= to; i++) {
    const u = to === from ? 0 : (i - from) / (to - from);
    out.push(...column(colX(i), Math.max(env(u) > 0.02 ? 1 : 0, Math.round(ROWS * env(u))), role));
  }
  return out;
}
const ghostCols = (cols: number[]) => cols.flatMap((i) => column(colX(i), 0, 'faint'));

function meter(kind: SvKind): El[] {
  switch (kind) {
    case 'endless':
      return [
        ...column(colX(0), ROWS, 'ink'),
        ...columns(1, 21, ENV.endless),
        { k: 'line', d: poly(sample(colX(4) + CW / 2, colX(21) + CW / 2, 40, (u) => FY - ROWS * RP * ENV.plain(0.15 + 0.85 * u))), role: 'faint', dash: '2 3' },
      ];
    case 'longtail':
      return [...column(colX(0), ROWS, 'ink'), ...columns(1, 21, ENV.longtail)];
    case 'swell':
      return [...column(colX(0), ROWS, 'ink'), ...columns(1, 21, ENV.swell)];
    case 'freeze': {
      // Played: eight columns. Then a gap at the tap, and one repeating pattern of four.
      const pat = [6, 7, 6, 5];
      const held: El[] = [];
      for (let i = 10; i <= 21; i++) held.push(...column(colX(i), pat[(i - 10) % 4], 'hot'));
      return [
        ...columns(0, 8, (u) => ENV.played(0.03 + 0.94 * u), 'ink'),
        { k: 'line', d: `M${colX(9) + CW / 2} 4V${FY}`, role: 'faint' },
        ...held,
      ];
    }
    case 'octave': {
      // A spectrum: low to high across, one octave every six columns.
      const lit: Record<number, [number, Role]> = { 5: [8, 'ink'], 11: [6, 'hot'] };
      return Array.from({ length: 22 }, (_, i) => i).flatMap((i) => column(colX(i), lit[i]?.[0] ?? 0, lit[i]?.[1] ?? 'faint'));
    }
    case 'tape': {
      // One column per cycle of a steady tone: the gaps crowd and stretch with wow and flutter;
      // the dropout dips two of them near the end.
      const out: El[] = [{ k: 'line', d: `M4 ${FY + 2.5}H156`, role: 'faint' }];
      const n = 20;
      for (let c = 0; c < n; c++) {
        // invert the phase: find u where tapePhase(u) = (c + 0.5) / n
        let u = (c + 0.5) / n;
        for (let it = 0; it < 8; it++) u -= tapePhase(u) - (c + 0.5) / n;
        const x = 6 + 146 * u - 1.5;
        out.push(...column(f(x), Math.round(ROWS * ENV.tape(u)), 'hot', false, 3));
      }
      return out;
    }
    case 'spaces':
      return [
        ...column(colX(0), 8, 'ink'), ...columns(1, 4, ENV.plate),
        ...ghostCols([5]),
        ...column(colX(6), 8, 'ink'), ...columns(7, 12, ENV.hall),
        ...ghostCols([13]),
        ...column(colX(14), 8, 'ink'), ...columns(15, 21, ENV.massive),
      ];
    case 'cv': {
      // An LED ring around the knob, and the voltage as columns.
      const ring: El[] = [];
      const segs = 15;
      for (let i = 0; i < segs; i++) {
        const p = i / (segs - 1);
        const [x0, y0] = cvAt(p, 16);
        const [x1, y1] = cvAt(p, 23);
        const role: Role = p <= CV.knob + 1e-6 ? 'ink' : p <= CV.cv + 1e-6 ? 'hot' : 'faint';
        ring.push({ k: 'line', d: `M${f(x0)} ${f(y0)}L${f(x1)} ${f(y1)}`, role, w: 3, cap: 'butt' });
      }
      const [hx, hy] = cvAt(CV.knob, 9);
      const cols: El[] = [];
      for (let i = 0; i < 11; i++) cols.push(...column(84 + i * 6.8, Math.round(ROWS * volts(i / 10)), 'hot'));
      return [...ring, { k: 'line', d: `M${CV.cx} ${CV.cy}L${f(hx)} ${f(hy)}`, role: 'ink', w: 2 }, { k: 'line', d: PLUS, role: 'ink', w: 1.75 }, ...cols];
    }
  }
}

const isSv = (k: DiagramKind): k is SvKind => (SV_KINDS as readonly string[]).includes(k);

/** An alternative drawing, or undefined for `line` and for kinds the alternatives don't cover. */
export function altShape(kind: DiagramKind, variant: Variant): AltShape | undefined {
  if (variant === 'line' || !isSv(kind)) return undefined;
  const els = variant === 'glow' ? glow(kind) : variant === 'dust' ? dust(kind) : meter(kind);
  return { label: LABELS[kind], els };
}

let uid = 0;
/** A per-drawing id for the gradients (several diagrams share one page). */
export const nextGradientId = () => `dg${(uid++).toString(36)}`;
