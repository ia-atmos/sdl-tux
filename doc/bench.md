# The port against the original

Measured on 2026-11-20 in the container (4 cores), compiler `ia-atmos/lang`
692af43 with `ATMOS_REAL=float`, `env-sdl` 85b23cc, the official game at
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

| Run | Original | Port |
|-----|----------|------|
| 1 tick, no drawing (the start up) | 0.74 s, 143 MB | 0.02 s, 13 MB |
| 2000 ticks, no drawing | 0.98 s, 143 MB | 0.15 s, 13 MB |
| the 2000 ticks alone (difference) | 0.25 s | 0.13 s |
| 1 tick, drawing | (see 2000) | 0.30 s, 71 MB |
| 2000 ticks, drawing (software renderer, dummy video driver) | 18.8 s, 143 MB | 18.2 s, 72 MB |

CPU is user + system time; memory is the maximum resident set. The port
without drawing is `tst/play.sh` (no SDL at all); with drawing it is the game
over `env-sdl` (`./tux.sh`). The original loads all of its data at start up
(sprites, tilesets, the Squirrel machine), which is most of its time and
memory; the drawing costs about the same in both, as both draw through the
SDL renderer in software.

## Floats

The original computes in 32-bit floats. With the build option
`ATMOS_REAL=float` (compiler 4565af1 and later), the reals of Atmos are C
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
