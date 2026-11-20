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
export ATMOS_REAL=float     # reals are 32-bit floats, as in the original
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

# a frame of the first level: identical to the one stored, and to the
# frame of the official game
if [ -d "$ROOT/../env-sdl" ] && [ -d "$OFFICIAL/data" ]; then
    mkdir -p out
    cc -O2 tools/pngdiff.c -lSDL2 -lSDL2_image -o out/pngdiff
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_ATM_MUTE=1 SDL_ATM_FRAMES=101:15 \
    SDL_ATM_KEYS="10:dn:Right" SDL_ATM_SHOT="100:$TMP/jb-100.png" \
        ./tux.sh /levels/world1/journey_begins.stl
    out/pngdiff tst/shots/port-jb-100.png "$TMP/jb-100.png" 0 > /dev/null || { echo "shot: differs from tst/shots/port-jb-100.png"; exit 1; }
    out/pngdiff tst/shots/official-jb-100.png "$TMP/jb-100.png" 0 > /dev/null || { echo "shot: jb-100 differs from the official one"; exit 1; }
    echo "shot: the first level ok (frame 100 the same as the official one)"
fi

# frames of a level with no blocks nor badguys, with decals: the same as
# the official ones (the second one but for a seam of one tilemap, see
# the TODO)
if [ -d "$ROOT/../env-sdl" ]; then
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_ATM_MUTE=1 SDL_ATM_FRAMES=201:15 \
    SDL_ATM_KEYS="10:dn:Right,120:up:Right" SDL_ATM_SHOT="50:$TMP/dec-50.png,200:$TMP/dec-200.png" \
        ./tux.sh tst/levels/decals.stl
    out/pngdiff tst/shots/official-dec-50.png "$TMP/dec-50.png" 0 > /dev/null || { echo "shot: dec-50 differs from the official one"; exit 1; }
    n=$(out/pngdiff tst/shots/official-dec-200.png "$TMP/dec-200.png" 0 | awk '{print $1}')
    [ "$n" -le 219 ] || { echo "shot: dec-200 differs from the official one in $n pixels"; exit 1; }
    echo "shot: decals ok (frame 50 the same as the official one, frame 200 in all but $n pixels)"
fi

# frames of a level with bonus blocks: blocks that bounce, turned, and
# empty, and coins that rise; the same as the official ones
if [ -d "$ROOT/../env-sdl" ]; then
    IN=$(grep '^bonus1 ' tst/runs.txt | awk '{print $4}')
    SDL_ATM_SHOT="165:$TMP/bonus-165.png,370:$TMP/bonus-370.png" \
        tst/env-play.sh tst/levels/bonus.stl 371 "$IN" > /dev/null
    for t in 165 370; do
        out/pngdiff tst/shots/official-bonus-$t.png "$TMP/bonus-$t.png" 0 > /dev/null || { echo "shot: bonus-$t differs from the official one"; exit 1; }
    done
    echo "shot: bonus blocks ok (frames 165 and 370 the same as the official ones)"
    IN=$(grep '^brick1 ' tst/runs.txt | awk '{print $4}')
    SDL_ATM_SHOT="165:$TMP/brick-165.png,300:$TMP/brick-300.png" \
        tst/env-play.sh tst/levels/brick.stl 301 "$IN" > /dev/null
    for t in 165 300; do
        out/pngdiff tst/shots/official-brick-$t.png "$TMP/brick-$t.png" 0 > /dev/null || { echo "shot: brick-$t differs from the official one"; exit 1; }
    done
    echo "shot: bricks ok (frames 165 and 300 the same as the official ones)"
    IN=$(grep '^grow1 ' tst/runs.txt | awk '{print $4}')
    SDL_ATM_SHOT="70:$TMP/grow-70.png,160:$TMP/grow-160.png,420:$TMP/grow-420.png" \
        tst/env-play.sh tst/levels/grow.stl 421 "$IN" > /dev/null
    for t in 70 160 420; do
        out/pngdiff tst/shots/official-grow-$t.png "$TMP/grow-$t.png" 0 > /dev/null || { echo "shot: grow-$t differs from the official one"; exit 1; }
    done
    echo "shot: powerups ok (the egg, the flower, the pieces of a brick: frames 70, 160 and 420 the same as the official ones)"
    # snowballs, one squished, one falling: all but a seam at the bottom of
    # a tilemap, when the camera is at a fraction of a pixel (see the TODO)
    IN=$(grep '^snow4 ' tst/runs.txt | awk '{print $4}')
    SDL_ATM_SHOT="599:$TMP/snow-599.png,2738:$TMP/snow-2738.png" \
        tst/env-play.sh tst/levels/snow.stl 2739 "$IN" > /dev/null
    for t in 599 2738; do
        n=$(out/pngdiff tst/shots/official-snow-$t.png "$TMP/snow-$t.png" 0 | awk '{print $1}')
        [ "$n" -le 207 ] || { echo "shot: snow-$t differs from the official one in $n pixels"; exit 1; }
    done
    echo "shot: snowballs ok (frames 599 and 2738 the same as the official ones but for 207 pixels of a seam)"
fi
