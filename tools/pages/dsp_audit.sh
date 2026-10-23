#!/bin/zsh
# The DSP audit's pick page (docs/research/dsp-audit.md, 24 Oct 2026): A today, B the prototype,
# for the three audit items whose prototype measured a real, audible difference and passed the
# energy check: A3 (a frozen pad holds still), E4 (the 1980s rack's noisy reads), E5 (the tape
# saturates the wet). Renders 100 % wet; the page's MIX fader blends the dry in (--wet-only --dry).
# Built WITHOUT --showcase: it asks the owner to pick. Setup and r() copied from
# tools/render_review_pages.sh (which this script doesn't touch).
#
# Usage: tools/pages/dsp_audit.sh        (IV_RENDER overrides the renderer; default build-wt/iv_render)
set -e
cd "$(dirname "$0")/../.."
R=${IV_RENDER:-build-wt/iv_render}
[ -x $R ] || { echo "$R missing: cmake --build build-wt"; exit 1; }
SRC=renders/_material
mkdir -p $SRC
PAGE=dsp_audit

# ---- Material (as tools/render_review_pages.sh) -----------------------------------
if [ ! -f $SRC/skank.wav ]; then
    [ -f test_audio/stimulus/04_skank.wav ] || python3 tools/make_stimulus.py
    ffmpeg -v error -y -i test_audio/stimulus/04_skank.wav -af "volume=3dB,apad=pad_dur=12" -c:a pcm_s24le $SRC/skank.wav
fi
[ -f $SRC/_click.wav ] || ffmpeg -v error -y -i test_audio/stimulus/07_click_single.wav -af "apad=whole_dur=40" -ar 48000 -c:a pcm_s24le $SRC/_click.wav
for m in pad keys voice drums bass fullmix; do
    if [ ! -f $SRC/$m.wav ] && [ -x .venv/bin/python ]; then .venv/bin/python tools/make_material.py $m > /dev/null; fi
done
# A3's held material: the first 3.5 s (faded), then silence to 95 s; frozen at 3 s, unfrozen at 88 s,
# so the pad is heard on its own for 85 s (the smear grows over minutes).
for m in pad vibes; do
    [ -f $SRC/${m}_held.wav ] || ffmpeg -v error -y -i $SRC/$m.wav -af "atrim=0:3.5,afade=t=out:st=3.1:d=0.4,apad=whole_dur=95" -c:a pcm_s24le $SRC/${m}_held.wav
    print -r -- '{ "buttons": [[3.0, 3.1], [88.0, 88.1]] }' > $SRC/freeze_held_${m}_held.json
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
    # One click through the same settings (no gestures): the card's Numbers come from this.
    mkdir -p renders/$page/_measure/$dir
    $R $SRC/_click.wav renders/$page/_measure/$dir/${tag}.wav --set mix=1 --sidecar "$@" > /dev/null
}

rm -rf renders/$PAGE; mkdir -p renders/$PAGE

# ---- A3: a frozen pad holds still (Costello: modulation depth 0 to freeze) -----------
MATERIALS=(pad_held vibes_held)
for sp in HALL MASSIVE PLATE; do
    f=freeze_${(L)sp}
    r $PAGE $f A_today freeze_held --set space=$sp --set size=0.6 --set decay=0.6 --set era=0
    IV_FREEZE_STILL=1 r $PAGE $f B_still freeze_held --set space=$sp --set size=0.6 --set decay=0.6 --set era=0
done

# ---- E4: ERA's mu-law stretch with the 1980s rack's noisy, coarse reads ---------------
MATERIALS=(pad keys voice vibes skank drums)
for sp in HALL MASSIVE; do
    f=vintage_${(L)sp}
    r $PAGE $f A_today "" --set space=$sp --set size=0.6 --set decay=0.65 --set era=0.8
    IV_VINTAGE_MOD=1 r $PAGE $f B_noisy_reads "" --set space=$sp --set size=0.6 --set decay=0.65 --set era=0.8
done

# ---- E5: the cassette stage saturates the wet on its way out --------------------------
MATERIALS=(bass drums pad keys skank fullmix)
for sp in HALL MASSIVE; do
    f=tape_${(L)sp}
    r $PAGE $f A_today "" --set space=$sp --set size=0.6 --set decay=0.6 --set era=0.55
    IV_TAPE_OUT=1 r $PAGE $f B_tape_out "" --set space=$sp --set size=0.6 --set decay=0.6 --set era=0.55
done

# Every B must really differ from its A (a prototype that didn't engage is a bug, not a version).
bad=0
for d in renders/$PAGE/*(/); do
    [ ${d:t} = _measure ] && continue
    for a in $d/*_A_today.wav; do
        for b in ${a%_A_today.wav}_B_*.wav; do
            if [ "$(md5 -q $a)" = "$(md5 -q $b)" ]; then echo "SAME: $a $b"; bad=1; fi
        done
    done
done
[ $bad -eq 0 ] || { echo "a B version renders identically to its A"; exit 1; }

print -r -- "Three questions from the DSP audit (24 Oct 2026), each a choice between today's reverb (A) and a change built the established way (B). Everything else is the same. Pick A or B on each card, and say why in a note if you can. Versions are level-matched, so pick on character, not loudness.

1. Freeze (freeze_hall, freeze_massive, freeze_plate): a chord is frozen at 3 s and held for 85 s on its own. Today the tank keeps its slow inner movement while frozen, so over a minute or more a held chord slowly smears into a shimmer of beating notes. B stops that movement while frozen (Valhalla's advice for infinite sounds: modulation to zero), so the chord stays exactly as you froze it. Should a frozen chord stay perfectly still, or keep slowly shimmering? Listen to the second half of the hold.

2. ERA's 1980s stretch (vintage_hall, vintage_massive; ERA 0.8): old Lexicon racks read their delays coarsely and with a noisy wobble, which put a faint halo of noise around long tails; Valhalla calls this the thing that mattered most for the old sound. B adds that to ERA's 1980s stretch (it comes in with the 1980s stage and stays to the end of the knob). Does the halo make it feel more like an old rack, or just noisier? It's clearest in HALL and on sustained notes; MASSIVE changes less.

3. ERA's tape stretch (tape_hall, tape_massive; ERA 0.55): today's tape stage darkens, wobbles and hisses, but its saturation never actually kicks in on music (measured: it only reaches its threshold on full-scale noise). B makes the tape saturate the reverb on its way out, like sending the return through a cassette deck: a little thicker and warmer, more so on loud notes. Is B's warmth what you'd expect from the tape stage?

freeze_hall      freeze, HALL (SIZE 0.6, DECAY 0.6)
freeze_massive   freeze, MASSIVE
freeze_plate     freeze, PLATE (today it already keeps only a little of its movement)
vintage_hall     ERA 0.8, HALL (SIZE 0.6, DECAY 0.65)
vintage_massive  ERA 0.8, MASSIVE
tape_hall        ERA 0.55, HALL (SIZE 0.6, DECAY 0.6)
tape_massive     ERA 0.55, MASSIVE

A_today          today's reverb
B_still          a frozen pad holds still: the tank's own movement stops while frozen
B_noisy_reads    ERA's 1980s stretch reads its delays the old Lexicon way: coarse steps and a noisy wobble
B_tape_out       ERA's tape saturates the wet on its way out" > renders/$PAGE/README.txt
python3 tools/review/make_review.py renders/$PAGE --title "Immensio: three questions from the DSP audit" --level-match --dry $SRC --wet-only > /dev/null
du -sh renders/$PAGE
echo "renders/$PAGE/index.html"
