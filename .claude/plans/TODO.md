# TODO

Everything that is pending in the session of `sdl-tux`: asked and not
decided, known and not done, or reported and not acknowledged. Keep it
current: remove an item when it is settled, add one whenever something
is left open.

The plan is `261113-sdl-tux.md`, in this directory.

Last update: 2026-11-20.

## Index of the items

Every item of this session put to the author has a number, `#N`: unique,
in order, never reused nor changed (an item that closes or moves keeps
it). From another session it is `sdl-tux#N`. Rule of the author of
2026-11-18 (`ia-atmos/lang`, `TODO.md`, "Standing rules"). The ids
`A1`, `B3`, `C7` used before stay valid for what was written with them;
below, each old id is mapped to its number.

States: **open** (waits for the author), **work** (known, not started or
in course, no decision needed), **done**, **closed** (settled with no
change).

| # | Item | Was | State |
|---|------|-----|-------|
| 1 | The form of the 32-bit reals: the author chose on 2026-11-19 a build option; done in `ia-atmos/lang` `4565af1` (`ATMOS_REAL=float`), on in every build of the game; the suite is identical with it | C7 | done 2026-11-19 |
| 2 | The init script of `between_glaciers.stl` defines a function (`release_treasure`), which two triggers of the level call: keep it as `:squirrel` until the second version | A1 | decided 2026-11-20: keep it so until the second version, then #2a (write that init script by hand in Atmos, `release_treasure` as a global function of the scripts) |
| 3 | The trace of the official game: coins, blocks, powerups and the end of a level enter it with T5. The badguys are in every trace now (`OBJS=1`), with the uid of the original (`UIDGenerator`: the n-th life has the generator 2n-1) | B1 | work |
| 4 | Where the images, sounds and music come from (today: the checkout of the official game) | B2 | work |
| 5 | T3, a level drawn: the snow, the HUD, the fade at the start of a level and at the death of Tux, the moving paths | B3 | work |
| 6 | 219 pixels differ at the edge of the tilemap `secret`, a seam at a fraction of a pixel; the same seam, 207 pixels, at the bottom of a tilemap when the camera is at a fraction of a pixel (`tst/levels/snow.stl`) | B4 | work |
| 7 | `env-sdl` lacks flips, rotation and tint of an image, and music that loops between two points | B5 | work (ask when needed) |
| 8 | T4: runs with the bonus: grow, duck, fire and the break of bricks are done (`grow1..4`); hurt, slopes and ice remain | B6 | work |
| 9 | T5: the other contents of the bonus block (star, 1up, rain, trampoline, ...), the drop of a powerup on a buttjump (`drop_growup_bonus`), the fireballs, the secret area, the HUD data | B7 | work |
| 10 | `default.nut` of World 1 becomes Atmos code when a level of the slice calls it | B8 | work |
| 11 | The order in which scripts that wake in the same step are woken | B9 | work |
| 12 | T6 in course: the base of the badguys (`game/badguy.atm`: states, activation, squish, fall, ledges by raycasts) and, by class, the snowball, the smartball, the spiky, the sleeping spiky, the jumpy, the stalactite, the bouncing and the flying snowball, the ice block and the smart block (flat, kicked, grabbed and thrown by the player, breaking blocks); 29 runs identical with the lines `B` and `E` of every badguy (`snow1..5`, `spiky1..3`, `jumpy1..3`, `stal1..3`, `fly1..3`, `ice1..12`), and their frames. A block that bounces kills the badguys on it. Not done in them: freezing, burning. Next: mrbomb (the explosion), fish; then T7 (the first levels: weak blocks, info blocks, the firefly, the secret area) to T9 | B10 | work |
| 13 | The decisions taken as recommended after "go ahead without stopping": tag, license, slice, scripts as data, SDL renderer | C1 | done, not acknowledged |
| 14 | Fidelity first, then expressiveness; the list of places where exactness forces the form | C2 | done, not acknowledged |
| 15 | The scripts as data are a small language (calls, waits, set, if, expressions): 193 of 196 scripts | C3 | done, not acknowledged |
| 16 | Two scripts of `deep_dive_chill.stl` have a syntax error in the original, and do nothing in both | C4 | done, not acknowledged |
| 17 | The tiles of a tilemap keep the runs of the original | C5 | done, not acknowledged |
| 18 | The order of the milestones: T4 before T2 and T3 | C6 | done, not acknowledged |
| 19 | The two migrations of `pin`, tests identical | C8 | done, not acknowledged |
| 20 | `atmos-pin` handles what concerns `pin` | C9 | done, not acknowledged |
| 21 | In the original, the drawing changes the game: Tux stops growing when the animation of his sprite ends, which advances only when he is drawn, and until then he cannot duck. The traces of the official game are now recorded drawing (`DRAW=1` in `tools/official-all.sh`; the 36 traces before are unchanged), and `graphicsRandom`, seeded by the time in the original, gets a fixed seed in the tests (`TUX_GSEED`, added to the test mode). The runner with no screen (`tst/play.sh`) ends the growing at once: it can differ in the steps of a growth; the runs through `env-sdl` draw, as the original | | done, not acknowledged |

The details of each item follow, by its old id.

## A. Decisions waiting for an answer

Asked one at a time. Each has the option recommended by this session.

| # | Decision | Recommended |
|---|----------|-------------|
| A1 | One script of World 1 is more than calls, waits, assignments, and conditions: the init script of `between_glaciers.stl` defines a function (`release_treasure`, one line, which sets `state.treasure`). The converter keeps it as `:squirrel ["..."]` and reports it | Leave it so until the second version (the level is not among the first five). Then either write that init script by hand as Atmos code, as the long scenes, or teach the converter to inline a function of one line |

See C for the decisions taken as recommended.

## B. Work known and not done

| # | Work | Note |
|---|------|------|
| B1 | The trace of the official game has only the player and the badguys. Coins, blocks, powerups, and the end of a level enter it when the port reaches them (T5) | |
| B2 | The images, sounds, and music are not in the repository (270 MB in the original). For now the game reads them from the checkout of the official game (`../official/supertux/data`). To decide: a script that copies what World 1 uses | |
| B3 | T3, in course: a level drawn over `env-sdl`, through its door in C (`game/draw.h`, `sprite.h`, `tuxdraw.h`, `view.atm`): gradients, backgrounds with parallax, tilemaps (with animated tiles, and on a stopped path), the camera (`game/camera.h`, the same as the official on the 30 runs), and Tux with his sprite. The objects are drawn through their looks (`game/tux.h`, `TgLook`), in the order of the sector, with the filter of textures of the original (`SDL_HINT_RENDER_SCALE_QUALITY` 2): coins, bonus blocks (turned when they bounce, empty), coins that rise. A frame of `journey_begins` is identical to the official one, pixel by pixel; two frames of a level with blocks too (`tst/levels/bonus.stl`). The first frames of a level differ: the fade at its start is not done. The decals are done: a level with no blocks nor badguys gives the official frames (`tst/levels/decals.stl`). Not done: the moving paths, the particles (snow), the HUD, the fade at the start of a level | In course |
| B4 | In the frame 200 of `tst/levels/decals.stl`, 219 pixels differ from the official one: a column and a row at the edges of the tiles of one tilemap (`secret`, z-pos 110), where the edge falls at a fraction of a pixel (x = 20.64). The other tilemaps, at the same translation, are the same. To find out: how the official renderer rounds there | |
| B5 | T2 is done: the game over `env-sdl` (`2723b34`, fetched by `tools/deps.sh`), with the window, the frames turned into steps, and the keys of the original; the 30 runs give the official trace through it too. T3, in course: a level drawn. `env-sdl` still lacks the flips, rotation and tint of an image, and the music that loops between two points: ask with a small program when they are needed | In course |
| B6 | T4 is done for the small Tux in an empty level: eleven scripted runs (up to 3000 ticks of random input, with death by falling and the restart) give the trace of the official game. To do in T4: runs with the bonus (grow, duck, fire, hurt), which need the powerups; slopes and ice from other levels | |
| B7 | T5, in course. Done: the objects of a sector as tasks in a pool, with their collisions as the event `:hit`; the tiles that become objects; the coin; the interpreter of the scripts of a level (`game/script.atm`), with `wait` timed as the original (32-bit game time, woken at the start of the step) and the player as the scripts see it; the script trigger (touch, button, oneshot), with the update phase of a step as the event `:update`; the end of a level (the sequence trigger, `endsequence` and `stoptux`, the game at half speed, Tux walking out, the end of the session); the restart after a death at the start of the next step, as the original; the bonus block with coins (hits by type and count, the box of 32x32.1 or of its sprite, the drop of a buttjump, the bounce, the script), the coin that rises; the brick (5 coins, or bounces for the small Tux, or breaks for the big one, in six pieces); the egg and the flowers, rising out of a block (`SpecialRiser`), with the objects that move by a physic and collide with the solids (`:solid`); the shake of the camera (a buttjump); `graphicsRandom` as the original (game/random.h: mt19937 and the distributions of libstdc++), with a fixed seed in the tests (`TUX_GSEED`, added to the test mode of the official game); 40 runs identical. Next: the other contents of the bonus block (they print a warning and give nothing), the secret area, the HUD data (time, coins, the statistics of a level) | In course |
| B8 | `default.nut` of World 1 (the keys of the treasure: `add_key`, `remove_key`, `treasure_init`) runs before every init script of the world in the original. It does not change the game of Tux; it becomes Atmos code when a level of the slice calls it | |
| B9 | When several scripts wake in the same step, the original wakes them in the order of their wake-up times (a heap); the port wakes them in the order of their tasks. The same when their times differ only within a step. To check when a level has two scripts waiting at once | |
| B10 | T3 to T9 | See the plan |

## C. Reported or decided, and not acknowledged

| # | What | When |
|---|------|------|
| C1 | On 2026-11-14 the author answered "go ahead without stopping, except on doubts" to the message that listed the tag, the packages, and the license. Taken as: the tag `v0.7.0`; the packages installed with `apt-get`; the repository under GPL-3-or-later. The three other items of the plan marked "(confirm)" (the slice, the scripts as data, the SDL renderer only) were not asked one by one, and go as recommended in the plan. He may revert any of them | 2026-11-14 |
| C2 | Rule relayed by the session of the compiler: after full fidelity, a port may relax exactness in favor of expressiveness, if the intent of the original author is kept. Until then a difference is a defect. So: build for fidelity first, and list below the places where exactness forces an ugly form | 2026-11-14 |
| C3 | The scripts as data are a small language, more than the calls and waits of the plan: `:call`, `:do`, `:wait`, `:set`, `:if`, and the expressions `:get`, `:op`, `:cond` (see `tools/squirrel.py`). It covers 193 of the 196 scripts of World 1, the worldmap included | 2026-11-14 |
| C4 | Two scripts of `deep_dive_chill.stl` have a syntax error in the original (a missing comma in `ease_scale(1.1, 3 "EaseQuadOut")`), so the official game never runs them. The port keeps them as `:squirrel`, which does nothing, as there | 2026-11-14 |
| C5 | The tiles of a tilemap stay with the runs of the original (`-5 0` is five times the tile `0`), decoded when the level loads. The largest level loads with `X.load` in 18 ms | 2026-11-14 |
| C6 | The order of the milestones changed: T4 (collisions and the player, with no screen) before T2 and T3, which need `env-sdl` | 2026-11-14 |
| C7 | 32-bit reals: the author chose on 2026-11-16 that the compiler gains them (a build option where a real is a C `float`, or an explicit type; the form is open). Sent to the session of the compiler with what Tux needs and a test. Until it exists, no badguys nor blocks are written; the player and the collisions stay in C until then | 2026-11-16 |
| C8 | The second migration of `pin` (rules of 2026-11-17, `ia-atmos/lang` `5da4b8e`) is done: two values given away by their block are `val`, and the pins that the first migration forced are back to `val` or to `pin var`. Tests identical. Reported to the session of the compiler | 2026-11-17 |
| C9 | The session of the compiler is now `atmos-lang` (formerly `ceu-atmos`, same ID). Since 2026-11-18, what is found about `pin` goes to the session `atmos-pin` (session_011eECogSDhkVfyt7qme5DLh), with a small program; anything else of the compiler still to `atmos-lang` | 2026-11-18 |

### Places where exactness forces the form (candidates to relax later)

- `game/coll.h`: the collision system and `Physic` in C, with callbacks,
  as in the C++. With 32-bit reals in the language, `Physic` and the
  handlers could be Atmos.
- `game/player.h`: the whole `Player` in C (3654 lines), with hooks for
  what reaches outside. Its timers and phases (dying, growing, safe,
  skidding, the grace times of the jump) are still flags and timers in the
  struct: they are the first candidates to become tasks.

## Overrides of the base of the badguys (for atmos-oo)

In the original, `BadGuy` is a C++ base with virtual methods; `WalkingBadguy`
derives from it, and each badguy class derives from one of them. In the port
(`game/badguy.atm`) one task `BadGuy` holds the base, and each method that a
class overrides is a local function that tests `cls.kind` (the class is a
table of `BADGUYS`) and then does the override or goes on to the base:

- `initialize`: walking (`WalkingBadguy`), sleeping spiky (`SSpiky`), bouncing
  and flying snowball; the base does nothing (jumpy, stalactite).
- `active_update`: walking, sleeping spiky (calls `WalkingBadguy::active_update`
  or `BadGuy::active_update`), jumpy, bouncing, flying, ice block (calls both),
  stalactite (no base call).
- `collision_solid`: walking, sleeping spiky (`BadGuy::collision_solid` while
  not walking), jumpy, bouncing, flying, ice block (falls to the walking one in
  its normal state), stalactite.
- `collision_badguy`: walking, sleeping spiky, jumpy, bouncing, flying (the
  base), ice block, stalactite.
- `collision_squished`: snowball, smartball, bouncing and flying snowball
  (`cls.squish`), ice block; the base answers false (spiky, jumpy, stalactite).
- `collision_player`: ice block (kicked from a side, else the base),
  stalactite; the base for the others.
- `kill_fall`: stalactite (does nothing).
- `deactivate`: stalactite (a fallen one is gone).
- `turn_around`: walking, bouncing (no check of the state).
- `activate`: flying snowball.
