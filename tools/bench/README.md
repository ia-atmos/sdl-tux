# The official game, as the target and the benchmark

The port is compared, tick by tick, with SuperTux at the tag `v0.7.0`
(`c11dfb2`), built with the SDL renderer and run with no screen.

    tools/bench/build.sh                    # fetch, patch, build (30 min)
    tools/bench/official.sh levels/world1/journey_begins.stl 400 \
        "50:right:1,150:jump:1,170:jump:0"  # a run, with its trace

## Packages (Ubuntu 24.04)

    apt-get install cmake g++ libsdl2-dev libsdl2-image-dev \
        libsdl2-ttf-dev libphysfs-dev libglm-dev libboost-all-dev \
        libopenal-dev libvorbis-dev libogg-dev libcurl4-openssl-dev \
        libfmt-dev libglew-dev libraqm-dev libfreetype-dev libpng-dev \
        zlib1g-dev

The build has no OpenGL and no networking (`-DENABLE_OPENGL=OFF
-DENABLE_NETWORKING=OFF`).

## The test mode (`official.patch`)

The patch adds `src/supertux/tuxtest.{hpp,cpp}` and a few lines of hooks.
All of it is off unless a variable is set:

| Variable | Effect |
|----------|--------|
| `TUX_SEED=n` | the seed of the random numbers of the game (`0` is the time) |
| `TUX_GSEED=n` | the seed of the random numbers of the graphics (`graphicsRandom`, the time in the original) |
| `TUX_INPUT=...` | scripted input: `tick:control:value,...`, applied before the logical step `tick` (the first step is the tick 1) |
| `TUX_TICKS=n` | exits after `n` logical steps |
| `TUX_TRACE=file` | writes the trace (`-` is the standard output) |
| `TUX_TRACE_OBJS=1` | the trace also has every badguy, every tick |
| `TUX_TRACE_CAMERA=1` | the trace also has the translation of the camera, every tick |
| `TUX_FAST=1` | exactly one logical step per iteration of the loop, with no sleep |
| `TUX_NODRAW=1` | draws no frame |
| `TUX_SHOT=t:f.png,...` | saves the screen to `f.png` after the frame of the tick `t` is drawn (`t` lower than `TUX_TICKS`) |
| `TUX_NOINTRO=1` | a level starts with no intro screen (the one with its name and statistics, which a key dismisses) |

A logical step is 15 ms (`LOGICAL_FPS = 1000/15`). The trace is the
same with and without `TUX_FAST` and `TUX_NODRAW` (checked on the run
above).

## The trace

Numbers are 32-bit floats printed with `%.9g`, which is exact.

    157 E 16777253 snowball 0>2 1184 768
    157 P 97.5674973 640.224976 104.5 15.000001 b=0 d=R g=0 j=0 k=0 s=0 w=0 y=0 x=0 e=0 c=100
    157 B 16777238 jumpy 5248 672 0 0 s=0 d=L

- `P`, per tick and per player, after the step: position `x y`,
  velocity `vx vy`, and `b` bonus (0 none, 1 grown, 2 fire, 3 ice, 4
  air, 5 earth), `d` direction, `g` on the ground, `j` jumping, `k`
  ducking, `s` skidding, `w` swimming, `y` dying, `x` dead, `e` in the
  end sequence, `c` coins.
- `E`, when a badguy changes state, during the step: its id, its
  class, the states `from>to`, and its position. The states are those
  of `BadGuy::State`: 0 init, 1 inactive, 2 active, 3 squished, 4
  squished and fading, 5 falling, 6 burning, 7 melting, 8 ground
  melting, 9 inside melting, 10 gear.
- `C`, per tick, with `TUX_TRACE_CAMERA`: the translation of the camera,
  `x y` (the position in the sector of the top left of the screen).
- `B`, per tick and per badguy, with `TUX_TRACE_OBJS`: its id, its
  class, position, velocity, `s` state, `d` direction.

## Measures

In the container (4 cores, software renderer over the dummy video
driver, window of 1280x800), on 2026-11-14:

| Run | Wall | CPU | Peak memory |
|-----|------|-----|-------------|
| `journey_begins`, real time, drawing, 667 ticks (10 s) | 10.81 s | 3.52 s | 142.5 MB |
| `journey_begins`, no pacing, drawing, 2000 ticks | 7.98 s | 7.89 s | 142.4 MB |
| `journey_begins`, no pacing, no drawing, 2000 ticks | 0.78 s | 0.75 s | 142.4 MB |
| `journey_begins`, 1 tick (the start up) | 0.74 s | 0.73 s | 142.2 MB |
| `crystal_mine` (the largest level), no pacing, drawing, 2000 ticks | 7.99 s | 7.90 s | 223.7 MB |
| `crystal_mine`, 1 tick (the start up) | 1.01 s | 1.00 s | 223.6 MB |

- A frame costs about 3.6 ms to draw (276 frames per second with no
  pacing), and the logic of an idle level almost nothing.
- The binary has 10.4 MB (8.8 MB stripped).

They are taken with `tools/bench/measure.py`, as in:

    NOTRACE=1 DRAW=1 tools/bench/measure.py label \
        tools/bench/official.sh levels/world1/journey_begins.stl 2000
