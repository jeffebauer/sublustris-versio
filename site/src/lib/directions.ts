import { readFileSync } from 'node:fs';
import { join } from 'node:path';
import { contrast, grade } from './contrast';

// The three style-tile directions (stop 3, BRIEF section 10). Each direction's colours live in
// one place, its CSS file (src/styles/directions/<id>.css); this reads the hex values between
// the file's `@tokens light` / `@tokens dark` markers, so the contrast table on the style tile
// is always computed from the colours the page really uses. Every text/background pair a
// direction uses is listed here and checked at build: a failing pair stops the build.

export type DirectionId = 'a' | 'b' | 'c';
type Need = 'text' | 'mark';   // 4.5:1 for text, 3:1 for large text and marks
type Fg = [token: string, label: string, need: Need];
export interface Surface { bg: string; name: string; use: string; fg: Fg[] }

export interface Direction {
  id: DirectionId;
  name: string;
  short: string;
  summary: string;
  hasDark: boolean;
  surfaces: Surface[];
  marks: { token: string; name: string; use: string }[];
}

const PAGE: Surface[] = [
  { bg: 'paper', name: 'Paper', use: 'The page, reading pages', fg: [['ink', 'Ink', 'text'], ['ink-2', 'Ink 2', 'text'], ['signal-text', 'Accent text', 'text'], ['signal', 'Accent mark', 'mark'], ['focus', 'Focus ring', 'mark']] },
  { bg: 'tile', name: 'Tile', use: 'Bento tiles, the download band, the panel well', fg: [['ink', 'Ink', 'text'], ['ink-2', 'Ink 2', 'text'], ['signal-text', 'Accent text', 'text'], ['signal', 'Accent mark', 'mark']] },
  { bg: 'raised', name: 'Raised', use: 'A card on a tile band', fg: [['ink', 'Ink', 'text'], ['ink-2', 'Ink 2', 'text']] },
];
const DARK: Surface[] = [
  { bg: 'dark', name: 'Dark band', use: 'The hero and sound bands', fg: [['on-dark', 'On dark', 'text'], ['on-dark-2', 'On dark 2', 'text'], ['signal-text-on-dark', 'Accent text', 'text'], ['signal-on-dark', 'Accent mark', 'mark']] },
  { bg: 'tile-dark', name: 'Tile on dark', use: 'Audio tiles on the dark band', fg: [['on-dark', 'On dark', 'text'], ['on-dark-2', 'On dark 2', 'text'], ['signal-on-dark', 'Accent mark', 'mark']] },
];
const BUTTON: Surface[] = [
  { bg: 'signal-ink', name: 'Accent fill', use: 'The one primary button', fg: [['on-signal', 'Button text', 'text']] },
  { bg: 'signal-hover', name: 'Accent hover', use: 'The primary button, hovered', fg: [['on-signal', 'Button text', 'text']] },
];
const PANEL: Surface[] = [
  { bg: 'sv-panel', name: 'Panel map', use: 'The panel diagram’s face', fg: [['sv-ink', 'Labels', 'text'], ['sv-ink-soft', 'Small print', 'text'], ['sv-led', 'LEDs', 'mark'], ['sv-accent', 'Button', 'mark']] },
];
const SLATE: Surface[] = [
  { bg: 'slate', name: 'Slate band', use: 'The middle band, between night and moonlight', fg: [['on-slate', 'On slate', 'text'], ['on-slate-2', 'On slate 2', 'text'], ['signal-text-on-dark', 'Accent text', 'text'], ['signal-on-dark', 'Accent mark', 'mark']] },
  { bg: 'tile-slate', name: 'Tile on slate', use: 'Tiles on the slate band', fg: [['on-slate', 'On slate', 'text'], ['on-slate-2', 'On slate 2', 'text'], ['signal-on-dark', 'Accent mark', 'mark']] },
];

export const DIRECTIONS: Record<DirectionId, Direction> = {
  a: {
    id: 'a', name: 'Dark-first, a faint glow', short: 'Dark, faint glow', hasDark: false,
    summary: 'The whole page is a cool near-black, never pure black, with text softly lit rather than white. One accent: a dim blue-violet LED behind frosted glass, glowing only on focus rings, LEDs, playheads and the one primary action.',
    surfaces: [...PAGE, ...DARK, ...BUTTON, ...PANEL],
    marks: [{ token: 'signal', name: 'Glow', use: 'LEDs, playheads, dots, diagram highlights' }],
  },
  b: {
    id: 'b', name: 'White body, a dusk accent', short: 'White, dusk accent', hasDark: true,
    summary: 'Resilio’s white page and near-black hero and sound bands, the closest sibling. The greys turn cool and the signal red becomes a dusk indigo, the blue-violet of the sky just after sunset.',
    surfaces: [...PAGE, ...DARK, ...BUTTON, ...PANEL],
    marks: [{ token: 'signal', name: 'Dusk', use: 'LEDs, playheads, dots, diagram highlights' }],
  },
  c: {
    id: 'c', name: 'Night-blue bands', short: 'Night-blue bands', hasDark: true,
    summary: 'A clouded night sky: deep night-blue bands alternate with slate and paler, moonlit ones. The accent is a faint warm glow, a lamp behind cloud: pale amber on the night bands, its deeper ochre on the moonlit ones.',
    surfaces: [...PAGE, ...DARK, ...SLATE, ...BUTTON, ...PANEL],
    marks: [{ token: 'signal', name: 'Lamp (moonlit)', use: 'Marks on the pale bands' }, { token: 'signal-on-dark', name: 'Lamp (night)', use: 'Marks and the glow on night and slate bands' }],
  },
};

function block(css: string, marker: 'light' | 'dark'): Record<string, string> {
  const start = css.indexOf(`/* @tokens ${marker} */`);
  if (start < 0) return {};
  const end = css.indexOf('/* @end */', start);
  const out: Record<string, string> = {};
  for (const m of css.slice(start, end).matchAll(/--([\w-]+):\s*(#[0-9a-fA-F]{6})\b/g)) out[m[1]] = m[2].toLowerCase();
  return out;
}

export interface Tokens { light: Record<string, string>; dark?: Record<string, string> }

export function tokensFor(id: DirectionId): Tokens {
  const css = readFileSync(join(process.cwd(), 'src', 'styles', 'directions', `${id}.css`), 'utf8');
  const light = block(css, 'light');
  const darkOnly = block(css, 'dark');
  return { light, dark: Object.keys(darkOnly).length ? { ...light, ...darkOnly } : undefined };
}

export interface PairRow { label: string; fg: string; ratio: number; grade: string; need: Need; pass: boolean }
export interface SurfaceRow { name: string; token: string; hex: string; use: string; pairs: PairRow[] }

/** Every surface of a theme with its pairs graded; throws when a pair fails its need. */
export function contrastTable(dir: Direction, theme: Record<string, string>, themeName: string): SurfaceRow[] {
  const rows = dir.surfaces
    .filter((s) => theme[s.bg])
    .map((s) => ({
      name: s.name, token: `--${s.bg}`, hex: theme[s.bg], use: s.use,
      pairs: s.fg.filter(([t]) => theme[t]).map(([t, label, need]) => {
        const ratio = contrast(theme[s.bg], theme[t]);
        return { label, fg: theme[t], ratio, grade: grade(ratio), need, pass: ratio >= (need === 'text' ? 4.5 : 3) };
      }),
    }));
  const fails = rows.flatMap((r) => r.pairs.filter((p) => !p.pass).map((p) => `${r.name} / ${p.label} ${p.ratio.toFixed(2)}:1`));
  if (fails.length) throw new Error(`Style tile ${dir.id} (${themeName}): contrast fails: ${fails.join('; ')}`);
  return rows;
}

/** Black or white, whichever reads better on a swatch. */
export const textOn = (hex: string) => (contrast(hex, '#0a0a0a') > contrast(hex, '#ffffff') ? '#0a0a0a' : '#ffffff');
