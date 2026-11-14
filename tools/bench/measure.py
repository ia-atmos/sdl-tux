#!/usr/bin/env python3
# Measures a command: wall time, CPU time, and peak memory.
#
#   tools/bench/measure.py LABEL COMMAND...
import os, resource, subprocess, sys, time

t0 = time.time()
subprocess.run(sys.argv[2:], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
dt = time.time() - t0
ru = resource.getrusage(resource.RUSAGE_CHILDREN)
print('%-28s wall %6.2fs   cpu %6.2fs   peak %6.1f MB' %
    (sys.argv[1], dt, ru.ru_utime + ru.ru_stime, ru.ru_maxrss / 1024))
