#!/bin/zsh
# Renders the P7 / SW1 listening demos (throwaway sketch, see proto.cpp).
# Usage: render.sh <proto binary> <dir with skank.f32 + vibes.f32> <out dir>
set -e
BIN=$1; SRC=$2; OUT=$3

render() { # folder letter_label args...
    local folder=$1 tag=$2; shift 2
    mkdir -p $OUT/$folder
    for m in skank vibes; do
        $BIN $SRC/$m.f32 $SRC/tmp.f32 "$@" 2>/dev/null
        ffmpeg -v error -y -f f32le -ar 48000 -ac 2 -i $SRC/tmp.f32 -c:a pcm_s24le $OUT/$folder/${m}_${tag}.wav
    done
}

# P7 as specced: DESK (resonant EQ + drive in the loop).
render desk A_off      decay=8 desk=0
render desk B_300Hz    decay=8 desk=0.3
render desk C_900Hz    decay=8 desk=0.6
render desk D_2500Hz   decay=8 desk=0.9
render desk E_sweep    decay=8 desksweep=1

# P7 alternative: modulation SPEED (Desmodus-style), depth fixed at 12 cents.
render speed A_still     decay=8 depth=0
render speed B_0.1Hz     decay=8 depth=12 rate=0.1
render speed C_0.5Hz     decay=8 depth=12 rate=0.5
render speed D_2Hz       decay=8 depth=12 rate=2
render speed E_6Hz       decay=8 depth=12 rate=6

# SW1 as specced: the three DUCK styles, at infinite decay.
render duck A_none      decay=0 size=180 damp=6000 duck=0
render duck B_darken    decay=0 size=180 damp=6000 duck=1
render duck C_pushback  decay=0 size=180 damp=6000 duck=2
render duck D_level     decay=0 size=180 damp=6000 duck=3

# SW1 ideas.
render idea_lowcut A_off    decay=8 lowcut=0
render idea_lowcut B_150Hz  decay=8 lowcut=150
render idea_lowcut C_400Hz  decay=8 lowcut=400
render idea_bloom  A_normal decay=10 bloom=0
render idea_bloom  B_bloom  decay=10 bloom=1

rm -f $SRC/tmp.f32
