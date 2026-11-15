#!/usr/bin/env python3
# Makes a level for the tests from a level of the game: the same
# tilemaps, spawnpoints, and camera, and only the kinds of objects asked.
#
#   tst/mklevel.py SRC.stl DST.stl [KIND ...] [+KIND:X:Y ...]
#
# `+coin:300:640` adds an object of the kind at the position.
#
# With no kinds, the level is empty: the ground and Tux. The tiles that
# the game turns into objects when it loads a level (coins, bonus blocks,
# bricks) are removed too, unless their kind is asked.
#
# The tileset is read from ../official/supertux/data/images/tiles.strf
# (or $OFFICIAL).
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

src, dst = sys.argv[1], sys.argv[2]
kinds = set(a for a in sys.argv[3:] if a[0] != '+')
extra = [ a[1:].split(':') for a in sys.argv[3:] if a[0] == '+' ]

# the tiles that become objects: id -> kind
OFFICIAL = os.environ.get('OFFICIAL',
    os.path.join(os.path.dirname(__file__), '..', '..', 'official', 'supertux'))
objtiles = {}
for t in sexp_read(open(os.path.join(OFFICIAL, 'data/images/tiles.strf'),
                        encoding='utf-8').read())[0]:
    if isinstance(t, list) and t[0] == 'tile':
        f = { v[0]: v[1] for v in t[1:] if isinstance(v, list) and len(v) == 2 }
        if 'object-name' in f:
            objtiles[f['id']] = f['object-name']

def tiles (v):
    out, i = [], 1
    while i < len(v):
        n, id = 1, v[i]
        if id < 0:
            n, id = -id, v[i+1]
            i += 1
        i += 1
        if id in objtiles and objtiles[id] not in kinds:
            id = 0
        out += [id] * n
    return [v[0]] + out

def tilemap (tm):
    return [ (tiles(v) if isinstance(v, list) and v[0] == 'tiles' else v)
                for v in tm ]
lvl = sexp_read(open(src, encoding='utf-8').read())[0]
out = []
for x in lvl:
    if isinstance(x, list) and x[0] == 'sector':
        x = [ v for v in x if not isinstance(v, list)
                or v[0] in KEEP or v[0] in kinds ]
        x = [ (tilemap(v) if isinstance(v, list) and v[0] == 'tilemap' else v)
                for v in x ]
        x += [ [Sym(k), [Sym('x'), int(px)], [Sym('y'), int(py)]]
                for k,px,py in extra ]
    if isinstance(x, list) and x[0] == 'name':
        x = [ Sym('name'), 'test' ]
    out.append(x)
open(dst, 'w', encoding='utf-8').write(write(out) + '\n')
