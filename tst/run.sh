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

# the logic of the player in C builds clean
cc -std=c99 -Wall -Wextra -O2 -fsanitize=address,undefined -Igame tst/player.c -lm -o "$TMP/player"
"$TMP/player" > /dev/null 2>&1
echo "player: ok"

# the scripted runs give the traces of the official game
tools/compare.sh

# the same runs, through the game over the SDL environment
if [ -d "$ROOT/../env-sdl" ]; then
    VIA=env tools/compare.sh | grep -c '^same' | sed 's/^/env: same /'
fi

# a frame of the first level: identical to the one stored, and, as
# information, how much it differs from the frame of the official game
# (what the port does not draw yet: Tux, the blocks, the badguys)
if [ -d "$ROOT/../env-sdl" ] && [ -d "$OFFICIAL/data" ]; then
    mkdir -p out
    cc -O2 tools/pngdiff.c -lSDL2 -lSDL2_image -o out/pngdiff
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_ATM_MUTE=1 SDL_ATM_FRAMES=101:15 \
    SDL_ATM_KEYS="10:dn:Right" SDL_ATM_SHOT="100:$TMP/jb-100.png" \
        ./tux.sh /levels/world1/journey_begins.stl
    out/pngdiff tst/shots/port-jb-100.png "$TMP/jb-100.png" 0 > /dev/null || { echo "shot: differs from tst/shots/port-jb-100.png"; exit 1; }
    echo "shot: ok, differs from the official game in $(out/pngdiff tst/shots/official-jb-100.png "$TMP/jb-100.png" 0 | awk '{print $3}') of the pixels"
fi
