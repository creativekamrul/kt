# Board proposal - skeleton

A one-page skeleton for both stages of the funding funnel. Fill the brackets; the facts
are already established by this repository.

## Stage 1 - university board (about 100 applicants, 3 selected)

Selection criteria: feasibility and execution quality. Lead with the algorithmic core,
the architecture and the evidence, not with art.

**Title.** [Working title]. An adaptive maths and problem-solving game for 6-10 year
olds, with a bandit-driven difficulty engine.

**Problem.** [One sentence on the gap: primary-school practice software is usually
one-size-fits-all, so the weakest children fail repeatedly and the strongest are bored.]

**What we built (already working).** A portable C++17 core, 64 automated tests with
49,221 assertions, and five interchangeable difficulty policies compared in a simulation
of 300 virtual children. Playable today as a console build; the graphical shell is a thin
layer on the same core.

**The algorithmic contribution.** Difficulty control is posed as a satisficing bandit:
keep each child's success probability near 0.75 with a monotone response model and
Thompson sampling over a latent ability parameter. Measured against a fixed-difficulty
baseline: the shipped policy holds all ability groups in the 70-80% flow zone (0.748 /
0.777 / 0.806), while fixed difficulty produces 0.404 for the weakest children and 0.913
for the strongest. A deliberate model mismatch between engine and simulated child barely
moves the shipped policy (0.043 to 0.046) but degrades the baseline by 29%.

**Engineering quality.** Engine decisions are a pure function of the persisted log, so
any session is replayable from the database; the simulation reproduces byte-identically.
The store is behind one interface with two implementations that are tested by one shared
test body. Content is data, not code.

**Evidence.** Figures in docs/figures, method and caveats in docs/simulation.md,
including the three defects the harness found. Every number is regenerable with three
commands.

**Feasibility.** Roadmap milestones 1-5 of 7 are complete. Milestone 4 alone is a
complete course project; the remainder is assets and packaging. One developer,
[C++ toolchain], no external services.

**Ask.** [Selection for the government-program stage / supervision / a pilot partner.]

## Stage 2 - government program (3 funded)

Selection criteria: impact story. Lead with curriculum fit, cost and data safety.

**Impact.** Every child works inside their own flow zone, so the weakest are not
humiliated and the strongest are not bored; the difficulty changes are invisible, and
visible progress never regresses.

**Curriculum alignment without recompilation.** Sub-skills, difficulty tiers, zone
ordering and question banks are editable data files. A curriculum officer can re-order
the syllabus; a translator can add a language by copying one JSON file.

**Cost.** No servers, no licences, no per-seat fees. Runs offline on old, low-end school
hardware; a few megabytes; all dependencies are public domain or MIT/zlib.

**Data protection.** All child data stays on the device in one local database. No
accounts, no network code, no telemetry. The only admissible data movement is an
on-request anonymised export (participant codes, no names or ages).

**Localisability and repairability.** Written in standard C++ with a vendored SQLite; a
local team can build, inspect and extend it without proprietary tools.

**What funding unlocks.** More subjects and languages, a teacher-facing progress view, and
a pilot study - all additive, because the core is a library with a documented seam for
both storage and difficulty policy.

**Risks and mitigations.** Content authoring (templated generation, three types per
domain); scope creep (2D vertical scroller only, explicit non-goals); ethics lead time
(start early, see docs/ethics-checklist.md).

**Ask.** [Funding amount and what it buys, in the currency of the program.]
