#!/bin/sh
# Encode the hero frames (render_hero.py, transparent PNGs) into the site's videos + posters.
#   site/renders/make_hero_video.sh FRAMES_DIR OUT_DIR [preview]
# hero-reveal      frames 1-240   (8 s, plays once)   (names: what site/src/components/HeroMedia.astro loads)
# hero-hold-loop   frames 241-360 (4 s, loops seamlessly; starts where the reveal ends)
# The background is transparent (owner, 30 Oct 2026: the haze shows through), so each film is
#   WebM  VP9 with alpha (Chrome, Firefox, Edge)
#   MP4   HEVC with alpha, hvc1 (Safari on macOS and iOS; made with VideoToolbox, so on a Mac)
# plus PNG posters: first and last frame of the reveal, first frame of the loop.
# "preview": WebM only, faster, named *-preview.
# Ported from Resilio Versio's make_hero_video.sh (the owner's MIT code).
set -e
F="$1"; O="$2"; MODE="$3"
SH="unsharp=5:5:0.6:5:5:0"   # light luma unsharp mask: the panel print is small
mkdir -p "$O"
SUF=""; CRF=30
[ "$MODE" = "preview" ] && SUF="-preview" && CRF=36
enc() { # name start count
  ffmpeg -y -loglevel error -framerate 30 -start_number "$2" -i "$F/f_%04d.png" -frames:v "$3" \
    -vf "format=yuva420p,$SH" -c:v libvpx-vp9 -pix_fmt yuva420p -auto-alt-ref 0 -crf $CRF -b:v 0 -row-mt 1 \
    "$O/$1$SUF.webm"
  if [ "$MODE" != "preview" ]; then
    ffmpeg -y -loglevel error -framerate 30 -start_number "$2" -i "$F/f_%04d.png" -frames:v "$3" \
      -vf "$SH,format=bgra" -c:v hevc_videotoolbox -allow_sw 1 -alpha_quality 0.8 -q:v 62 -tag:v hvc1 \
      -movflags +faststart "$O/$1.mp4"
  fi
}
enc hero-reveal 1 240
enc hero-hold-loop 241 120
poster() { cp "$F/f_$1.png" "$O/$2$SUF.png"; }
poster 0001 hero-reveal-first
poster 0240 hero-reveal-poster
poster 0241 hero-hold-loop-poster
find "$O" -name 'hero-*' -exec du -k {} +
