#!/bin/sh
# Runs every scripted run of tst/runs.txt in the official game, and
# stores its trace in tst/official/NAME.txt.
#
#   tools/official-all.sh
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
mkdir -p tst/official
grep -v '^#' tst/runs.txt | while read -r name level ticks input; do
    [ -z "$name" ] && continue
    [ "$input" = "-" ] && input=
    tools/bench/official.sh "$level" "$ticks" "$input" > "tst/official/$name.txt"
    echo "$name: $(wc -l < "tst/official/$name.txt") lines"
done
