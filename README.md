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
- [ ] T2: the game over `env-sdl`.

## Tests

    tst/run.sh

It needs the compiler in `../lang` (`ia-atmos/lang`), and, to check the
conversion again, the official game in `../official/supertux` (see
`tools/bench/README.md`).

[1]: https://github.com/SuperTux/supertux
[2]: https://github.com/ia-atmos/lang
[3]: https://github.com/ia-atmos/env-sdl

## License

GPL-3.0-or-later, as SuperTux, from whose source this port is made. The
art and the music of SuperTux have their own free licenses, listed in
its repository.
