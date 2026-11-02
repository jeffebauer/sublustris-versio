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
# IV_PAGES_ROOT: the checkout whose renders/ the pages land in (default: this one). From a worktree, point it
# at the main checkout and IV_RENDER at the worktree's renderer, so a prototype's page lands in main's renders/.
cd "${IV_PAGES_ROOT:-$(dirname "$0")/..}"
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
    finish m1_hall "Sublustris M1: the Hall" "The HALL tank on its own (ERA clean), after your M1 notes: SIZE now goes from a small room (shorter lines, and its tails are shorter too, as a small room's are) to a huge hall, which also stops the buzzy build-up on long small rooms; TONE's ends go further (dark: more lows, fewer highs; bright: the opposite; noon unchanged). SIZE columns with DECAY as versions; TONE dark / noon / bright.

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
        r m4_massive $f D_shimmer "" --set space=$sp --set era=0 --set attack=SHIMMER --set time=0.6 --set decay=0.45 --set size=0.7
    done
    r m4_massive infinite A_hall "" --set space=HALL --set era=0 --set decay=1 --set size=0.7
    r m4_massive infinite B_massive "" --set space=MASSIVE --set era=0 --set decay=1 --set size=0.7
    finish m4_massive "Sublustris M4: MASSIVE and ATTACK" "After your M4 notes: MASSIVE starts with what you play and swells from there (it was silent for over a second), SIZE fully CCW is a wash, not rhythmic echoes, and DARKEN lets go sooner between phrases. The four ATTACK settings in HALL and MASSIVE, and infinite DECAY (ERA clean). SHIMMER (ATTACK's third position since v1.0.33) adds your input an octave up.

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
D_shimmer      ATTACK SHIMMER, TIME 0.6
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
    finish m5_plate_freeze "Sublustris M5: PLATE, freeze and paint" "The PLATE; freeze, now the twin you picked: a tap freezes the tank into a still pad, and what you play goes into a twin of it, a quieter layer over the pad that fades back out when you stop, leaving the pad as it was; a second tap unfreezes. Paint (a long hold while frozen) blends a little of that layer into the pad, never more than ~1.5 dB, so the pad stays itself. ERA at its default (a touch of Neve).

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
    for ec in 0.1:A_neve 0.3:B_midiverb 0.5:C_bbd 0.7:D_cassette 0.9:E_radio; do
        r m6_character era_hall ${ec##*:} "" --set space=HALL --set decay=0.6 --set size=0.6 --set era=${ec%%:*}
    done
    for ec in 0.3:B_midiverb 0.7:D_cassette 0.9:E_radio; do
        r m6_character era_massive ${ec##*:} "" --set space=MASSIVE --set decay=0.65 --set size=0.7 --set era=${ec%%:*}
    done
    for dc in 0:A_tape 0.5:B_still 1:C_wander; do
        r m6_character drift ${dc##*:} "" --set space=HALL --set decay=0.65 --set size=0.7 --set drift=${dc%%:*}
    done
    for tc in 0:A_dark 0.5:B_noon 1:C_bright; do
        r m6_character tone ${tc##*:} "" --set space=HALL --set decay=0.6 --set size=0.6 --set tone=${tc%%:*}
    done
    finish m6_character "Sublustris M6: ERA, DRIFT and TONE" "ERA's five eras (ADR 0021), DRIFT both ways and TONE's full range, from today's reverb. ERA now runs from sweet to gritty, each era on its own, with the Neve's warmth under all of them: the Neve, the Midiverb, a bucket-brigade delay line, the cassette and AM radio (each card is the middle of its stretch). Notes only.

era_hall     ERA's five eras in HALL (DECAY 0.6, SIZE 0.6)
era_massive  three eras in MASSIVE (DECAY 0.65, SIZE 0.7)
drift        DRIFT left, noon, right (HALL, DECAY 0.65)
tone         TONE dark, noon, bright (HALL, DECAY 0.6)

A_neve      ERA 0.1: the Neve (the base under every era)
B_midiverb  ERA 0.3: the Midiverb
C_bbd       ERA 0.5: the bucket-brigade
D_cassette  ERA 0.7: the cassette
E_radio     ERA 0.9: AM radio
A_tape      DRIFT fully left (wow and flutter)
B_still     DRIFT noon
C_wander    DRIFT fully right (Wander)
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
    finish m7_duck "Sublustris M7: ducking" "Ducking at infinite DECAY (ADR 0014, amended v1.0.17): while you play, the tail is compressed 4:1 (attack 30 ms, release 0.4 s, about 3 dB) so the dry stays clear, without darkening; when you stop, it comes back over about a second.

duck_hall      ducking in HALL (SIZE 0.7)
duck_massive   ducking in MASSIVE (SIZE 0.7)

A_long_no_duck   DECAY 0.8 (long, finite: no ducking)
B_infinite_duck  DECAY 0.9 (infinite, ducking)
C_full_duck      DECAY 1 (infinite, full ducking)"
fi

# ---- Decision pages (built WITHOUT --showcase: they ask the owner to pick) -------------------

if on valhalla_compare; then
    # Sublustris beside the owner's Valhalla recordings (docs/reference-recording-guide.md; read from their
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
    print -r -- "Sublustris beside your Valhalla recordings, at matched decays: each column rings for about 2, 5 or 15 seconds (our DECAY set to match; ERA clean, DRIFT noon). In each card, A is Sublustris and B and C are the Valhalla takes from your Ableton set: HALL against Room (Large Room, Dark Chamber), PLATE against Plate (Chrome, Lithium), MASSIVE against Supermassive (Sagittarius, Andromeda). The Andromeda in the 2 s column is its 0 % feedback take, which rings only about half a second. The Compare with menu adds any of your takes to every card as R. Notes only: what do they do that we don't, and the other way round?

hall_02s      HALL, about 2 s (DECAY 0.30, SIZE 0.6)
hall_05s      HALL, about 5 s (DECAY 0.51)
hall_15s      HALL, about 15 s (DECAY 0.77)
plate_02s     PLATE, about 2 s (DECAY 0.40, SIZE 0.6)
plate_05s     PLATE, about 5 s (DECAY 0.62)
plate_15s     PLATE, about 15 s (DECAY 0.82)
massive_02s   MASSIVE, about 2 s (DECAY 0.07, SIZE 0.7: its shortest is 2.2 s)
massive_05s   MASSIVE, about 5 s (DECAY 0.20)
massive_15s   MASSIVE, about 15 s (DECAY 0.41)

A_immensio                    Sublustris
B_valhalla_room_large_room    Valhalla Room, Large Room
C_valhalla_room_dark_chamber  Valhalla Room, Dark Chamber
B_valhalla_plate_chrome       Valhalla Plate, Chrome
C_valhalla_plate_lithium      Valhalla Plate, Lithium
B_supermassive_sagittarius    Valhalla Supermassive, Sagittarius
C_supermassive_andromeda      Valhalla Supermassive, Andromeda" > renders/valhalla_compare/README.txt
    python3 tools/review/make_review.py renders/valhalla_compare --title "Sublustris beside Valhalla" --level-match --showcase --dry $SRC --wet-only "${refs[@]}" > /dev/null
    echo "renders/valhalla_compare/index.html"
fi

if on knob_moves; then
    # PROTOTYPES (owner, plugin): MASSIVE's SIZE "moves too slowly causing slow pitch shifts that make
    # it sound really out of tune"; "changing TIME rapidly also creates a large wash of noise"; HALL's
    # SIZE moved rapidly has "a brief moment of aliasing-like sound". One cause: delays that slide
    # sweep the sound's pitch, and inside the loop retune the whole tail (0.3 -> 0.8 at a fast glide:
    # an octave flat). Measured on a held note, energy away from it during rapid moves: TIME -1..-6 dB,
    # HALL SIZE -0.6 (steady -24..-25). Crossfades instead (PASP): the delays jump and the old setting
    # fades out (equal power). SIZE: once the knob rests 50 ms, the lines, early taps and HALL's in-loop
    # allpasses together, the level that follows the length gliding; TIME: back to back, before the tank.
    # Measured: tail pitch within 2 cents; TIME -34 dB, HALL SIZE -41 dB during moves; infinite holds;
    # no clicks (24 MASSIVE renders).
    saved=("${MATERIALS[@]}"); MATERIALS=()
    for m in pad vibes keys voice skank stabs; do [ -f $SRC/$m.wav ] && MATERIALS+=($m); done
    for m in $MATERIALS; do
        .venv/bin/python - "$SRC/$m.wav" "$SRC" "$m" <<'PY'
import sys, json, numpy as np, soundfile as sf
x, sr = sf.read(sys.argv[1], always_2d=True)
src, m = sys.argv[2], sys.argv[3]
on = np.nonzero(np.abs(x).max(axis=1) > 1e-3)[0]
a, b = on[0] / sr, on[-1] / sr
mid = a + 0.4 * (b - a)
def k(key, t, v): return {"t": round(t, 3), "key": key, "value": v}
def dump(name, bp): json.dump({"breakpoints": bp}, open(f"{src}/{name}_{m}.json", "w"))
dump("sizeplay", [k("size", 0, 0.3), k("size", mid, 0.3), k("size", mid + 2, 0.8), k("size", mid + 6, 0.8), k("size", mid + 7, 0.45)])
dump("sizetail", [k("size", 0, 0.3), k("size", b + 1, 0.3), k("size", b + 3, 0.8)])
def rapid(key, lo, hi):
    bp, t = [k(key, 0, lo), k(key, mid, lo)], mid
    for v in (hi, lo, hi, lo, hi): t += 0.3; bp.append(k(key, t, v))
    return bp
dump("sizerapid", rapid("size", 0.15, 0.85))
dump("timerapid", rapid("time", 0.1, 0.9))
PY
    done
    page knob_moves
    for t in "1_massive_size_while_playing:sizeplay:MASSIVE" "2_massive_size_in_the_tail:sizetail:MASSIVE" "3_hall_size_rapid:sizerapid:HALL" "4_time_rapid:timerapid:HALL"; do
        IFS=: read col auto sp <<< "$t"
        for v in "A_today:" "B_crossfade_quick:IV_SIZE_XFADE=0.1 IV_PREDELAY_XFADE=0.1" "C_crossfade_slow:IV_SIZE_XFADE=0.3 IV_PREDELAY_XFADE=0.3"; do
            tag=${v%%:*}; e=${v#*:}
            ( [ -n "$e" ] && export ${=e}; r knob_moves $col $tag "$auto" --set space=$sp --set decay=0.5 --set size=0.3 --set attack=NORMAL )
        done
    done
    MATERIALS=("${saved[@]}")
    print -r -- "Moving SIZE and TIME (your notes from the plugin: MASSIVE's SIZE sounding out of tune for a long time after a move; TIME turned quickly making a wash of noise; HALL's SIZE turned quickly sounding briefly like aliasing). All three have one cause: when a delay changes length by sliding, it bends the pitch of everything passing through it, and inside a reverb's loop it retunes the whole tail for the rest of its life (a faster slide only makes a bigger bend: an octave flat for column 1's move). B and C stop sliding: the delays jump to the new setting and the old one fades out (in 0.1 s for B, 0.3 s for C), so nothing is retuned and nothing is swept; you hear the space change rather than bend. Measured: the tail stays within 2 cents of its pitch; quick TIME and HALL SIZE moves no longer add noise; no clicks. Not level-matched.

1_massive_size_while_playing   MASSIVE, SIZE 0.3 to 0.8 and back to 0.45 while the music plays
2_massive_size_in_the_tail     MASSIVE, SIZE 0.3 to 0.8 after the music stops (the tail only)
3_hall_size_rapid              HALL, SIZE flicked between 0.15 and 0.85 five times in 1.5 s
4_time_rapid                   HALL, TIME (pre-delay) flicked between 0.1 and 0.9 five times in 1.5 s

A_today             Today: the delays slide (pitch bends, the tail retuned)
B_crossfade_quick   No sliding: the old setting fades out in 0.1 s
C_crossfade_slow    No sliding: the old setting fades out in 0.3 s" > renders/knob_moves/README.txt
    python3 tools/review/make_review.py renders/knob_moves --title "Moving SIZE and TIME" --no-level-match --dry $SRC --wet-only > /dev/null
    echo "renders/knob_moves/index.html"
fi

if on era_sweep; then
    # ERA turned from fully left to fully right over 40 s while the music loops (renders/_material_sweep,
    # 40 s loops + 12 s of tail), then the tail at ERA 1; chaptered at each era's start (ADR 0021).
    SW=renders/_material_sweep
    mkdir -p $SW
    [ -f $SW/_click.wav ] || cp $SRC/_click.wav $SW/_click.wav
    print -r -- '{ "breakpoints": [ {"t": 0, "key": "era", "value": 0}, {"t": 40, "key": "era", "value": 1}, {"t": 52, "key": "era", "value": 1} ] }' > $SW/era_lin.json
    saved=("${MATERIALS[@]}"); MATERIALS=()
    for m in skank vibes stabs pad lead drums bass keys voice fullmix; do
        [ -f $SRC/$m.wav ] || continue
        MATERIALS+=($m)
        for f in $m ${m}_send; do
            [ -f $SRC/$f.wav ] || continue
            [ -f $SW/$f.wav ] && continue
            d=$(ffprobe -v error -show_entries format=duration -of csv=p=0 $SRC/$f.wav)
            play=$(python3 -c "print(max(1.0, $d - 12))")
            ffmpeg -v error -y -i $SRC/$f.wav -af "atrim=0:$play,aloop=loop=-1:size=2e9" -t 40 -c:a pcm_f32le $SW/_loop_$f.wav
            ffmpeg -v error -y -i $SW/_loop_$f.wav -af "afade=t=out:st=39.9:d=0.1,apad=pad_dur=12" -c:a pcm_f32le $SW/$f.wav
            rm -f $SW/_loop_$f.wav
        done
        cp $SW/era_lin.json $SW/era_lin_$m.json
    done
    page era_sweep
    SRC0=$SRC; SRC=$SW
    r era_sweep hall A_era_sweep era_lin --set space=HALL --set decay=0.6 --set size=0.6
    r era_sweep massive A_era_sweep era_lin --set space=MASSIVE --set decay=0.65 --set size=0.7
    SRC=$SRC0; MATERIALS=("${saved[@]}")
    print -r -- "ERA turned from fully left to fully right over 40 s while the music plays, then the tail rings at full ERA. The bar under each card is in chapters at each era's start: click one to jump there (Neve 0 s, Midiverb 8 s, BBD 16 s, Cassette 24 s, Radio 32 s, Tail 40 s). Each era comes in over the first third of its chapter, then runs from its sweet end to its gritty end. Notes only.

hall      HALL (DECAY 0.6, SIZE 0.6)
massive   MASSIVE (DECAY 0.65, SIZE 0.7)

A_era_sweep  ERA, fully left to fully right" > renders/era_sweep/README.txt
    python3 tools/review/make_review.py renders/era_sweep --title "Sublustris: ERA, knob sweep" --showcase --no-level-match --dry $SW --wet-only \
        --chapters "Neve:0,Midiverb:8,BBD:16,Cassette:24,Radio:32,Tail:40" > /dev/null
    echo "renders/era_sweep/index.html"
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
    python3 tools/review/make_review.py renders/grainy_start --title "Sublustris: do big spaces start grainy?" --level-match --showcase --dry $SRC --wet-only > /dev/null
    echo "renders/grainy_start/index.html"
fi

if on fit_cpu; then
    # PROTOTYPES (M3 rounds 1-8, 25 Oct 2026): the module runs ~2.3x over its CPU budget at worst
    # (MASSIVE with BLOOM); the speed-ups that keep the sound identical are nearly used up (round 8:
    # the tank is short of arithmetic). Three ways to fit, each changing the sound a little:
    # B the HALL / MASSIVE tank at 24 kHz (halfbands either side, the still-lows tank unchanged),
    # C BLOOM's swell (and MASSIVE's built-in mass) with 12 points per side instead of 24, D one early
    # reflection per line instead of three (the same early energy), E all three. Checked: level within
    # 0.6 dB of today, T60 within 3 %, no clicks, infinite tails hold 3 min.
    saved=("${MATERIALS[@]}"); MATERIALS=()
    for m in pad vibes skank keys; do [ -f $SRC/$m.wav ] && MATERIALS+=($m); done
    page fit_cpu
    B=IV_FIT_HALF_TANK=1; C=IV_FIT_SWELL_TAPS=12; D=IV_FIT_EARLY_TAPS=1
    for t in "1_plate_bloom:PLATE:BLOOM:A C" "2_hall_normal:HALL:NORMAL:A B D E" "3_hall_bloom:HALL:BLOOM:A B C D E" \
             "4_massive_normal:MASSIVE:NORMAL:A B C D E" "5_massive_bloom:MASSIVE:BLOOM:A B C D E"; do
        IFS=: read col sp at vs <<< "$t"
        for v in ${=vs}; do
            case $v in
                A) tag=A_today; e="" ;;
                B) tag=B_tank_24k; e="$B" ;;
                C) tag=C_swell_12; e="$C" ;;
                D) tag=D_early_1; e="$D" ;;
                E) tag=E_all_three; e="$B $C $D" ;;
            esac
            ( [ -n "$e" ] && export ${=e}; r fit_cpu $col $tag "" --set space=$sp --set attack=$at --set decay=0.6 --set size=0.6 --set time=0.6 )
        done
    done
    MATERIALS=("${saved[@]}")
    print -r -- "Fitting the module (the M3 CPU checks): at worst, MASSIVE with BLOOM, the module needs about 2.3 times what it can give; the speed-ups that keep the sound identical are nearly used up. These are three ways to fit, each changing the sound a little. Level-matched; renders are 100 % wet, so use the page's MIX fader to blend the dry in.

What each saves on the module (estimated from the measurements): B about 45-50 % in HALL and MASSIVE; C about 12 % wherever the swell runs (BLOOM, and MASSIVE's built-in mass); D about 8-15 %. Even E (all three) probably leaves MASSIVE with BLOOM around 90 %, against a 75 % target, so more would follow; PLATE gets under with C alone.

1_plate_bloom      PLATE, BLOOM (only C applies)
2_hall_normal      HALL, NORMAL
3_hall_bloom       HALL, BLOOM
4_massive_normal   MASSIVE, NORMAL (its built-in mass uses the swell)
5_massive_bloom    MASSIVE, BLOOM

A_today       Today
B_tank_24k    The tank at 24 kHz, like classic digital reverbs: the tail's top end stops around 11-12 kHz (the dry and the attack keep everything)
C_swell_12    The swell with 12 points per side instead of 24 (BLOOM and MASSIVE's mass)
D_early_1     One early reflection per line instead of three (the same early energy)
E_all_three   B, C and D together" > renders/fit_cpu/README.txt
    python3 tools/review/make_review.py renders/fit_cpu --title "Fitting the module: three ways" --level-match --dry $SRC --wet-only > /dev/null
    echo "renders/fit_cpu/index.html"
fi

if on swell_points; then
    # A correction (25 Oct 2026): fit_cpu's "C, 12 swell points" didn't render 12 points: the main swell
    # had laid out its 24 before the prototype's count arrived, so only its smear took the 12-point
    # spacing (24 points, a longer smear; MASSIVE NORMAL didn't change). Re-listen, all with the tank at
    # 24 kHz (built, v1.0.35): A today's 24 points, B what was picked, C a true 12 points (the CPU saving).
    # IV_REF_RENDER: a build of 60d296d (the prototypes); IV_RENDER: the v1.0.35 build.
    saved=("${MATERIALS[@]}"); MATERIALS=()
    for m in pad vibes skank keys; do [ -f $SRC/$m.wav ] && MATERIALS+=($m); done
    page swell_points
    NEW=$R
    for t in "1_plate_bloom:PLATE:BLOOM" "2_hall_bloom:HALL:BLOOM" "3_massive_bloom:MASSIVE:BLOOM" "4_massive_normal:MASSIVE:NORMAL"; do
        IFS=: read col sp at <<< "$t"
        set=(--set space=$sp --set attack=$at --set decay=0.6 --set size=0.6 --set time=0.6)
        ( export IV_FIT_HALF_TANK=1; R=$IV_REF_RENDER; r swell_points $col A_today_24_points "" $set )
        ( export IV_FIT_HALF_TANK=1 IV_FIT_SWELL_TAPS=12; R=$IV_REF_RENDER; r swell_points $col B_what_you_picked "" $set )
        ( R=$NEW; r swell_points $col C_true_12_points "" $set )
    done
    MATERIALS=("${saved[@]}")
    print -r -- "A correction to the last page, sorry: its C (\"12 swell points\") didn't really have 12 points. Because of a bug in the prototype, the swell kept its 24 points and only its smear got longer, so that's what you picked. Here are the three side by side, all with the tank at 24 kHz (your pick, now built): A is today's swell, B is what you picked, C is a true 12-point swell (the one that saves ~12 % of the CPU with BLOOM). Level-matched; renders are 100 % wet, so use the MIX fader for the dry.

1_plate_bloom      PLATE, BLOOM
2_hall_bloom       HALL, BLOOM
3_massive_bloom    MASSIVE, BLOOM
4_massive_normal   MASSIVE, NORMAL (its built-in mass uses the swell's points)

A_today_24_points    Today: 24 points per side
B_what_you_picked    What you picked on the last page: 24 points, a longer smear
C_true_12_points     12 points per side (the CPU saving)" > renders/swell_points/README.txt
    python3 tools/review/make_review.py renders/swell_points --title "BLOOM's swell: 24 or 12 points" --level-match --dry $SRC --wet-only > /dev/null
    echo "renders/swell_points/index.html"
fi

if on swell_seed; then
    # Owner (via the spec session, confirmed here): "the 12 points seemed to accentuate the mids more than
    # the 24 point versions. Adjusting the seed to create a more even sonic balance would be good."
    # Measured first: on pink noise 12 and 24 points are within ~1 dB in every third-octave band; note by
    # note both are about equally uneven (~5.3 dB, the comb any set of copies makes), and other seeds
    # range 5.05-5.76 (today's 5.44). B-D: the three most even of 1,500 seeds (4.98-5.01), by ear.
    saved=("${MATERIALS[@]}"); MATERIALS=()
    for m in pad vibes keys skank; do [ -f $SRC/$m.wav ] && MATERIALS+=($m); done
    page swell_seed
    NEW=$R
    for t in "1_plate_bloom:PLATE:BLOOM" "2_hall_bloom:HALL:BLOOM" "3_massive_bloom:MASSIVE:BLOOM" "4_massive_normal:MASSIVE:NORMAL"; do
        IFS=: read col sp at <<< "$t"
        set=(--set space=$sp --set attack=$at --set decay=0.6 --set size=0.6 --set time=0.6)
        ( R=$NEW; r swell_seed $col A_today "" $set )
        ( export IV_SWELL_SEED=0xE02DE8EF; R=$NEW; r swell_seed $col B_seed_1 "" $set )
        ( export IV_SWELL_SEED=0xB650EBD2; R=$NEW; r swell_seed $col C_seed_2 "" $set )
        ( export IV_SWELL_SEED=0x2C341331; R=$NEW; r swell_seed $col D_seed_3 "" $set )
        ( export IV_FIT_HALF_TANK=1; R=$IV_REF_RENDER; r swell_seed $col E_24_points "" $set )
    done
    MATERIALS=("${saved[@]}")
    print -r -- "BLOOM's swell: other seeds for its 12 points (your note: \"the 12 points seemed to accentuate the mids more than the 24 point versions\"). The seed decides where each of the 12 copies falls and whether it's flipped. Measured first: on noise, 12 and 24 points are just as even (within about 1 dB everywhere); on music, each version boosts some notes and dips others by a few dB, like a fine comb, and which notes moves with TIME and with the seed. B, C and D are the three most even of 1,500 seeds, a little more even than today on average; E is the old 24 points (12 % more CPU), for reference. Level-matched; all with the tank at 24 kHz; renders are 100 % wet, so use the MIX fader for the dry.

1_plate_bloom      PLATE, BLOOM
2_hall_bloom       HALL, BLOOM
3_massive_bloom    MASSIVE, BLOOM
4_massive_normal   MASSIVE, NORMAL (its built-in mass uses the swell's points)

A_today        Today's 12 points
B_seed_1       12 points, another seed (the most even of 1,500)
C_seed_2       12 points, another seed (second)
D_seed_3       12 points, another seed (third)
E_24_points    The old 24 points, for reference (costs about 12 % more CPU)" > renders/swell_seed/README.txt
    python3 tools/review/make_review.py renders/swell_seed --title "BLOOM's swell: other seeds" --level-match --dry $SRC --wet-only > /dev/null
    echo "renders/swell_seed/index.html"
fi

if on early_hits; then
    # fit_cpu's D (one early reflection per line instead of six; the page said three, wrongly) was
    # picked as part of E "if needed", but those six were the owner's v1.0.21 pick (renders/knob_feel)
    # against a dip before a MASSIVE hit's tail ("uneven on Hits"). Measured on a click, deepest dip
    # before the peak: MASSIVE SIZE 1 six 1.9 dB, four 2.9, three 4.6, two 6.0, one 16.9; HALL SIZE 1
    # six 0.7, two 0.9, one 2.8. On hits, by ear: A today, B HALL 2 / MASSIVE 4, C one everywhere.
    saved=("${MATERIALS[@]}"); MATERIALS=()
    for m in skank stabs drums; do [ -f $SRC/$m.wav ] && MATERIALS+=($m); done
    page early_hits
    for t in "1_hall_size_half:HALL:0.5:2" "2_hall_size_full:HALL:1.0:2" "3_massive_size_half:MASSIVE:0.5:4" "4_massive_size_full:MASSIVE:1.0:4"; do
        IFS=: read col sp sz mid <<< "$t"
        set=(--set space=$sp --set attack=NORMAL --set decay=0.6 --set size=$sz)
        r early_hits $col A_today_six "" $set
        ( export IV_FIT_EARLY_TAPS=$mid; r early_hits $col B_middle_${mid} "" $set )
        ( export IV_FIT_EARLY_TAPS=1; r early_hits $col C_one "" $set )
    done
    MATERIALS=("${saved[@]}")
    print -r -- "Early reflections on hits. Each line of HALL's and MASSIVE's tank has six early reflections: your v1.0.21 pick, which filled the hole between a hit and its tail in MASSIVE (\"uneven on Hits\"). Fewer saves CPU (one per line: roughly 10 %), but the hole comes back: measured on a click, the deepest dip before the tail's peak at SIZE full is MASSIVE 1.9 dB with six, 2.9 with four, 16.9 with one; HALL 0.7 with six, 0.9 with two, 2.8 with one. (The last page said one instead of three; it's one instead of six, sorry.) Level-matched; all with the tank at 24 kHz; 100 % wet, so use the MIX fader for the dry.

1_hall_size_half      HALL, SIZE noon
2_hall_size_full      HALL, SIZE full
3_massive_size_half   MASSIVE, SIZE noon
4_massive_size_full   MASSIVE, SIZE full

A_today_six   Six early reflections per line (today)
B_middle_2    Two per line (HALL's middle ground)
B_middle_4    Four per line (MASSIVE's middle ground)
C_one         One per line (fit_cpu's D)" > renders/early_hits/README.txt
    python3 tools/review/make_review.py renders/early_hits --title "Early reflections on hits" --level-match --dry $SRC --wet-only > /dev/null
    echo "renders/early_hits/index.html"
fi

if on onset_phase; then
    # Review 2 §1.1 (v1.0.63's onset item): HALL's and CANYON's first 50 ms lean anti-phase on a centred source:
    # while one early tap is all that sounds, L and R carry it with the product of two signs, and with the taps
    # out on line i + 3 a centred source's two earliest (lines 0 and 2) are both out of phase. B re-lays which
    # line's signs each line's taps go out with (IV_ONSET_PHASE=2, Fdn::earlyOut): lines 0 and 2 on opposite
    # phases, 5 and 7 likewise, the earliest in phase. 0 B, 0 CPU; the tails are untouched (the taps are output
    # only). Short excerpts: hits, drums, stabs and keys, near-mono like most of what goes into the module.
    O=$SRC/onset; mkdir -p $O
    [ -f test_audio/stimulus/02_hits.wav ] || python3 tools/make_stimulus.py
    [ -f $O/hits.wav ] || ffmpeg -v error -y -i test_audio/stimulus/02_hits.wav -af "atrim=0:8,apad=pad_dur=4" -ac 2 -ar 48000 -c:a pcm_s24le $O/hits.wav
    for m in drums stabs keys; do
        [ -f $O/$m.wav ] || ffmpeg -v error -y -i $SRC/$m.wav -af "atrim=0:7,afade=t=out:st=6.5:d=0.5,apad=pad_dur=4" -c:a pcm_s24le $O/$m.wav
    done
    [ -f $O/_click.wav ] || cp $SRC/_click.wav $O/_click.wav # r measures each column on the click
    saved=("${MATERIALS[@]}"); savedSrc=$SRC; SRC=$O; MATERIALS=(hits drums stabs keys)
    page onset_phase
    for t in "1_hall_noon:HALL:0.5" "2_hall_big:HALL:0.8" "3_canyon_noon:CANYON:0.5" "4_canyon_big:CANYON:0.8"; do
        IFS=: read col sp sz <<< "$t"
        set=(--set space=$sp --set attack=NATURAL --set decay=0.5 --set size=$sz)
        r onset_phase $col A_today "" $set
        ( export IV_ONSET_PHASE=2; r onset_phase $col B_centred "" $set )
    done
    print -r -- "The first moment of HALL and CANYON. Review 2 found that in the first 50 ms of their sound (in CANYON, the first 50 ms once it arrives), left and right lean out of phase on a centred sound: the start can sound a little hollow or wide-but-thin, and in mono it loses 2-5 dB until the tail fills in. B re-lays which way each early reflection faces in the stereo field so the start sits evenly between in and out of phase, as the reference units do. The tails after that are untouched, and it costs nothing on the module. A tiny difference: listen to the start of each hit, on headphones and in mono if you can. Level-matched; 100 % wet, so use the MIX fader for the dry.

1_hall_noon     HALL, SIZE noon
2_hall_big      HALL, SIZE 0.8
3_canyon_noon   CANYON, SIZE noon
4_canyon_big    CANYON, SIZE 0.8

A_today         As today
B_centred       The start re-balanced (in and out of phase evenly)" > renders/onset_phase/README.txt
    python3 tools/review/make_review.py renders/onset_phase --title "HALL and CANYON: the first moment" --level-match --dry $SRC --wet-only > /dev/null
    SRC=$savedSrc; MATERIALS=("${saved[@]}")
    echo "renders/onset_phase/index.html"
fi
