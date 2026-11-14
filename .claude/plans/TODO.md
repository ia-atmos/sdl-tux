# TODO

Everything that is pending in the session of `sdl-tux`: asked and not
decided, known and not done, or reported and not acknowledged. Keep it
current: remove an item when it is settled, add one whenever something
is left open.

The plan is `261113-sdl-tux.md`, in this directory.

Last update: 2026-11-14.

## A. Decisions waiting for an answer

Asked one at a time. Each has the option recommended by this session.

None open. See C for those taken as recommended.

## B. Work known and not done

| # | Work | Note |
|---|------|------|
| 1 | T0: the official game built at `v0.7.0` in the container, with the SDL renderer and the dummy video driver | In course |
| 2 | T0: `tools/bench/official.patch`: scripted input by tick, a fixed seed, the trace | After B1 |
| 3 | T0: the measures of the official game (memory, CPU, frames) | After B1 |
| 4 | T1 to T9 | See the plan |

## C. Reported or decided, and not acknowledged

| # | What | When |
|---|------|------|
| 1 | On 2026-11-14 the author answered "go ahead without stopping, except on doubts" to the message that listed the tag, the packages, and the license. Taken as: the tag `v0.7.0`; the packages installed with `apt-get`; the repository under GPL-3-or-later. The three other items of the plan marked "(confirm)" (the slice, the scripts as data, the SDL renderer only) were not asked one by one, and go as recommended in the plan. He may revert any of them | 2026-11-14 |
| 2 | Rule relayed by the session of the compiler: after full fidelity, a port may relax exactness in favor of expressiveness, if the intent of the original author is kept. Until then a difference is a defect. So: build for fidelity first, and list below the places where exactness forces an ugly form | 2026-11-14 |

### Places where exactness forces the form (candidates to relax later)

None yet.
