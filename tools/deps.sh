#!/bin/sh
# Fetches what the game is built with, next to this repository, at the
# commits that the game was tested with:
#
#   ../lang      the compiler (ia-atmos/lang), built with make
#   ../env-sdl   the SDL environment (ia-atmos/env-sdl)
#
#   tools/deps.sh
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
LANG_COMMIT=692af43
ENV_COMMIT=85b23cc

fetch () {   # dir repository commit
    if [ ! -d "$ROOT/../$1" ]; then
        git clone -q "https://github.com/ia-atmos/$2.git" "$ROOT/../$1"
    fi
    (cd "$ROOT/../$1" && git fetch -q origin && git checkout -q "$3")
}

fetch lang lang $LANG_COMMIT
fetch env-sdl env-sdl $ENV_COMMIT
make -C "$ROOT/../lang" > /dev/null
echo "lang $LANG_COMMIT, env-sdl $ENV_COMMIT"
