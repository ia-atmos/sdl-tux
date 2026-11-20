#!/bin/sh
# Runs every scripted run of tst/runs.txt in the official game, and
# stores its trace in tst/official/NAME.txt. The game draws its frames
# (to the dummy video driver): in the original, the drawing of Tux ends
# his growing (`m_growing`, by the animation of his sprite), which the
# input reads; with no drawing, a big Tux could never duck.
#
#   tools/official-all.sh
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
mkdir -p tst/official
grep -v '^#' tst/runs.txt | while read -r name level ticks input; do
    [ -z "$name" ] && continue
    [ "$input" = "-" ] && input=
    DRAW=1 CAMERA=1 tools/bench/official.sh "$level" "$ticks" "$input" > "tst/official/$name.txt"
    echo "$name: $(wc -l < "tst/official/$name.txt") lines"
done
