#!/bin/sh
# Encode the hero frames (render_hero.py) into the site's videos + posters.
#   site/renders/make_hero_video.sh FRAMES_DIR OUT_DIR [preview]
# hero-reveal      frames 1-240   (8 s, plays once)   (names: what site/src/components/HeroMedia.astro loads)
# hero-hold-loop   frames 241-360 (4 s, loops seamlessly; starts where the reveal ends)
# Each as MP4 (H.264, CRF 20, yuv420p, faststart) and WebM (VP9, CRF 32),
# plus posters: first and last frame of the reveal, first frame of the loop.
# "preview": MP4 only, CRF 23, files named *-preview (for the contact sheet).
# Ported from Resilio Versio's make_hero_video.sh (the owner's MIT code).
set -e
F="$1"; O="$2"; MODE="$3"
SH="unsharp=5:5:0.6:5:5:0"   # light luma unsharp mask: the panel print is small
mkdir -p "$O"
SUF=""; CRF=20
[ "$MODE" = "preview" ] && SUF="-preview" && CRF=23
enc() { # name start count
  ffmpeg -y -loglevel error -framerate 30 -start_number "$2" -i "$F/f_%04d.jpg" -frames:v "$3" -vf "$SH" \
    -c:v libx264 -crf $CRF -preset slow -pix_fmt yuv420p -movflags +faststart "$O/$1$SUF.mp4"
  if [ "$MODE" != "preview" ]; then
    ffmpeg -y -loglevel error -framerate 30 -start_number "$2" -i "$F/f_%04d.jpg" -frames:v "$3" -vf "$SH" \
      -c:v libvpx-vp9 -crf 32 -b:v 0 -row-mt 1 -pix_fmt yuv420p "$O/$1.webm"
  fi
}
enc hero-reveal 1 240
enc hero-hold-loop 241 120
poster() { ffmpeg -y -loglevel error -i "$F/f_$1.jpg" -vf "$SH" -q:v 3 "$O/$2$SUF.jpg"; }
poster 0001 hero-reveal-first
poster 0240 hero-reveal-poster
poster 0241 hero-hold-loop-poster
ls -la "$O" | grep hero-
