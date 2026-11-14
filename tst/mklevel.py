#!/usr/bin/env python3
# Makes a level for the tests from a level of the game: the same
# tilemaps, spawnpoints, and camera, and only the kinds of objects asked.
#
#   tst/mklevel.py SRC.stl DST.stl [KIND ...]
#
# With no kinds, the level is empty: the ground and Tux.
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from convert import sexp_read, Sym

KEEP = { 'name', 'tilemap', 'spawnpoint', 'camera' }

def write (x, ind=0):
    if isinstance(x, list):
        if all(not isinstance(v, list) for v in x):
            return '(' + ' '.join(write(v) for v in x) + ')'
        sp = '\n' + ' '*(ind+2)
        return '(' + write(x[0]) + ''.join(
            (sp if isinstance(v, list) else ' ') + write(v, ind+2)
                for v in x[1:]) + '\n' + ' '*ind + ')'
    if isinstance(x, Sym):
        return x
    if isinstance(x, bool):
        return '#t' if x else '#f'
    if isinstance(x, str):
        return '"' + x.replace('\\', '\\\\').replace('"', '\\"') + '"'
    return repr(x)

src, dst, kinds = sys.argv[1], sys.argv[2], set(sys.argv[3:])
lvl = sexp_read(open(src, encoding='utf-8').read())[0]
out = []
for x in lvl:
    if isinstance(x, list) and x[0] == 'sector':
        x = [ v for v in x if not isinstance(v, list)
                or v[0] in KEEP or v[0] in kinds ]
    if isinstance(x, list) and x[0] == 'name':
        x = [ Sym('name'), 'test' ]
    out.append(x)
open(dst, 'w', encoding='utf-8').write(write(out) + '\n')
