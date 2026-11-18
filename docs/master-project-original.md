# SDP 100 — Adaptive Learning Game (Master Project Document)

> One-page elevator pitch: **A 2D puzzle game for children aged 6–10 (math, problem-solving, quizzes) where levels are generated dynamically and difficulty adapts to each child via a multi-armed bandit engine, wrapped in a mountain-climb progression world. All data stays local (SQLite). Built in C++ (SFML/raylib). Designed to compete in the SDP 100 funding funnel: university board → government program → public deployment.**

---

## 1. Project Overview

| Item | Detail |
|---|---|
| Course | SDP 100 — Software Development (C++ requirement) |
| Product | Adaptive educational puzzle game |
| Audience | Children aged 6–10 |
| Domains | Math, problem-solving, quizzes |
| Core mechanic | Dynamic level generation driven by per-child performance |
| Platform | Desktop, offline, local-only data (SQLite) |
| Tech | C++17+, SFML/SDL2/raylib (2D), SQLite, nlohmann/json |
| Funding path | ~100 uni applicants → 3 selected by uni board → government program funding |

### Goals
1. Pass SDP 100 (working, well-architected, documented C++ project).
2. Win the university board selection (stand out among ~100 mostly-CRUD submissions).
3. Win government program funding (impact, cost, offline fit, localizability).
4. Long-term: funding enables "next level" — more subjects, teacher dashboards, possibly cloud sync.

---

## 2. Core Design Principles

1. **Visible progress never regresses.** Wrong answers never move the child down the mountain. The adaptive engine does difficulty tuning *invisibly*; the world rewards *engagement*.
2. **Flow-zone targeting.** The engine keeps each child's success rate around **70–80%** — hard enough to engage, easy enough to avoid frustration.
3. **No deep RL.** Deep RL needs millions of episodes; a child gives ~30–60 answers/session. Per-skill **multi-armed bandits** (Thompson sampling / UCB1) are the right tool: real RL theory, no training data needed, microsecond-fast, defensible in a report.
4. **Engine-agnostic core.** The adaptive engine is a pure C++ library with **zero graphics dependencies**; the SFML/raylib layer is a thin shell. Keeps the core testable, portable, and board-friendly.
5. **State vs. content separation.** Player state lives in SQLite; human-editable content (question banks, templates) lives in JSON/CSV files.

---

## 3. Game Concept: The Mountain World

Instead of a flat quiz session, the child **climbs a mountain**:

- **1 solved problem = 1 step up.** Simple, predictable, visible.
- **Zones**: forest → rocky slopes → snowline → summit. Each zone is a visual palette swap (cheap assets, big perceived variety) and gates a set of sub-skills — zones double as curriculum ordering.
- **Landmarks every ~10 steps**: a flag, a cabin, an animal friend, a view unlock. One image + small animation; huge dopamine payoff.
- **Checkpoint challenges** at zone boundaries: "boss" problems mixing previously mastered sub-skills. This is **spaced retrieval practice** — pedagogically sound and a strong report talking point.
- **Session = one "day of climbing"**, ending at a camp/summit screen: natural save points, natural stopping (parent-friendly), and a satisfying end-of-session summary ("You reached the snowline today!").
- **Wrong answer**: gentle feedback, retry offered (same sub-skill, possibly −1 difficulty step), no altitude loss, no shame mechanics.

### Presentation
2D **vertical scroller**: camera follows the sprite up the mountain. No explorable side-scrolling world, no walking physics — scope explosion with zero adaptive-learning payoff.

---

## 4. The Adaptive Engine

### Model
- Each child has a model **per sub-skill** (e.g., counting, addition, subtraction, fractions, patterns, logic).
- Each sub-skill tracks: current difficulty parameter + a **Beta(α, β) posterior** over success probability (Thompson sampling).
- After each attempt: update posterior with the outcome; adjust difficulty toward the flow zone.
- Difficulty knobs (from `DifficultyParams`): number ranges, operand count, distractor count, time limit, etc.

### Core contract
```cpp
struct LevelSpec {
    Domain   domain;      // Math | ProblemSolving | Quiz
    SubSkill skill;       // addition, patterns, ...
    DifficultyParams diff;// number ranges, operand count, distractors, time limit
    uint32_t seed;        // RNG seed => deterministic, reproducible generation
};

class AdaptiveEngine {
public:
    LevelSpec nextLevel(const PerformanceHistory& history);
};
```

Levels are generated **deterministically from the seed** → testable, replayable, debuggable.

### Game loop (pseudocode)
```
load child profile (DataStore) -> load skill_state + progression
loop:
    LevelSpec = engine.nextLevel(history)
    present puzzle (LevelGenerator renders spec into concrete puzzle)
    record attempt (correct, response_ms)
    if correct: climb animation + step up; check landmark / checkpoint / zone-up
    else: gentle feedback, offer retry (same sub-skill, maybe -1 difficulty)
    DataStore.logAttempt(...)      // SQLite insert
    engine.update(posterior)       // bandit update
    DataStore.saveSkillState(...)
until session end (camp) -> save progression, show summary screen
```

---

## 5. Architecture

| Module | Responsibility | Notes |
|---|---|---|
| GameShell | Rendering, input, audio, UI, scenes | SFML or raylib; thin layer |
| AdaptiveEngine | Bandits, posteriors, difficulty policy | Pure C++, no graphics deps |
| LevelGenerator | LevelSpec → concrete puzzles | Deterministic from seed |
| ContentLoader | Loads question banks / templates (JSON/CSV) | Human-editable files |
| DataStore | All SQLite access | Single wrapper class; **no SQL outside this class** |
| Progression | Altitude, zone, milestones, checkpoints | Presentation-layer state machine |
| SimHarness | Synthetic-learner simulation | Pure C++ CLI, no graphics |

Key rule: `AdaptiveEngine`, `DataStore`, `LevelGenerator`, bandit logic form a **portable core library**; the graphical shell sits on top. This enables headless testing, the simulation harness, and future v2 work without rewrites.

---

## 6. Data Design (SQLite)

### Schema
```sql
CREATE TABLE children (
    id         INTEGER PRIMARY KEY,
    name       TEXT NOT NULL,
    age        INTEGER,
    created_at TEXT DEFAULT (datetime('now'))
);

CREATE TABLE attempts (
    id           INTEGER PRIMARY KEY,
    child_id     INTEGER NOT NULL REFERENCES children(id),
    domain       TEXT NOT NULL,     -- 'math' | 'problem_solving' | 'quiz'
    sub_skill    TEXT NOT NULL,
    difficulty   REAL NOT NULL,
    correct      INTEGER NOT NULL,  -- 1/0
    response_ms  INTEGER,
    spec_json    TEXT,              -- optional: exact LevelSpec used (replay/debug)
    created_at   TEXT DEFAULT (datetime('now'))
);

CREATE TABLE skill_state (
    child_id   INTEGER NOT NULL REFERENCES children(id),
    sub_skill  TEXT NOT NULL,
    difficulty REAL NOT NULL,
    alpha      REAL,   -- Beta posterior params for Thompson sampling
    beta       REAL,
    updated_at TEXT,
    PRIMARY KEY (child_id, sub_skill)
);

CREATE TABLE progression (
    child_id   INTEGER PRIMARY KEY REFERENCES children(id),
    altitude   INTEGER NOT NULL DEFAULT 0,
    zone_id    INTEGER NOT NULL DEFAULT 0,
    updated_at TEXT
);

CREATE TABLE milestones (
    child_id  INTEGER NOT NULL REFERENCES children(id),
    milestone TEXT NOT NULL,   -- 'first_flag', 'snowline', ...
    earned_at TEXT DEFAULT (datetime('now')),
    PRIMARY KEY (child_id, milestone)
);
```

### Key query (engine input — success rate per skill)
```sql
SELECT sub_skill, AVG(correct) AS success_rate, COUNT(*) AS n
FROM attempts
WHERE child_id = ?
  AND created_at > datetime('now', '-7 days')
GROUP BY sub_skill;
```

### Storage split
| SQLite (state) | JSON/CSV (content) |
|---|---|
| children / profiles | quiz question banks |
| attempts (performance log) | puzzle/logic templates |
| skill_state (posteriors) | difficulty parameter tables |
| progression + milestones | localization strings |

Practical tips: `PRAGMA foreign_keys = ON;` per connection; WAL mode only if multi-process access is ever needed. C++ integration via raw `sqlite3` C API or the **SQLiteCpp** RAII wrapper.

### Why not JSON for state
The engine's needs are queries ("success rate on addition, last 15 attempts"), multi-child profiles, and crash-safe persistence — all native to SQLite. Bonus: analytics and CSV export for plots are single queries.

---

## 7. Content Strategy

**The hidden cost of this project class is content authoring.** Mitigations:

- **Parametrized templates**: math puzzles are generated procedurally from `DifficultyParams` (e.g., `a + b = ?` with ranges from the spec).
- **Problem-solving & quiz content**: 5–10 parametrized logic/pattern templates + question banks per topic; the LevelSpec picks template + fills parameters.
- **Rule of thumb**: 3 puzzle types per domain done *well* beats 10 done badly.
- Content files are hand-editable text → curriculum alignment and translation without recompiles (a key government-program selling point).

---

## 8. Art & Audio Plan

- Scope: **~15–20 assets total**. One character sprite (climb / idle / celebrate animations), one parallax background per zone, step markers, landmark images, UI icons, correct/wrong audio cues.
- UX for 6–10: big buttons, bright colors, audio feedback, minimal text.
- Target stack: SFML, SDL2, or raylib (all zlib-licensed, lightweight, publication-safe).
- Explicit non-goals: 3D, explorable worlds, walking physics, multiplayer, accounts/servers.

---

## 9. Roadmap

1. **Core loop with hardcoded levels** (playable math quiz) — first playable.
2. **LevelSpec + LevelGenerator** — same game, now data-driven and seed-reproducible.
3. **Telemetry** — every answer logged to `attempts`.
4. **Bandit controller wired in** — full adaptive loop + SQLite persistence.
5. **Sim harness** — synthetic-learner evidence charts (see §10). *Build before polishing assets.*
6. **Assets, audio, zones, landmarks, checkpoints** — the mountain comes alive.
7. **Publication track** — licensing doc, packaging, README, demo video, pilot study.

Milestone 4 alone = complete, defensible course project; everything after is upside.

---

## 10. Evidence & Evaluation

### A. Synthetic-learner simulation (highest leverage, zero ethics overhead)
- Simulate 200–1000 virtual children; each has a latent ability per sub-skill (fixed success probability per difficulty).
- Run them through `AdaptiveEngine` for a few hundred questions each.
- Deliverable charts:
  1. Success rate pinned at ~75% for **every** child (weak and strong).
  2. Difficulty climbing as ability grows.
  3. **Adaptive vs. static difficulty** comparison — static fails weak children and bores strong ones.
- Pure C++ against the engine, no graphics; ~a weekend of work; converts "trust me, it adapts" into quantitative evidence for both boards. Later, the real pilot confirms what the simulation predicted.

### B. Real-child pilot (if time/ethics permit)
- 5–10 children, parental consent, 2–3 sessions each.
- **Check university ethics-approval requirements for testing minors early** (lead times).
- Evidence: success rate held near target across different children; visible adaptation in `attempts` logs.

---

## 11. Publication-Readiness Track

| Area | Plan |
|---|---|
| Licensing | SFML/SDL2/raylib (zlib), SQLite (public domain), nlohmann/json (MIT) — all publication-safe. Keep `THIRD_PARTY.md`. |
| Privacy | Local-only SQLite, offline-first, no accounts/servers/telemetry. Headline: **"all child data stays on the device."** |
| Data movement (future) | Only mitigations ever needed: on-request **anonymized aggregate CSV export** (one SQL query away). Cloud sync / dashboards are v2, enabled by the DataStore seam. |
| Packaging | Installer or portable build per OS, README, 2–3 min demo video, crash-free on weird input. |
| Deployment target | Runs offline, on old/low-end school hardware; few-MB footprint (C++ advantage over engine-based builds). |

Deployment context: if approved, the game would be run by a government program for one of their projects (non-commercial). Award + funding would either fund bigger projects or a "next level" of this one (more subjects, languages, teacher dashboards, optional cloud sync) — all additive thanks to the modular core.

---

## 12. Two-Board Pitch Strategy

One product, two decks:

**Stage 1 — University board (~100 → 3).** Academics selecting on feasibility & execution quality.
- Emphasize: algorithmic core (bandits), schema, architecture diagram, simulation charts.
- Position against the pool: most submissions will be CRUD apps; this looks like *research*, not homework.
- Working demo + evidence charts > polished art at this stage.

**Stage 2 — Government program (3 → funded).** Selects on impact story.
- Emphasize: national curriculum alignment, localizability (editable content files → translation without recompile), offline + low-hardware fit, zero operating cost, no data-risk surface.
- The adaptive-vs-static comparison chart is the money slide at both stages.

---

## 13. Risks & Scope Guardrails

| Risk | Mitigation |
|---|---|
| Content authoring eats all time | Parametrized templates; 3 types per domain max; question banks as data files |
| Temptation to build deep RL | Bandits are the defensible, correct tool; say so explicitly in the report |
| Scope creep toward "real game engine" | 2D vertical scroller only; no physics worlds, no 3D, no multiplayer |
| Storage refactor late | DataStore wrapper from day one; no SQL outside it |
| Ethics approval delays pilot | Start early; sim harness provides evidence meanwhile |
| Over-polish before core works | Sim harness + telemetry before asset polish (roadmap order) |

---

## 14. Open Items

- [ ] Working title + mascot concept
- [ ] SFML vs. raylib vs. SDL2 final decision (all fine; decide by "which do you want to learn")
- [ ] Game-loop state machine diagram (menus → session → puzzle → checkpoint → session end)
- [ ] DataStore class interface sketch
- [ ] Thompson sampling update rule written up for the report
- [ ] One-page proposal skeleton for the board submission
- [ ] Simulation design details (honest virtual-learner modeling)
- [ ] Ethics/consent process check with the university