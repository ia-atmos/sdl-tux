#!/bin/sh
# Builds the runner with no screen once, and plays a level:
#
#   tst/play.sh LEVEL TICKS [INPUT]         (see tst/play.atm)
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ATMOS=${ATMOS:-$ROOT/../lang/atmos}
BIN=$ROOT/out/play
cd "$ROOT"
if [ ! -x "$BIN" ] || [ -n "$(find game tst/play.atm -newer "$BIN" 2>/dev/null)" ]; then
    mkdir -p out
    ATMOS_CFLAGS="${ATMOS_CFLAGS:--O2} -I$ROOT" ATMOS_LDFLAGS="-lm" \
        "$ATMOS" -o "$BIN" tst/play.atm || exit 1
fi
exec "$BIN" "$@"
