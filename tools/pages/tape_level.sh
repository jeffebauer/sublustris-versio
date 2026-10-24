#!/bin/zsh
# Tape out hearing a steady level (picked on renders/fullmix_send as part of "both"), checked on held notes
# first: it drives a quiet held note harder into the tape. IV_TAPE_LEVEL_CAP: 1 off, 4 (+12 dB), 16 (+24 dB).
# Usage: tools/pages/tape_level.sh <iv_render>
set -e
cd "$(dirname "$0")/../.."
R=${1:?iv_render}; PAGE=tape_level; SRC=renders/_material; LONG=renders/_material_long
for m in vibes pad keys drums; do [ -f $LONG/${m}75.wav ] || ffmpeg -v error -y -i $SRC/$m.wav -af "apad=whole_dur=75" -c:a pcm_s24le $LONG/${m}75.wav; done
ffmpeg -v error -y -i $SRC/fullmix.wav -af "apad=whole_dur=75" -c:a pcm_f32le $LONG/fullmix75.wav
ffmpeg -v error -y -i $SRC/fullmix_send.wav -af "apad=whole_dur=75" -c:a pcm_f32le $LONG/fullmix75_send.wav
rm -rf renders/$PAGE; mkdir -p renders/$PAGE
for c in "HALL:0.64:1:hall_cassette_held" "HALL:1:1:hall_full_held" "MASSIVE:0.64:0.65:massive_cassette"; do
    IFS=: read sp era dc col <<< "$c"
    mkdir -p renders/$PAGE/$col
    for v in 1:A_no_steady_level 4:B_steady_level_12dB 16:C_steady_level_24dB; do
        for m in vibes pad keys drums fullmix; do
            in=$LONG/${m}75.wav; [ -f $LONG/${m}75_send.wav ] && in=$LONG/${m}75_send.wav
            IV_TAPE_LEVEL_CAP=${v%%:*} $R $in renders/$PAGE/$col/${m}75_${v##*:}.wav --set mix=1 --sidecar --set space=$sp --set era=$era --set decay=$dc --set size=0.6 > /dev/null
        done
    done
done
for a in renders/$PAGE/*/*_A_no_steady_level.wav; do for t in B_steady_level_12dB C_steady_level_24dB; do [ "$(md5 -q $a)" != "$(md5 -q ${a/A_no_steady_level/$t})" ] || echo "same: $a $t"; done; done
print -r -- "Tape that hears a steady level, checked on held notes. You picked 'both' on the full mix (a hotter send, and tape that hears a steady level). The steady level works by driving quiet sound harder into the tape and giving the level back after it, which brings tape character to a quiet send but also to a quiet held note: on the vibes it lifts the tape's harmonics to about 36 dB under the note. Your other question answered: with the tape bypassed, the held vibes' harmonics are 56-78 dB under the note, as with ERA clean, so what you heard even with no lopsidedness was the tape's own (odd) saturation; it doesn't build up over time. All versions have the new tape curve (a third of the lopsidedness) and the full mix has its new, hotter send. A no steady level; B the steady level, quiet sound driven up by at most 12 dB; C at most 24 dB (what you heard on the full-mix page). Held notes ring 75 s at infinite DECAY; the last column is MASSIVE at DECAY 0.65 with drums and the full mix. Level-matched. Which balance of tape character and clean held notes?

hall_cassette_held   HALL, ERA 0.64, DECAY infinite
hall_full_held       HALL, ERA fully CW, DECAY infinite
massive_cassette     MASSIVE, ERA 0.64, DECAY 0.65

A_no_steady_level     today's tape (a third of the lopsidedness)
B_steady_level_12dB   the tape hears a steady level, quiet sound driven up at most 12 dB
C_steady_level_24dB   at most 24 dB" > renders/$PAGE/README.txt
python3 tools/review/make_review.py renders/$PAGE --title "Immensio: tape and held notes" --level-match --dry $LONG --wet-only > /dev/null
echo "renders/$PAGE/index.html"
