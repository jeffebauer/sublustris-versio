#!/usr/bin/env bash
# Build a release: a universal plugin (VST3 + AU, Apple Silicon + Intel,
# macOS 12+), the Versio release firmware and the read-me (releases/README.md),
# packaged into the main checkout's dist/release/<sound>-<sha>/.
# Ported from Resilio Versio's tools/make_release.sh (its ADR 0045).
#   tools/make_release.sh [<ref>]   build + package <ref> (default HEAD, which
#                                   must have no uncommitted code changes)
#   --publish                       also create the GitHub Release, tagged with
#                                   the sound's version (v1.0.62), marked Latest.
#                                   Only on go-live day or after: it refuses
#                                   while the repo is private, and needs <ref>
#                                   to be on origin/main.
#   --dry-run (with --publish)      print the gh command instead of running it
#   --notes <file>                  "what's new" notes into the read-me
#   --release-notes <file.md>       the GitHub Release's notes (default: the read-me)
#   tools/make_release.sh --candidate <ref> <label> [--notes <file>] [--publish]
#       a sound candidate for friends to A/B: the plugin is built as
#       "Sublustris Versio <label>", with its own plugin code and bundle ID, so
#       it installs next to the main plugin (as tools/install_plugin.sh <ref>
#       <label>). No firmware (a candidate hasn't had its CPU run on the
#       module); published as a pre-release, never Latest.
#   tools/make_release.sh --beta <ref> <n> [--notes <file>] [--publish]
#       a beta for players to try (ADR 0046): the firmware and the normal
#       plugin, tagged <sound>-beta.<n> (v1.0.70-beta.1) and published as a
#       pre-release, never Latest, so the site's /latest/ downloads don't move.
#       <ref> may be on any branch pushed to origin (a beta/<sound> branch), not
#       only main. Keep the tag even if the branch is dropped: it holds the
#       released source (the plugin is AGPLv3).
# Assets: the dated SublustrisVersio_<sound>-<sha>.zip and
# sublustris_versio_firmware_<sound>-<sha>.bin, plus, on full releases, the
# same files under the stable names the site links to
# (sublustris-versio-plugin-macos.zip, sublustris-versio-firmware.bin), so
# .../releases/latest/download/<name> always works, and SHA256SUMS.txt. The zip
# carries the firmware too, LICENSE, NOTICE, LICENSES/AGPL-3.0.txt (the plugin
# binaries are AGPLv3 because of JUCE) and the read-me with a link to the
# source at the commit.
# The plugin installed in Ableton is separate: tools/install_plugin.sh <commit>.
# Never touches build/ or firmware/build in any checkout: it builds in its own
# worktree (.claude/worktrees/release), with its own build dir (build-release)
# and the main checkout's libDaisy.
set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO"
MAIN="$(dirname "$(git rev-parse --path-format=absolute --git-common-dir)")"   # the main checkout
GH_REPO="jeffebauer/sublustris-versio"

PUBLISH=0 DRY=0 REF="" LABEL="" BETA="" NOTES="" RELNOTES=""
abspath() { echo "$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"; }
while [ $# -gt 0 ]; do
    case "$1" in
        --publish) PUBLISH=1; shift ;;
        --dry-run) DRY=1; shift ;;
        --candidate) REF="$2"; LABEL="$3"; shift 3 ;;
        --beta) REF="$2"; BETA="$3"; shift 3 ;;
        --notes) NOTES="$(abspath "$2")"; shift 2 ;;
        --release-notes) RELNOTES="$(abspath "$2")"; shift 2 ;;
        -*) echo "Unknown option: $1" >&2; exit 1 ;;
        *) REF="$1"; shift ;;
    esac
done
[ "$DRY" = 0 ] || [ "$PUBLISH" = 1 ] || { echo "--dry-run goes with --publish." >&2; exit 1; }
[ -z "$LABEL" ] || [ -z "$BETA" ] || { echo "--candidate and --beta don't go together." >&2; exit 1; }
[ -z "$BETA" ] || [[ "$BETA" =~ ^[1-9][0-9]*$ ]] || { echo "--beta <ref> <n>: n is 1, 2, 3..." >&2; exit 1; }

if [ -z "$REF" ] || [ "$REF" = HEAD ]; then
    git diff --quiet HEAD -- core firmware plugin host CMakeLists.txt \
        || { echo "Uncommitted changes in code: commit first." >&2; exit 1; }
    REF=HEAD
fi
FULL_SHA="$(git rev-parse "$REF^{commit}")"
SHA="$(git rev-parse --short "$FULL_SHA")"
N="$(git rev-list --count "$SHA")"; PLUGIN_VERSION="1.$((N / 100)).$((N % 100))"   # always goes up (plugin/CMakeLists.txt)
# The sound's version: the newest SPEC changelog line at that commit.
SOUND="$(git show "$SHA:SPEC.md" | awk 'match($0, /^- v[0-9]+\.[0-9]+\.[0-9]+/) && !v { v = substr($0, 3, RLENGTH - 2) } END { print v }')"
[ -n "$SOUND" ] || { echo "No SPEC changelog version at $SHA." >&2; exit 1; }
if [ -n "$LABEL" ]; then
    NAME="Sublustris Versio $LABEL"
    BUNDLE="com.OmbraAudio.ImmensioVersio$LABEL"
    CODE="ImV$(printf '%s' "$LABEL" | tr '[:lower:]' '[:upper:]' | cut -c1)"   # 4 characters, as install_plugin.sh
    STAMP="$SOUND-$SHA-candidate-$LABEL" TAG="$SOUND-$SHA-candidate-$LABEL"
elif [ -n "$BETA" ]; then
    NAME="Sublustris Versio" CODE="ImVs" BUNDLE="com.OmbraAudio.ImmensioVersio"   # the normal plugin: it replaces the release one
    STAMP="$SOUND-$SHA-beta.$BETA" TAG="$SOUND-beta.$BETA"
else
    NAME="Sublustris Versio" CODE="ImVs" BUNDLE="com.OmbraAudio.ImmensioVersio"
    STAMP="$SOUND-$SHA" TAG="$SOUND"
fi

# Publishing checks first, before anything is built.
if [ "$PUBLISH" = 1 ]; then
    if [ -n "$BETA" ]; then
        git fetch -q origin
        [ -n "$(git branch -r --contains "$FULL_SHA")" ] \
            || { echo "$SHA isn't on any branch on origin: push its beta branch first." >&2; exit 1; }
    else
        git fetch -q origin main
        git merge-base --is-ancestor "$FULL_SHA" origin/main \
            || { echo "$SHA isn't on origin/main: merge and push it first." >&2; exit 1; }
    fi
    VIS="$(gh repo view "$GH_REPO" --json visibility -q .visibility)"
    if [ "$VIS" != PUBLIC ]; then
        if [ "$DRY" = 1 ]; then echo "Note: the repo is $VIS; a real --publish would refuse." >&2
        else echo "The repo is $VIS: make it public first (go-live day)." >&2; exit 1; fi
    fi
    if gh release view "$TAG" -R "$GH_REPO" >/dev/null 2>&1; then
        echo "A release $TAG already exists." >&2; exit 1
    fi
fi

OUT="$MAIN/dist/release/$STAMP"
SOURCE_URL="https://github.com/$GH_REPO/tree/$FULL_SHA"
mkdir -p "$OUT"

WT="$MAIN/.claude/worktrees/release"
if [ -d "$WT" ]; then git -C "$WT" checkout -q --detach "$SHA"; else git worktree add -q --detach "$WT" "$SHA"; fi
[ -L "$WT/libs/JUCE" ] || { rmdir "$WT/libs/JUCE" 2>/dev/null || true; ln -s "$MAIN/libs/JUCE" "$WT/libs/JUCE"; }

FW=""
if [ -z "$LABEL" ]; then
    echo "== Firmware (release) at $SHA, sound $SOUND"
    export PATH="$HOME/.local/arm-gnu-toolchain/bin:$PATH"
    [ -f "$MAIN/libs/libDaisy/build/libdaisy.a" ] || { echo "Build libDaisy first: make -C libs/libDaisy -j8" >&2; exit 1; }
    make -C "$WT/firmware" MODE=release "LIBDAISY_DIR=$MAIN/libs/libDaisy" >/dev/null
    FW="$OUT/sublustris_versio_firmware_$STAMP.bin"
    cp "$WT/firmware/build/sublustris_versio.bin" "$FW"
    FW_BYTES=$(stat -f %z "$FW")
    [ "$FW_BYTES" -le 131072 ] || { echo "Firmware over 128 KB ($FW_BYTES B)." >&2; exit 1; }
    [ "$FW_BYTES" -le 112640 ] || echo "Warning: firmware over the 110 KB release budget ($FW_BYTES B, ADR 0002)." >&2
    echo "   $FW_BYTES bytes"
fi

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
RVERSION="$SOUND"; [ -z "$BETA" ] || RVERSION="$TAG"   # the read-me names a beta as v1.0.70-beta.1
python3 - "$REPO/releases/README.md" "$README" "$RVERSION" "$NAME" "$(basename "${FW:-none}")" "${NOTES:-}" "$SOURCE_URL" "$SHA" <<'EOF'
import re, sys
src, dst, sound, name, fw, notes, source, sha = sys.argv[1:9]
s = open(src).read()
if fw == "none":
    s = re.sub(r"\{\{FIRMWARE_START\}\}\n.*?\{\{FIRMWARE_END\}\}\n", "", s, flags=re.S)
else:
    s = s.replace("{{FIRMWARE_START}}\n", "").replace("{{FIRMWARE_END}}\n", "")
s = s.replace("{{NOTES}}\n", ("\n" + open(notes).read().rstrip("\n") + "\n") if notes else "")
s = (s.replace("{{VERSION}}", sound).replace("{{NAME}}", name).replace("{{FIRMWARE}}", fw)
      .replace("{{SOURCE}}", source).replace("{{COMMIT}}", sha))
left = re.findall(r"\{\{[A-Z_]+\}\}", s)
if left: sys.exit(f"Unfilled placeholders in the read-me: {left}")
open(dst, "w").write(s)
EOF
# Licence files from the released commit (older refs fall back to this checkout's).
LIC="$WT"; [ -f "$LIC/NOTICE" ] || LIC="$REPO"
rm -rf "$OUT/LICENSES"; mkdir -p "$OUT/LICENSES"
cp "$LIC/LICENSE" "$OUT/LICENSE"
sed "s#https://github.com/$GH_REPO/tree/<tag>#$SOURCE_URL#" "$LIC/NOTICE" > "$OUT/NOTICE"
cp "$LIC/LICENSES/AGPL-3.0.txt" "$OUT/LICENSES/AGPL-3.0.txt"
ZIP="SublustrisVersio_$STAMP.zip"
FILES=("$NAME.vst3" "$NAME.component" "$(basename "$README")" LICENSE NOTICE LICENSES)
[ -n "$FW" ] && FILES+=("$(basename "$FW")")
(cd "$OUT" && rm -f "$ZIP" && zip -q -r -y "$ZIP" "${FILES[@]}")
echo "Built $OUT/$ZIP"

# Stable names for .../releases/latest/download/<name> (full releases only:
# candidates and betas are pre-releases, which /latest/ skips anyway).
ASSETS=("$OUT/$ZIP"); [ -n "$FW" ] && ASSETS+=("$FW")
if [ -z "$LABEL$BETA" ]; then
    cp "$OUT/$ZIP" "$OUT/sublustris-versio-plugin-macos.zip"
    cp "$FW" "$OUT/sublustris-versio-firmware.bin"
    ASSETS+=("$OUT/sublustris-versio-plugin-macos.zip" "$OUT/sublustris-versio-firmware.bin")
    # The site's fallback (site/src/data/release.json) when GitHub's API can't be reached.
    echo "   release.json: version $TAG, firmware $(stat -f %z "$OUT/sublustris-versio-firmware.bin") B, plugin $(stat -f %z "$OUT/sublustris-versio-plugin-macos.zip") B"
fi
NAMES=(); for a in "${ASSETS[@]}"; do NAMES+=("$(basename "$a")"); done
(cd "$OUT" && shasum -a 256 "${NAMES[@]}" > SHA256SUMS.txt)
ASSETS+=("$OUT/SHA256SUMS.txt")

if [ "$PUBLISH" = 1 ]; then
    echo "== GitHub Release $TAG"
    if [ -n "$LABEL$BETA" ]; then KIND=(--prerelease --latest=false); else KIND=(--latest); fi
    TITLE="Sublustris Versio $SOUND"; [ -z "$BETA" ] || TITLE="Sublustris Versio $SOUND beta $BETA"
    CMD=(gh release create "$TAG" "${ASSETS[@]}" -R "$GH_REPO" --target "$FULL_SHA" "${KIND[@]}"
        --title "$TITLE" --notes-file "${RELNOTES:-$README}")
    if [ "$DRY" = 1 ]; then printf 'Dry run, would run:'; printf ' %q' "${CMD[@]}"; echo; else "${CMD[@]}"; fi
fi
