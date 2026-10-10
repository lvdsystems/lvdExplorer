#!/bin/bash
# Builds the Ubuntu-20.04-baseline AppImage (OUTPUT_SUFFIX=ubuntu2004)
# inside an actual Ubuntu 20.04 Docker container, via
# docker/ubuntu2004-build.sh -- see that script's doc comment for why
# this needs a real matching container rather than just downloading Qt
# 6.5.3 and compiling on whatever host is running this script.
#
# Needs only Docker; everything else (CMake, Qt, build deps) is installed
# inside the container. First run downloads a few hundred MB (the Ubuntu
# 20.04 base image, CMake, Qt 6.5.3) -- cached in $QT_CACHE_DIR
# (default ~/.cache/lvdexplorer-docker-qt) across runs.
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
QT_CACHE_DIR="${QT_CACHE_DIR:-$HOME/.cache/lvdexplorer-docker-qt}"
mkdir -p "$QT_CACHE_DIR"

docker run --rm \
    -v "$REPO_ROOT:/work" \
    -v "$QT_CACHE_DIR:/opt" \
    ubuntu:20.04 bash /work/packaging/linux/docker/ubuntu2004-build.sh
