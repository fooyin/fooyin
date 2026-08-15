#!/usr/bin/env bash

set -euo pipefail

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly SOURCE_DIR="$(cd -- "$SCRIPT_DIR/.." && pwd)"

BUILD_DIR="${FOOYIN_APPIMAGE_BUILD_DIR:-$SOURCE_DIR/build-appimage}"
APPDIR="${FOOYIN_APPIMAGE_APPDIR:-$BUILD_DIR/AppDir}"
OUTPUT_DIR="${FOOYIN_APPIMAGE_OUTPUT_DIR:-$BUILD_DIR/output}"
TOOLS_DIR="${FOOYIN_APPIMAGE_TOOLS_DIR:-$BUILD_DIR/tools}"
QMAKE_BIN="${QMAKE:-qmake6}"

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

LINUXDEPLOY="${FOOYIN_LINUXDEPLOY:-$TOOLS_DIR/linuxdeploy-$APPIMAGE_ARCH.AppImage}"
LINUXDEPLOY_URL="https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-$APPIMAGE_ARCH.AppImage"
APPIMAGETOOL="${FOOYIN_APPIMAGETOOL:-$TOOLS_DIR/appimagetool-$APPIMAGE_ARCH.AppImage}"
APPIMAGETOOL_URL="https://github.com/AppImage/appimagetool/releases/download/1.9.1/appimagetool-$APPIMAGE_ARCH.AppImage"
APPIMAGE_RUNTIME="${FOOYIN_APPIMAGE_RUNTIME:-$TOOLS_DIR/runtime-$APPIMAGE_ARCH}"
APPIMAGE_RUNTIME_URL="https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-$APPIMAGE_ARCH"

mkdir -p "$BUILD_DIR" "$OUTPUT_DIR" "$TOOLS_DIR"

if [[ ! -x "$LINUXDEPLOY" ]]; then
    echo "Downloading linuxdeploy"
    curl --fail --location --retry 3 --output "$LINUXDEPLOY" "$LINUXDEPLOY_URL"
    chmod +x "$LINUXDEPLOY"
fi

if [[ ! -x "$APPIMAGETOOL" ]]; then
    echo "Downloading appimagetool"
    curl --fail --location --retry 3 --output "$APPIMAGETOOL" "$APPIMAGETOOL_URL"
    chmod +x "$APPIMAGETOOL"
fi

if [[ ! -f "$APPIMAGE_RUNTIME" ]]; then
    echo "Downloading AppImage runtime"
    curl --fail --location --retry 3 --output "$APPIMAGE_RUNTIME" "$APPIMAGE_RUNTIME_URL"
fi

cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DBUILD_TESTING=OFF \
    -DFETCH_PROJECTM=ON \
    -DENABLE_SYSTEM_GLM=ON \
    -DBUILD_CCACHE="${BUILD_CCACHE:-ON}" \
    -DBUILD_PCH="${BUILD_PCH:-ON}"
cmake --build "$BUILD_DIR" --parallel

cmake -E remove_directory "$APPDIR"
DESTDIR="$APPDIR" cmake --install "$BUILD_DIR" --component fooyin

readonly QT_PLUGIN_DIR="$($QMAKE_BIN -query QT_INSTALL_PLUGINS)"
readonly QT_TRANSLATION_DIR="$($QMAKE_BIN -query QT_INSTALL_TRANSLATIONS)"

copy_qt_plugin()
{
    local relative_path="$1"
    local requirement="${2:-optional}"
    local source_path="$QT_PLUGIN_DIR/$relative_path"

    if [[ ! -f "$source_path" ]]; then
        if [[ "$requirement" == "required" ]]; then
            echo "Required Qt plugin not found: $source_path" >&2
            exit 1
        fi
        return
    fi

    install -Dm755 "$source_path" "$APPDIR/usr/plugins/$relative_path"
}

copy_qt_plugin platforms/libqxcb.so required
copy_qt_plugin sqldrivers/libqsqlite.so required
copy_qt_plugin platforms/libqoffscreen.so
copy_qt_plugin platforms/libqwayland.so
copy_qt_plugin platforms/libqwayland-egl.so
copy_qt_plugin platforms/libqwayland-generic.so
copy_qt_plugin iconengines/libqsvgicon.so
copy_qt_plugin platformthemes/libqxdgdesktopportal.so
copy_qt_plugin networkinformation/libqglib.so
copy_qt_plugin tls/libqcertonlybackend.so
copy_qt_plugin tls/libqopensslbackend.so
copy_qt_plugin xcbglintegrations/libqxcb-egl-integration.so
copy_qt_plugin xcbglintegrations/libqxcb-glx-integration.so

for plugin in \
    libqgif.so \
    libqicns.so \
    libqico.so \
    libqjpeg.so \
    libqmng.so \
    libqsvg.so \
    libqtga.so \
    libqtiff.so \
    libqwbmp.so \
    libqwebp.so; do
    copy_qt_plugin "imageformats/$plugin"
done

for plugin_dir in \
    wayland-decoration-client \
    wayland-graphics-integration-client \
    wayland-shell-integration; do
    if [[ -d "$QT_PLUGIN_DIR/$plugin_dir" ]]; then
        while IFS= read -r -d '' plugin; do
            copy_qt_plugin "${plugin#"$QT_PLUGIN_DIR/"}"
        done < <(find "$QT_PLUGIN_DIR/$plugin_dir" -maxdepth 1 -type f -name '*.so' -print0)
    fi
done

install -Dm644 "$SOURCE_DIR/dist/linux/appimage/qt.conf" "$APPDIR/usr/bin/qt.conf"

if [[ -d "$QT_TRANSLATION_DIR" ]]; then
    mkdir -p "$APPDIR/usr/translations"
    find "$QT_TRANSLATION_DIR" -maxdepth 1 -type f -name 'qt*.qm' \
        -exec install -m644 -t "$APPDIR/usr/translations" {} +
fi

readonly DESKTOP_FILE="$APPDIR/usr/share/applications/org.fooyin.fooyin.desktop"
readonly ICON_FILE="$APPDIR/usr/share/icons/hicolor/256x256/apps/org.fooyin.fooyin.png"
readonly APP_VERSION="${FOOYIN_APPIMAGE_VERSION:-$(sed -nE 's/^[[:space:]]*VERSION[[:space:]]+([^[:space:])]+).*/\1/p' "$SOURCE_DIR/CMakeLists.txt" | head -n1)}"
readonly OUTPUT_FILE="$OUTPUT_DIR/fooyin-$APP_VERSION-$APPIMAGE_ARCH.AppImage"

cmake -E rm -f "$OUTPUT_FILE"

linuxdeploy_args=(
    --appdir "$APPDIR"
    --executable "$APPDIR/usr/bin/fooyin"
    --desktop-file "$DESKTOP_FILE"
    --icon-file "$ICON_FILE"
)

while IFS= read -r -d '' plugin_dir; do
    linuxdeploy_args+=(--deploy-deps-only "$plugin_dir")
done < <(find "$APPDIR/usr/plugins" -mindepth 1 -maxdepth 1 -type d -print0)

APPIMAGE_EXTRACT_AND_RUN=1 \
ARCH="$APPIMAGE_ARCH" \
"$LINUXDEPLOY" "${linuxdeploy_args[@]}"

if [[ ! -e "$APPDIR/usr/lib/libQt6XcbQpa.so.6" ]]; then
    echo "Required runtime library was not deployed: libQt6XcbQpa.so.6" >&2
    exit 1
fi

# linuxdeploy gives nested plugins a RUNPATH that reaches fooyin's libraries, but not the deployed dependencies in usr/lib
readonly PLUGIN_DEPENDENCY_RPATH='$ORIGIN/../..:$ORIGIN/../../..'
while IFS= read -r -d '' plugin; do
    patchelf --add-rpath "$PLUGIN_DEPENDENCY_RPATH" "$plugin"
done < <(find "$APPDIR/usr/lib" -type f -path '*/fooyin/plugins/*.so' -print0)

APPIMAGE_EXTRACT_AND_RUN=1 \
ARCH="$APPIMAGE_ARCH" \
VERSION="$APP_VERSION" \
"$APPIMAGETOOL" \
    --no-appstream \
    --runtime-file "$APPIMAGE_RUNTIME" \
    "$APPDIR" \
    "$OUTPUT_FILE"

echo "Built $OUTPUT_FILE"
