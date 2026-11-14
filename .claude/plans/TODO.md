# TODO

Everything that is pending in the session of `sdl-tux`: asked and not
decided, known and not done, or reported and not acknowledged. Keep it
current: remove an item when it is settled, add one whenever something
is left open.

The plan is `261113-sdl-tux.md`, in this directory.

Last update: 2026-11-14.

## A. Decisions waiting for an answer

Asked one at a time. Each has the option recommended by this session.

| # | Decision | Recommended | State |
|---|----------|-------------|-------|
| 1 | The target: the tag `v0.7.0` (`c11dfb2`) or master | The tag: master needs SDL3, which Ubuntu 24.04 does not package; the tag builds with SDL2 and has `--renderer sdl` | Asked on 2026-11-13 |
| 2 | Permission to install the packages that the build of the official game needs (SDL2 and its libraries, the other dependencies of CMake) | Install them with `apt-get`, and list them in `tools/bench/README.md` | Not asked yet |
| 3 | The slice: World 1, in two steps (a first version with ten enemies and five levels, then the rest) | As in the plan | Not asked yet |
| 4 | The scripts: those embedded in the levels become data that a small task interprets; the long scenes (`.nut`) become Atmos code | As in the plan | Not asked yet |
| 5 | The renderer: SDL2 only, no OpenGL, no shaders, lights out of the first version | As in the plan | Not asked yet |
| 6 | The license: the repository was created with MIT, and SuperTux is GPL-3 (code) with art under several free licenses. A port made from its C++, which carries its data, is a derived work | GPL-3-or-later for the repository, with a note on the licenses of the art | Not asked yet |

## B. Work known and not done

| # | Work | Note |
|---|------|------|
| 1 | T0: the official game built at `v0.7.0` in the container, with the SDL renderer and the dummy video driver | Waits for A1 and A2 |
| 2 | T0: `tools/bench/official.patch`: scripted input by tick, a fixed seed, the trace | After B1 |
| 3 | T0: the measures of the official game (memory, CPU, frames) | After B1 |
| 4 | T1 to T9 | See the plan |

## C. Reported or decided, and not acknowledged

| # | What | When |
|---|------|------|
| 1 | Rule relayed by the session of the compiler: after full fidelity, a port may relax exactness in favor of expressiveness, if the intent of the original author is kept. Until then a difference is a defect. So: build for fidelity first, and list below the places where exactness forces an ugly form | 2026-11-14 |

### Places where exactness forces the form (candidates to relax later)

None yet.
