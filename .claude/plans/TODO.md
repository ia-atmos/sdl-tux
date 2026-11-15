# TODO

Everything that is pending in the session of `sdl-tux`: asked and not
decided, known and not done, or reported and not acknowledged. Keep it
current: remove an item when it is settled, add one whenever something
is left open.

The plan is `261113-sdl-tux.md`, in this directory.

Last update: 2026-11-14.

## A. Decisions waiting for an answer

Asked one at a time. Each has the option recommended by this session.

| # | Decision | Recommended |
|---|----------|-------------|
| 7 | One script of World 1 is more than calls, waits, assignments, and conditions: the init script of `between_glaciers.stl` defines a function (`release_treasure`, one line, which sets `state.treasure`). The converter keeps it as `:squirrel ["..."]` and reports it | Leave it so until the second version (the level is not among the first five). Then either write that init script by hand as Atmos code, as the long scenes, or teach the converter to inline a function of one line |

See C for the decisions taken as recommended.

## B. Work known and not done

| # | Work | Note |
|---|------|------|
| 3 | The trace of the official game has only the player and the badguys. Coins, blocks, powerups, and the end of a level enter it when the port reaches them (T5) | |
| 5 | The images, sounds, and music are not in the repository (270 MB in the original). Decide at T2 how the game gets them: a script that copies what World 1 uses from the checkout of the official game | |
| 6 | T2 and T3 wait for `env-sdl`, which has no code yet; the list of what the game needs was sent to its session on 2026-11-14. Meanwhile the order is T4 first: collisions, physics, and the player with no screen | |
| 7 | T4, in course. Done: the collision kernel and the logic of the player in C (`game/coll.h`, `game/player.h`, translations of the C++), a runner with no screen (`tst/play.sh`), and four scripted runs in an empty level (idle, walk, skid, jump) with the same trace as the official game. Next: more runs (duck and grow need the bonus; slopes; death by falling; the edges of the level), then the sector and the player as tasks in Atmos over these kernels: today the runner calls the kernels in a plain loop | In course |
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
| 7 | 32-bit reals: reported to the session of the compiler, which is putting it to the author. Until then the arithmetic of the player and of the badguys goes in C kernels | 2026-11-14 |

### Places where exactness forces the form (candidates to relax later)

- `game/coll.h`: the collision system and `Physic` in C, with callbacks,
  as in the C++. With 32-bit reals in the language, `Physic` and the
  handlers could be Atmos.
- `game/player.h`: the whole `Player` in C (3654 lines), with hooks for
  what reaches outside. Its timers and phases (dying, growing, safe,
  skidding, the grace times of the jump) are still flags and timers in the
  struct: they are the first candidates to become tasks.
