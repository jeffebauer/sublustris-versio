import type { DiagramKind } from '../components/diagrams/shapes';

// One line per control for Home's annotated panel (DESIGN-v2 V7). Each line condenses the
// Manual's own description (docs/minisite/content/manual.md) and links to that control's
// anchor there. `at` is the control's centre on the panel in mm from the top-left
// (docs/minisite/panel/versio_panel_coordinates.csv); `side` is where its callout sits;
// `slot` sorts a callout higher in its column than its height on the panel would (keeps
// leader lines from crossing).
export interface Control { name: string; anchor: string; line: string; at: [number, number]; side: 'l' | 'r'; slot?: number }

export const CONTROLS: Control[] = [
  { name: 'BLEND', anchor: 'blend', line: 'Dry ↔ wet, equal-power. Fully right is 100 % wet, for a send and return.', at: [7.77, 18.53], side: 'l' },
  { name: 'LEDs', anchor: 'leds', line: 'Input and output meters. The output pair turns cool blue while frozen.', at: [25.83, 19.8], side: 'r', slot: 10 },
  { name: 'DECAY', anchor: 'decay', line: 'A short room to minutes-long tails. Endless from about 3 o’clock.', at: [43.33, 18.53], side: 'r' },
  { name: 'TONE', anchor: 'tone', line: 'Darker or brighter, flat at noon. Never harsh at the ends.', at: [25.169, 28.69], side: 'l' },
  { name: 'WEAR', anchor: 'wear', line: 'One knob of tape, bucket-brigade and bit wear over a warm desk colour.', at: [7.77, 39.485], side: 'l' },
  { name: 'SIZE', anchor: 'size', line: 'The size of the space. In CANYON, a long wash to a boundless one.', at: [43.33, 39.485], side: 'r' },
  { name: 'MOTION', anchor: 'motion', line: 'Left: tape wow, flutter and dropouts. Right: an old radio’s narrow, phasing band.', at: [25.169, 49.328], side: 'l' },
  { name: 'DEPTH', anchor: 'depth', line: 'What ONSET does: NATURAL’s predelay, BLOOM’s swell time, SHIMMER’s amount.', at: [43.33, 60.44], side: 'r' },
  { name: 'SPACE', anchor: 'space-top-switch', line: 'PLATE · HALL · CANYON.', at: [8.405, 57.9], side: 'l' },
  { name: 'ONSET', anchor: 'onset-bottom-switch', line: 'NATURAL · BLOOM · SHIMMER: how each sound enters the reverb.', at: [8.405, 67.425], side: 'l' },
  { name: 'FREEZE / PAINT', anchor: 'freeze--paint-button', line: 'Tap to freeze. Hold while frozen to paint what you play into the pad.', at: [26.185, 69.33], side: 'r' },
  { name: 'Gate', anchor: 'gate-input-freeze', line: 'FREEZE, with the same gestures as the button.', at: [44.6, 97.27], side: 'r' },
];

// Features as bento tiles (DESIGN-v2 V9): one big statement each, a tiny diagram (V10) and
// one line, condensed from overview.md and manual.md. The diagrams are the owner's picks
// (30 Oct 2026). Checked against the manual: DECAY's top tails are about 30 s (PLATE),
// 60 s (HALL) and 120 s (CANYON) before endless; SHIMMER is one octave; 7 CV inputs, 0–5 V.
export interface FeatureTile { big: string; line: string; diagram: DiagramKind; size?: 'big' | 'wide' }

// MOTION gets two tiles, tape and radio (owner, 1 Nov 2026); Freeze / paint is no longer wide,
// so the big tile plus eight singles fill every row at three columns and at two.
export const FEATURE_TILES: FeatureTile[] = [
  { big: 'Endless hold', size: 'big', diagram: 'endless-infinity', line: 'Past about 3 o’clock DECAY never lets go. While you play it sits at the level of the longest tail before it, and when you stop it holds where it is, its notes staying notes.' },
  { big: '3 spaces', diagram: 'spaces', line: 'A dense PLATE, a big HALL, and CANYON, which swells in and grows into a boundless wash.' },
  { big: '120 s tails', diagram: 'longtail', line: 'About 30 s in PLATE, 60 s in HALL and 120 s in CANYON. Then endless.' },
  { big: 'Freeze / paint', diagram: 'freeze', line: 'Tap to freeze the tail into a still pad and play over it. Hold while frozen to paint a little of what you play into the pad.' },
  { big: 'Swells', diagram: 'swell-family', line: 'ONSET on BLOOM: the reverb swells in behind each sound, from a breath to four seconds.' },
  { big: '1 octave', diagram: 'octave', line: 'SHIMMER: one steady octave up, in tune, that doesn’t climb.' },
  { big: 'Tape wobble', diagram: 'tape-head', line: 'MOTION to the left: tape wow and flutter, with dropouts at the very end.' },
  { big: 'Old radio', diagram: 'radio-band', line: 'MOTION to the right: the tail through an old AM radio’s narrow band, with a slow notch wandering through it, so it phases.' },
  { big: '7 CV inputs', diagram: 'cv-scale', line: 'One per knob, 0–5 V, added to the knob’s position. Plus a gate for FREEZE.' },
];
