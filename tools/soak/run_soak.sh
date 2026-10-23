#!/bin/sh
# Runs the soak harness over many seeds in parallel (docs/research/soak-audit.md).
#   tools/soak/run_soak.sh <build dir> <out dir> <first seed> <last seed> <minutes per seed> [jobs]
# Seed s runs at 44.1 kHz when s % 3 == 0, 48 kHz when s % 3 == 1, 96 kHz when s % 3 == 2;
# its block size comes from the seed (1..256, fixed or random per block). SOAK_BIN (exported) picks
# another binary in the build dir (a frozen copy, so rebuilding during a run changes nothing).
set -e
build=$1; out=$2; first=$3; last=$4; minutes=$5; jobs=${6:-8}
mkdir -p "$out"
seq "$first" "$last" | xargs -P "$jobs" -I{} sh -c '
  s={}; case $((s % 3)) in 0) r=44100;; 1) r=48000;; 2) r=96000;; esac
  "'"$build"'/${SOAK_BIN:-iv_soak}" --seed $s --rate $r --minutes '"$minutes"' --log "'"$out"'/soak_$s.jsonl"'
