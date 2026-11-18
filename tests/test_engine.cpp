#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "sdp/engine.hpp"
#include "test_framework.hpp"

using namespace sdp;

namespace {

Attempt attemptFor(const LevelSpec& spec, bool correct) {
  Attempt attempt;
  attempt.child_id = spec.child_id != 0 ? spec.child_id : 1;
  attempt.domain = spec.domain;
  attempt.skill = spec.skill;
  attempt.difficulty = spec.diff.difficulty;
  attempt.tier = spec.diff.tier;
  attempt.correct = correct;
  attempt.response_ms = 2500;
  return attempt;
}

void fold(AdaptiveEngine& engine, PerformanceHistory& history, const LevelSpec& spec,
          bool correct) {
  const Attempt attempt = attemptFor(spec, correct);
  history.add(attempt);
  engine.observe(attempt);
}

}  // namespace

SDP_TEST(engine, next_level_respects_the_documented_contract) {
  AdaptiveEngine engine;
  PerformanceHistory history(1);
  const LevelSpec spec = engine.nextLevel(history);

  CHECK(!spec.skill.empty());
  CHECK(spec.seed != 0u);
  CHECK(spec.diff.tier >= 0);
  CHECK(spec.diff.tier < engine.curriculum().tierCount(spec.skill.view()));
  CHECK_EQ(spec.child_id, 1);
  CHECK_EQ(spec.attempt_index, 0);
  CHECK(spec.kind == LevelKind::Practice);
}

SDP_TEST(engine, identical_history_yields_identical_levels) {
  AdaptiveEngine a;
  AdaptiveEngine b;
  PerformanceHistory ha(1);
  PerformanceHistory hb(1);

  for (int i = 0; i < 40; ++i) {
    const LevelSpec sa = a.nextLevel(ha);
    const LevelSpec sb = b.nextLevel(hb);
    CHECK_EQ(sa.skill.id, sb.skill.id);
    CHECK_EQ(sa.diff.tier, sb.diff.tier);
    CHECK_EQ(sa.seed, sb.seed);
    const bool correct = (i % 4) != 0;
    fold(a, ha, sa, correct);
    fold(b, hb, sb, correct);
  }
}

SDP_TEST(engine, replay_from_the_log_equals_incremental_folding) {
  AdaptiveEngine live;
  PerformanceHistory history(1);
  for (int i = 0; i < 45; ++i) {
    const LevelSpec spec = live.nextLevel(history);
    fold(live, history, spec, (i % 3) != 0);
  }

  AdaptiveEngine resumed;
  resumed.replay(history);

  CHECK_EQ(resumed.observations(), live.observations());
  const LevelSpec from_live = live.nextLevel(history);
  const LevelSpec from_resumed = resumed.nextLevel(history);
  CHECK_EQ(from_live.skill.id, from_resumed.skill.id);
  CHECK_EQ(from_live.diff.tier, from_resumed.diff.tier);
  CHECK_EQ(from_live.seed, from_resumed.seed);
}

SDP_TEST(engine, altitude_counts_solved_problems_only) {
  PerformanceHistory history(1);
  for (int i = 0; i < 10; ++i) {
    Attempt attempt;
    attempt.child_id = 1;
    attempt.skill = skills::Counting;
    attempt.correct = (i % 2 == 0);
    history.add(attempt);
  }
  CHECK_EQ(AdaptiveEngine::altitudeFrom(history), 5);
}

SDP_TEST(engine, the_configured_policy_is_actually_applied) {
  // Regression: the engine used to decide whether a bandit needed (re)building
  // by comparing tier counts. Every shipped skill has 12 tiers, so the check
  // never fired and the configured policy was silently ignored.
  EngineConfig config;
  config.bandit.kind = PolicyKind::Static;
  config.bandit.static_tier = 5;
  AdaptiveEngine engine(config, makeDefaultCurriculum());

  PerformanceHistory history(1);
  for (int i = 0; i < 30; ++i) {
    const LevelSpec spec = engine.nextLevel(history);
    CHECK_EQ(spec.diff.tier, 5);  // a static policy must never move
    fold(engine, history, spec, (i % 3) != 0);
  }

  EngineConfig other;
  other.bandit.kind = PolicyKind::Static;
  other.bandit.static_tier = 9;
  AdaptiveEngine second(other, makeDefaultCurriculum());
  PerformanceHistory other_history(1);
  for (int i = 0; i < 10; ++i) {
    const LevelSpec spec = second.nextLevel(other_history);
    CHECK_EQ(spec.diff.tier, 9);
    fold(second, other_history, spec, true);
  }
}

SDP_TEST(engine, practice_is_spread_across_sub_skills) {
  // Regression: with equal priorities the deterministic tie-break served the
  // first sub-skill in curriculum order almost exclusively.
  AdaptiveEngine engine;
  PerformanceHistory history(1);
  std::map<std::string, int> counts;
  for (int i = 0; i < 60; ++i) {
    const LevelSpec spec = engine.nextLevel(history);
    counts[spec.skill.id] += 1;
    fold(engine, history, spec, (i % 4) != 0);
  }

  CHECK(counts.size() >= 3);           // several sub-skills get attention
  int worst = 0;
  for (const auto& entry : counts) worst = std::max(worst, entry.second);
  CHECK(worst <= 40);                  // no single skill monopolises practice
}

SDP_TEST(engine, zone_boundary_produces_a_checkpoint) {
  AdaptiveEngine engine;
  PerformanceHistory history(1);
  for (int i = 0; i < 30; ++i) {
    const LevelSpec spec = engine.nextLevel(history);
    CHECK(spec.kind != LevelKind::Checkpoint);  // only at the boundary itself
    fold(engine, history, spec, true);
  }
  CHECK_EQ(AdaptiveEngine::altitudeFrom(history), 30);

  const LevelSpec boss = engine.nextLevel(history);
  CHECK(boss.kind == LevelKind::Checkpoint);
  CHECK(!boss.review_skills.empty());
}

SDP_TEST(engine, spaced_review_is_interleaved) {
  AdaptiveEngine engine;
  PerformanceHistory history(1);
  bool saw_review = false;
  for (int i = 0; i < 5; ++i) {
    const LevelSpec spec = engine.nextLevel(history);
    if (spec.kind == LevelKind::Review) saw_review = true;
    fold(engine, history, spec, true);
  }
  CHECK(saw_review);  // review_interval == 5
}

SDP_TEST(engine, retry_is_never_harder_than_the_failed_tier) {
  // The session asks for "one step easier than what just failed"; the engine
  // must never hand back something harder.
  AdaptiveEngine engine;
  PerformanceHistory history(1);
  for (int i = 0; i < 60; ++i) {
    const LevelSpec spec = engine.nextLevelForSkill(history, skills::Addition, 4);
    CHECK(spec.diff.tier <= 4);
    fold(engine, history, spec, false);  // the child keeps struggling
  }
}

SDP_TEST(engine, skill_states_reflect_evidence) {
  AdaptiveEngine engine;
  PerformanceHistory history(1);
  for (int i = 0; i < 25; ++i) {
    const LevelSpec spec = engine.nextLevel(history);
    fold(engine, history, spec, true);
  }
  const std::vector<SkillState> states = engine.skillStates();
  CHECK(!states.empty());
  for (const auto& state : states) {
    CHECK(state.alpha + state.beta > 2.0);
    CHECK(state.tier >= 0);
    CHECK(state.difficulty >= 0.0);
  }
}

SDP_TEST(engine, resume_from_stored_states_is_usable) {
  AdaptiveEngine first;
  PerformanceHistory history(1);
  for (int i = 0; i < 12; ++i) {
    const LevelSpec spec = first.nextLevel(history);
    fold(first, history, spec, (i % 2) == 0);
  }

  AdaptiveEngine resumed;
  resumed.loadSkillStates(first.skillStates());
  const LevelSpec spec = resumed.nextLevel(history);
  CHECK(!spec.skill.empty());
  CHECK(spec.diff.tier >= 0);
  CHECK(spec.diff.tier < resumed.curriculum().tierCount(spec.skill.view()));
}

SDP_TEST(curriculum, default_curriculum_is_consistent) {
  const Curriculum curriculum = makeDefaultCurriculum();
  std::string error;
  CHECK(curriculum.validate(&error));

  CHECK_EQ(curriculum.zoneCount(), 4);
  CHECK_EQ(curriculum.unlockedAt(0).size(), static_cast<std::size_t>(4));
  CHECK(curriculum.unlockedAt(30).size() > curriculum.unlockedAt(0).size());
  CHECK_EQ(curriculum.zoneForAltitude(0)->key, std::string("forest"));
  CHECK_EQ(curriculum.zoneForAltitude(95)->key, std::string("summit"));
  CHECK(curriculum.isZoneBoundary(30));
  CHECK(!curriculum.isZoneBoundary(31));
  CHECK_EQ(curriculum.zoneBoundaryAbove(0), 30);
  CHECK_EQ(curriculum.zoneBoundaryAbove(90), -1);

  for (const auto& def : curriculum.skills()) {
    CHECK_EQ(static_cast<int>(def.tiers.size()), 12);
  }

  const DifficultyParams* easy = curriculum.paramsFor("math.addition", 0);
  const DifficultyParams* hard = curriculum.paramsFor("math.addition", 11);
  CHECK(easy != nullptr && hard != nullptr);
  CHECK(hard->max_value > easy->max_value);
  CHECK(!easy->allow_carry);
  CHECK(hard->allow_carry);
  CHECK_EQ(curriculum.tierCount("does.not.exist"), 0);
}
