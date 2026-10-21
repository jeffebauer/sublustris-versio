#!/bin/zsh
# Renders every milestone listening page (renders/m1_hall, m4_massive,
# m5_plate_freeze, m6_character, m7_duck) with the current build/iv_render,
# so all pages always reflect the same code. Owner listening: open each
# renders/<page>/index.html in a browser.
#
# Material: the dub skank (test_audio/stimulus/04_skank.wav, from
# tools/make_stimulus.py) and a sparse vibes phrase built from the CC0 vibes
# samples in Resilio Versio's test_audio/demo_sources (not in either repo;
# the vibes page rows are skipped if they're missing). Each gets 12 s of
# silence appended so tails ring out.
#
# Usage: tools/render_review_pages.sh [page ...]   (default: all)
set -e
cd "$(dirname "$0")/.."
R=build/iv_render
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
MATERIALS=(skank)
[ -f $SRC/vibes.wav ] && MATERIALS+=(vibes)

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

# r <page> <folder> <tag> <auto prefix or ""> --set ...   (env IV_FREEZE_VOICING passes through)
r() {
    local page=$1 dir=$2 tag=$3 auto=$4; shift 4
    mkdir -p renders/$page/$dir
    for m in $MATERIALS; do
        local a=()
        [ -n "$auto" ] && a=(--auto $SRC/${auto}_${m}.json)
        $R $SRC/$m.wav renders/$page/$dir/${m}_${tag}.wav --set mix=1 --sidecar "${a[@]}" "$@" > /dev/null
    done
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
    finish m1_hall "Immensio M1: the Hall" "The HALL tank on its own (ERA clean). SIZE columns with DECAY as versions; TONE dark / noon / bright.

size_small   SIZE fully CCW: a big room
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
    finish m4_massive "Immensio M4: MASSIVE and ATTACK" "After your M4 notes: MASSIVE starts with what you play and swells from there (it was silent for over a second), SIZE fully CCW is a wash, not rhythmic echoes, and DARKEN lets go sooner between phrases. The four ATTACK settings in HALL and MASSIVE, and infinite DECAY (ERA clean). REVERSE here is today's; the smoother one is on renders/reverse_options.

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
    finish m5_plate_freeze "Immensio M5: PLATE, freeze and paint" "The PLATE; freeze (a tap freezes the tank into a still pad, the rest plays over it through the small companion reverb, a second tap unfreezes); paint (a long hold while frozen blends what's playing into the pad). ERA at its default (a touch of Neve).

plate    the PLATE from small and short to large and infinite
freeze   a tap freezes; the rest plays over the pad; a tap unfreezes
paint    freeze, then a long hold while frozen blends the playing into the pad

A_small_short     PLATE, SIZE CCW, DECAY 0.3
B_medium          PLATE, SIZE noon, DECAY 0.5
C_large_long      PLATE, SIZE CW, DECAY 0.7
D_infinite        PLATE, SIZE 0.7, DECAY infinite
A_plate_freeze    freeze in PLATE
B_hall_freeze     freeze in HALL
C_massive_freeze  freeze in MASSIVE
A_hall_paint      freeze + paint in HALL
B_massive_paint   freeze + paint in MASSIVE"
fi

if on m6_character; then
    page m6_character
    for ec in 0:A_clean 0.3:B_neve 0.55:C_cassette 0.8:D_mulaw 1:E_midiverb; do
        r m6_character era_hall ${ec##*:} "" --set space=HALL --set decay=0.6 --set size=0.6 --set era=${ec%%:*}
    done
    for ec in 0:A_clean 0.55:C_cassette 1:E_midiverb; do
        r m6_character era_massive ${ec##*:} "" --set space=MASSIVE --set decay=0.65 --set size=0.7 --set era=${ec%%:*}
    done
    for dc in 0:A_tape 0.5:B_still 1:C_drift; do
        r m6_character drift ${dc##*:} "" --set space=HALL --set decay=0.65 --set size=0.7 --set drift=${dc%%:*}
    done
    for tc in 0:A_dark 0.5:B_noon 1:C_bright; do
        r m6_character tone ${tc##*:} "" --set space=HALL --set decay=0.6 --set size=0.6 --set tone=${tc%%:*}
    done
    finish m6_character "Immensio M6: ERA, DRIFT and TONE" "ERA's five stacked stages (loudness matched), DRIFT both ways, TONE's full range. Starting numbers from the spec, for your ears to tune.

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
C_drift     DRIFT fully right (slow Cathedra drift)
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
    finish m7_duck "Immensio M7: DARKEN" "Ducking is always DARKEN and only from DECAY's infinite stretch up: while you play, the tail darkens and dips so the dry stays clear; when you stop, it brightens back.

duck_hall      DARKEN in HALL (SIZE 0.7)
duck_massive   DARKEN in MASSIVE (SIZE 0.7)

A_long_no_duck   DECAY 0.8 (long, finite: no ducking)
B_infinite_duck  DECAY 0.9 (infinite, ducking)
C_full_duck      DECAY 1 (infinite, full ducking)"
fi

# ---- Decision pages (built WITHOUT --showcase: they ask the owner to pick) -------------------

if on freeze_options; then
    page freeze_options
    for sp in HALL MASSIVE PLATE; do
        for fv in 0:A_companion 1:B_feed 2:C_both; do
            IV_FREEZE_VOICING=${fv%%:*} r freeze_options ${(L)sp} ${fv##*:} freeze --set space=$sp --set size=0.7 --set decay=0.6
        done
    done
    print -r -- "How should playing over a frozen pad work? (Owner, M5: \"the frozen reverb and the secondary reverb sound too disparate... maybe the input should influence the frozen buffer\".) Each clip: a tap freezes, the rest plays over the pad, a long hold paints (vibes 6-9.5 s, skank 9-12 s), a tap unfreezes. Pick the one that feels most like one instrument.

hall      in HALL (SIZE 0.7, DECAY 0.6)
massive   in MASSIVE (SIZE 0.7, DECAY 0.6)
plate     in PLATE (SIZE 0.7, DECAY 0.6)

A_companion  today: what you play gets its own small plate reverb, on top of the pad
B_feed       what you play goes into the frozen pad itself, gently (no second reverb); holding feeds it fully
C_both       B, plus the small plate at half level" > renders/freeze_options/README.txt
    # Level-matched by default: B and C run 1.5-7 dB hotter than A, and louder must not win.
    python3 tools/review/make_review.py renders/freeze_options --title "Immensio: playing over a frozen pad" --level-match --dry $SRC --wet-only > /dev/null
    echo "renders/freeze_options/index.html"
fi

if on freeze_twin; then
    page freeze_twin
    for sp in HALL MASSIVE PLATE; do
        for fv in 1:A_feed_as_picked 3:B_twin_recedes; do
            IV_FREEZE_VOICING=${fv%%:*} r freeze_twin ${(L)sp} ${fv##*:} freeze --set space=$sp --set size=0.7 --set decay=0.6
        done
    done
    print -r -- "Confirming your pick (B, feed the pad) with your notes applied: subtler, receding back to the frozen pad, no predelay on what you play, no drop on unfreeze. A is B as you picked it; B is the twin: what you play goes into a twin of the frozen tank (same space and colour, its own decay), heard beside the untouched pad, receding; holding paints it into the pad. Freeze 2.4 s / 3 s, paint 6-9.5 s / 9-12 s, unfreeze 14.5 s / 18 s.

hall      in HALL (SIZE 0.7, DECAY 0.6)
massive   in MASSIVE (SIZE 0.7, DECAY 0.6)
plate     in PLATE (SIZE 0.7, DECAY 0.6)

A_feed_as_picked   the input feeds the frozen pad (what you picked)
B_twin_recedes     the twin: subtler, recedes back to the pad" > renders/freeze_twin/README.txt
    python3 tools/review/make_review.py renders/freeze_twin --title "Immensio: freeze, confirming the pick" --level-match --dry $SRC --wet-only > /dev/null
    echo "renders/freeze_twin/index.html"
fi

if on reverse_options; then
    page reverse_options
    for col in massive:MASSIVE:0.4 massive_long:MASSIVE:0.8 hall:HALL:0.4; do
        IFS=: read c sp t <<< "$col"
        for v in 0:A_today 1:B_smooth; do
            IV_REVERSE_SMOOTH=${v%%:*} r reverse_options $c ${v##*:} "" --set space=$sp --set era=0 --set attack=REVERSE --set time=$t --set decay=0.45 --set size=0.7
        done
    done
    print -r -- "Your M4 note: REVERSE sounds choppy and disjointed rather than a reversed reverb. A is today's: the reverb is cut into TIME-long pieces, each played backwards, rising to a cut. B plays the same backwards pieces as two overlapping readers that fade in and out, so there are no cuts: a continuous reversed wash, still TIME late.

massive       MASSIVE, TIME 0.4
massive_long  MASSIVE, TIME 0.8 (longer pieces)
hall          HALL, TIME 0.4

A_today    today's REVERSE: pieces that rise to a cut
B_smooth   overlapping, faded pieces: no cuts" > renders/reverse_options/README.txt
    python3 tools/review/make_review.py renders/reverse_options --title "Immensio: REVERSE, choppy or smooth" --level-match --dry $SRC --wet-only > /dev/null
    echo "renders/reverse_options/index.html"
fi
