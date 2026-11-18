# Architecture

## The one rule

**The adaptive core is a pure C++ library with zero graphics dependencies.**

sdp_core contains the learning logic, the level generation, the persistence and the
progression rules. It does not know that a game exists. Everything that draws a pixel,
plays a sound or reads a key lives in a shell, and a shell is thin.

This is not architectural decoration. It is what makes the following possible with no
extra work:

* the 64-test suite and the 150,000-answer simulation run headless, on any machine;
* a second shell (console today, SFML next) proves the core is genuinely reusable;
* a future teacher dashboard, web build or mobile build can reuse the core unchanged.

## Modules

| Module | Responsibility | Graphics deps |
|---|---|---|
| sdp/types.hpp | Domain, SubSkill, DifficultyParams, LevelSpec, Attempt, PerformanceHistory, persisted state structs | none |
| sdp/rng.hpp | deterministic PRNG (xoshiro256**), own Gamma/Beta/Normal samplers | none |
| sdp/bandit.hpp | difficulty policies behind IDifficultyPolicy: latent Thompson, independent Beta arms, UCB1, greedy, static | none |
| sdp/curriculum.hpp | SkillDef (difficulty tier table), ZoneDef, curriculum ordering | none |
| sdp/engine.hpp | AdaptiveEngine: which sub-skill, which tier, checkpoints, spaced review, retry | none |
| sdp/generator.hpp | LevelGenerator: LevelSpec to a concrete multiple-choice puzzle, deterministically | none |
| sdp/store.hpp | IDataStore: the only persistence interface; SqliteStore and MemoryStore | none (SQLite is private) |
| sdp/progression.hpp | altitude, zones, landmarks, checkpoints, milestone keys | none |
| sdp/session.hpp | SessionRunner: one "day of climbing", wiring engine, generator, store, progression | none |
| sdp/content.hpp | ContentLoader + StringTable: curriculum JSON, quiz CSV, localisation | none |

## Data flow for one answered puzzle

~~~mermaid
sequenceDiagram
    participant Shell
    participant Session
    participant Engine
    participant Gen
    participant Store
    Shell->>Session: nextPuzzle()
    Session->>Engine: nextLevel(history)
    Engine-->>Session: LevelSpec(domain, skill, diff, seed, kind)
    Session->>Gen: generate(spec)
    Gen-->>Session: Puzzle(prompt, options, answer, spec_json)
    Session-->>Shell: Puzzle
    Shell->>Session: submitAnswer(index, response_ms)
    Session->>Store: beginBatch()
    Session->>Store: logAttempt(attempt)
    Session->>Engine: observe(attempt) -> bandit update
    Session->>Store: saveProgression / saveSkillState
    Session->>Store: endBatch()
    Session-->>Shell: SessionResult(feedback, altitude, milestones)
~~~

## The two seams that matter

### 1. The DataStore seam

Every read and write goes through IDataStore. No SQL exists outside
core/src/sqlite_store.cpp - the design document's rule, enforced by the fact that
nothing else includes sqlite3.h.

Two implementations exist and are tested identically by one shared test body
(tests/test_store.cpp):

* **SqliteStore** - the shipping implementation, with the five tables exactly as
  specified, foreign keys on, write batching and WAL mode.
* **MemoryStore** - used by the unit tests and by the simulation, which can then run
  hundreds of virtual children without touching a disk.

Because the simulation and the game use the same interface, the harness can be pointed
at the real database (sdp_sim --store sqlite) to exercise the production path, which is
what the performance numbers in docs/performance.md were measured with.

### 2. The policy seam

Every difficulty decision goes through IDifficultyPolicy. This is what let the
simulation compare five policies as a like-for-like swap, and it is what makes the
"which algorithm is best" question answerable with evidence rather than opinion.

## Schema mapping

The design document's schema is implemented exactly. Two pieces of information the
engine needs have no column of their own, and both are carried inside
attempts.spec_json (described in the schema as "optional: exact LevelSpec used
(replay/debug)"):

| Needed by the engine | Where it lives | How it is read back |
|---|---|---|
| difficulty tier | spec_json.diff.tier | level_spec_from_json() on load |
| checkpoint flag | spec_json.kind == "checkpoint" | level_spec_from_json() on load |
| exact puzzle provenance | the whole spec_json | replay/debug of any single item |

Likewise skill_state stores the Beta parameters the schema specifies; for the latent
policy these are an equivalent summary of the belief, and the exact representation is
the attempts log itself. That is deliberate: **replaying the attempts log
(AdaptiveEngine::replay) is the fidelity path**, and a test asserts that a replayed
engine and an incrementally updated engine produce the same next level.

## Lifetime and ownership rules

Two ownership bugs were found by tests during development, so the rules are now stated
explicitly: services own a copy of the curriculum rather than a pointer, because
constructing them from a temporary (makeDefaultCurriculum()) dangles immediately. The
generator and progression service therefore hold their own copy, and everything else
takes references that the composition root owns.

## What a shell must not do

The console shell (apps/cli) shows the contract: it renders, it reads input, and it
calls session.nextPuzzle() and session.submitAnswer(). It contains no learning logic,
no difficulty maths and no SQL. The optional SFML shell follows the same rule.
