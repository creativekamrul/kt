# The adaptive engine

## The objective: get to, and stay in, the flow zone

The engine does **not** maximise correct answers - that would just serve the easiest
item forever. It solves a *satisficing* problem: keep the probability that this child
answers this item correctly near a target (default 0.75), the middle of the pedagogical
flow zone. Every policy below is a different way of estimating and hunting that target.

A second, invisible job is to keep visible progress encouraging: difficulty moves, the
mountain only ever goes up (see docs/architecture.md and ProgressionService).

## Why bandits, not deep RL

* A child produces 30-60 answers per session; deep RL wants millions of episodes.
* There is no pre-training data, and there never will be without a large pilot.
* A bandit's behaviour is analysable and explainable to a teacher or an ethics board:
  each arm carries an explicit belief, and the update rule is Bayes' theorem.
* Inference is microseconds, so it runs on a low-end school PC.

## Policy 1 (shipped): Thompson sampling over a latent ability

The naive bandit formulation puts one Beta posterior on each difficulty tier and picks
the arm whose *sampled* success probability is closest to the target. It has a structural
flaw: information learned at one tier says nothing about any other tier, so a child far
from the initial prior has to be re-discovered one tier at a time. The simulation
measures this failure (docs/simulation.md): strong children were parked at a tier they
answered correctly ~90% of the time, because the target tier's prior looked worse than
the already-known easy tier.

The shipped policy instead uses **one posterior over the child's latent ability** and a
monotone response model. Ability theta is measured in tier units, so "theta = 7" means
"this child's 75% success point is tier 7".

Response model:

~~~
P(correct | tier, theta) = guess + (1 - guess) * logistic((theta - tier) * slope + offset)
~~~

with guess = 0.25 (the multiple-choice guessing floor for four options), slope = 0.8 (an
assumption, not a measurement) and offset = 0.693, chosen so that logistic(0.693) = 2/3
and therefore P(correct) = 0.75 exactly when theta equals the tier.

The posterior over theta is a discrete grid (four points per tier, 0 to 11) updated
exactly by Bayes:

~~~
w_i  <-  w_i * P(outcome | tier, grid_i)
w_i  <-  (1 - temper) * w_i / sum(w) + temper / N        # temper = 0.012
~~~

**Why this is still Thompson sampling.** Sampling an ability from the posterior and then
acting greedily with respect to the objective is exactly Thompson sampling - the
difference from the naive version is only that the reward model is *structured* (monotone
in difficulty, parameterised by one number) instead of a bag of independent arms. One
observation at one tier re-prices every tier at once, which is the whole point.

**Why the temper term exists.** The uniform floor keeps a small amount of probability on
every ability value. Without it the posterior collapses and can no longer follow a child
whose ability is *growing during the session* (the thing we are trying to cause). It is
deliberately small (1.2%) so it costs little precision.

Selection is then: draw theta, return the allowed tier whose predicted success is closest
to the target.

## Policies 2-5 (comparison arms)

| Kind | Mechanism | Reported result (tail success: weak / mid / strong) |
|---|---|---|
| ThompsonSatisficing | independent Beta(alpha, beta) per tier, sampled-success vs target | 0.609 / 0.776 / 0.902 |
| Ucb1Shaped | UCB1 over a reward shaped as closeness to the target | 0.477 / 0.651 / 0.822 |
| GreedyWindow | pure exploitation of the posterior mean | 0.769 / 0.871 / 0.927 |
| Static | a fixed tier (the no-adaptation baseline) | 0.404 / 0.720 / 0.913 |

Keeping the ablations in the shipped library is deliberate: it costs nothing, and it lets
the report answer "why not just X?" with measurements instead of assertion.

## Curriculum policy: which sub-skill, not just how hard

Difficulty is the bandit's job; *what to practise* is a teaching decision, so it is a
separate, explicit rule:

1. **New material first.** Any unlocked sub-skill with no attempts yet has top priority.
2. **Weakest relative to target.** Otherwise serve the sub-skill whose estimated success
   at its own flow tier is lowest.
3. **Rotation pressure.** Subtract 0.35 * staleness, where staleness grows with the
   number of attempts since that sub-skill was last seen.

Rule 3 exists because of a measured failure: once several sub-skills are tuned, their
estimated success all sits at the target, priorities become equal, and the deterministic
tie-break then served the first sub-skill in curriculum order almost exclusively
(measured: 187 of 300 attempts on one skill). A regression test now asserts that no
single sub-skill monopolises practice.

Mastered sub-skills (estimated success at their flow tier above 0.90) are pushed to the
back of the queue.

## Checkpoints and spaced retrieval

* **Checkpoint** when altitude lands exactly on a zone boundary: the "boss" item mixes
  the sub-skills of the zones already climbed, and for arithmetic it is rendered as a
  mixed expression (a + b - c) so it is genuinely a cross-skill retrieval test.
* **Review** every review_interval attempts, on the unlocked sub-skill practised longest
  ago. This is spaced retrieval practice, and it is the reason review items are not just
  "more of the same".

## The retry rule

Wrong answers are gentle and never move the mountain. The child is offered **one** retry
on the same sub-skill, capped at one tier below the *practice* tier:

* one retry only - two failures in front of a six-year-old is where frustration starts;
* anchored to the practice tier, not the previous attempt. The first implementation
  anchored it to the previous attempt and a struggling child cascaded downward without
  limit, which silently overrode the difficulty policy. The simulation exposed it:
  every policy produced identical numbers, because the retry cascade was doing the
  adapting instead of the bandit.

## Determinism and replay

Every decision is a pure function of (persisted attempts log, config, root seed).
nextLevel derives a fresh RNG from the attempt index, so folding the same log twice
gives the same sequence of levels. Concretely:

* a test asserts that an engine updated incrementally and an engine that replayed the log
  from scratch propose the identical next level;
* a test asserts that two independent game sessions, given the same answers, produce
  byte-identical attempt logs;
* the simulation asserts that rerunning with the same seed produces byte-identical CSV.

This is why the database - not the engine's memory - is the source of truth, and why a
bug report can be replayed from the attempts table alone.

## Persistence

skill_state stores difficulty, alpha and beta as the schema specifies. For the latent
policy those Beta parameters are an equivalent summary of the belief at the flow tier;
the exact representation is the attempts log, so AdaptiveEngine::replay is the
high-fidelity resume path and loadSkillStates is the cheap one. Both are exercised by
tests.

## Tuning parameters

| Parameter | Default | Meaning |
|---|---|---|
| target_success | 0.75 | flow-zone centre |
| latent_prior_mean / sd | 1.0 / 1.3 | a new child starts near the bottom of the mountain |
| latent_guess | 0.25 | four-option guessing floor |
| latent_slope | 0.8 | assumed steepness of the response curve |
| latent_temper | 0.012 | how fast the belief can follow a growing child |
| review_interval | 5 | attempts between spaced-review items |
| mastery_success | 0.90 | when a sub-skill stops being prioritised |
| checkpoint_at_zone_boundaries | true | boss items at zone gates |

All of these live in EngineConfig and can be changed without touching the algorithm.

## Known limitations (honest list)

1. **Lag behind a growing child.** Ability grows during practice while the controller
   tracks it, so the served difficulty trails by roughly one tier. The measured effect is
   a steady-state success rate about 3-5 points above the target for the strongest group
   (0.806 instead of 0.750). A drift term in the posterior update is the obvious next
   improvement; it was not added because it would have to be tuned to a learning rate we
   have not measured on real children yet.
2. **The response model is an assumption.** Slope and the guessing floor are not fitted to
   data. The robustness run in docs/simulation.md deliberately mismatches them between
   engine and simulated child, and the shipped policy still holds.
3. **The difficulty floor is tier 0.** A child below the easiest content cannot be given
   anything easier; the engine then simply serves tier 0 and accepts a lower success
   rate. Adding easier content (or finer tiers) is a content-side fix.
4. **Twelve tiers per sub-skill** is a design choice, not a measurement. More tiers mean
   finer control and slower convergence.
5. **Time limits are carried in DifficultyParams but never enforced** - the field exists
   for the shell to use, and no policy adapts on it yet.
6. **No forgetting model.** A child returning after two weeks resumes with the same
   posteriors. Spaced review mitigates but does not solve this.
