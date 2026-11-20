#!/bin/sh
# Builds the runner with no screen once, and plays a level:
#
#   tst/play.sh LEVEL TICKS [INPUT]         (see tst/play.atm)
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ATMOS=${ATMOS:-$ROOT/../lang/atmos}
export ATMOS_REAL=float     # reals are 32-bit floats, as in the original
BIN=$ROOT/out/play
cd "$ROOT"
if [ ! -x "$BIN" ] || [ -n "$(find game tst/play.atm -newer "$BIN" 2>/dev/null)" ]; then
    mkdir -p out
    ATMOS_CFLAGS="${ATMOS_CFLAGS:--O2} -I$ROOT" ATMOS_LDFLAGS="-lm" \
        "$ATMOS" -o "$BIN" tst/play.atm || exit 1
fi
# the seeds of the random numbers, as tools/bench/official.sh gives them
export TUX_SEED=${TUX_SEED:-1} TUX_GSEED=${TUX_GSEED:-1}
exec "$BIN" "$@"
