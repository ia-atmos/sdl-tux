#!/usr/bin/env python3

# Translates the Squirrel scripts embedded in the levels of SuperTux to
# data, which a task of the game interprets (see "Scripts" in the plan).
#
#   torch1.set_burning(true);       [:call, "torch1", :set_burning, true]
#   wait(2.2);                      [:wait, 2.2]
#   play_sound("sounds/x.ogg");     [:do, :play_sound, "sounds/x.ogg"]
#   state.treasure <- false;        [:set, "state.treasure", false]
#   if (c) { a } else { b }         [:if, c, [a], [b]]
#
# and, in expressions:
#
#   Camera.get_x()                  [:call, "Camera", :get_x]
#   state.treasure                  [:get, "state.treasure"]
#   ANCHOR_BOTTOM                   [:get, "ANCHOR_BOTTOM"]
#   a == b   !a   "k" in t          [:op, "==", a, b]  [:op, "!", a]  [:op, "in", "k", t]
#   c ? a : b                       [:cond, c, a, b]
#   _("text")                       "text"
#
# A script is a vector of statements. What is not in this list, such as
# the definition of a function, a loop, or a local variable, is not
# translated: `translate` raises `Unsupported` with the reason.
#
# The result uses `Tag` for the tags and python lists for the vectors.

import re

class Unsupported (Exception):
    pass

class Tag (str):
    pass

TOKEN = re.compile(r'''
    (?P<ws>\s+|//[^\n]*|/\*.*?\*/)            |
    (?P<num>\d+\.?\d*|\.\d+)                  |
    (?P<str>"(?:[^"\\]|\\.)*")                |
    (?P<id>[A-Za-z_][A-Za-z0-9_]*)            |
    (?P<op><-|==|!=|<=|>=|&&|\|\||::|[(){}\[\],;.!?:|&+\-*/<>=])
''', re.X | re.S)

def tokens (src):
    out, i = [], 0
    while i < len(src):
        m = TOKEN.match(src, i)
        if not m:
            raise Unsupported('unexpected character %r' % src[i])
        i = m.end()
        k = m.lastgroup
        if k != 'ws':
            out.append((k, m.group(k)))
    out.append(('eof', ''))
    return out

BINARY = [      # from the lowest precedence
    ['||'], ['&&'], ['|'], ['&'], ['==', '!='],
    ['<', '>', '<=', '>=', 'in'], ['+', '-'], ['*', '/'],
]

KEYWORDS = {
    'function', 'local', 'while', 'for', 'foreach', 'return', 'class',
    'switch', 'do', 'try', 'throw', 'const', 'enum', 'break', 'continue',
}

class Parser:
    def __init__ (self, src):
        self.tks = tokens(src)
        self.i = 0

    def peek (self):
        return self.tks[self.i]

    def next (self):
        tk = self.tks[self.i]
        self.i += 1
        return tk

    def accept (self, s):
        if self.peek()[1] == s and self.peek()[0] in ('op','id'):
            self.i += 1
            return True
        return False

    def expect (self, s):
        if not self.accept(s):
            raise Unsupported('expected `%s`, found `%s`' % (s, self.peek()[1]))

    def script (self):
        out = []
        while self.peek()[0] != 'eof':
            out += self.stmt()
        return out

    def block (self):
        if self.accept('{'):
            out = []
            while not self.accept('}'):
                if self.peek()[0] == 'eof':
                    raise Unsupported('missing `}`')
                out += self.stmt()
            return out
        return self.stmt()

    # A list with zero or one statement.
    def stmt (self):
        k, s = self.peek()
        if s == ';' and k == 'op':
            self.next()
            return []
        if k == 'id' and s in KEYWORDS:
            raise Unsupported('`%s`' % s)
        if k == 'id' and s == 'if':
            self.next()
            self.expect('(')
            c = self.expr()
            self.expect(')')
            t = self.block()
            f = self.block() if self.accept('else') else []
            return [[Tag('if'), c, t, f]]
        e = self.expr()
        if self.accept('<-') or self.accept('='):
            if not (isinstance(e, list) and e[0] == 'get'):
                raise Unsupported('assignment to an expression')
            v = self.expr()
            e = [Tag('set'), e[1], v]
        elif not (isinstance(e, list) and e[0] in ('call','do','wait')):
            raise Unsupported('an expression as a statement')
        self.accept(';')
        return [e]

    def expr (self):
        c = self.binary(0)
        if self.accept('?'):
            a = self.expr()
            self.expect(':')
            b = self.expr()
            return [Tag('cond'), c, a, b]
        return c

    def binary (self, lvl):
        if lvl == len(BINARY):
            return self.unary()
        a = self.binary(lvl+1)
        while self.peek()[1] in BINARY[lvl] and self.peek()[0] in ('op','id'):
            op = self.next()[1]
            b = self.binary(lvl+1)
            a = [Tag('op'), op, a, b]
        return a

    def unary (self):
        if self.accept('!'):
            return [Tag('op'), '!', self.unary()]
        if self.accept('-'):
            v = self.unary()
            if isinstance(v, (int,float)) and not isinstance(v, bool):
                return -v
            return [Tag('op'), 'neg', v]
        return self.postfix()

    def postfix (self):
        k, s = self.next()
        if k == 'num':
            return float(s) if ('.' in s) else int(s)
        if k == 'str':
            return unescape(s)
        if k == 'op' and s == '(':
            e = self.expr()
            self.expect(')')
            return e
        if k != 'id':
            raise Unsupported('unexpected `%s`' % s)
        if s in ('true', 'false'):
            return s == 'true'
        if s == 'null':
            raise Unsupported('`null`')
        if s in KEYWORDS:
            raise Unsupported('`%s`' % s)
        path = [s]
        while self.accept('.'):
            k, s = self.next()
            if k != 'id':
                raise Unsupported('unexpected `%s` after `.`' % s)
            path.append(s)
        if self.accept('('):
            args = []
            while not self.accept(')'):
                args.append(self.expr())
                if self.peek()[1] != ')':
                    self.expect(',')
            if self.peek()[1] in ('.', '(', '['):
                raise Unsupported('a call on the result of a call')
            if len(path) == 1:
                if path[0] == '_':
                    if len(args) != 1 or not isinstance(args[0], str):
                        raise Unsupported('`_` with no text')
                    return args[0]
                if path[0] == 'wait':
                    return [Tag('wait')] + args
                return [Tag('do'), Tag(path[0])] + args
            return [Tag('call'), '.'.join(path[:-1]), Tag(path[-1])] + args
        if self.peek()[1] == '[':
            raise Unsupported('an index')
        return [Tag('get'), '.'.join(path)]

def unescape (s):
    def f (m):
        c = m.group(1)
        return {'n':'\n', 't':'\t', 'r':'\r'}.get(c, c)
    return re.sub(r'\\(.)', f, s[1:-1])

def translate (src):
    return Parser(src).script()

if __name__ == '__main__':
    import sys
    print(translate(sys.stdin.read()))
