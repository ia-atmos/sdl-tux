#!/bin/sh
# Builds the game (once per change) and plays a level:
#
#   ./tux.sh LEVEL [ARGS]       LEVEL: /levels/world1/journey_begins.stl,
#                               or a level file of the tests
#
# It needs ../lang and ../env-sdl (tools/deps.sh), and the images of the
# official game (tools/bench/README.md).
ROOT=$(cd "$(dirname "$0")" && pwd)
ATMOS=${ATMOS:-$ROOT/../lang/atmos}
export ATMOS_REAL=float     # reals are 32-bit floats, as in the original
BIN=$ROOT/out/tux
cd "$ROOT"
if [ ! -x "$BIN" ] || [ -n "$(find game ../env-sdl/sdl.atm -newer "$BIN" 2>/dev/null)" ]; then
    mkdir -p out
    ATMOS_CFLAGS="${ATMOS_CFLAGS:--O2} -I$ROOT" \
    ATMOS_LDFLAGS="-lSDL2 -lSDL2_image -lSDL2_ttf -lSDL2_mixer -lm" \
        "$ATMOS" -o "$BIN" game/main.atm || exit 1
fi
exec "$BIN" "$@"
