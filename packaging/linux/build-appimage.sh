#!/bin/bash
# Builds lvdExplorer in Release and packages it as an AppImage using
# linuxdeploy + linuxdeploy-plugin-qt (downloaded on first run and cached
# under packaging/linux/tools/ -- not vendored into the repo since they're
# large platform-specific binaries with their own release cadence).
#
# Two optional env vars support building a second, differently-glibc-
# targeted AppImage from an older base distro (e.g. Ubuntu 20.04, for
# broader compatibility than whatever's newest) without disturbing the
# default build:
#   QT_PREFIX_PATH  - a non-system Qt6 install (e.g. an aqtinstall
#                      prefix) to build against instead of find_package's
#                      default resolution. Needed on a base old enough
#                      that it predates Qt6 in its own package repos.
#   OUTPUT_SUFFIX    - appended to the build dir and output filename
#                      (e.g. "ubuntu2004") so the two builds' artifacts
#                      and intermediate CMake caches never collide.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build/linux-release${OUTPUT_SUFFIX:+-$OUTPUT_SUFFIX}"
APPDIR="$BUILD_DIR/AppDir"
TOOLS_DIR="$SCRIPT_DIR/tools"
OUTPUT_DIR="$REPO_ROOT/dist"

mkdir -p "$TOOLS_DIR" "$OUTPUT_DIR"

CMAKE_EXTRA_ARGS=()
if [ -n "$QT_PREFIX_PATH" ]; then
    echo "==> Using Qt at $QT_PREFIX_PATH"
    CMAKE_EXTRA_ARGS+=("-DCMAKE_PREFIX_PATH=$QT_PREFIX_PATH")
    export PATH="$QT_PREFIX_PATH/bin:$PATH"
    export PKG_CONFIG_PATH="$QT_PREFIX_PATH/lib/pkgconfig:$PKG_CONFIG_PATH"
    export LD_LIBRARY_PATH="$QT_PREFIX_PATH/lib:$LD_LIBRARY_PATH"
fi

echo "==> Configuring (Release)..."
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release "${CMAKE_EXTRA_ARGS[@]}"

echo "==> Building..."
cmake --build "$BUILD_DIR" --target lvdExplorer -j"$(nproc)"

VERSION_HEADER="$BUILD_DIR/app/generated/Version.h"
if [ ! -f "$VERSION_HEADER" ]; then
    echo "Generated Version.h not found at $VERSION_HEADER" >&2
    exit 1
fi
APP_VERSION="$(sed -n 's/.*LVDEXPLORER_VERSION_STRING "\([^"]*\)".*/\1/p' "$VERSION_HEADER")"
if [ -z "$APP_VERSION" ]; then
    echo "Could not parse LVDEXPLORER_VERSION_STRING out of $VERSION_HEADER" >&2
    exit 1
fi
echo "==> Version: $APP_VERSION"

echo "==> Fetching linuxdeploy tools (cached under $TOOLS_DIR)..."
LINUXDEPLOY="$TOOLS_DIR/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="$TOOLS_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"

if [ ! -x "$LINUXDEPLOY" ]; then
    curl -L -o "$LINUXDEPLOY" \
        https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
    chmod +x "$LINUXDEPLOY"
fi
if [ ! -x "$LINUXDEPLOY_QT" ]; then
    curl -L -o "$LINUXDEPLOY_QT" \
        https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
    chmod +x "$LINUXDEPLOY_QT"
fi
# linuxdeploy's --plugin qt looks for an executable literally named
# linuxdeploy-plugin-qt on PATH, not the versioned download filename.
ln -sf "$LINUXDEPLOY_QT" "$TOOLS_DIR/linuxdeploy-plugin-qt"
export PATH="$TOOLS_DIR:$PATH"
# WSL (and many containers) lack /dev/fuse, which these AppImages need to
# mount themselves; extract-and-run sidesteps that unconditionally.
export APPIMAGE_EXTRACT_AND_RUN=1
LINUXDEPLOY_RUN=("$LINUXDEPLOY" --appimage-extract-and-run)

echo "==> Generating app icon PNG from IconFactory (no static asset needed)..."
ICON_PNG="$BUILD_DIR/lvdExplorer.png"
# -std=c++2a rather than -std=c++20: the latter flag name didn't exist
# until GCC 10, so it fails outright on an older toolchain (e.g. GCC 9.4
# on Ubuntu 20.04) building a legacy-base AppImage -- both names select
# the same standard mode on any GCC that understands -std=c++20.
g++ -std=c++2a -fPIC $(pkg-config --cflags Qt6Widgets) -I"$REPO_ROOT" \
    "$REPO_ROOT/resources/tools/dumpicon.cpp" "$REPO_ROOT/ui/resources/iconfactory.cpp" \
    -o "$BUILD_DIR/dumpicon" $(pkg-config --libs Qt6Widgets)
QT_QPA_PLATFORM=offscreen "$BUILD_DIR/dumpicon" "$ICON_PNG" 256

echo "==> Preparing AppDir..."
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" "$APPDIR/usr/share/icons/hicolor/256x256/apps"
cp "$BUILD_DIR/app/lvdExplorer" "$APPDIR/usr/bin/"
cp "$ICON_PNG" "$APPDIR/usr/share/icons/hicolor/256x256/apps/lvdExplorer.png"

cat > "$APPDIR/usr/share/applications/lvdExplorer.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=lvdExplorer
Comment=Fast, lightweight multi-pane file manager
Exec=lvdExplorer
Icon=lvdExplorer
Categories=System;FileManager;Utility;
Terminal=false
EOF

echo "==> Running linuxdeploy..."
# linuxdeploy writes into its current directory using a name it derives
# itself (the .desktop Name= plus VERSION below) -- running it *inside*
# the shared dist/ directory means two builds that happen to produce the
# same intermediate filename (e.g. the default build and an
# OUTPUT_SUFFIX'd one both naming it "lvdExplorer-$APP_VERSION-
# x86_64.AppImage" once VERSION is honored) silently clobber each other
# there before the "normalize the name" step below ever runs. A
# build-specific scratch directory keeps that collision from being able
# to happen at all; only the final, already-uniquely-named file gets
# copied into dist/.
APPIMAGE_STAGING_DIR="$BUILD_DIR/appimage-out"
mkdir -p "$APPIMAGE_STAGING_DIR"
rm -f "$APPIMAGE_STAGING_DIR"/*.AppImage
cd "$APPIMAGE_STAGING_DIR"
# linuxdeploy's appimage output plugin reads VERSION to stamp the output
# filename as <Name>-<VERSION>-<arch>.AppImage.
export VERSION="$APP_VERSION"
"${LINUXDEPLOY_RUN[@]}" --appdir "$APPDIR" \
    --plugin qt \
    --output appimage \
    --desktop-file "$APPDIR/usr/share/applications/lvdExplorer.desktop" \
    --icon-file "$ICON_PNG"

# Normalize the output name regardless of whether this linuxdeploy build
# actually honored VERSION above, then move the one resulting AppImage
# into dist/ under its final, collision-proof name. OUTPUT_SUFFIX (e.g.
# "ubuntu2004") is what actually keeps a legacy-base build from
# overwriting the default one there.
VERSIONED_APPIMAGE="$OUTPUT_DIR/lvdExplorer-$APP_VERSION-x86_64${OUTPUT_SUFFIX:+-$OUTPUT_SUFFIX}.AppImage"
PRODUCED_APPIMAGE="$(ls "$APPIMAGE_STAGING_DIR"/*.AppImage | head -n1)"
if [ -z "$PRODUCED_APPIMAGE" ]; then
    echo "linuxdeploy did not produce an AppImage in $APPIMAGE_STAGING_DIR" >&2
    exit 1
fi
mv -f "$PRODUCED_APPIMAGE" "$VERSIONED_APPIMAGE"

echo "==> Done. AppImage(s) in $OUTPUT_DIR:"
ls -la "$OUTPUT_DIR"/*.AppImage
