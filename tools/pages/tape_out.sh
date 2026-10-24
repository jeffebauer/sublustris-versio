#!/bin/zsh
# Tape out, stronger (owner, 24 Oct 2026: "very subtle — really only audible on MASSIVE drums. Can we make
# the saturation a little stronger? Should we explore adding tape hysteresis as well?"; core/dsp/Tape.h,
# docs/research/tape-out.md). IV_TAPE_MODE = strength × 10 + hysteresis (strength 0 today, 1 a little stronger;
# hysteresis 0 none, 1 light, 2 full). Round 2 (owner: "the strongest saturation is making it too difficult to
# hear the hysteresis"): hysteresis on today's and the little-stronger tape. Usage: tools/pages/tape_out.sh <iv_render>
set -e
cd "$(dirname "$0")/../.."
R=${1:?iv_render}; PAGE=tape_out; SRC=renders/_material
rm -rf renders/$PAGE; mkdir -p renders/$PAGE
for c in "HALL:0.55:hall_cassette" "MASSIVE:0.55:massive_cassette" "HALL:0.64:hall_cassette_full" "MASSIVE:0.64:massive_cassette_full"; do
    sp=${c%%:*}; rest=${c#*:}; era=${rest%%:*}; col=${rest#*:}
    mkdir -p renders/$PAGE/$col
    for v in 0:A_today 10:B_a_little_stronger 1:C_today_light_hysteresis 2:D_today_full_hysteresis 11:E_stronger_light_hysteresis 12:F_stronger_full_hysteresis; do
        for m in drums skank pad bass keys fullmix; do
            in=$SRC/$m.wav; [ -f $SRC/${m}_send.wav ] && in=$SRC/${m}_send.wav
            IV_TAPE_MODE=${v%%:*} $R $in renders/$PAGE/$col/${m}_${v##*:}.wav --set mix=1 --sidecar --set space=$sp --set era=$era --set decay=0.65 --set size=0.7 > /dev/null
        done
    done
done
for a in renders/$PAGE/*/*_A_today.wav; do for t in B_a_little_stronger C_today_light_hysteresis D_today_full_hysteresis E_stronger_light_hysteresis F_stronger_full_hysteresis; do [ "$(md5 -q $a)" != "$(md5 -q ${a/A_today/$t})" ] || echo "same: $a $t"; done; done
print -r -- "Tape out, round 2, re-made on the settled tape (24 Oct: a third of the lopsidedness, no steady level; the first render of this page used the old curve). Hysteresis without the heavy saturation. You found the strongest saturation hid the hysteresis, so here it sits on today's tape and on the little-stronger one. Hysteresis is tape's magnetism lagging behind the signal: a little grit at low levels, a softer, smeared attack and a slight thickening, separate from the saturation's warmth on peaks. A is today. B is a little stronger (the highs pushed into the tape first, drive up to +12 dB). C and D add light and full hysteresis to today's tape; E and F add them to B. Light is a simple, cheap model (fine for the module); full is the physical tape model (Jiles-Atherton, as in ChowTape), which may be too heavy for the module's processor, so it's here mainly to show whether hysteresis is worth having. Level-matched; ERA at the cassette stage (0.55) and its fullest (0.64). Does hysteresis add something you want, and on which strength?

hall_cassette           HALL, ERA 0.55 (the cassette stage)
massive_cassette        MASSIVE, ERA 0.55
hall_cassette_full      HALL, ERA 0.64 (the cassette at its fullest)
massive_cassette_full   MASSIVE, ERA 0.64

A_today                        today's tape out
B_a_little_stronger            a little stronger
C_today_light_hysteresis       today's tape plus light hysteresis
D_today_full_hysteresis        today's tape plus full hysteresis
E_stronger_light_hysteresis    a little stronger plus light hysteresis
F_stronger_full_hysteresis     a little stronger plus full hysteresis" > renders/$PAGE/README.txt
python3 tools/review/make_review.py renders/$PAGE --title "Immensio: tape out, round 2 (settled tape)" --level-match --dry $SRC --wet-only > /dev/null
echo "renders/$PAGE/index.html"
