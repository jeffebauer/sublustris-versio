#!/bin/zsh
# The M8 knob-feel pick page (docs/research/knob-feel-audit.md): renders/knob_feel.
# Per knob that measured uneven, A is today's curve and B the smoother prototype
# (IV_KNOB_FEEL=<name>, a hook in host/render/main.cpp), at a few positions across
# the stretch that jumps or sits still. Renders are 100 % wet (--wet-only --dry: the
# page's MIX fader blends the dry back in live), level-matched, and the page asks for
# picks (no --showcase). Setup and r() copied from tools/render_review_pages.sh.
#
# Usage: tools/pages/knob_feel.sh        (IV_RENDER=build-wt/iv_render to use another build)
set -e
cd "$(dirname "$0")/../.."
R=${IV_RENDER:-build/iv_render}
[ -x $R ] || { echo "$R missing: cmake --build build"; exit 1; }
SRC=renders/_material
mkdir -p $SRC

# ---- Material (as tools/render_review_pages.sh makes it) --------------------------
if [ ! -f $SRC/skank.wav ]; then
    [ -f test_audio/stimulus/04_skank.wav ] || python3 tools/make_stimulus.py
    ffmpeg -v error -y -i test_audio/stimulus/04_skank.wav -af "volume=3dB,apad=pad_dur=12" -c:a pcm_s24le $SRC/skank.wav
fi
[ -f $SRC/hits.wav ] || ffmpeg -v error -y -i test_audio/stimulus/02_hits.wav -af "apad=pad_dur=8" -ac 2 -ar 48000 -c:a pcm_s24le $SRC/hits.wav
[ -f $SRC/_click.wav ] || ffmpeg -v error -y -i test_audio/stimulus/07_click_single.wav -af "apad=whole_dur=40" -ar 48000 -c:a pcm_s24le $SRC/_click.wav
for m in drums pad piano; do
    if [ ! -f $SRC/$m.wav ] && [ -x .venv/bin/python ]; then .venv/bin/python tools/make_material.py $m > /dev/null; fi
done

PAGE=knob_feel
rm -rf renders/$PAGE; mkdir -p renders/$PAGE

# r <knob> <position> <tag> <feel or ""> <materials> --set ...
# One render per material at renders/knob_feel/<knob>/<position>/<material>_<tag>.wav, and one
# click through the same settings for the card's Numbers.
r() {
    local knob=$1 pos=$2 tag=$3 feel=$4 mats=$5; shift 5
    local dir=renders/$PAGE/$knob/$pos
    mkdir -p $dir renders/$PAGE/_measure/$knob/$pos
    for m in ${=mats}; do
        IV_KNOB_FEEL=$feel $R $SRC/$m.wav $dir/${m}_${tag}.wav --set mix=1 --sidecar "$@" > /dev/null
    done
    IV_KNOB_FEEL=$feel $R $SRC/_click.wav renders/$PAGE/_measure/$knob/$pos/${tag}.wav --set mix=1 --sidecar "$@" > /dev/null
}
# ab <knob> <position> <feel> <materials> --set ...: today and the prototype at one position.
ab() {
    local knob=$1 pos=$2 feel=$3 mats=$4; shift 4
    r $knob $pos A_today "" "$mats" "$@"
    r $knob $pos B_smoother $feel "$mats" "$@"
}

# DECAY in MASSIVE at full SIZE: its first notches.
for p in 0 0.1 0.2; do
    ab 1_decay decay_$p decay "skank drums" --set space=MASSIVE --set era=0 --set time=0 --set size=1 --set decay=$p
done
# ERA in HALL: the first notch, then into the Midiverb stretch.
for p in 0.05 0.875 0.95; do
    ab 2_era era_$p era "skank pad" --set space=HALL --set era=$p --set time=0 --set size=0.5 --set decay=0.5
done
# TIME (NORMAL) in MASSIVE: predelay across the first third.
for p in 0.1 0.2 0.3; do
    ab 3_time time_$p time "skank drums" --set space=MASSIVE --set attack=NORMAL --set era=0 --set size=0.5 --set decay=0.5 --set time=$p
done
# TIME (BLOOM) in PLATE: its first notches.
for p in 0 0.1 0.2; do
    ab 4_bloom bloom_$p bloom "skank piano" --set space=PLATE --set attack=BLOOM --set era=0 --set size=0.5 --set decay=0.5 --set time=$p
done
# MASSIVE on hits (the owner's note): medium and large, the grainy_start settings.
for p in 0.5:medium 1:large; do
    ab 5_hits size_${p##*:} hits "hits drums" --set space=MASSIVE --set era=0 --set time=0 --set decay=0.4 --set size=${p%%:*}
done

# Each B must really differ from its A (a prototype that didn't switch on would look identical).
same=0
for a in renders/$PAGE/[0-9]*/*/*_A_today.wav; do
    b=${a/_A_today/_B_smoother}
    if [ "$(md5 -q $a)" = "$(md5 -q $b)" ]; then echo "SAME: $a"; same=$((same+1)); fi
done
[ $same -eq 0 ] || { echo "$same B renders are identical to A: the hook didn't switch on"; exit 1; }

print -r -- "Every knob should change the sound evenly from the first notch to the last, with no stretch that does nothing and no notch that jumps. The M8 audit measured each knob after today's changes (docs/research/knob-feel-audit.md); these are the ones that still jump or sit still. In every card A is today and B is a smoother curve; both are at the same knob position. Pick the one that feels more even (or say neither), and note anything B loses.

Each column is one knob; its rows are positions along the stretch that measured uneven. Play the positions in order, A on all of them and then B, and listen to how much each notch changes.

1_decay   DECAY in MASSIVE, SIZE fully CW. Does DECAY now grow evenly from the first notch? Today fully CCW is barely a tail (the build and one echo: its lines are 1.2-2.8 s long and a 2 s tail loses 30 dB a pass), then the first notch jumps to a real tail. B's shortest tail lasts three passes of the lines (6 s at full SIZE, 2.5 s at noon, still 2 s at SIZE fully CCW), so every notch adds about the same. B makes fully CCW longer in big MASSIVEs: is that still short enough?
2_era     ERA in HALL. Does ERA start doing something from the first notch, and does the Midiverb come in gradually? Today the first 10 % is clean and Neve creeps in; the Midiverb (from 85 %) switches fully on within 2 % of the knob, cutting the tail to a quarter, and the last 13 % then does nothing. B: clean only in the first 3 %, and the Midiverb fades in across its whole stretch, so 87.5 % is partly in and 95 % mostly.
3_time    TIME (NORMAL predelay) in MASSIVE. Does TIME now move the start from the first notch? MASSIVE's wet already starts 0.3 s after a hit, so today's curve (gentle at first) adds almost nothing for the first third. B moves the predelay in straight, even steps (same 0-250 ms range).
4_bloom   TIME (BLOOM) in PLATE. Does BLOOM's first notch now swell no more than the next? Today BLOOM fully CCW still lets some of the dry attack straight through, so the first notch jumps to a swell. B starts at the shortest full swell (0.12 s instead of 0.1 s).
5_hits    MASSIVE on hits, medium and large SIZE (your note: \"a little uneven on Hits\"). Does the swell after each hit now build in one even rise? Today its early taps sit in three bunches along each line and the build dips 3-6 dB before the lines' first echoes take over. B has twice as many early taps, each line's offset from the others so they interleave (same total level): the build dips 3 dB at most.

decay_0     DECAY fully CCW
decay_0.1   DECAY 0.1
decay_0.2   DECAY 0.2
era_0.05    ERA 5 %
era_0.875   ERA 87.5 % (just into the Midiverb)
era_0.95    ERA 95 %
time_0.1    TIME 0.1
time_0.2    TIME 0.2
time_0.3    TIME 0.3
bloom_0     TIME fully CCW
bloom_0.1   TIME 0.1
bloom_0.2   TIME 0.2
size_medium SIZE noon, DECAY 0.4
size_large  SIZE fully CW, DECAY 0.4

A_today     today's curve
B_smoother  the smoother curve" > renders/$PAGE/README.txt
python3 tools/review/make_review.py renders/$PAGE --title "Immensio: do the knobs feel even?" --columns folder1 --rows folder2,material --level-match --dry $SRC --wet-only > /dev/null
echo "renders/$PAGE/index.html"
