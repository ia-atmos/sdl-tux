#!/bin/sh
# The tests of the port.
#
#   tst/run.sh
#
# Environment:
#   ATMOS      the compiler (default: ../lang/atmos)
#   OFFICIAL   the checkout of the official game (default:
#              ../official/supertux); the conversion is only checked
#              again if it is there
set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
ATMOS=${ATMOS:-$ROOT/../lang/atmos}
OFFICIAL=${OFFICIAL:-$ROOT/../official/supertux}
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
cd "$ROOT"

# the translation of the scripts
python3 tst/squirrel.py

# the conversion gives the data that is in the repository
if [ -d "$OFFICIAL/data" ]; then
    python3 tools/convert.py "$OFFICIAL/data" "$TMP/data" > "$TMP/convert.txt"
    diff -r "$TMP/data" data > /dev/null || { echo "convert: differs from data/"; exit 1; }
    echo "convert: ok ($(grep '^files' "$TMP/convert.txt" | awk '{print $2}') files)"
fi

# every data file loads
find data -name '*.atm' | sort > "$TMP/files.txt"
"$ATMOS" tools/check.atm "$TMP/files.txt"

# the collision kernel in C: it builds clean, and Tux falls and lands as
# in the official game (the stored lines are those of its trace)
cc -std=c99 -Wall -Wextra -O2 -fsanitize=address,undefined -Igame tst/coll.c -lm -o "$TMP/coll"
"$TMP/coll" > /dev/null
echo "coll: ok"
ATMOS_CFLAGS="-O2 -I$ROOT" ATMOS_LDFLAGS="-lm" \
    "$ATMOS" tst/fall.atm /levels/world1/journey_begins.stl 12 > "$TMP/fall.txt"
cmp "$TMP/fall.txt" tst/fall.txt
echo "fall: ok"
