#!/bin/zsh
# Renders every milestone listening page (renders/m1_hall, m4_massive,
# m5_plate_freeze, m6_character, m7_duck) with the current build/iv_render,
# so all pages always reflect the same code. Owner listening: open each
# renders/<page>/index.html in a browser.
#
# Material: the dub skank (test_audio/stimulus/04_skank.wav, from
# tools/make_stimulus.py), the wider set from tools/make_material.py (stabs,
# pad, lead, drums, bass, keys, voice; needs .venv) and a sparse vibes phrase built from the CC0 vibes
# samples in Resilio Versio's test_audio/demo_sources (not in either repo;
# the vibes page rows are skipped if they're missing). Each gets 12 s of
# silence appended so tails ring out.
#
# Usage: tools/render_review_pages.sh [page ...]   (default: all)
set -e
cd "$(dirname "$0")/.."
R=${IV_RENDER:-build/iv_render}
[ -x $R ] || { echo "build/iv_render missing: cmake --build build"; exit 1; }
SRC=renders/_material
mkdir -p $SRC

# ---- Material -------------------------------------------------------------------
if [ ! -f $SRC/skank.wav ]; then
    [ -f test_audio/stimulus/04_skank.wav ] || python3 tools/make_stimulus.py
    ffmpeg -v error -y -i test_audio/stimulus/04_skank.wav -af "volume=3dB,apad=pad_dur=12" -c:a pcm_s24le $SRC/skank.wav
fi
VIBES=../resilio-versio/test_audio/demo_sources/cc0/vibes_soft
if [ ! -f $SRC/vibes.wav ] && [ -d $VIBES ]; then
    notes="A2:0 E5:600 C5:1200 A4:1800 F2:6000 F4:6600 C5:7200 A4:7800 G3:12000 D4:12600 B3:13200 E5:13800"
    args=(); fil=""; i=0
    for nt in ${=notes}; do
        n=${nt%%:*}; d=${nt##*:}
        args+=(-i $VIBES/Vibes_soft_${n}_v1_rr1_Main.wav)
        fil="${fil}[${i}:a]aformat=channel_layouts=stereo,aresample=48000,adelay=${d}|${d}[a${i}];"
        i=$((i+1))
    done
    mix=""; for j in $(seq 0 $((i-1))); do mix="${mix}[a${j}]"; done
    ffmpeg -v error -y "${args[@]}" -filter_complex "${fil}${mix}amix=inputs=${i}:normalize=0,atrim=0:18,volume=16dB,apad=pad_dur=12" \
        -ac 2 -ar 48000 -c:a pcm_s24le $SRC/vibes.wav
fi
# The measuring click: one click, then 40 s for the longest finite tails to decay.
[ -f $SRC/_click.wav ] || ffmpeg -v error -y -i test_audio/stimulus/07_click_single.wav -af "apad=whole_dur=40" -ar 48000 -c:a pcm_s24le $SRC/_click.wav
MATERIALS=(skank)
[ -f $SRC/vibes.wav ] && MATERIALS+=(vibes)
# The wider set (tools/make_material.py): pads, stabs, a lead, drums, bass, keys, a voice,
# a low voice and a dry piano (for "metallic?"), and a full mix (testing plan item 6).
EXTRA=(stabs pad lead drums bass keys voice voicelow piano fullmix)
for m in $EXTRA; do
    if [ ! -f $SRC/$m.wav ] && [ -x .venv/bin/python ]; then .venv/bin/python tools/make_material.py $m > /dev/null; fi
done
for m in $EXTRA; do [ -f $SRC/$m.wav ] && MATERIALS+=($m); done

# Freeze gestures per material (seconds): [press, release] pairs.
cat > $SRC/freeze_only_skank.json <<'EOF'
{ "buttons": [[3.0, 3.1], [18.0, 18.1]] }
EOF
cat > $SRC/freeze_only_vibes.json <<'EOF'
{ "buttons": [[2.4, 2.5], [14.5, 14.6]] }
EOF
cat > $SRC/freeze_skank.json <<'EOF'
{ "buttons": [[3.0, 3.1], [9.0, 12.0], [18.0, 18.1]] }
EOF
cat > $SRC/freeze_vibes.json <<'EOF'
{ "buttons": [[2.4, 2.5], [6.0, 9.5], [14.5, 14.6]] }
EOF

# The wider set's gestures: freeze at 3 s, paint 8-11 s, unfreeze at 16 s.
for m in $EXTRA; do
    print -r -- '{ "buttons": [[3.0, 3.1], [16.0, 16.1]] }' > $SRC/freeze_only_$m.json
    print -r -- '{ "buttons": [[3.0, 3.1], [8.0, 11.0], [16.0, 16.1]] }' > $SRC/freeze_$m.json
done

# r <page> <folder> <tag> <auto prefix or ""> --set ...   (env vars pass through)
r() {
    local page=$1 dir=$2 tag=$3 auto=$4; shift 4
    mkdir -p renders/$page/$dir
    for m in $MATERIALS; do
        local a=()
        [ -n "$auto" ] && a=(--auto $SRC/${auto}_${m}.json)
        # A material with a send (fullmix) feeds the reverb only that; the page's dry is the whole mix.
        local in=$SRC/$m.wav
        [ -f $SRC/${m}_send.wav ] && in=$SRC/${m}_send.wav
        $R $in renders/$page/$dir/${m}_${tag}.wav --set mix=1 --sidecar "${a[@]}" "$@" > /dev/null
    done
    # One click through the same settings (no gestures): the card's Numbers (decay per band,
    # grainy, clarity, width) come from this, since music never leaves a clean decay.
    mkdir -p renders/$page/_measure/$dir
    $R $SRC/_click.wav renders/$page/_measure/$dir/${tag}.wav --set mix=1 --sidecar "$@" > /dev/null
}
page() { # page title readme
    rm -rf renders/$1; mkdir -p renders/$1
}
# The renders are 100 % wet (MIX fully up): the page's MIX fader blends the
# dry back in live (--wet-only --dry), so any balance can be heard.
# Every page here shows settings, it doesn't ask for a decision, so it's a
# --showcase (no "My pick", no blind mode; notes only). A page that asks the
# owner to choose (like renders/p7_sw1_demos) is built without it.
finish() {
    print -r -- "$3" > renders/$1/README.txt
    python3 tools/review/make_review.py renders/$1 --title "$2" --no-level-match --showcase --dry $SRC --wet-only > /dev/null
    echo "renders/$1/index.html"
}

want() { [ $# -eq 0 ] || return 0; }
PAGES=("$@")
on() { [ ${#PAGES} -eq 0 ] || (( ${PAGES[(Ie)$1]} )); }

if on m1_hall; then
    page m1_hall
    for sz in 0:small 0.5:medium 1:large; do
        for dc in 0.15:A_short 0.45:B_medium 0.7:C_long 0.9:D_infinite; do
            r m1_hall size_${sz##*:} ${dc##*:} "" --set space=HALL --set era=0 --set time=0 --set size=${sz%%:*} --set decay=${dc%%:*}
        done
    done
    for tc in 0:A_dark 0.5:B_noon 1:C_bright; do
        r m1_hall tone ${tc##*:} "" --set space=HALL --set era=0 --set time=0 --set size=0.6 --set decay=0.6 --set tone=${tc%%:*}
    done
    finish m1_hall "Immensio M1: the Hall" "The HALL tank on its own (ERA clean), after your M1 notes: SIZE now goes from a small room (shorter lines, and its tails are shorter too, as a small room's are) to a huge hall, which also stops the buzzy build-up on long small rooms; TONE's ends go further (dark: more lows, fewer highs; bright: the opposite; noon unchanged). SIZE columns with DECAY as versions; TONE dark / noon / bright.

size_small   SIZE fully CCW: a small room
size_medium  SIZE noon
size_large   SIZE fully CW: a huge hall
tone         TONE at SIZE 0.6, DECAY 0.6

A_short     DECAY 0.15
B_medium    DECAY 0.45
C_long      DECAY 0.70
D_infinite  DECAY 0.90 (infinite)
A_dark      TONE fully CCW
B_noon      TONE noon
C_bright    TONE fully CW"
fi

if on m4_massive; then
    page m4_massive
    for sz in 0:A_smallest 0.5:B_middle 1:C_largest; do
        r m4_massive massive_size ${sz##*:} "" --set space=MASSIVE --set era=0 --set decay=0.7 --set size=${sz%%:*}
    done
    for sp in HALL MASSIVE; do
        f=attack_${(L)sp}
        r m4_massive $f A_normal "" --set space=$sp --set era=0 --set attack=NORMAL --set time=0.2 --set decay=0.55 --set size=0.7
        r m4_massive $f B_bloom_short "" --set space=$sp --set era=0 --set attack=BLOOM --set time=0.3 --set decay=0.55 --set size=0.7
        r m4_massive $f C_bloom_long "" --set space=$sp --set era=0 --set attack=BLOOM --set time=0.8 --set decay=0.55 --set size=0.7
        r m4_massive $f D_reverse "" --set space=$sp --set era=0 --set attack=REVERSE --set time=0.4 --set decay=0.45 --set size=0.7
    done
    r m4_massive infinite A_hall "" --set space=HALL --set era=0 --set decay=1 --set size=0.7
    r m4_massive infinite B_massive "" --set space=MASSIVE --set era=0 --set decay=1 --set size=0.7
    finish m4_massive "Immensio M4: MASSIVE and ATTACK" "After your M4 notes: MASSIVE starts with what you play and swells from there (it was silent for over a second), SIZE fully CCW is a wash, not rhythmic echoes, and DARKEN lets go sooner between phrases. The four ATTACK settings in HALL and MASSIVE, and infinite DECAY (ERA clean). REVERSE is shown in full on renders/reverse.

massive_size    MASSIVE's SIZE, DECAY 0.7
attack_hall     the ATTACK switch in HALL
attack_massive  the ATTACK switch in MASSIVE
infinite        DECAY fully up, both FDN spaces

A_smallest     MASSIVE, SIZE fully CCW
B_middle       MASSIVE, SIZE noon
C_largest      MASSIVE, SIZE fully CW
A_normal       ATTACK NORMAL (TIME = short predelay)
B_bloom_short  ATTACK BLOOM, TIME 0.3
C_bloom_long   ATTACK BLOOM, TIME 0.8
D_reverse      ATTACK REVERSE, TIME 0.4
A_hall         HALL, DECAY infinite
B_massive      MASSIVE, DECAY infinite"
fi

if on m5_plate_freeze; then
    page m5_plate_freeze
    r m5_plate_freeze plate A_small_short "" --set space=PLATE --set size=0 --set decay=0.3
    r m5_plate_freeze plate B_medium "" --set space=PLATE --set size=0.5 --set decay=0.5
    r m5_plate_freeze plate C_large_long "" --set space=PLATE --set size=1 --set decay=0.7
    r m5_plate_freeze plate D_infinite "" --set space=PLATE --set size=0.7 --set decay=0.95
    r m5_plate_freeze freeze A_plate_freeze freeze_only --set space=PLATE --set size=0.6 --set decay=0.6
    r m5_plate_freeze freeze B_hall_freeze freeze_only --set space=HALL --set size=0.7 --set decay=0.6
    r m5_plate_freeze freeze C_massive_freeze freeze_only --set space=MASSIVE --set size=0.7 --set decay=0.6
    r m5_plate_freeze paint A_hall_paint freeze --set space=HALL --set size=0.7 --set decay=0.6
    r m5_plate_freeze paint B_massive_paint freeze --set space=MASSIVE --set size=0.7 --set decay=0.6
    r m5_plate_freeze paint C_plate_paint freeze --set space=PLATE --set size=0.7 --set decay=0.6
    finish m5_plate_freeze "Immensio M5: PLATE, freeze and paint" "The PLATE; freeze, now the twin you picked: a tap freezes the tank into a still pad, and what you play goes into a twin of it, a quieter layer over the pad that fades back out when you stop, leaving the pad as it was; a second tap unfreezes. Paint (a long hold while frozen) blends a little of that layer into the pad, never more than ~1.5 dB, so the pad stays itself. ERA at its default (a touch of Neve).

plate    the PLATE from small and short to large and infinite
freeze   a tap freezes; your playing is a layer over the pad, then fades back out; a tap unfreezes
paint    freeze, then a long hold while frozen blends a little of your playing into the pad

A_small_short     PLATE, SIZE CCW, DECAY 0.3
B_medium          PLATE, SIZE noon, DECAY 0.5
C_large_long      PLATE, SIZE CW, DECAY 0.7
D_infinite        PLATE, SIZE 0.7, DECAY infinite
A_plate_freeze    freeze in PLATE
B_hall_freeze     freeze in HALL
C_massive_freeze  freeze in MASSIVE
A_hall_paint      freeze + paint in HALL
B_massive_paint   freeze + paint in MASSIVE
C_plate_paint     freeze + paint in PLATE"
fi

if on m6_character; then
    page m6_character
    for ec in 0:A_clean 0.3:B_neve 0.55:C_cassette 0.8:D_mulaw 1:E_midiverb; do
        r m6_character era_hall ${ec##*:} "" --set space=HALL --set decay=0.6 --set size=0.6 --set era=${ec%%:*}
    done
    for ec in 0:A_clean 0.55:C_cassette 1:E_midiverb; do
        r m6_character era_massive ${ec##*:} "" --set space=MASSIVE --set decay=0.65 --set size=0.7 --set era=${ec%%:*}
    done
    for dc in 0:A_tape 0.5:B_still 1:C_wander; do
        r m6_character drift ${dc##*:} "" --set space=HALL --set decay=0.65 --set size=0.7 --set drift=${dc%%:*}
    done
    for tc in 0:A_dark 0.5:B_noon 1:C_bright; do
        r m6_character tone ${tc##*:} "" --set space=HALL --set decay=0.6 --set size=0.6 --set tone=${tc%%:*}
    done
    finish m6_character "Immensio M6: ERA, DRIFT and TONE" "ERA's five stacked stages, DRIFT both ways and TONE's full range, rendered from today's reverb (v1.0.28, 24 Oct 2026). Since your last M6 listen, ERA has changed the most: it does something from its first notch and the Midiverb fades in across its stretch (knob feel, v1.0.21); across the μ-law stretch the HALL and MASSIVE tails read the old-Lexicon way, a little grain and noise (v1.0.22); a held Midiverb tail keeps a quarter of its dark loop (v1.0.23); and the cassette stage warms the wet like tape, a little stronger with light hysteresis (tape out, v1.0.22 to v1.0.27). The HALL starts right behind the note and short tails stay brighter (v1.0.25), and the reverb keeps its loudness whatever DECAY is (v1.0.28). ERA stays loudness-matched on music. Does ERA feel like going back in time, stage by stage, and is each stage's character right? Are DRIFT's tape side and its Wander side what you hear in your head? Notes only.

era_hall     ERA's five stages in HALL (DECAY 0.6, SIZE 0.6)
era_massive  three ERA stages in MASSIVE (DECAY 0.65, SIZE 0.7)
drift        DRIFT left, noon, right (HALL, DECAY 0.65)
tone         TONE dark, noon, bright (HALL, DECAY 0.6)

A_clean     ERA 0
B_neve      ERA 0.3
C_cassette  ERA 0.55 (hiss while you play)
D_mulaw     ERA 0.8
E_midiverb  ERA 1
A_tape      DRIFT fully left (wow and flutter)
B_still     DRIFT noon
C_wander    DRIFT fully right (Wander: quick, small random movement; your pick, v1.0.20)
A_dark      TONE fully CCW
B_noon      TONE noon
C_bright    TONE fully CW (highs outlast lows)"
fi

if on m7_duck; then
    page m7_duck
    for dc in 0.8:A_long_no_duck 0.9:B_infinite_duck 1:C_full_duck; do
        r m7_duck duck_hall ${dc##*:} "" --set space=HALL --set size=0.7 --set decay=${dc%%:*}
        r m7_duck duck_massive ${dc##*:} "" --set space=MASSIVE --set size=0.7 --set decay=${dc%%:*}
    done
    finish m7_duck "Immensio M7: ducking" "Ducking at infinite DECAY (ADR 0014, amended v1.0.17): while you play, the tail is compressed 4:1 (attack 30 ms, release 0.4 s, about 3 dB) so the dry stays clear, without darkening; when you stop, it comes back over about a second.

duck_hall      ducking in HALL (SIZE 0.7)
duck_massive   ducking in MASSIVE (SIZE 0.7)

A_long_no_duck   DECAY 0.8 (long, finite: no ducking)
B_infinite_duck  DECAY 0.9 (infinite, ducking)
C_full_duck      DECAY 1 (infinite, full ducking)"
fi

# ---- Decision pages (built WITHOUT --showcase: they ask the owner to pick) -------------------

if on valhalla_compare; then
    # Immensio beside the owner's Valhalla recordings (docs/reference-recording-guide.md; read from their
    # Ableton set and split by tools/ingest_reference.py into test_audio/reference/, never committed).
    # Our DECAY set so the tail rings 2 / 5 / 15 s (T60 at 500 Hz-1 kHz, measured 23 Oct 2026, v1.0.19).
    REF=test_audio/reference
    [ -d $REF/valhallaroom_largeroom_medium ] || { echo "valhalla_compare: no recordings in $REF"; exit 1; }
    saved=("${MATERIALS[@]}"); MATERIALS=()
    for m in skank vibes stabs pad lead drums bass keys voice; do [ -f $SRC/$m.wav ] && MATERIALS+=($m); done
    page valhalla_compare
    refcopy() { # refcopy <col> <tag> <reference set>: the set's piece per material, as a version in the card
        for m in $MATERIALS; do [ -f $REF/$3/$3_$m.wav ] && cp $REF/$3/$3_$m.wav renders/valhalla_compare/$1/${m}_$2.wav; done
    }
    for t in "2:short:0.296:0.398:0.068:007:000" "5:medium:0.509:0.624:0.198:030:012" "15:long:0.772:0.816:0.406:073:054"; do
        IFS=: read s tgt dh dp dm fs fa <<< "$t"
        s=$(printf %02d $s) # columns sort as text: 02s, 05s, 15s
        r valhalla_compare hall_${s}s A_immensio "" --set space=HALL --set era=0 --set size=0.6 --set decay=$dh
        refcopy hall_${s}s B_valhalla_room_large_room valhallaroom_largeroom_$tgt
        refcopy hall_${s}s C_valhalla_room_dark_chamber valhallaroom_darkchamber_$tgt
        r valhalla_compare plate_${s}s A_immensio "" --set space=PLATE --set era=0 --set size=0.6 --set decay=$dp
        refcopy plate_${s}s B_valhalla_plate_chrome valhallaplate_chrome_$tgt
        refcopy plate_${s}s C_valhalla_plate_lithium valhallaplate_lithium_$tgt
        r valhalla_compare massive_${s}s A_immensio "" --set space=MASSIVE --set era=0 --set size=0.7 --set decay=$dm
        refcopy massive_${s}s B_supermassive_sagittarius supermassive_sagittarius_fb$fs
        refcopy massive_${s}s C_supermassive_andromeda supermassive_andromeda_fb$fa
    done
    MATERIALS=("${saved[@]}")
    refs=(); for d in $REF/*(/N); do [ -f $d/${d:t}_skank.wav ] && refs+=(--reference $d); done
    print -r -- "Immensio beside your Valhalla recordings, at matched decays: each column rings for about 2, 5 or 15 seconds (our DECAY set to match; ERA clean, DRIFT noon). In each card, A is Immensio and B and C are the Valhalla takes from your Ableton set: HALL against Room (Large Room, Dark Chamber), PLATE against Plate (Chrome, Lithium), MASSIVE against Supermassive (Sagittarius, Andromeda). The Andromeda in the 2 s column is its 0 % feedback take, which rings only about half a second. The Compare with menu adds any of your takes to every card as R. Notes only: what do they do that we don't, and the other way round?

hall_02s      HALL, about 2 s (DECAY 0.30, SIZE 0.6)
hall_05s      HALL, about 5 s (DECAY 0.51)
hall_15s      HALL, about 15 s (DECAY 0.77)
plate_02s     PLATE, about 2 s (DECAY 0.40, SIZE 0.6)
plate_05s     PLATE, about 5 s (DECAY 0.62)
plate_15s     PLATE, about 15 s (DECAY 0.82)
massive_02s   MASSIVE, about 2 s (DECAY 0.07, SIZE 0.7: its shortest is 2.2 s)
massive_05s   MASSIVE, about 5 s (DECAY 0.20)
massive_15s   MASSIVE, about 15 s (DECAY 0.41)

A_immensio                    Immensio
B_valhalla_room_large_room    Valhalla Room, Large Room
C_valhalla_room_dark_chamber  Valhalla Room, Dark Chamber
B_valhalla_plate_chrome       Valhalla Plate, Chrome
C_valhalla_plate_lithium      Valhalla Plate, Lithium
B_supermassive_sagittarius    Valhalla Supermassive, Sagittarius
C_supermassive_andromeda      Valhalla Supermassive, Andromeda" > renders/valhalla_compare/README.txt
    python3 tools/review/make_review.py renders/valhalla_compare --title "Immensio beside Valhalla" --level-match --showcase --dry $SRC --wet-only "${refs[@]}" > /dev/null
    echo "renders/valhalla_compare/index.html"
fi

if on era_discrete; then
    # Stacked ERA (today, ADR 0003) against discrete eras (owner, 24 Oct 2026): each stage on its own,
    # only neighbours blending. Prototype hook IV_ERA_DISCRETE (core/params/Mappings.h eraMix). Each
    # stage near its full strength: cassette 0.62, mu-law 0.83, Midiverb 1.
    page era_discrete
    vers() { # vers <col> --set ...: stacked, discrete
        local col=$1; shift
        IV_ERA_DISCRETE=0 r era_discrete $col A_stacked "" "$@"
        IV_ERA_DISCRETE=1 r era_discrete $col B_discrete "" "$@"
    }
    H=(--set space=HALL --set decay=0.6 --set size=0.6)
    M=(--set space=MASSIVE --set decay=0.65 --set size=0.7)
    vers hall_1_cassette "${H[@]}" --set era=0.62
    vers hall_2_mulaw "${H[@]}" --set era=0.83
    vers hall_3_midiverb "${H[@]}" --set era=1
    vers massive_1_cassette "${M[@]}" --set era=0.62
    vers massive_2_mulaw "${M[@]}" --set era=0.83
    vers massive_3_midiverb "${M[@]}" --set era=1
    print -r -- "Should ERA's stages stack, or should each era sound on its own? A is today, stacked: each stage keeps the ones before it, so the μ-law still has the cassette's tape, hiss and wow under it, and the Midiverb has all three. B is discrete: only that era. The cassette without the Neve's warmth; the μ-law as an early digital unit alone (no tape, hiss or wow); the Midiverb as just the Midiverb (its 13 bits and dark loop, no tape, hiss or μ-law grain). On the knob, discrete eras crossfade only with their neighbours. Each stage is near full strength (ERA 0.62, 0.83 and 1). Loudness matches within about 0.2 dB, and the page is level-matched. Without the cassette's darkening under them, discrete μ-law and Midiverb tails keep their highs a little longer (at the μ-law stage, 2 kHz rings 4.1 s instead of 3.5). The 10-minute infinite holds pass. The Neve has nothing under it, so it sounds the same either way and isn't on the page. Pick per card, with notes.

hall_1_cassette     HALL, ERA 0.62 (cassette)
hall_2_mulaw        HALL, ERA 0.83 (μ-law)
hall_3_midiverb     HALL, ERA 1 (Midiverb)
massive_1_cassette  MASSIVE, ERA 0.62 (cassette)
massive_2_mulaw     MASSIVE, ERA 0.83 (μ-law)
massive_3_midiverb  MASSIVE, ERA 1 (Midiverb)

A_stacked           Stacked (today): each era on top of the ones before
B_discrete          Discrete: each era on its own" > renders/era_discrete/README.txt
    python3 tools/review/make_review.py renders/era_discrete --title "Immensio: ERA stacked or discrete?" --level-match --dry $SRC --wet-only > /dev/null
    echo "renders/era_discrete/index.html"
fi

if on reverse; then
    page reverse
    RV=(--set era=0 --set attack=REVERSE)
    for tm in 0:A_0.2s 0.4:B_0.5s 0.8:C_1.3s 1:D_2s; do
        r reverse time ${tm##*:} "" --set space=HALL $RV --set time=${tm%%:*} --set decay=0.45 --set size=0.5
    done
    for dc in 0:A_sharp 0.5:B_middle 1:C_soft; do
        r reverse decay ${dc##*:} "" --set space=HALL $RV --set time=0.4 --set decay=${dc%%:*} --set size=0.5
    done
    for sz in 0:A_fine 1:B_coarse; do
        r reverse size ${sz##*:} "" --set space=HALL $RV --set time=0.4 --set decay=0.45 --set size=${sz%%:*}
    done
    r reverse attack A_normal "" --set space=HALL --set era=0 --set attack=NORMAL --set time=0 --set decay=0.45 --set size=0.5
    r reverse attack B_bloom "" --set space=HALL --set era=0 --set attack=BLOOM --set time=0.4 --set decay=0.45 --set size=0.5
    r reverse attack C_reverse "" --set space=HALL $RV --set time=0.4 --set decay=0.45 --set size=0.5
    finish reverse "Immensio: REVERSE, the hardware way" "REVERSE rebuilt the established way (AMS RMX16, Lexicon and Midiverb reverse programs): each note blooms into a swell that starts with it, rises over TIME, then stops. Nothing plays backwards; the reverb's shape is reversed. Inside: a delay with 32 taps per side that get louder over TIME, into a short reverb of its own that smooths them into one swell. TIME is the swell's length, DECAY how sudden the stop is, SIZE the short reverb's size (the swell's grain). It's the same in every SPACE (MASSIVE adds its predelay), as on the hardware, where reverse is its own program.

time     TIME (HALL, DECAY 0.45)
decay    DECAY, the stop (TIME 0.4)
size     SIZE, the grain (TIME 0.4)
attack   the three ATTACK settings side by side (TIME 0.4)

A_0.2s     TIME fully CCW: a 0.2 s swell
B_0.5s     TIME 0.4: 0.5 s
C_1.3s     TIME 0.8: 1.3 s
D_2s       TIME fully CW: 2 s
A_sharp    DECAY fully CCW: the swell stops within ~0.2 s
B_middle   DECAY noon
C_soft     DECAY fully CW: a softer stop (~1 s)
A_fine     SIZE fully CCW: a fine, dense grain
B_coarse   SIZE fully CW: a coarser, roomier grain
A_normal   NORMAL: the reverb as usual
B_bloom    BLOOM: swells over TIME, then decays naturally
C_reverse  REVERSE: swells over TIME, then stops"
fi

if on grainy_start; then
    # The grainy check's slow starts (docs/research/reverb-testing-practices.md, session 3):
    # each SPACE's quick-to-smooth setting beside its slow ones, on material that shows grain.
    [ -f $SRC/clicks.wav ] || ffmpeg -v error -y -i test_audio/stimulus/01_clicks.wav -af "atrim=0:20,apad=pad_dur=12" -ac 2 -ar 48000 -c:a pcm_s24le $SRC/clicks.wav
    [ -f $SRC/hits.wav ] || ffmpeg -v error -y -i test_audio/stimulus/02_hits.wav -af "apad=pad_dur=8" -ac 2 -ar 48000 -c:a pcm_s24le $SRC/hits.wav
    saved=("${MATERIALS[@]}"); MATERIALS=(clicks hits piano)
    page grainy_start
    r grainy_start plate A_small "" --set space=PLATE --set era=0 --set size=0 --set decay=0.4
    r grainy_start plate B_medium "" --set space=PLATE --set era=0 --set size=0.5 --set decay=0.4
    r grainy_start plate C_large "" --set space=PLATE --set era=0 --set size=1 --set decay=0.4
    r grainy_start hall A_hall_medium "" --set space=HALL --set era=0 --set size=0.5 --set decay=0.4
    r grainy_start hall B_hall_large "" --set space=HALL --set era=0 --set size=1 --set decay=0.4
    r grainy_start massive A_massive_medium "" --set space=MASSIVE --set era=0 --set size=0.5 --set decay=0.4
    r grainy_start massive B_massive_small "" --set space=MASSIVE --set era=0 --set size=0 --set decay=0.4
    r grainy_start massive C_massive_large "" --set space=MASSIVE --set era=0 --set size=1 --set decay=0.4
    MATERIALS=("${saved[@]}")
    print -r -- "The new grainy check: every tail ends up a smooth wash, but in big spaces the first part of the tail may stay a little grainy (separate reflections you can almost count, a sandy flutter) for longer than the 0.3 s target. Real big rooms do take longer to fill in, so this may be fine or even part of the size. A in each column is a setting that fills in quickly, for comparison. Listen to the first half-second after each click or hit: smooth, or sandy? Tail (T) and Loop help. DECAY 0.4 everywhere, ERA clean. The Numbers under each card give 'smooth after', but treat it as rough: MASSIVE sits right at the line from the start, so its reading jumps between 0.1 and 1.5 s on tiny changes.

plate     PLATE
hall      HALL
massive   MASSIVE

A_small            SIZE fully CCW (fills in fast)
B_medium           SIZE noon
C_large            SIZE fully CW (slowest PLATE)
A_hall_medium      SIZE noon (fills in fast)
B_hall_large       SIZE fully CW
A_massive_medium   SIZE noon
B_massive_small    SIZE fully CCW
C_massive_large    SIZE fully CW" > renders/grainy_start/README.txt
    python3 tools/review/make_review.py renders/grainy_start --title "Immensio: do big spaces start grainy?" --level-match --showcase --dry $SRC --wet-only > /dev/null
    echo "renders/grainy_start/index.html"
fi
