#!/bin/sh
# Fetches the official game at the tag v0.7.0, applies the test mode
# (official.patch), and builds it with the SDL renderer only.
#
#   tools/bench/build.sh [DIR]      (default: ../official/supertux)
#
# The packages it needs are listed in README.md. A full build takes
# about 30 minutes with 4 cores.
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
DIR=${1:-$ROOT/../official/supertux}

if [ ! -d "$DIR" ]; then
    git clone --depth 1 --branch v0.7.0 --recurse-submodules \
        --shallow-submodules https://github.com/SuperTux/supertux.git "$DIR"
fi
cd "$DIR"
if [ ! -f src/supertux/tuxtest.cpp ]; then
    git apply "$ROOT/tools/bench/official.patch"
fi
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_OPENGL=OFF -DENABLE_NETWORKING=OFF
make -j"$(nproc)"
