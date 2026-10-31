# Demos: the plan

29 Oct 2026, draft for the owner's pick (stop 1). Ten scenes drafted; the owner cut 4 (Plate on drums) and 9 (Wander) on 29 Oct 2026, so eight remain. Each is rendered by Sublustris's own renderer (`build/iv_render`, automation with `--auto`), 100 % wet with `--wet-only --dry` so the page's BLEND fader blends the dry in live, as on Resilio's site. Each clip 15-30 s, loudness-matched (about −20.5 LUFS at its default BLEND), MP3, never autoplay.

**Material:** only what the project may publish. Synthesised in code (`tools/make_material.py`'s pad, stabs, lead, piano, or new parts written the same way), or CC0 samples from Resilio's `test_audio/demo_sources/cc0` (Virtuosity and Rusty drums, Baby Blue bass, Emily guitar, VCSL bongos, the 808 cowbell), credited on the credits page as Resilio's are. The Wurlitzer is CC BY 3.0: usable with credit, or left out. Never the owner's reference recordings or any commercial unit's sound; no macOS speech voices.

| # | Scene | Material | Settings (panel names) | What it shows |
|---|---|---|---|---|
| 1 | Endless pad | Synth pad, four slow chords | MASSIVE, NATURAL; BLEND 3, DECAY 5 (endless), TONE 11, WEAR 7, SIZE 2, MOTION 12, DEPTH 9 | The tail that never ends, keeping its notes, chords joining chords |
| 2 | Freeze, then paint | Electric piano chord, then a synth lead over it | HALL, NATURAL; BLEND 1, DECAY 2:30, TONE 12, WEAR 8, SIZE 12, MOTION 12, DEPTH 9; tap freeze on the chord, play the lead, hold to paint the last phrase, tap to release | A still pad you play over, and painting into it |
| 3 | Dub techno stabs | Chord stabs over a kick and rim groove (CC0 drums), the stabs on the send | HALL, NATURAL; BLEND 11, DECAY 4 (endless), TONE 11:30, WEAR 9, SIZE 1, MOTION 11, DEPTH 10 | The endless tail ducking under the stabs and filling the gaps |
| 5 | Bloom swell | Single synthesised piano notes, spaced | MASSIVE, BLOOM; BLEND 1, DECAY 2, TONE 11:30, WEAR 7, SIZE 12, MOTION 1, DEPTH 2 | Each note swelling in behind itself |
| 6 | Shimmer melody | Slow synth lead | HALL, SHIMMER; BLEND 1, DECAY 3, TONE 12, WEAR 7, SIZE 2, MOTION 1:30, DEPTH 12 | One steady octave above the melody |
| 7 | Worn guitar | CC0 Emily guitar, slow chords | HALL, NATURAL; BLEND 12, DECAY 1, TONE 10, WEAR 7 → 2 (turned up across the clip), SIZE 11, MOTION 12, DEPTH 9 | WEAR coming in: tape, breath, grain, the radio's band |
| 8 | Tape side | Electric piano or synth keys | HALL, NATURAL; BLEND 1, DECAY 2, TONE 11, WEAR 9, SIZE 12, MOTION 7:15 (crinkle and dropouts; they start below about 7:15), DEPTH 9 | MOTION left: wow, flutter, crinkle and a dropout or two |
| 10 | Dark drone, SIZE moving | Low synthesised drone | MASSIVE, NATURAL; BLEND 4, DECAY 4:30, TONE 9, WEAR 11, SIZE 9 → 5 (swept slowly), MOTION 10, DEPTH 11 | A dark-ambient wash growing as the space grows |

Order on the page: 1, 2, 3 first (the brief's "within ten seconds": the long tail, the freeze, the endless hold), then the rest.

Each clip's `demos.json` entry: title, caption (one line of what you hear), settings (panel names, clock positions), transcript (a short description of the sound), duration, default BLEND, the dry and wet file names.
