# Synthetic-learner simulation

## Why it exists

The claim "the difficulty adapts to each child" is worthless without evidence, and a
real pilot needs ethics approval, consenting families and months of lead time. A
simulation needs none of that, runs in under a minute, and can be repeated exactly.
It is the highest-leverage artefact in the project: it converts a promise into numbers.

It also earned its keep by finding two real bugs (below) that no unit test had caught.

## What is simulated

Each virtual child has a latent ability **per sub-skill** (in tier units), drawn from a
population mixture:

| Group | Base ability | Note |
|---|---|---|
| weak | uniform 0.5 - 3.5 | struggles with most of the shipped content |
| mid | uniform 3.0 - 7.0 | in the middle of the ladder |
| strong | uniform 7.0 - 11.0 | would be bored by a fixed difficulty |

Per-sub-skill noise (sd 1.2) makes profiles spiky, because a real child is not equally
able at counting and at odd-one-out.

After that, four modelled effects, all documented here rather than buried in code:

1. **Response curve.** P(correct) = guess + (1-guess) * logistic((theta - tier) * slope +
   0.693), with guess = 0.25 (four options) and slope = 0.9 by default. The engine
   assumes slope 0.8, so the engine and the child do **not** share the same model unless
   asked to.
2. **Slips.** A 4% chance of tapping the wrong button despite knowing the answer.
3. **End-of-session fatigue.** The last three items of each ten-item session are 6%
   harder to get right. A controller that does not adapt will drift *down* within a
   session, which is a realistic and useful stress.
4. **Learning.** Ability rises during practice, and rises fastest inside the flow zone
   (0.025 per in-zone answer, 35% of that otherwise, 15% bonus when correct). This is
   what makes "difficulty climbs as ability grows" a non-trivial claim: an engine that
   parks a child at 100% success teaches it almost nothing.

Latency is modelled as 900 ms + 1500 * difficulty + 1200 * (1 - P(correct)) + noise, so
the response_ms telemetry looks plausible.

## Design choices that make the comparison trustworthy

* **Paired population.** Every arm faces the *identical* children, constructed from the
  same per-child seed. Arm-to-arm differences are therefore attributable to the policy,
  not to sampling different children.
* **Same code path as the game.** The harness drives SessionRunner - engine, generator,
  store, progression, telemetry, retry rule - rather than a stripped-down loop. If a
  policy looks good here, it is good in the shipped game.
* **Two stores.** The main run uses the in-memory store for speed; a separate run uses
  the production SQLite store and produces identical CSVs, which validates the storage
  abstraction and exercises the real persistence path under load.
* **Determinism.** The same seed reproduces the output byte for byte (verified with a
  file comparison across runs and across stores).

## Results: 300 children x 300 answers, seed 20260925

Success rate in the last third of the run (steady state), and the mean absolute distance
from the 0.75 target:

| Policy | Weak | Mid | Strong | Mean distance | Mean tier first -> last (strong) |
|---|---|---|---|---|---|
| latent Thompson (shipped) | 0.748 | 0.777 | 0.806 | **0.043** | 2.74 -> 6.89 |
| independent Beta arms | 0.609 | 0.776 | 0.902 | 0.117 | 4.15 -> 4.50 |
| UCB1 (shaped) | 0.477 | 0.651 | 0.822 | 0.152 | 3.94 -> 5.46 |
| greedy (no exploration) | 0.769 | 0.871 | 0.927 | 0.115 | 3.20 -> 3.05 |
| static difficulty | 0.404 | 0.720 | 0.913 | 0.207 | 4.93 -> 4.93 |

Readings:

* The shipped policy is the only one that holds all three groups in or next to the
  70-80% zone.
* Static difficulty reproduces the classic failure mode exactly: 40% success for weak
  children (frustration) and 91% for strong ones (boredom).
* Greedy is a good illustration of why exploration matters: it parks children at
  whichever tier first looked right (mean tier drifts *down* as ability grows) and ends
  up too easy for everyone.
* The shipped policy is the only one whose mean tier climbs substantially during the run
  (2.74 -> 6.89 for the strong group), i.e. it actually tracks a growing child.

## Robustness: a deliberately mismatched learner

200 children x 250 answers, learner slope 1.4 and guessing floor 0.15 (the engine still
assumes 0.8 and 0.25):

| Policy | Mean distance from target (matched learner) | (mismatched learner) |
|---|---|---|
| latent Thompson (shipped) | 0.043 | **0.046** |
| independent Beta arms | 0.117 | 0.132 |
| UCB1 | 0.152 | 0.193 |
| greedy | 0.115 | 0.117 |
| static | 0.207 | 0.266 |

The shipped policy barely notices; static difficulty degrades by 29%. Because the engine
never sees the simulator's parameters, this is the fairest available check that the
result is not an artefact of a matched model.

## Bugs this harness found

Both were invisible to unit tests and both were found only because the arms were compared

1. **A retry cascade defeated the difficulty policy.** Each wrong answer offered a retry
   one tier below the *previous attempt* rather than below the practice tier, so a
   struggling child walked downwards indefinitely. Every policy produced identical
   results, because the retry rule was doing the adapting. Fixed by anchoring retries to
   the practice tier and allowing one retry per mistake; a regression test now asserts
   that two retries can never be adjacent.
2. **The configured policy was silently ignored.** The engine decided whether a policy
   needed (re)building by comparing tier counts - and every shipped skill happens to have
   exactly 12 tiers, so the check never fired and the default policy was used for every
   arm. Fixed by tracking the applied configuration explicitly; a regression test asserts
   that a static policy never moves the tier.
3. **Curriculum starvation.** Equal priorities plus a deterministic tie-break served one
   sub-skill for 187 of 300 attempts. Fixed with rotation pressure; a regression test
   asserts no sub-skill monopolises practice.

The general lesson, and the reason this document exists: *a comparison you cannot run is
a comparison you cannot trust*. Every arm here is a one-line configuration change, so
the claims in the report are reproducible by anyone with the repository.

## Honest caveats

1. **Simulated children are not children.** The model is plausible, not validated. It
   cannot capture attention, motivation, fatigue curves across days, or reading ability.
2. **Fitted knowledge.** The engine knows the multiple-choice guessing floor (0.25) and
   the simulator uses the same value. That is defensible - the floor follows from the
   item format, not from the child - but it is an assumption worth stating.
3. **Learning is modelled, not measured.** The learning rate is a plausible guess. The
   engine's conservative temper setting is chosen to be robust to that uncertainty rather
   than optimal for it.
4. **One population shape.** Three ability groups with spiky per-skill profiles. Real
   classrooms have different distributions, and a pilot would test them.
5. **The metric is success rate.** Time-on-task, enjoyment and retention - the outcomes a
   government program actually cares about - are not modelled at all.

A pilot with 5-10 children remains the only way to validate any of this, which is exactly
what the design document's evidence plan says. The simulation's job is to make sure the
pilot is worth running: it predicts that a fixed-difficulty build would leave a third of
the children failing two items in three.

## Reproducing it

~~~
python tools/build.py
build/apps/sdp_sim.exe --children 300 --attempts 300 --seed 20260925 --out out/main
build/apps/sdp_sim.exe --children 200 --attempts 250 --seed 777001 \
    --learner-slope 1.4 --learner-guess 0.15 --out out/steep
python tools/make_charts.py --main out/main --steep out/steep --out docs/figures
~~~

Add --store sqlite to any run to exercise the production persistence path instead of the
in-memory store. Figures land in docs/figures, and a machine-readable summary table is
written to docs/figures/summary_table.csv so no number in the report is retyped by hand.
