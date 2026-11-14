#!/usr/bin/env python3

# One-time converter of the data of SuperTux, from s-expressions to
# Atmos data, which the game reads with `X.load`.
#
#   tools/convert.py <data of SuperTux/supertux@v0.7.0> <data of the port>
#
# It follows the converter of the port of Pingus (ia-atmos/sdl-pingus,
# tools/convert.py), whose reader of s-expressions and writer of Atmos
# data it reuses.
#
# Reads the levels and the worldmap of World 1, and every sprite,
# tileset, surface, and music description of the game, and writes
# `<file>.atm` in the same relative path of the destination.
# Then it checks the result:
#
#   - the number of files per kind is the expected one;
#   - every file converts back to the original s-expression (round
#     trip), up to what is listed in "Losses" below;
#   - no two fields of a table collide after the renaming.
#
# That every converted file loads with `X.load` is checked apart, by
# `tools/check.atm`, since it needs the compiler.
#
# Mapping:
#
#   (supertux-level ...)            :supertux.level [...]
#   (name v)                        name = v            (`-` becomes `_`)
#   (color 1 1 1)                   color = [1, 1, 1]   (see VECTORS)
#   (images "a.png")                images = ["a.png"]  (see VECTORS)
#   (name (_ "text"))               name = "text"
#   (statistics (k v) ...)          statistics = [k=v, ...]
#   (sector (name "main")           :sector [name="main",
#       (coin (x 1) (y 2)) ...)         :coin [x=1, y=2], ...]
#   (particles-snow)                :particles_snow []
#   #t  #f                          true  false
#
# A script in Squirrel becomes data that the game interprets (see
# tools/squirrel.py):
#
#   (script "torch1.set_burning(true);  script = [
#            wait(2.2);")                   [:call, "torch1", :set_burning, true],
#                                           [:wait, 2.2],
#                                       ]
#
# A script that the translation does not cover is kept as it is, and
# reported with its place and the reason:
#
#   (script "function f() {...}")       script = :squirrel ["function f() {...}"]
#
# The round trip is checked on the conversion with no translation.
#
# The tiles of a tilemap stay as in the original, with runs: `-5 0` is
# five times the tile `0`.
#
# A table holds its fields by name, and, by position and in the order
# of the file, the objects it is made of: the sectors of a level, the
# objects of a sector, the nodes of a path, the actions of a sprite,
# the tiles of a tileset (see OBJECTS). The order of the objects of a
# sector is the order in which the game creates them.
#
# Losses, which the round trip sets aside:
#
#   - spacing and comments;
#   - `_` and `-` in names are the same (`top_color`, `z-pos`);
#   - `(_ "text")`, which marks a text to translate, is the text;
#   - the lexeme of a number (`.5` is `0.5`).

import os
import re
import sys

import squirrel
from squirrel import Tag

# The files to convert: (directory, extension, recursive, count).
SOURCES = [
    ('levels/world1', '.stl',     False,  31),
    ('levels/world1', '.stwm',    False,   1),
    ('images',        '.sprite',  True,  393),
    ('images',        '.strf',    False,   3),
    ('images',        '.surface', True,    8),
    ('music',         '.music',   True,   72),
]

# Fields that are always vectors, whatever the number of their values.
VECTORS = {
    'color', 'top_color', 'bottom_color', 'lock-color', 'tint', 'tiles',
    'images', 'hitbox', 'animate', 'flip', 'rotate-center', 'translate',
    'ids', 'attributes', 'atrributes', 'datas', 'region', 'changing-tiles',
    'authors',
}

# Fields that are a vector only in some kinds of file: (extension, name).
VECTORS_IN = {
    ('.sprite', 'scale'), ('.strf', 'image'),
}

# Fields with a script in Squirrel, in levels and worldmaps.
SCRIPTS = { 'script', 'init-script' }

# Parents whose sublists are objects, by position, and not fields.
# A `None` means all sublists, except the listed fields.
OBJECTS = {
    'supertux-level':  { 'sector' },
    'sector':          None,
    'path':            { 'node' },
    'supertux-sprite': { 'action' },
    'supertux-tiles':  { 'tile', 'tiles', 'tilegroup' },
    'images':          { 'surface' },
}
SECTOR_FIELDS = { 'name', 'gravity', 'init-script', 'init-script-run-once' }

ID  = re.compile(r'^[A-Za-z_][A-Za-z0-9_]*$')
NUM = re.compile(r'^[-+]?(\d+\.?\d*|\.\d+)([eE][-+]?\d+)?$')

class Sym (str):
    pass

###############################################################################
# S-EXPRESSIONS
###############################################################################

# A file as a list of expressions. An expression is a list, a `Sym`, a
# `str`, a number, or a boolean.
def sexp_read (src):
    stack = [[]]
    i, n = 0, len(src)
    while i < n:
        c = src[i]
        if c in ' \t\r\n':
            i += 1
        elif c == ';':
            while i < n and src[i] != '\n':
                i += 1
        elif c == '(':
            stack.append([])
            i += 1
        elif c == ')':
            x = stack.pop()
            stack[-1].append(x)
            i += 1
        elif c == '"':
            out = []
            i += 1
            while src[i] != '"':
                if src[i] == '\\':
                    i += 1
                    out.append({'n':'\n', 't':'\t'}.get(src[i], src[i]))
                else:
                    out.append(src[i])
                i += 1
            i += 1
            stack[-1].append(''.join(out))
        else:
            j = i
            while j < n and src[j] not in ' \t\r\n()";':
                j += 1
            tk = src[i:j]
            i = j
            if tk == '#t':
                stack[-1].append(True)
            elif tk == '#f':
                stack[-1].append(False)
            elif NUM.match(tk):
                isint = re.match(r'^[-+]?\d+$', tk)
                stack[-1].append(int(tk) if isint else float(tk))
            else:
                stack[-1].append(Sym(tk))
    assert len(stack) == 1, 'unbalanced parentheses'
    return stack[0]

###############################################################################
# ATMOS DATA: a table is `Tab`, with `tag`, positional `vec`, and named
# `map` (a list of pairs, to keep the order).
###############################################################################

TRANSLATE = [True]      # False in the conversion for the round trip
REPORT = []             # (path, reason) of the scripts not translated

class Tab:
    def __init__ (self, tag=None):
        self.tag = tag
        self.vec = []
        self.map = []

def name_to_atm (s):
    return s.replace('-', '_')

def tag_to_atm (s, top):
    if top and s.startswith('supertux-'):
        s = 'supertux.' + s[len('supertux-'):]
    return s.replace('-', '_')

def is_scalar (v):
    return not isinstance(v, list)

def is_text (v):
    return isinstance(v, list) and len(v) == 2 and v[0] == '_' \
        and isinstance(v[0], Sym) and isinstance(v[1], str)

def scalar_to_atm (v):
    return str(v) if isinstance(v, Sym) else v

def is_vector (ext, name):
    return (name in VECTORS) or ((ext, name) in VECTORS_IN)

# `(head ...)` as an object: a tagged table.
def obj_to_atm (x, top, ext, path):
    assert x and isinstance(x[0], Sym), (path, 'object with no head')
    t = Tab(tag_to_atm(x[0], top))
    body_to_atm(t, x[0], x[1:], ext, path+'/'+x[0])
    return t

# The contents of a list into `t`: scalars are positional, sublists are
# fields or objects.
def body_to_atm (t, name, rest, ext, path):
    if all(is_scalar(v) for v in rest):
        t.vec = [ scalar_to_atm(v) for v in rest ]
        return
    assert all(not is_scalar(v) for v in rest), (path, 'mixed contents')
    objs = OBJECTS.get(name, set())
    seen = set()
    for v in rest:
        assert v and isinstance(v[0], Sym), (path, 'field with no name')
        if (v[0] in objs) if (objs is not None) else (v[0] not in SECTOR_FIELDS):
            t.vec.append(obj_to_atm(v, False, ext, path))
        else:
            k = name_to_atm(v[0])
            assert k not in seen, (path, 'field twice', k)
            assert k != 'tag', (path, 'field `tag`')
            seen.add(k)
            t.map.append((k, field_to_atm(v[0], v[1:], ext, path+'/'+v[0])))

def script_to_atm (src, path):
    def tab (v):
        if isinstance(v, list):
            t = Tab()
            t.vec = [ tab(x) for x in v ]
            return t
        return v
    try:
        return tab(squirrel.translate(src))
    except squirrel.Unsupported as e:
        REPORT.append((path, str(e)))
        t = Tab('squirrel')
        t.vec = [src]
        return t

def field_to_atm (name, rest, ext, path):
    if len(rest) == 1 and is_text(rest[0]):
        return rest[0][1]
    if TRANSLATE[0] and (name in SCRIPTS) and ext in ('.stl', '.stwm') \
            and len(rest) == 1 and isinstance(rest[0], str):
        return script_to_atm(rest[0], path)
    if all(is_scalar(v) for v in rest) and not is_vector(ext, name):
        assert len(rest) <= 1, (path, 'not in VECTORS', len(rest))
        if len(rest) == 1:
            return scalar_to_atm(rest[0])
    t = Tab()
    body_to_atm(t, name, rest, ext, path)
    return t

def file_to_atm (exprs, ext, path):
    assert len(exprs) == 1 and exprs[0][0].startswith('supertux-'), path
    return obj_to_atm(exprs[0], True, ext, path)

def atm_str (s):
    out = []
    for c in s:
        if   c == '\n': out.append('\\n')
        elif c == '\t': out.append('\\t')
        elif c == '\r': out.append('\\r')
        elif c == '"':  out.append('\\x22')
        elif c == '\\': out.append('\\\\')
        elif ord(c) < 32:
            out.append('\\x%02X' % ord(c))
        else:
            out.append(c)
    return '"' + ''.join(out) + '"'

def atm_num (v):
    if isinstance(v, int):
        return str(v)
    s = repr(v)
    if ('e' in s) or ('inf' in s) or ('nan' in s):
        s = '%.10f' % v
    return s

def atm_write (v, ind=0):
    if isinstance(v, Tab):
        pre = (':' + v.tag + ' ') if v.tag else ''
        items = [ k + ' = ' + atm_write(x,ind+4) for k,x in v.map ] + \
                [ atm_write(x,ind+4) for x in v.vec ]
        scalars = all(not isinstance(x,Tab) for x in v.vec) and not v.map
        if scalars and len(items) > 16:
            # a long vector of numbers, such as the tiles of a tilemap
            sp = '\n' + ' '*(ind+4)
            rows = [ ', '.join(items[i:i+16]) for i in range(0,len(items),16) ]
            return pre + '[' + ''.join(sp + r + ',' for r in rows) + \
                   '\n' + ' '*ind + ']'
        flat = pre + '[' + ', '.join(items) + ']'
        simple = all(not isinstance(x,Tab) or (not x.map and not x.tag and
            all(not isinstance(y,Tab) for y in x.vec))
                for x in v.vec + [x for _,x in v.map])
        if not items or (simple and len(flat)+ind <= 78 and '\n' not in flat):
            return flat
        sp = '\n' + ' '*(ind+4)
        return pre + '[' + ''.join(sp + i + ',' for i in items) + \
               '\n' + ' '*ind + ']'
    elif isinstance(v, Tag):
        return ':' + v
    elif isinstance(v, bool):
        return 'true' if v else 'false'
    elif isinstance(v, str):
        return atm_str(v)
    else:
        return atm_num(v)

# Reads back what `atm_write` writes (and no more than that).
def atm_read (src):
    pos = [0]
    TK = re.compile(r'''\s*(?:
        (?P<str>"(?:[^"\\]|\\.)*") |
        (?P<tag>:[A-Za-z0-9_.]+)   |
        (?P<num>-?\d[0-9.]*)       |
        (?P<id>[A-Za-z_][A-Za-z0-9_]*) |
        (?P<sym>[\[\],=])
    )''', re.X)
    def nxt (peek=False):
        m = TK.match(src, pos[0])
        assert m, ('invalid data', src[pos[0]:pos[0]+40])
        if not peek:
            pos[0] = m.end()
        return m.lastgroup, m.group(m.lastgroup)
    def unstr (s):
        def f (m):
            c = m.group(1)
            if c[0] == 'x':
                return chr(int(c[1:],16))
            return {'n':'\n', 't':'\t', 'r':'\r', '\\':'\\'}[c]
        return re.sub(r'\\(x[0-9A-Fa-f]{2}|.)', f, s[1:-1])
    def value ():
        k, s = nxt()
        if k == 'str':
            return unstr(s)
        elif k == 'num':
            return float(s) if '.' in s else int(s)
        elif k == 'id':
            return {'true':True, 'false':False}[s]
        elif k == 'tag':
            assert nxt() == ('sym','['), ('tag with no table', s)
            return table(s[1:])
        else:
            assert s == '[', ('invalid data', s)
            return table(None)
    def table (tag):
        t = Tab(tag)
        while True:
            k, s = nxt(True)
            if (k,s) == ('sym',']'):
                nxt()
                return t
            if k == 'id' and s not in ('true','false'):
                nxt()
                assert nxt() == ('sym','=')
                t.map.append((s, value()))
            else:
                t.vec.append(value())
            k, s = nxt()
            if (k,s) == ('sym',']'):
                return t
            assert (k,s) == ('sym',','), ('invalid data', s)
    v = value()
    assert src[pos[0]:].strip() == '', 'garbage after the value'
    return v

###############################################################################
# BACK TO S-EXPRESSIONS, and the comparison of the round trip.
###############################################################################

def name_to_sexp (s):
    return s.replace('_', '-')

def tag_to_sexp (s):
    if s.startswith('supertux.'):
        s = 'supertux-' + s[len('supertux.'):]
    return s.replace('_', '-')

def body_to_sexp (t):
    return [ field_to_sexp(k,v) for k,v in t.map ] + \
           [ obj_to_sexp(v) if isinstance(v,Tab) else v for v in t.vec ]

def obj_to_sexp (t):
    return [tag_to_sexp(t.tag)] + body_to_sexp(t)

def field_to_sexp (k, v):
    if isinstance(v, Tab):
        return [name_to_sexp(k)] + body_to_sexp(v)
    return [name_to_sexp(k), v]

# The original, with the accepted losses applied: the names with `-`,
# the texts with no mark, and the fields before the objects.
def normal (x):
    if isinstance(x, list):
        head = x[0].replace('_', '-')
        rest = [ (v[1] if is_text(v) else v) for v in x[1:] ]
        if rest and all(not is_scalar(v) for v in rest):
            objs = OBJECTS.get(x[0], set())
            isobj = lambda v: (v[0] in objs) if (objs is not None) \
                                else (v[0] not in SECTOR_FIELDS)
            rest = [ v for v in rest if not isobj(v) ] + \
                   [ v for v in rest if isobj(v) ]
        return [head] + [ normal(v) for v in rest ]
    elif isinstance(x, Sym):
        return str(x)
    return x

def same (a, b):
    if isinstance(a, list) or isinstance(b, list):
        return isinstance(a, list) and isinstance(b, list) and \
            len(a) == len(b) and all(same(x,y) for x,y in zip(a,b))
    if isinstance(a, bool) or isinstance(b, bool):
        return a is b
    if isinstance(a, (int,float)) and isinstance(b, (int,float)):
        return abs(a-b) <= 1e-9 * max(1, abs(a), abs(b))
    return type(a) == type(b) and a == b

###############################################################################
# MAIN
###############################################################################

def files_of (src, dir, ext, rec):
    out = []
    for d, ds, fs in os.walk(os.path.join(src, dir)):
        if not rec:
            ds[:] = []
        out += [ os.path.relpath(os.path.join(d,f), src)
                    for f in fs if f.endswith(ext) ]
    return sorted(out)

def main (src, dst):
    total = 0
    for dir, ext, rec, n in SOURCES:
        files = files_of(src, dir, ext, rec)
        assert len(files) == n, ('count', dir, ext, len(files), n)
        for rel in files:
            txt = open(os.path.join(src,rel), encoding='utf-8').read()
            exprs = sexp_read(txt)

            # round trip, from the text that is written, with the
            # scripts as they are
            TRANSLATE[0] = False
            out = atm_write(file_to_atm(exprs, ext, rel)) + '\n'
            a = obj_to_sexp(atm_read(out))
            b = normal(exprs[0])
            assert same(a, b), (rel, 'round trip')

            TRANSLATE[0] = True
            out = atm_write(file_to_atm(exprs, ext, rel)) + '\n'

            os.makedirs(os.path.dirname(os.path.join(dst,rel)), exist_ok=True)
            with open(os.path.join(dst,rel+'.atm'), 'w', encoding='utf-8') as fd:
                fd.write(out)
        print('%-14s %-9s %d' % (dir, ext, len(files)))
        total += len(files)
    print('files          ', total)
    print('scripts not translated:', len(REPORT))
    for path, why in REPORT:
        print('    %s : %s' % (path, why))

if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit('usage: convert.py <source data> <destination data>')
    main(sys.argv[1], sys.argv[2])
