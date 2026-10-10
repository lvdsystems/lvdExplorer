#!/bin/bash
# Builds lvdExplorer in Release and packages it as a .deb, hand-rolled with
# dpkg-deb rather than CPack (consistent with the rest of packaging/, which
# doesn't use CPack for the Windows installer either). Unlike the AppImage,
# this is a "thin" package -- it depends on the build machine's own Qt6 and
# libarchive via apt, rather than bundling them, which is the normal shape
# for a .deb and keeps it small. That also means a .deb built here is only
# good for this distro release (or a close match); it does not have the
# AppImage's OUTPUT_SUFFIX/QT_PREFIX_PATH legacy-baseline option, since
# "works on an older glibc" isn't a thing a thin package can promise.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="$REPO_ROOT/build/linux-release"
OUTPUT_DIR="$REPO_ROOT/dist"

mkdir -p "$OUTPUT_DIR"

echo "==> Configuring (Release)..."
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=Release

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

ARCH="$(dpkg --print-architecture)"

echo "==> Generating app icon PNG from IconFactory (no static asset needed)..."
ICON_PNG="$BUILD_DIR/lvdExplorer.png"
g++ -std=c++2a -fPIC $(pkg-config --cflags Qt6Widgets) -I"$REPO_ROOT" \
    "$REPO_ROOT/resources/tools/dumpicon.cpp" "$REPO_ROOT/ui/resources/iconfactory.cpp" \
    -o "$BUILD_DIR/dumpicon" $(pkg-config --libs Qt6Widgets)
QT_QPA_PLATFORM=offscreen "$BUILD_DIR/dumpicon" "$ICON_PNG" 256

echo "==> Preparing package tree..."
# On a native Linux filesystem, not under $BUILD_DIR -- dpkg-deb requires
# DEBIAN/ at exactly 0755-0775, and a Windows drive mounted into WSL
# (DrvFs/9p) doesn't honor chmod at all (every path reads back as 777
# regardless), which dpkg-deb then rejects. Only the finished .deb needs
# to cross back onto the Windows-mounted dist/ directory, as a plain file
# copy that doesn't depend on exact permission bits surviving.
PKG_ROOT="$(mktemp -d)"
trap 'rm -rf "$PKG_ROOT"' EXIT
chmod 0755 "$PKG_ROOT" # mktemp -d defaults to 0700
mkdir -p "$PKG_ROOT/DEBIAN" "$PKG_ROOT/usr/bin" "$PKG_ROOT/usr/share/applications" \
    "$PKG_ROOT/usr/share/icons/hicolor/256x256/apps" "$PKG_ROOT/usr/share/doc/lvdexplorer"

cp "$BUILD_DIR/app/lvdExplorer" "$PKG_ROOT/usr/bin/"
cp "$ICON_PNG" "$PKG_ROOT/usr/share/icons/hicolor/256x256/apps/lvdExplorer.png"

cat > "$PKG_ROOT/usr/share/applications/lvdExplorer.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=lvdExplorer
Comment=Fast, lightweight multi-pane file manager
Exec=lvdExplorer
Icon=lvdExplorer
Categories=System;FileManager;Utility;
Terminal=false
EOF

cat > "$PKG_ROOT/usr/share/doc/lvdexplorer/copyright" << 'EOF'
Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: lvdExplorer
Source: https://github.com/lvdsystems/lvdExplorer

Files: *
Copyright: lvd systems
License: GPL-3+

License: GPL-3+
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 .
 On Debian systems, the complete text of the GNU General Public
 License version 3 can be found in /usr/share/common-licenses/GPL-3.
EOF

echo "==> Detecting runtime dependencies from the actual binary (ldd + dpkg -S)..."
# Real, version-portable dependency detection rather than hardcoding
# package names: Debian/Ubuntu have repeatedly renamed Qt's runtime
# packages across releases (the libqt6*6 -> libqt6*6t64 time64
# transition being the most recent), so whatever's actually linked and
# actually installed on *this* build machine is the only reliable source.
# realpath matters: ldd resolves through /lib -> /usr/lib (usrmerge) and
# through unversioned .so symlinks, but dpkg's own file list only has the
# fully-resolved, fully-versioned path (e.g. libQt6Core.so.6.8.2) --
# dpkg -S on the symlinked/unversioned form just silently finds nothing.
LIB_PATHS="$(ldd "$PKG_ROOT/usr/bin/lvdExplorer" | awk '{print $3}' | grep '^/' | xargs -r realpath | sort -u)"
DEPENDS="$(dpkg -S $LIB_PATHS 2>/dev/null | cut -d: -f1 | sort -u | paste -sd,)"
DEPENDS="${DEPENDS//,/, }"
if [ -z "$DEPENDS" ]; then
    echo "Could not detect any runtime dependencies via dpkg -S -- aborting rather than" >&2
    echo "shipping a .deb with an empty Depends: field." >&2
    exit 1
fi
echo "==> Depends: $DEPENDS"

cat > "$PKG_ROOT/DEBIAN/control" << EOF
Package: lvdexplorer
Version: $APP_VERSION
Section: utils
Priority: optional
Architecture: $ARCH
Depends: $DEPENDS
Maintainer: lvd systems <info@lvdsystems.eu>
Homepage: https://www.lvdopen.eu
Description: Fast, lightweight multi-pane file manager
 lvdExplorer is an open-source, Qt6-based multi-pane file manager for
 Windows and Linux, with integrated search, grep, and file hashing.
 Browses into ZIP/7z/RAR archives like regular folders, and supports
 batch rename, folder compare, and user-defined custom context-menu
 actions.
EOF

DEB_PATH="$OUTPUT_DIR/lvdexplorer_${APP_VERSION}_${ARCH}.deb"
rm -f "$DEB_PATH"
# dpkg-deb requires DEBIAN/ at exactly 0755-0775; building under a mount
# that doesn't preserve Unix permission bits (e.g. WSL's DrvFs on a
# Windows drive) can otherwise leave it at 777.
chmod 0755 "$PKG_ROOT/DEBIAN"
# --root-owner-group: ships the package's files owned by root:root (correct
# for anything installed under /usr) without actually needing root or
# fakeroot to build it.
dpkg-deb --build --root-owner-group "$PKG_ROOT" "$DEB_PATH"

echo "==> Done: $DEB_PATH"
