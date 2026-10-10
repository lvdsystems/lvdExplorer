#!/bin/bash
# Runs *inside* an ubuntu:20.04 Docker container (see
# ../build-appimage-legacy-docker.sh, the thing that actually launches
# it) to build an older-glibc-baseline AppImage against that container's
# real system libraries. The output filename is tagged with the actual
# glibc version linked against (e.g. "glibc2.31"), not "ubuntu2004" --
# see the OUTPUT_SUFFIX line below for why.
#
# Why this has to happen inside an actual Ubuntu 20.04 environment rather
# than just pointing QT_PREFIX_PATH at a downloaded Qt 6.5.3 while
# compiling on whatever host happens to be running build-appimage.sh:
# linuxdeploy's Qt plugin bundles Qt's own libraries into the AppImage,
# but it does *not* bundle the baseline system libraries underneath them
# (libc, libstdc++, libsystemd, libicu, libglib, ...) -- those still get
# linked from whatever's actually installed on the machine doing the
# compiling. A first attempt at this did exactly that on a Debian 13
# build host and produced an AppImage that *claimed* Ubuntu 20.04
# compatibility but actually required GLIBC_2.38/GLIBC_2.39 (Debian 13's
# own glibc) -- it failed even on Ubuntu 22.04, two releases newer than
# the thing it was supposedly built for. Verified fixed by checking the
# resulting binary's highest required GLIBC/CXXABI symbol versions
# (`objdump -T ... | grep -oE 'GLIBC_[0-9.]+' | sort -V`) and by actually
# running the result on a real Ubuntu 22.04 machine.
set -ex

DEBIAN_FRONTEND=noninteractive apt-get update -qq
DEBIAN_FRONTEND=noninteractive apt-get install -y -qq build-essential ninja-build libarchive-dev \
    libgl1-mesa-dev libxkbcommon-dev libxkbcommon-x11-dev libdbus-1-dev libfontconfig1-dev libfreetype6-dev \
    curl ca-certificates pkg-config python3 python3-venv python3-dev file git \
    libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 \
    libxcb-render-util0 libxcb-shape0 libxcb-sync1 libxcb-xfixes0 libxcb-xinerama0 libxcb-xkb1
# ^ The last block is Qt's documented xcb platform plugin runtime set
# (https://doc.qt.io/qt-6/linux-requirements.html).

# Ubuntu 20.04's apt cmake (3.16) is below this project's 3.23 minimum --
# a prebuilt binary release sidesteps needing a PPA.
if [ ! -x /opt/cmake/bin/cmake ]; then
    curl -L -o /tmp/cmake.tar.gz https://github.com/Kitware/CMake/releases/download/v3.28.3/cmake-3.28.3-linux-x86_64.tar.gz
    mkdir -p /opt/cmake
    tar -xzf /tmp/cmake.tar.gz -C /opt/cmake --strip-components=1
fi
export PATH="/opt/cmake/bin:$PATH"

# Ubuntu 20.04 predates Qt6 in its own apt repos, hence aqtinstall for an
# official Qt 6.5.3 build (the oldest Qt6 LTS point release with
# reasonable Ubuntu-20.04-era toolchain support).
if [ ! -x /opt/qt/6.5.3/gcc_64/bin/qmake6 ]; then
    python3 -m venv /opt/aqt-venv
    /opt/aqt-venv/bin/pip install -q aqtinstall
    /opt/aqt-venv/bin/python -m aqt install-qt linux desktop 6.5.3 gcc_64 -O /opt/qt
fi

# The output is named after the actual glibc version linked against
# (e.g. "glibc2.31"), not the distro ("ubuntu2004") -- glibc compatibility
# is what actually determines where the AppImage runs, and naming it that
# way doesn't imply a false promise about any other distro/version. It's
# detected here rather than hardcoded so it can't drift stale if the base
# image in build-appimage-legacy-docker.sh ever changes.
GLIBC_VERSION="$(getconf GNU_LIBC_VERSION | awk '{print $2}')"
OUTPUT_SUFFIX="glibc${GLIBC_VERSION}"

cd /work
# Stale from a build at a different mount path (e.g. the WSL host's own
# /mnt/d/... vs this container's /work) -- CMakeCache.txt bakes in
# absolute paths, so it has to go rather than be reused across the two.
rm -rf "build/linux-release-${OUTPUT_SUFFIX}"
QT_PREFIX_PATH=/opt/qt/6.5.3/gcc_64 OUTPUT_SUFFIX="$OUTPUT_SUFFIX" bash packaging/linux/build-appimage.sh
