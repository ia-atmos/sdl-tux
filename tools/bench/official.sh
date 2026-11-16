#!/bin/sh
# Runs a level in the official game with no screen, with scripted input,
# and prints its trace.
#
#   tools/bench/official.sh LEVEL TICKS [INPUT]
#
#   LEVEL   relative to the data of the game: levels/world1/journey_begins.stl,
#           or a file: tst/levels/empty.stl
#   TICKS   logical steps to run (15 ms each)
#   INPUT   "tick:control:value,...": "50:right:1,150:jump:1,170:jump:0"
#           controls: left right up down jump action item escape ...
#
# Environment:
#   OFFICIAL    the checkout built by build.sh (default: ../official/supertux)
#   OBJS=1      the trace also has every badguy, every tick
#   CAMERA=1    the trace also has the translation of the camera, every tick
#   DRAW=1      draws the frames (to the dummy video driver)
#   REALTIME=1  keeps the pace of 15 ms per step
#   NOTRACE=1   no trace (to measure)
#   INTRO=1     keeps the intro screen of the level, which a key dismisses
#   SHOT=t:f.png,...  saves the screen after the frame of the tick t (drawn);
#               t must be lower than TICKS
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OFFICIAL=${OFFICIAL:-$ROOT/../official/supertux}
UD=$(mktemp -d)
trap 'rm -rf "$UD"' EXIT

export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
export TUX_SEED=1 TUX_TICKS="$2" TUX_INPUT="$3"
[ -z "$NOTRACE" ]  && export TUX_TRACE=-
[ -z "$INTRO" ]    && export TUX_NOINTRO=1
[ -n "$SHOT" ]     && export TUX_SHOT="$SHOT" DRAW=1
[ -n "$OBJS" ]     && export TUX_TRACE_OBJS=1
[ -n "$CAMERA" ]   && export TUX_TRACE_CAMERA=1
[ -z "$DRAW" ]     && export TUX_NODRAW=1
[ -z "$REALTIME" ] && export TUX_FAST=1

LEVEL="$OFFICIAL/data/$1"
[ -f "$1" ] && LEVEL=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")

"$OFFICIAL/build/supertux2" --renderer sdl --datadir "$OFFICIAL/data" \
    --userdir "$UD" --disable-sound --disable-music -g 1280x800 \
    "$LEVEL" 2>/dev/null | grep '^[0-9]* [PBEC] '
