#pragma once
// ===========================================================================
// sdp/engine.hpp - the adaptive engine.
//
// Contract (unchanged from the design document):
//
//     struct LevelSpec { Domain domain; SubSkill skill;
//                        DifficultyParams diff; uint32_t seed; };
//     class AdaptiveEngine { LevelSpec nextLevel(const PerformanceHistory&); };
//
// Two decisions deserve emphasis:
//
// 1. The engine owns one bandit PER SUB-SKILL (an arm is a difficulty tier).
//    It also decides WHICH sub-skill to serve, which is a curriculum question:
//    introduce new material, otherwise serve the skill where the child is
//    currently weakest relative to the flow zone, and interleave spaced review.
//
// 2. Every decision is a pure function of (persisted history, config, root
//    seed). There is no hidden randomness: nextLevel() derives a fresh RNG from
//    the attempt index, so folding the same log twice gives the same sequence
//    of levels. That is what makes the engine replayable from the database.
// ===========================================================================

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "sdp/bandit.hpp"
#include "sdp/curriculum.hpp"
#include "sdp/types.hpp"

namespace sdp {

struct EngineConfig {
  BanditConfig bandit{};  // tier_count is overridden per skill by its table
  int review_interval = 5;         // every N attempts, insert spaced review
  double mastery_success = 0.90;   // success at the flow tier that counts as mastered
  bool checkpoint_at_zone_boundaries = true;
  std::uint32_t root_seed = 0x5D0100u;  // fixes the whole decision stream
};

class AdaptiveEngine {
 public:
  AdaptiveEngine() : AdaptiveEngine(EngineConfig{}, makeDefaultCurriculum()) {}
  AdaptiveEngine(EngineConfig config, Curriculum curriculum);

  // ---- the public contract ------------------------------------------------
  LevelSpec navigationLevel(const PerformanceHistory& history, int max_tier = -1);
  LevelSpec nextLevel(const PerformanceHistory& history);

  /// Retry path: re-offer the same sub-skill at a tier no harder than
  /// max_tier (the session passes "one step below the tier that just failed",
  /// so a retry is always gentler or equal). Never returns a tier above the
  /// cap, and falls back to nextLevel() for an unknown skill.
  LevelSpec nextLevelForSkill(const PerformanceHistory& history, const SubSkill& skill,
                              int max_tier = -1);

  /// Fold one observed attempt into the posteriors (the bandit update step).
  void observe(const Attempt& attempt);

  /// Rebuild all internal state from an attempts log. Deterministic, and the
  /// reason the game can resume from SQLite without storing engine internals.
  void replay(const PerformanceHistory& history);

  // ---- persistence / inspection ------------------------------------------
  std::vector<SkillState> skillStates() const;
  void loadSkillStates(const std::vector<SkillState>& states);
  void reset();

  const Curriculum& curriculum() const noexcept { return curriculum_; }
  const EngineConfig& config() const noexcept { return config_; }
  /// The policy in use for a sub-skill (nullptr before it is first needed).
  const IDifficultyPolicy* policyFor(std::string_view skill) const;
  IDifficultyPolicy* policyFor(std::string_view skill);
  int observations() const noexcept { return observations_; }

  /// Altitude implied by a log (1 solved problem = 1 step). Identical rule to
  /// ProgressionService, kept in one place so the two can never disagree.
  static int altitudeFrom(const PerformanceHistory& history) noexcept;

 private:
  struct SkillRuntime {
    std::unique_ptr<IDifficultyPolicy> policy;
    BanditConfig applied{};   // the config the current policy was built with
    bool configured = false;
    int attempts = 0;
  };

  /// Returns a policy that is guaranteed to be built from config_.bandit.
  ///
  /// This exists because a "does the tier count match?" test is NOT a valid
  /// freshness check: every shipped skill happens to have 12 tiers, so the
  /// engine silently kept its default-constructed policy and ignored the
  /// configured kind completely. The simulation harness caught it, because
  /// every arm produced identical numbers; see docs/simulation.md.
  IDifficultyPolicy& policyForSkill(std::string_view skill, int tier_count);

  SkillRuntime& ensureRuntime(std::string_view skill);   // creates on demand
  const SkillRuntime* runtimeFor(std::string_view skill) const;  // never inserts
  SubSkill selectSkill(const PerformanceHistory& history) const;
  double skillPriority(const SkillDef& def, const PerformanceHistory& history) const;
  LevelKind decideKind(const PerformanceHistory& history, std::vector<SubSkill>* review) const;
  LevelSpec buildSpec(const PerformanceHistory& history, const SubSkill& skill, LevelKind kind,
                      const std::vector<SubSkill>& review, int forbidden_tier, int max_tier);

  EngineConfig config_{};
  Curriculum curriculum_{};
  std::map<std::string, SkillRuntime, std::less<>> runtimes_;
  int observations_ = 0;
};

}  // namespace sdp
