#include "sdp/engine.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace sdp {

AdaptiveEngine::AdaptiveEngine(EngineConfig config, Curriculum curriculum)
    : config_(std::move(config)), curriculum_(std::move(curriculum)) {}

int AdaptiveEngine::altitudeFrom(const PerformanceHistory& history) noexcept {
  return history.correctCount();
}

AdaptiveEngine::SkillRuntime& AdaptiveEngine::ensureRuntime(std::string_view skill) {
  return runtimes_[std::string(skill)];
}

const AdaptiveEngine::SkillRuntime* AdaptiveEngine::runtimeFor(std::string_view skill) const {
  auto it = runtimes_.find(skill);
  return it == runtimes_.end() ? nullptr : &it->second;
}

IDifficultyPolicy* AdaptiveEngine::policyFor(std::string_view skill) {
  auto it = runtimes_.find(skill);
  return it == runtimes_.end() ? nullptr : it->second.policy.get();
}

const IDifficultyPolicy* AdaptiveEngine::policyFor(std::string_view skill) const {
  const SkillRuntime* runtime = runtimeFor(skill);
  return runtime == nullptr ? nullptr : runtime->policy.get();
}

void AdaptiveEngine::reset() {
  runtimes_.clear();
  observations_ = 0;
}

IDifficultyPolicy& AdaptiveEngine::policyForSkill(std::string_view skill, int tier_count) {
  SkillRuntime& runtime = ensureRuntime(skill);
  BanditConfig cfg = config_.bandit;
  if (skill == skills::Navigation.view()) {
    // One observation covers the whole ordered route. Uniformly guessing a
    // four-way sign is a different input mechanic.
    cfg.ordered_route = true;
  }
  cfg.tier_count = std::max(1, tier_count);

  const bool needs_build = !runtime.configured || runtime.policy == nullptr ||
                           runtime.applied.kind != cfg.kind ||
                           runtime.applied.tier_count != cfg.tier_count ||
                           runtime.applied.static_tier != cfg.static_tier ||
                           runtime.applied.target_success != cfg.target_success ||
                           runtime.applied.discount != cfg.discount ||
                           runtime.applied.min_pulls_per_tier != cfg.min_pulls_per_tier ||
                           runtime.applied.latent_slope != cfg.latent_slope ||
                           runtime.applied.latent_prior_mean != cfg.latent_prior_mean;
  if (needs_build) {
    runtime.policy = makeDifficultyPolicy(cfg, cfg.tier_count);
    runtime.applied = cfg;
    runtime.configured = true;
    runtime.attempts = 0;
  }
  return *runtime.policy;
}

// ---------------------------------------------------------------------------
// The bandit update step
// ---------------------------------------------------------------------------
void AdaptiveEngine::observe(const Attempt& attempt) {
  const int tier_count = attempt.skill == skills::Navigation ? 4 : curriculum_.tierCount(attempt.skill.view());
  if (tier_count <= 0) return;

  SkillRuntime& runtime = ensureRuntime(attempt.skill.view());
  policyForSkill(attempt.skill.view(), tier_count).observe(attempt.tier, attempt.correct);
  runtime.attempts += 1;
  observations_ += 1;
}

void AdaptiveEngine::replay(const PerformanceHistory& history) {
  reset();
  for (const auto& attempt : history.attempts()) observe(attempt);
}

std::vector<SkillState> AdaptiveEngine::skillStates() const {
  std::vector<SkillState> states;
  for (const auto& def : curriculum_.skills()) {
    const SkillRuntime* runtime = runtimeFor(def.skill.view());
    if (runtime == nullptr || runtime->attempts == 0) continue;

    if (runtime->policy == nullptr) continue;
    const int tier = runtime->policy->estimatedFlowTier();
    SkillState state;
    state.skill = def.skill;
    const DifficultyParams* params = curriculum_.paramsFor(def.skill.view(), tier);
    state.difficulty = params != nullptr ? params->difficulty : 0.0;
    state.tier = tier;
    const std::pair<double, double> evidence = runtime->policy->evidenceAt(tier);
    state.alpha = evidence.first;
    state.beta = evidence.second;
    states.push_back(std::move(state));
  }
  return states;
}

void AdaptiveEngine::loadSkillStates(const std::vector<SkillState>& states) {
  reset();
  for (const auto& state : states) {
    const int tier_count = curriculum_.tierCount(state.skill.view());
    if (tier_count <= 0) continue;

    SkillRuntime& runtime = ensureRuntime(state.skill.view());
    IDifficultyPolicy& policy = policyForSkill(state.skill.view(), tier_count);

    const int tier = std::max(0, std::min(state.tier, tier_count - 1));
    policy.setPosterior(tier, state.alpha, state.beta);
    policy.setPulls(tier, 1);  // marks the tier as visited
    policy.setLastTier(tier);
    runtime.attempts = 1;
  }
}

// ---------------------------------------------------------------------------
// Curriculum policy: which sub-skill to serve next
//
// Not a bandit decision - it is a teaching decision. Rule: introduce new
// material first, then serve whichever unlocked sub-skill sits furthest from
// the flow zone (lowest estimated success), and let spaced review interleave.
// ---------------------------------------------------------------------------
double AdaptiveEngine::skillPriority(const SkillDef& def, const PerformanceHistory& history) const {
  const SkillRuntime* runtime = runtimeFor(def.skill.view());
  if (runtime == nullptr || runtime->attempts == 0) return -1.0;  // new material first

  if (runtime->policy == nullptr) return -1.0;
  const int flow_tier = runtime->policy->estimatedFlowTier();
  const double estimated = runtime->policy->estimatedSuccess(flow_tier);
  if (estimated >= config_.mastery_success) return 2.0 + estimated;  // mastered: deprioritise

  // Rotation pressure. Once a sub-skill is tuned, its estimated success sits at
  // the target - so every converged skill would score the same and the stable
  // tie-break would serve the first one forever. Boost the skills that have
  // waited longest, which also gives the interleaving that retrieval practice
  // wants.
  const int since = history.attemptsSinceSkill(def.skill.view());
  const double staleness = std::min(
      1.0, static_cast<double>(since) / static_cast<double>(std::max(1, config_.review_interval * 2)));
  return estimated - 0.35 * staleness;
}

SubSkill AdaptiveEngine::selectSkill(const PerformanceHistory& history) const {
  const int altitude = altitudeFrom(history);
  std::vector<SubSkill> unlocked = curriculum_.unlockedAt(altitude);
  if (unlocked.empty()) {
    for (const auto& def : curriculum_.skills()) unlocked.push_back(def.skill);
  }
  if (unlocked.empty()) throw std::runtime_error("curriculum defines no sub-skills");

  SubSkill best = unlocked.front();
  double best_priority = std::numeric_limits<double>::infinity();
  for (const auto& candidate : unlocked) {
    const SkillDef* def = curriculum_.skill(candidate.view());
    if (def == nullptr) continue;
    const double priority = skillPriority(*def, history);
    // Strict '<' over a deterministically ordered list keeps ties stable.
    if (priority < best_priority - 1e-12) {
      best_priority = priority;
      best = candidate;
    }
  }
  return best;
}

LevelKind AdaptiveEngine::decideKind(const PerformanceHistory& history,
                                     std::vector<SubSkill>* review) const {
  review->clear();
  const int altitude = altitudeFrom(history);

  if (config_.checkpoint_at_zone_boundaries && curriculum_.isZoneBoundary(altitude)) {
    // Zone-boundary boss: mix the sub-skills of the zones already climbed.
    // This is spaced retrieval practice at a natural dramatic beat.
    *review = curriculum_.unlockedAt(altitude - 1);
    return LevelKind::Checkpoint;
  }

  if (config_.review_interval > 0 && history.attemptIndex() > 0 &&
      (history.attemptIndex() % config_.review_interval) == (config_.review_interval - 1)) {
    const std::vector<SubSkill> unlocked = curriculum_.unlockedAt(altitude);
    std::string oldest;
    int most_distant = -1;
    for (const auto& candidate : unlocked) {
      const int distance = history.attemptsSinceSkill(candidate.view());
      if (distance > most_distant) {
        most_distant = distance;
        oldest = candidate.id;
      }
    }
    if (!oldest.empty()) {
      review->push_back(SubSkill{oldest});
      return LevelKind::Review;
    }
  }

  return LevelKind::Practice;
}

// ---------------------------------------------------------------------------
// Spec construction
// ---------------------------------------------------------------------------
LevelSpec AdaptiveEngine::buildSpec(const PerformanceHistory& history, const SubSkill& skill,
                                    LevelKind kind, const std::vector<SubSkill>& review,
                                    int forbidden_tier, int max_tier) {
  const SkillDef* def = curriculum_.skill(skill.view());
  if (def == nullptr) throw std::runtime_error("unknown sub-skill: " + skill.id);

  const int tier_count = static_cast<int>(def->tiers.size());
  if (tier_count <= 0) throw std::runtime_error("sub-skill has no difficulty tiers: " + skill.id);

  IDifficultyPolicy& policy = policyForSkill(skill.view(), tier_count);

  // A fresh, deterministic RNG per decision: the same history always yields the
  // same tier, which is what makes the engine replayable from the log.
  Rng rng(deriveSeed({config_.root_seed,
                      static_cast<std::uint64_t>(history.childId()),
                      static_cast<std::uint64_t>(history.attemptIndex()),
                      hashString(skill.view())}));

  int tier = policy.selectTier(rng, forbidden_tier, max_tier);
  if (tier < 0) tier = 0;
  tier = std::max(0, std::min(tier, tier_count - 1));

  const DifficultyParams* params = curriculum_.paramsFor(skill.view(), tier);

  LevelSpec spec;
  spec.domain = def->domain;
  spec.skill = skill;
  spec.diff = params != nullptr ? *params : DifficultyParams{};
  spec.diff.tier = tier;
  spec.kind = kind;
  spec.review_skills = review;
  spec.child_id = history.childId();
  spec.attempt_index = history.attemptIndex();
  spec.seed = deriveSeed({config_.root_seed ^ 0x9E3779B9ull,
                          static_cast<std::uint64_t>(history.childId()),
                          static_cast<std::uint64_t>(history.attemptIndex()),
                          hashString(skill.view()),
                          static_cast<std::uint64_t>(tier),
                          static_cast<std::uint64_t>(static_cast<int>(kind))});
  return spec;
}

LevelSpec AdaptiveEngine::navigationLevel(const PerformanceHistory& history, int cap) {
  LevelSpec spec;
  spec.skill=skills::Navigation;
  spec.domain=Domain::ProblemSolving;
  spec.child_id=history.childId();
  spec.attempt_index=history.attemptIndex();
  spec.seed=deriveSeed({config_.root_seed, hashString(spec.skill.id), static_cast<unsigned>(spec.child_id),
                        static_cast<unsigned>(spec.attempt_index)});
  Rng rng(spec.seed);
  spec.diff.tier=policyForSkill(spec.skill.id,4).selectTier(rng,-1,cap);
  spec.diff.difficulty=spec.diff.tier/3.0;
  return spec;
}

LevelSpec AdaptiveEngine::nextLevel(const PerformanceHistory& history) {
  std::vector<SubSkill> review;
  const LevelKind kind = decideKind(history, &review);

  SubSkill skill = selectSkill(history);
  if (kind == LevelKind::Checkpoint && !review.empty()) {
    // The boss item is generated from whichever review skill looks weakest.
    double weakest = std::numeric_limits<double>::infinity();
    for (const auto& candidate : review) {
      const SkillRuntime* runtime = runtimeFor(candidate.view());
      const double value = (runtime == nullptr || runtime->attempts == 0 ||
                            runtime->policy == nullptr)
                               ? -1.0
                               : runtime->policy->estimatedSuccess(
                                     runtime->policy->estimatedFlowTier());
      if (value < weakest - 1e-12) {
        weakest = value;
        skill = candidate;
      }
    }
  }

  return buildSpec(history, skill, kind, review, -1, -1);
}

LevelSpec AdaptiveEngine::nextLevelForSkill(const PerformanceHistory& history,
                                            const SubSkill& skill, int max_tier) {
  if (curriculum_.skill(skill.view()) == nullptr) return nextLevel(history);
  return buildSpec(history, skill, LevelKind::Practice, {}, -1, max_tier);
}

}  // namespace sdp
