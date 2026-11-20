#!/bin/sh
# Plays a level as tst/play.sh does, but through the game over the SDL
# environment (game/main.atm), with no screen: the scripted input becomes
# keys of the environment, and a frame is one step.
#
#   tst/env-play.sh LEVEL TICKS [INPUT]
ROOT=$(cd "$(dirname "$0")/.." && pwd)
KEYS=$(echo "${3:--}" | tr ',' '\n' | awk -F: '
    BEGIN { n["left"]="Left"; n["right"]="Right"; n["up"]="Up"; n["down"]="Down";
            n["jump"]="Space"; n["action"]="Left Ctrl"; n["item"]="Left Shift" }
    NF == 3 { printf "%s%s:%s:%s", (k++ ? "," : ""), $1, ($3 == "1" ? "dn" : "up"), n[$2] }')
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy SDL_ATM_MUTE=1 \
SDL_ATM_FRAMES="$2:15" SDL_ATM_KEYS="$KEYS" TUX_TRACE=1 TUX_SEED=1 TUX_GSEED=1 \
    exec "$ROOT/tux.sh" "$1"
