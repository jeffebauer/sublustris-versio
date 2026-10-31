# Story renders: what the real things were

Research behind `studio.py` (round 2 of the story renders, 30 Oct 2026). Sources are linked; claims that only weak sources make are marked.

## The tape machines

- **Fripp & Eno, *(No Pussyfooting)*.** The first side was recorded on 8 September 1972 in Eno's home studio in Maida Vale, London. Two reel-to-reel recorders stood side by side: the first recorded, the second played back, and the playback was routed back into the first. That makes a long looping delay, and the delay time depends on the distance between the machines ([Wikipedia](https://en.wikipedia.org/wiki/(No_Pussyfooting))).
- **The machines were two Revox A77s.** The album credit lists them, and Guitar Player describes the session ([Guitar Player](https://www.guitarplayer.com/guitarists/robert-fripp-on-the-device-that-made-him-feel-truly-liberated), [Strawberry Bricks](https://strawberrybricks.com/guide/releases/no-pussyfooting), [Valhalla DSP blog](https://web.archive.org/web/2015/https://valhalladsp.com/2010/05/26/early-tape-loop-experiments-with-eno-fripp-terry-riley-and-pauline-oliveros/)). In the original setup one tape ran from the first machine's supply reel to the second machine's take-up reel.
- **Discreet Music (1975).** An EMS Synthi AKS with its built-in sequencer, a graphic equaliser and an echo unit fed a tape-delay loop on two Revox machines. The sleeve carries an "operational diagram" of the setup ([Wikipedia](https://en.wikipedia.org/wiki/Discreet_Music), [Reverb Machine](https://reverbmachine.com/blog/deconstructing-brian-enos-discreet-music)). Reverb Machine names the echo unit as a Maestro Echoplex; that is one blog's claim.
- **Revox A77 (Studer-Revox, introduced 1967).** Revox was Studer's consumer brand ([Wikipedia: Studer](https://en.wikipedia.org/wiki/Studer)). The A77 Mk III measures 415 × 359 × 180 mm and weighs 15 kg ([Radiomuseum](https://radiomuseum.org/r/studer_a77a_7_3.html)).
  - It has 3 motors and 3 heads, runs at 9.5 and 19 cm/s, and takes reels up to 26.5 cm.
  - It works upright or flat. It came as a portable, a built-in unit or a table model in a walnut case.
  - The Mk III has grey reel tables, a grey control front and a black textured transport cover.
  - Two VU meters sit at the right, flanked by red record buttons; the transport keys are push buttons ([Radiomuseum](https://radiomuseum.org/r/studer_a77a_7_3.html), [Stereonomono](https://stereonomono.blogspot.com/2011/10/revox-77-1967.html), [service manual](https://www.worldradiohistory.com/Archive-Catalogs/Revox/ReVox-A-77-Service-Manual.pdf)).
- **Studio classics of the era (Studer A80, B67).** These are what a professional studio had. Eno's documented loops were made at home on the semi-pro A77.
- **What we depict.** Two A77-layout decks, upright, in walnut cases, unbranded. They carry 7-inch reels, so they fit the desk and the case without overhang. As the owner asked, each runs its own tape, and the decks are cross-patched (A's outputs into B's inputs, B's outputs back into A's). That is the feedback half of the system rather than the single shared tape, so the page copy shouldn't claim the picture shows the exact 1972 threading.

## The room and the racks

- **Eno's Maida Vale flat (1970s).** Contemporary accounts describe it as "quite a dark flat". Coloured lamps changed its mood (this is where his interest in light installations began), and a 1977 interviewer sat at a white formica-topped table. Eno later called his house "the smallest museum in the world", full of collected curiosities ([Reverb News](https://reverb.com/news/brian-enos-most-surprising-music-gear-choices); search summaries of interviews in [Rock's Backpages](https://www.rocksbackpages.com/Library/Article/the-brian-eno-interview)).
  - No well-documented photograph of its racks exists.
  - So the room is a *typical* cosy home studio of the era: lamp-lit, wood, records and tape boxes, a plant. The image must not claim to be his studio.
- **Rack effects Eno is documented using.**
  - **Lexicon 224:** Eno had one installed at Compass Point (Talking Heads, *Remain in Light*, 1980), according to [search summaries](https://www.vintagedigital.com.au/lexicon-224-digital-reverberator) (that page refuses automated fetches).
  - **Apollo-era gear (1983):** the Eventide H910 Harmonizer and the Lexicon Prime Time delay appear on a gear list for *Apollo* ([80s album gear lists](https://80salbumgearlists.blogspot.com/2023/05/brian-eno-apollo-atmospheres.html), unsourced blog).
  - **Eventide H3000:** praised by Eno around 1990 ([Mixdown](https://mixdownmag.com.au/features/gear-rundown-brian-eno/)).
  - **PCM70:** I found no source tying Eno to it.
- **Lexicon 224.** It was unveiled at the 1978 AES convention. It was rack-mounted and cheaper than EMT's 250, and became one of the most widely adopted early digital reverbs. The PCM-60 (1984) and PCM-70 followed as smaller units ([Wikipedia: Lexicon](https://en.wikipedia.org/wiki/Lexicon_(company))).
  - The original 224 had a black front panel and a separate black/cream metal remote with sliders. The later LARC remote was white with alphanumeric LED displays ([Soundgas](https://soundgas.com/products/lexicon-224x-digital-reverb-9478-1), [Reverb listing](https://reverb.com/item/78519379-lexicon-224x-digital-reverberator-with-larc-1980s-black-with-blue-remote)).
- **What we depict.**
  - The reverb is a black mainframe: a red LED display, program buttons with LEDs, headroom ladders, level pots, rack ears and screws, worn edges.
  - Its black/cream remote sits on the desk with six sliders and a red display.
  - Around it are period-style units: a delay (Prime Time-like role), a pitch shifter (H910-like role), a graphic EQ and a patch bay.
  - Everything is unbranded and modelled from the descriptions above, not traced from any photograph.

## Doubts

- The exact A77 front layout (key order, knob count) is from memory and the descriptions above, not from a measured drawing; the service manual's front-panel figure is the place to check before a final.
- Rack heights of the 224 mainframe are not confirmed by the sources (a dealer lists 16 × 19 × 8 in for a 224X bundle).

## Round 3: the deck in detail (30 Oct 2026)

The owner asked for the deck to be "better and more accurate" in its details, materials and lighting. `studio.deck77()` is modelled on the A77 Mk III/IV (the Dolby-era machines of 1971–77, contemporary with *Discreet Music*). No photograph was traced or copied; the layout comes from the descriptions below.

**What the sources say**
- **Size.** The 1972 equipment profile gives 16¼ × 14¾ × 8¾ in (413 × 375 × 222 mm, depth over the controls) and 34 lb ([Audio, Nov. 1972](https://gammaelectronics.xyz/audio_11-1972_revox.html)). Radiomuseum lists the Mk III at 415 × 359 × 180 mm and 15 kg ([Radiomuseum](https://radiomuseum.org/r/studer_a77a_7_3.html)). We use 420 × 378 × 215 mm for the walnut case.
- **Finish.** In the Dolby-era model the upper (transport) part has "a stainless steel front over the plastic main panel", where the first model had dark grey plastic. The lower panel went the other way: brushed stainless at first, dark grey plastic later ([Audio, Nov. 1972](https://gammaelectronics.xyz/audio_11-1972_revox.html)). So we use a brushed stainless transport plate over a dark grey control panel.
- **Control panel** ([Audio, Nov. 1972](https://gammaelectronics.xyz/audio_11-1972_revox.html)).
  - Five transport push-buttons at the left: rewind, fast forward, play, stop, and record (the record key is red).
  - Below them, two knobs with concentric switches (playback level, balance). Each switch is worked by "a clear plastic disc at the base of the knob".
  - At the right, two VU meters, "flanked by two red push-push switch buttons", with two more knob/switch pairs below them (record levels and source).
  - At the far right, a bar knob for power and speed, with a pilot light above it. The meters are lit whenever the power is on.
  - Between the two groups, three jacks (headphones, two microphones), and two toggles below the controls.
- **Transport** ([Audio, Nov. 1972](https://gammaelectronics.xyz/audio_11-1972_revox.html), [Stereonomono](https://stereonomono.blogspot.com/2011/10/revox-77-1967.html)).
  - The heads are erase, record and playback (the playback head shielded).
  - The heads, capstan and tape guides are mounted on one die-cast block.
  - A plastic tape-path shield swings down for threading.
  - The reel turntables carry spring-loaded sleeves on splined spindles.
  - A four-digit counter sits between the reels, with a reset push-button.
  - The left tape guide is a roller and the right one is fixed; the supply side has a spring-loaded guide post.
  - Reels up to 26.5 cm (10½ in) are accepted.
  - The connectors are along the top of the rear.

**What is still a guess**
- **Reels.** We use 7-inch reels (spun-aluminium flanges, three windows) so that they sit inside the case. The A77 also takes 10½-inch NAB reels on adapters, which overhang the case.
- **Exact positions.** These were judged, not measured from a drawing: the spindle spacing (±104 mm), reel height, head-cover shape, the capstan and pinch-roller positions and arm, the tension-lever pivots, and the spacing of the controls.
- **Colours.** The meter faces are warm cream with a black scale and red zone, lit warm. The key colours (grey, with a red record key) and the knob style (black body, brushed aluminium cap, white index line) are also guesses.
- **Rear sockets.** The phono sockets are drawn as four RCA sockets. The real rear panel has DIN and phono connectors, and a remote socket.
- **Patching.** The leads hang in the gap between the cases, so that the cross-patch reads from the front.
- **Text.** No legends or text appear anywhere. The real panel is lettered, but we leave it blank: no names, logos or model numbers.
