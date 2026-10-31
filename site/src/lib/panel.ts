// Sublustris's knobs and switches, for drawing a set panel (the presets page). The keys are
// the group ids in docs/minisite/assets/panel/sublustris-versio-panel.svg.

export type KnobKey = 'blend' | 'decay' | 'tone' | 'wear' | 'size' | 'motion' | 'depth';
export type SwitchKey = 'space' | 'onset';
export type PanelSettings = Record<KnobKey, number> & Record<SwitchKey, string>;

export interface Knob { key: KnobKey; name: string }
export const KNOBS: Knob[] = [
  { key: 'blend', name: 'BLEND' },
  { key: 'decay', name: 'DECAY' },
  { key: 'tone', name: 'TONE' },
  { key: 'wear', name: 'WEAR' },
  { key: 'size', name: 'SIZE' },
  { key: 'motion', name: 'MOTION' },
  { key: 'depth', name: 'DEPTH' },
];

// Three-way toggles, the first option up (as printed on the map).
export interface Switch { key: SwitchKey; name: string; options: string[] }
export const SWITCHES: Switch[] = [
  { key: 'space', name: 'SPACE', options: ['PLATE', 'HALL', 'MASSIVE'] },
  { key: 'onset', name: 'ONSET', options: ['NATURAL', 'BLOOM', 'SHIMMER'] },
];
