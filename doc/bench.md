# The port against the original

Measured on 2026-11-20 in the container (4 cores), compiler `ia-atmos/lang`
8c46149 with `ATMOS_FLOAT=32`, `env-sdl` 85b23cc, the official game at
v0.7.0 built with the SDL renderer (tools/bench/README.md). Both run the first
level, `journey_begins`, with the same input (right held from tick 10), at
no pacing.

## Size

The port, in non-blank lines (2026-11-20):

| Part | Lines | What |
|------|-------|------|
| `game/*.atm` (Atmos) | 2694 | the session, the lives, the objects as tasks (coins, blocks, powerups, triggers, the end of a level), the badguys, the scripts of the levels, the view, the sprites as the logic sees them |
| `game/coll.h` (C) | 2377 | the collision system, translated statement by statement |
| `game/player.h` (C) | 3217 | the player, translated statement by statement, with hooks |
| other `game/*.h` (C) | 2052 | the camera, the glue with the sector (`tux.h`), the drawing (`draw.h`, `sprite.h`, `tuxdraw.h`), the random numbers |

The C++ of the original that this covers (the player, the collision system,
the camera, the base of the badguys and the ten classes done, the blocks,
coins, powerups, particles, triggers, the end sequence) is about 10700
lines, without the engine (video, sound, the Squirrel machine, the object
manager, the level loader), which `env-sdl`, the converted data and Atmos
itself replace.

## CPU and memory

The CPU time of the process (user + system), median of 3 runs, per frame:
the run minus its start up (the same run of 1 tick), divided by the
frames. The game logic alone is the run with no drawing at all (the
original with `TUX_NODRAW`, the port with `tst/play.sh`, which has no SDL),
over 20000 ticks, as the logic of a tick is too small to measure over fewer.
The whole frame is the run that draws (software renderer, dummy video
driver: the original with `DRAW=1`, the port over `env-sdl`), over 2000
ticks.

| Measure | Original | Port | Ratio |
|---------|----------|------|-------|
| game logic alone, per frame | 0.0145 ms | 0.0614 ms | 4.2 |
| whole frame, per frame | 8.62 ms | 8.11 ms | 0.94 |
| start up, no drawing | 0.82 s, 143 MB | 0.02 s, 13 MB | |
| start up, drawing | | 0.30 s, 71 MB | |

Memory is the maximum resident set. The logic of the port, Atmos with the
collision system and the player in C, takes about four times the CPU of
the original's C++; it is still under a tenth of a millisecond per frame,
and the drawing, through the SDL renderer in software in both, is almost
all of a frame. The original loads all of its data at start up (sprites,
tilesets, the Squirrel machine), which is most of its start up time and
memory.

## Floats

The original computes in 32-bit floats. With the build option
`ATMOS_FLOAT=32` (compiler 4565af1 and later), the reals of Atmos are C
floats, and the arithmetic of the objects written in Atmos (the coins, the
blocks and their bounce, the powerups, the badguys and their classes, the
timers in game time, the animation of the sprites as the logic sees them)
gives the trace of the original bit by bit. The last float formula kept in C
for want of `fmod` (the flight of the flying snowball) went back to Atmos:
`%` on reals is `fmodf`, and `math.pow` and `math.sin` are `powf` and `sinf`
under the option. What stays in C is what was translated statement by
statement before the option existed (the collision system, the player, the
camera), and the queries of the badguys to the collision system (raycasts,
free-of-tiles checks).
