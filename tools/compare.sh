#!/bin/sh
# Compares the scripted runs of tst/runs.txt, in the port, with their
# traces in the official game (tst/official/), and shows the first lines
# that differ.
#
#   tools/compare.sh [NAME ...]         (default: all)
#
# With VIA=env, the runs go through the game over the SDL environment
# (tst/env-play.sh), and not through the runner with no environment.
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
mkdir -p out/compare
PLAY=tst/play.sh
[ "$VIA" = env ] && PLAY=tst/env-play.sh
$PLAY tst/levels/empty.stl 1 > /dev/null 2> out/compare/build.err || { head -5 out/compare/build.err; exit 1; }
bad=0
while read -r name level ticks input; do
    case "$name" in ''|\#*) continue ;; esac
    if [ $# -gt 0 ]; then
        case " $* " in *" $name "*) ;; *) continue ;; esac
    fi
    $PLAY "$level" "$ticks" "$input" > "out/compare/$name.txt" 2> "out/compare/$name.err"
    if cmp -s "out/compare/$name.txt" "tst/official/$name.txt"; then
        echo "same    $name ($(wc -l < "out/compare/$name.txt") lines)"
    else
        echo "differ  $name"
        diff "tst/official/$name.txt" "out/compare/$name.txt" | head -${LINES_SHOWN:-6}
        head -3 "out/compare/$name.err"
        bad=1
    fi
done < tst/runs.txt
exit $bad
