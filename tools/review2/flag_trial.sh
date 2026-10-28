#!/bin/sh
# Flash trial (review 2 audit, 28 Oct 2026): recompile the release build's Core objects (Engine, Fdn, Plate) with
# extra compiler flags and relink, printing the .bin size against the shipped one. Run from firmware/ after
# `make all-variants` (it reuses build/obj-release-48's main.o / startup.o and the logged commands).
# usage: tools/review2/flag_trial.sh <fw.log> <outdir> "<extra flags>" [objects...]
set -e
LOG=$1; OUT=$2; EXTRA=$3; shift 3
OBJS=${*:-Engine Fdn Plate}
mkdir -p "$OUT"
for o in $OBJS; do
  cmd=$(grep -E "^arm-none-eabi-g\+\+ -c .*-DIV_MODE_RELEASE.*/$o\.cpp -o" "$LOG" | head -1 \
        | sed -E "s#-Wa,-a,-ad,-alms=[^ ]+##; s#-MMD -MP -MF\"[^\"]+\"##; s#-o build/obj-release-48/$o\.o#$EXTRA -o $OUT/$o.o#")
  sh -c "$cmd"
done
link=$(grep -E "^arm-none-eabi-g\+\+ build/obj-release-48/startup" "$LOG" | head -1)
for o in $OBJS; do link=$(echo "$link" | sed "s#build/obj-release-48/$o\.o#$OUT/$o.o#"); done
link=$(echo "$link" | sed -E "s#-Wl,-Map=[^ ]+#-Wl,-Map=$OUT/t.map#; s#-o build/obj-release-48/sublustris_versio.elf#-o $OUT/t.elf#; s#-Wl,--print-memory-usage##")
sh -c "$link" 2>/dev/null
arm-none-eabi-objcopy -O binary -S "$OUT/t.elf" "$OUT/t.bin"
echo "$(wc -c < "$OUT/t.bin" | tr -d ' ') B  (shipped $(wc -c < build/sublustris_versio.bin | tr -d ' '))  flags: $EXTRA  objs: $OBJS"
