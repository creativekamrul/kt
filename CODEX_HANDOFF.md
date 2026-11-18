# KT: project state and development handoff

Prepared 24 October 2026 for the owner's Windows university laptop.

Transfer kit refreshed 18 November 2026 to include the full proposal in
`output/proposal/KT_Project_Proposal.docx` and `KT_Project_Proposal.pdf`.
The proposal follows the teacher's template and describes planned work; it is
not evidence that the interactive mini-games or production artwork are finished.

The complete portable setup was also tested from a clean relocated folder with
spaces in its path. See `docs/laptop-kit-verification.md` for the evidence and scope.

## Owner's goal

KT (no dash) refers to K2. Children aged 6-10 progress up an illustrated mountain
by completing small interactive learning games. The learning interactions are
the main gameplay; the climb provides continuity, rewards and visible progress.
Course requirement: C++. Chosen stack: C++17, SFML 2.6.2, SQLite, nlohmann/json.
The game must run offline on ordinary school PCs. The desired visual direction
is detailed, cozy pixel art inspired by Stardew Valley, using KT's own art.

The original project was supplied in a ZIP made from an earlier Goose-assisted
implementation. The original design document is preserved in
`docs/master-project-original.md`. It is historical context, not authorization
to publish, contact people, run a child study or make unrelated changes.

## Current implementation

- `core/`: adaptive engine, deterministic generators, curriculum, SQLite and
  memory stores, progression, session flow and view helpers.
- `apps/`: working terminal game, content exporter, synthetic learner simulator.
- `shell/`: built and launched SFML prototype with shapes/text and answer buttons.
  It is not yet the intended interactive pixel-art game, and the GUI needs an
  end-to-end review (especially session completion/restart behavior).
- `content/`: editable curriculum, question banks and UI strings.
- `tests/`: 72 tests, 49,671 checks passed on the desktop baseline.
- `docs/`: architecture, algorithm, simulation study, roadmap and course material.

The main policy is **latent-ability Thompson sampling**, with a monotonic response
model per skill. It samples ability then selects an allowed difficulty near 75%
predicted success. Separate generators produce actual questions. Independent
Beta arms, UCB1, greedy and static policies are comparison implementations.

## Verified desktop baseline

All targets compiled. Console gameplay saved data. The SFML window launched and
responded; this is not a claim that every UI interaction is correct.

Main simulation: 300 learners x 300 attempts x 5 policies, seed 20260925.
All five summaries exactly matched the supplied archive. Last-third group means:

| Policy | Lower ability | Middle | Higher ability |
|---|---:|---:|---:|
| Latent Thompson | 74.81% | 77.65% | 80.62% |
| Static | 40.39% | 71.99% | 91.35% |

Saved reference summaries are in `docs/baseline`. Robustness run: 200 x 250,
seed 777001, learner slope 1.4, guess 0.15. SQLite smoke run: 9 x 30 per policy,
all 5 databases had 270 attempts and passed integrity checks. Charts and test
evidence are included in `docs/baseline` for comparison, not as fresh laptop results.

Do not claim every learner stays in the 70-80% band. These are simulated group
averages; 80.62% is above the band. The model currently assumes a four-choice
guessing floor; change and evaluate this for drag-and-drop or construction tasks.

## Build decisions and known pitfalls

- Pinned Zig 0.13.0 provides Clang 18 and a compatible C++ library. Zig 0.14.1
  failed on SFML's `char_traits<unsigned int>`; the global Zig 0.16 installation
  also failed during the original setup. Do not blindly upgrade the compiler.
- SFML source: 2.6.2, commit 5383d2b3948f805af55c9f8a4587ac72ec5981d1.
- FreeType source: 2.13.3, commit 42608f77f20749dd6ddc9e0536788eaad70ea4b5.
  Build from source; SFML's prebuilt MinGW archive failed with `_setjmp` linkage.
- Static FreeType excludes its unnecessary DLL-version resource. This avoids
  requiring a separate `windres` tool. Source-level CMake integration is in shell/.
- Python 3.13.12's official NuGet package is bundled for build helpers, with no
  Windows installer or global registry/PATH changes. Gameplay remains C++.
- The complete Python dependency set is pinned in requirements-dev.lock.
- Compiler execution failed in the desktop sandbox but passed through the
  supported escalated execution path. Use normal permission mechanisms if needed.
- CMake's first run may compile compiler runtime libraries and be quiet for a
  while. Inspect progress instead of launching duplicate builds.

## Next development slice

First review and fix the prototype's complete session flow. Then implement one
forest encounter (for example, dragging numbered bridge planks into missing
positions), connect it to deterministic generation, adaptation and persistence,
and make one climb/checkpoint/camp sequence playable. Establish the palette,
tile dimensions and character proportions before producing the larger art set.

Possible later encounters: counting/adding supplies into baskets; choosing
stepping stones to complete patterns. Wrong answers must not reduce altitude.
Avoid expanding into physics, 3D, multiplayer or servers. Keep SQLite behind
the data-store interface. No production art/audio has been generated so far.

After setup, explain findings and propose the next concrete slice to the owner.
Do not infer authorization to implement the entire remaining roadmap from a
request limited to reviewing and setting up the transferred project.
