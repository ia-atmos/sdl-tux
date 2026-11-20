# sdl-tux

A port of a slice of [SuperTux][1] (the tag `v0.7.0`) to [Atmos][2], over
the shared SDL environment ([env-sdl][3]).

The plan is in `.claude/plans/261113-sdl-tux.md`, and what is pending in
`.claude/plans/TODO.md`.

## Status

- [x] T0: the official game built in the container, with scripted input
      and a trace (`tools/bench/`).
- [x] T1: the data of World 1 as Atmos data (`tools/convert.py`, `data/`):
      31 levels, the worldmap, 393 sprites, 3 tilesets. The scripts of the
      levels are data too (`tools/squirrel.py`): 193 of 196.
- [x] T2: the game over `env-sdl` (`game/main.atm`, `./tux.sh LEVEL`).
- [ ] T3, in course: a level drawn: a frame of the first level is the
      official one, pixel by pixel, with its blocks and badguys; not yet
      the snow, the HUD, the fades, the moving paths.
- [x] T4, for the small Tux: the player and the collisions with no
      screen (`game/`, `tst/play.sh`), death and restart included; the
      big and fire Tux in part.
- [ ] T5, in course: objects as tasks (coins, bonus blocks, bricks, the
      egg and the flowers, triggers), the interpreter of the scripts of a
      level (`game/script.atm`), and the end of a level.
- [ ] T6, in course: the badguys as tasks (`game/badguy.atm`): ten of the
      classes of World 1.

69 scripted runs, of up to 3000 ticks, give the trace of the official
game line by line (`tools/compare.sh`), the badguys included.

How it was written: the parts of the original that the trace depends on
most finely (the collision system, the player, the camera) were first
translated to C statement by statement; the rest (the session, the
objects, the badguys) is Atmos, with 32-bit reals (`ATMOS_REAL=float`), and
each piece was compared with the official game, tick by tick and pixel by
pixel, before the next. Its size, CPU and memory against the original are
in `doc/bench.md`.

## Tests

    tst/run.sh

`tools/deps.sh` fetches the compiler (`../lang`) and the SDL environment
(`../env-sdl`) at the commits that the game was tested with. To check the
conversion again, the official game in `../official/supertux` (see
`tools/bench/README.md`).

[1]: https://github.com/SuperTux/supertux
[2]: https://github.com/ia-atmos/lang
[3]: https://github.com/ia-atmos/env-sdl

## License

GPL-3.0-or-later, as SuperTux, from whose source this port is made. The
art and the music of SuperTux have their own free licenses, listed in
its repository.
