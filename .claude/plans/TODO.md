# TODO

Everything that is pending in the session of `sdl-tux`: asked and not
decided, known and not done, or reported and not acknowledged. Keep it
current: remove an item when it is settled, add one whenever something
is left open.

The plan is `261113-sdl-tux.md`, in this directory.

Last update: 2026-11-16.

## A. Decisions waiting for an answer

Asked one at a time. Each has the option recommended by this session.

| # | Decision | Recommended |
|---|----------|-------------|
| 8 | The rules of `pin` changed again on 2026-11-17 (`ia-atmos/lang` `0ba7afc`, `98547d3`): `val`/`var` may hold a fresh value, and a `pin` may be reassigned. The project compiles and gives the same results with the newest compiler (`b463c23`), checked on 2026-11-17. Go back to the direct forms where `pin` was forced (about 10 places, as `set slow = [...]` in `tools/check.atm`)? | Not now: the forced places are few and work; the newer compiler is taken when 32-bit reals arrive, and the direct forms then, in the same move |
| 7 | One script of World 1 is more than calls, waits, assignments, and conditions: the init script of `between_glaciers.stl` defines a function (`release_treasure`, one line, which sets `state.treasure`). The converter keeps it as `:squirrel ["..."]` and reports it | Leave it so until the second version (the level is not among the first five). Then either write that init script by hand as Atmos code, as the long scenes, or teach the converter to inline a function of one line |

See C for the decisions taken as recommended.

## B. Work known and not done

| # | Work | Note |
|---|------|------|
| 3 | The trace of the official game has only the player and the badguys. Coins, blocks, powerups, and the end of a level enter it when the port reaches them (T5) | |
| 5 | The images, sounds, and music are not in the repository (270 MB in the original). For now the game reads them from the checkout of the official game (`../official/supertux/data`). To decide: a script that copies what World 1 uses | |
| 11 | T3, in course: a level drawn over `env-sdl`, through its door in C (`game/draw.h`, `sprite.h`, `tuxdraw.h`, `view.atm`): gradients, backgrounds with parallax, tilemaps (with animated tiles, and on a stopped path), the camera (`game/camera.h`, the same as the official on the 30 runs), and Tux with his sprite. A frame of `journey_begins` is identical to the official one, pixel by pixel, but for the three bonus blocks and the snowball (0.39% of the pixels), which wait for the 32-bit reals. The decals are done: a level with no blocks nor badguys gives the official frames (`tst/levels/decals.stl`). Not done: the moving paths, the particles (snow), the HUD, the fade at the start of a level | In course |
| 12 | In the frame 200 of `tst/levels/decals.stl`, 219 pixels differ from the official one: a column and a row at the edges of the tiles of one tilemap (`secret`, z-pos 110), where the edge falls at a fraction of a pixel (x = 20.64). The other tilemaps, at the same translation, are the same. To find out: how the official renderer rounds there | |
| 6 | T2 is done: the game over `env-sdl` (`2723b34`, fetched by `tools/deps.sh`), with the window, the frames turned into steps, and the keys of the original; the 30 runs give the official trace through it too. T3, in course: a level drawn. `env-sdl` still lacks the flips, rotation and tint of an image, and the music that loops between two points: ask with a small program when they are needed | In course |
| 7 | T4 is done for the small Tux in an empty level: eleven scripted runs (up to 3000 ticks of random input, with death by falling and the restart) give the trace of the official game. To do in T4: runs with the bonus (grow, duck, fire, hurt), which need the powerups; slopes and ice from other levels | |
| 8 | T5, in course. Done: the objects of a sector as tasks in a pool, with their collisions as the event `:hit`; the tiles that become objects; the coin; the interpreter of the scripts of a level (`game/script.atm`), with `wait` timed as the original (32-bit game time, woken at the start of the step) and the player as the scripts see it; the script trigger (touch, button, oneshot), with the update phase of a step as the event `:update`; the end of a level (the sequence trigger, `endsequence` and `stoptux`, the game at half speed, Tux walking out, the end of the session); the restart after a death at the start of the next step, as the original; 30 runs identical. Waits for 32-bit reals: the bonus block, the brick, the powerups. Next without them: the secret area, the HUD data (time, coins, the statistics of a level) | In course |
| 9 | `default.nut` of World 1 (the keys of the treasure: `add_key`, `remove_key`, `treasure_init`) runs before every init script of the world in the original. It does not change the game of Tux; it becomes Atmos code when a level of the slice calls it | |
| 10 | When several scripts wake in the same step, the original wakes them in the order of their wake-up times (a heap); the port wakes them in the order of their tasks. The same when their times differ only within a step. To check when a level has two scripts waiting at once | |
| 4 | T3 to T9 | See the plan |

## C. Reported or decided, and not acknowledged

| # | What | When |
|---|------|------|
| 1 | On 2026-11-14 the author answered "go ahead without stopping, except on doubts" to the message that listed the tag, the packages, and the license. Taken as: the tag `v0.7.0`; the packages installed with `apt-get`; the repository under GPL-3-or-later. The three other items of the plan marked "(confirm)" (the slice, the scripts as data, the SDL renderer only) were not asked one by one, and go as recommended in the plan. He may revert any of them | 2026-11-14 |
| 2 | Rule relayed by the session of the compiler: after full fidelity, a port may relax exactness in favor of expressiveness, if the intent of the original author is kept. Until then a difference is a defect. So: build for fidelity first, and list below the places where exactness forces an ugly form | 2026-11-14 |

| 3 | The scripts as data are a small language, more than the calls and waits of the plan: `:call`, `:do`, `:wait`, `:set`, `:if`, and the expressions `:get`, `:op`, `:cond` (see `tools/squirrel.py`). It covers 193 of the 196 scripts of World 1, the worldmap included | 2026-11-14 |
| 4 | Two scripts of `deep_dive_chill.stl` have a syntax error in the original (a missing comma in `ease_scale(1.1, 3 "EaseQuadOut")`), so the official game never runs them. The port keeps them as `:squirrel`, which does nothing, as there | 2026-11-14 |
| 5 | The tiles of a tilemap stay with the runs of the original (`-5 0` is five times the tile `0`), decoded when the level loads. The largest level loads with `X.load` in 18 ms | 2026-11-14 |

| 6 | The order of the milestones changed: T4 (collisions and the player, with no screen) before T2 and T3, which need `env-sdl` | 2026-11-14 |
| 7 | 32-bit reals: the author chose on 2026-11-16 that the compiler gains them (a build option where a real is a C `float`, or an explicit type; the form is open). Sent to the session of the compiler with what Tux needs and a test. Until it exists, no badguys nor blocks are written; the player and the collisions stay in C until then | 2026-11-16 |

### Places where exactness forces the form (candidates to relax later)

- `game/coll.h`: the collision system and `Physic` in C, with callbacks,
  as in the C++. With 32-bit reals in the language, `Physic` and the
  handlers could be Atmos.
- `game/player.h`: the whole `Player` in C (3654 lines), with hooks for
  what reaches outside. Its timers and phases (dying, growing, safe,
  skidding, the grace times of the jump) are still flags and timers in the
  struct: they are the first candidates to become tasks.
