#!/usr/bin/env bash
# Build a shareable Mac package of the plugin for a friend: a universal VST3 +
# AU (Apple Silicon + Intel, macOS 12+), the read-me (releases/README.md) and
# the licence files, zipped into the main checkout's dist/release/<tag>/.
# Plugin only: no firmware until the module runs cleanly. Nothing is uploaded
# anywhere; the owner sends the zip themself. Adapted from Resilio Versio's
# tools/make_release.sh (its --publish is left out here).
#   tools/make_release.sh [<ref>]   the plugin at <ref> (default HEAD, which
#                                   must have no uncommitted code changes)
#   tools/make_release.sh --candidate <ref> <label>
#       a sound candidate for friends to A/B: built as "Sublustris Versio
#       <label>", with its own plugin code and bundle ID, so it installs next
#       to the main plugin (as tools/install_plugin.sh <ref> <label>).
#   --notes <file>                  "what's new" notes into the read-me
# The zip carries LICENSE, NOTICE, LICENSES/AGPL-3.0.txt (the plugin binaries
# are AGPLv3 because of JUCE, as Resilio's ADR 0045) and the read-me.
# The plugin installed in Ableton is separate: tools/install_plugin.sh <commit>.
# Never touches build/ or any other worktree's build dir: it builds in its own
# worktree (.claude/worktrees/release) and build dir (build-release).
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO"
MAIN="$(dirname "$(git rev-parse --path-format=absolute --git-common-dir)")"   # the main checkout

REF="" LABEL="" NOTES=""
while [ $# -gt 0 ]; do
    case "$1" in
        --candidate) REF="$2"; LABEL="$3"; shift 3 ;;
        --notes) NOTES="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"; shift 2 ;;
        --publish) echo "No --publish here: the owner sends the zip themself." >&2; exit 1 ;;
        -*) echo "Unknown option: $1" >&2; exit 1 ;;
        *) REF="$1"; shift ;;
    esac
done

if [ -z "$REF" ] || [ "$REF" = HEAD ]; then
    git diff --quiet HEAD -- core firmware plugin host CMakeLists.txt \
        || { echo "Uncommitted changes in code: commit first." >&2; exit 1; }
    REF=HEAD
fi
SHA="$(git rev-parse --short "$REF^{commit}")"
N="$(git rev-list --count "$SHA")"; PLUGIN_VERSION="1.$((N / 100)).$((N % 100))"   # always goes up (plugin/CMakeLists.txt)
# The sound's version: the newest SPEC changelog line at that commit.
SOUND="$(git show "$SHA:SPEC.md" | awk 'match($0, /^- v[0-9]+\.[0-9]+\.[0-9]+/) && !v { v = substr($0, 3, RLENGTH - 2) } END { print v }')"
[ -n "$SOUND" ] || { echo "No SPEC changelog version at $SHA." >&2; exit 1; }
if [ -n "$LABEL" ]; then
    NAME="Sublustris Versio $LABEL"
    BUNDLE="com.OmbraAudio.ImmensioVersio$LABEL"
    CODE="ImV$(printf '%s' "$LABEL" | tr '[:lower:]' '[:upper:]' | cut -c1)"   # 4 characters, as install_plugin.sh
    TAG="$SOUND-$SHA-candidate-$LABEL"
else
    NAME="Sublustris Versio" CODE="ImVs" BUNDLE="com.OmbraAudio.ImmensioVersio" TAG="$SOUND-$SHA"
fi
OUT="$MAIN/dist/release/$TAG"
SOURCE_URL="https://github.com/jeffebauer/sublustris-versio/tree/$(git rev-parse "$SHA")"
mkdir -p "$OUT"

WT="$MAIN/.claude/worktrees/release"
if [ -d "$WT" ]; then git -C "$WT" checkout -q --detach "$SHA"; else git worktree add -q --detach "$WT" "$SHA"; fi
[ -L "$WT/libs/JUCE" ] || { rmdir "$WT/libs/JUCE" 2>/dev/null || true; ln -s "$MAIN/libs/JUCE" "$WT/libs/JUCE"; }

echo "== Universal plugin \"$NAME\" at $SHA (sound $SOUND, version $PLUGIN_VERSION)"
cmake -S "$WT" -B "$WT/build-release" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" -DIV_INSTALL_PLUGIN=OFF \
    "-DIV_PLUGIN_VERSION=$PLUGIN_VERSION" "-DIV_PLUGIN_NAME=$NAME" "-DIV_PLUGIN_CODE=$CODE" "-DIV_PLUGIN_BUNDLE_ID=$BUNDLE" >/dev/null
cmake --build "$WT/build-release" --target ImmensioVersio_VST3 ImmensioVersio_AU >/dev/null
ART="$WT/build-release/plugin/ImmensioVersio_artefacts/Release"
rm -rf "$OUT/$NAME.vst3" "$OUT/$NAME.component"
cp -R "$ART/VST3/$NAME.vst3" "$ART/AU/$NAME.component" "$OUT/"
for b in "$OUT/$NAME.vst3" "$OUT/$NAME.component"; do
    codesign --force --deep -s - "$b" 2>/dev/null
    codesign -v "$b"
    lipo -archs "$b/Contents/MacOS/$NAME" | grep -q "x86_64 arm64\|arm64 x86_64" || { echo "Not universal: $b" >&2; exit 1; }
done

echo "== Package"
README="$OUT/READ ME - $NAME.txt"
python3 - "$REPO/releases/README.md" "$README" "$SOUND" "$NAME" "${NOTES:-}" "$SHA" <<'EOF'
import sys
src, dst, sound, name, notes, sha = sys.argv[1:7]
s = open(src).read()
s = s.replace("{{NOTES}}\n", ("\n" + open(notes).read().rstrip("\n") + "\n") if notes else "")
s = s.replace("{{VERSION}}", sound).replace("{{NAME}}", name).replace("{{COMMIT}}", sha)
open(dst, "w").write(s)
EOF
# Licence files from the released commit (older refs fall back to this checkout's).
LIC="$WT"; [ -f "$LIC/NOTICE" ] || LIC="$REPO"
rm -rf "$OUT/LICENSES"; mkdir -p "$OUT/LICENSES"
cp "$LIC/LICENSE" "$OUT/LICENSE"
sed "s#https://github.com/jeffebauer/sublustris-versio/tree/<tag>#$SOURCE_URL#" "$LIC/NOTICE" > "$OUT/NOTICE"
cp "$LIC/LICENSES/AGPL-3.0.txt" "$OUT/LICENSES/AGPL-3.0.txt"
ZIP="SublustrisVersio_$TAG.zip"
(cd "$OUT" && rm -f "$ZIP" && zip -q -r -y "$ZIP" "$NAME.vst3" "$NAME.component" "$(basename "$README")" LICENSE NOTICE LICENSES)
(cd "$OUT" && shasum -a 256 "$ZIP" > SHA256SUMS.txt)
echo "Built $OUT/$ZIP"
