#!/usr/bin/env python3
# Tests of the translation of Squirrel to data (tools/squirrel.py).
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'tools'))
from squirrel import translate, Unsupported

def ok (src, exp):
    got = translate(src)
    assert got == exp, (src, got)

def no (src, why):
    try:
        translate(src)
    except Unsupported as e:
        assert why in str(e), (src, str(e))
    else:
        assert False, (src, 'should not translate')

ok('', [])
ok('torch1.set_burning(true);', [['call', 'torch1', 'set_burning', True]])
ok('wait(2.2);\nlift2.start_moving()', [['wait', 2.2], ['call', 'lift2', 'start_moving']])
ok('secret1_cover.fade(0, 1);', [['call', 'secret1_cover', 'fade', 0, 1]])
ok('play_sound("sounds/splash.ogg");', [['do', 'play_sound', 'sounds/splash.ogg']])
ok('sector.Camera.ease_scale(1.1, 3.5, "EaseQuadOut");',
    [['call', 'sector.Camera', 'ease_scale', 1.1, 3.5, 'EaseQuadOut']])
ok('Camera.scroll_to(Camera.get_x(), 460, 5);',
    [['call', 'Camera', 'scroll_to', ['call', 'Camera', 'get_x'], 460, 5]])
ok('Text.set_anchor_offset(0, -160);', [['call', 'Text', 'set_anchor_offset', 0, -160]])
ok('Text.set_anchor_point(ANCHOR_BOTTOM);',
    [['call', 'Text', 'set_anchor_point', ['get', 'ANCHOR_BOTTOM']]])
ok('Text.set_text(_("Hello"));', [['call', 'Text', 'set_text', 'Hello']])
ok('settings.ambient_r <- 0.5;', [['set', 'settings.ambient_r', 0.5]])
ok('// a comment\n///Effect.x(1);\nwait(1); /* b */', [['wait', 1]])
ok('if (state.treasure == true){\n  a.b();\n}',
    [['if', ['op', '==', ['get', 'state.treasure'], True], [['call', 'a', 'b']], []]])
ok('if(! ("treasure" in state)){ state.treasure <- false; } else x.y();',
    [['if', ['op', '!', ['op', 'in', 'treasure', ['get', 'state']]],
        [['set', 'state.treasure', False]], [['call', 'x', 'y']]]])
ok('state.v <- ("v" in state ? (state.v | DIR_SOUTH) : DIR_SOUTH);',
    [['set', 'state.v', ['cond', ['op', 'in', 'v', ['get', 'state']],
        ['op', '|', ['get', 'state.v'], ['get', 'DIR_SOUTH']], ['get', 'DIR_SOUTH']]]])
ok('logo <- FloatingImage("images/logo.png");',
    [['set', 'logo', ['do', 'FloatingImage', 'images/logo.png']]])

no('function f(x){ a.b(); }', '`function`')
no('local x = 1;', '`local`')
no('while (true) { wait(1); }', '`while`')
no('a.b().c();', 'a call on the result of a call')
no('x;', 'an expression as a statement')
no('a.b(1', 'expected')

print('squirrel: ok')
