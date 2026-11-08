#!/usr/bin/env bash

set -euo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly SOURCE_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="${FOOYIN_APPIMAGE_BUILD_DIR:-$SOURCE_DIR/build-appimage}"
APPDIR="${FOOYIN_APPIMAGE_APPDIR:-$BUILD_DIR/AppDir}"
OUTPUT_DIR="${FOOYIN_APPIMAGE_OUTPUT_DIR:-$BUILD_DIR/output}"

case "$(uname -m)" in
    x86_64)
        APPIMAGE_ARCH=x86_64
        ;;
    aarch64 | arm64)
        APPIMAGE_ARCH=aarch64
        ;;
    *)
        echo "Unsupported AppImage architecture: $(uname -m)" >&2
        exit 1
        ;;
esac

QUICK_SHARUN="${FOOYIN_QUICK_SHARUN:-quick-sharun}"

mkdir -p "$BUILD_DIR" "$OUTPUT_DIR"

cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DBUILD_TESTING=OFF \
    -DFETCH_PROJECTM=ON \
    -DENABLE_SYSTEM_GLM=ON \
    -DBUILD_CCACHE="${BUILD_CCACHE:-ON}" \
    -DBUILD_PCH="${BUILD_PCH:-ON}"
cmake --build "$BUILD_DIR" --parallel

cmake --install "$BUILD_DIR" --component fooyin
cmake -E remove_directory "$APPDIR"

VERSION="$(sed -nE 's/^[[:space:]]*VERSION[[:space:]]+([^[:space:])]+).*/\1/p' "$SOURCE_DIR/CMakeLists.txt" | head -n1)"
VERSION="${FOOYIN_APPIMAGE_VERSION:-$VERSION${FOOYIN_VERSION_SUFFIX:-}}"
ARCH="$APPIMAGE_ARCH"
OUTPATH="$OUTPUT_DIR"
OUTNAME="fooyin-$VERSION-$ARCH.AppImage"
DESKTOP=/usr/share/applications/org.fooyin.fooyin.desktop
ICON=/usr/share/icons/hicolor/256x256/apps/org.fooyin.fooyin.png
UPDATE_REPOSITORY="${GITHUB_REPOSITORY:-fooyin/fooyin}"
UPDATE_CHANNEL="${FOOYIN_APPIMAGE_UPDATE_CHANNEL:-latest}"
UPINFO="gh-releases-zsync|${UPDATE_REPOSITORY%/*}|${UPDATE_REPOSITORY#*/}|$UPDATE_CHANNEL|fooyin-*-$ARCH.AppImage.zsync"

export APPDIR ARCH OUTPATH OUTNAME VERSION DESKTOP ICON UPINFO
export DEPLOY_PIPEWIRE=1
export QT_QPA_PLATFORM=offscreen

"$QUICK_SHARUN" /usr/bin/fooyin /usr/lib/fooyin/plugins
"$QUICK_SHARUN" --make-appimage
(
    cd "$BUILD_DIR"
    "$QUICK_SHARUN" --test "$OUTPUT_DIR/$OUTNAME"
)

echo "Built $OUTPUT_DIR/$OUTNAME"
