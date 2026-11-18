#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "sdp/store.hpp"
#include "../shell/src/family_report.hpp"
#include "test_framework.hpp"

using namespace sdp;

namespace {

/// One suite, run against every IDataStore implementation. Identical behaviour
/// across backends is what lets the simulation use the in-memory store with
/// confidence.
void exerciseStore(IDataStore& store) {
  const int child = store.createChild("Ada", 8);
  CHECK(child > 0);

  const auto profile = store.child(child);
  CHECK(profile.has_value());
  CHECK_EQ(profile->name, std::string("Ada"));
  CHECK_EQ(profile->age, 8);
  CHECK(!store.child(child + 999).has_value());
  CHECK_EQ(store.children().size(), static_cast<std::size_t>(1));

  const int sibling=store.createChild("Robin",7);
  store.recordPlay(sibling,"Bracken Rise",17);
  store.recordPlay(sibling,"Bracken Rise",9);
  CHECK_EQ(store.areaPlay(sibling).at(0).seconds,26);
  CHECK(store.areaPlay(child).empty());
  store.saveCamp(sibling,"Bracken Rise");
  CHECK_EQ(store.savedCamp(sibling),std::string("Bracken Rise"));
  CHECK(store.savedCamp(child).empty());
  store.resetProgress(sibling);
  CHECK(store.savedCamp(sibling).empty());
  CHECK(store.areaPlay(sibling).empty());
  store.recordPlay(sibling,"Willow Bay",11);
  store.deleteChild(sibling);
  CHECK(store.areaPlay(sibling).empty());

  Attempt attempt;
  attempt.child_id = child;
  attempt.domain = Domain::Math;
  attempt.skill = skills::Addition;
  attempt.difficulty = 0.3;
  attempt.tier = 4;
  attempt.correct = true;
  attempt.response_ms = 4200;
  store.logAttempt(attempt);

  Attempt wrong = attempt;
  wrong.correct = false;
  wrong.tier = 3;
  store.logAttempt(wrong);

  const PerformanceHistory history = store.history(child);
  CHECK_EQ(history.size(), static_cast<std::size_t>(2));
  CHECK_EQ(history.correctCount(), 1);
  CHECK_EQ(history[0].tier, 4);  // recovered from spec_json
  CHECK_EQ(history[1].tier, 3);
  CHECK(!history[0].spec_json.empty());
  CHECK(!history[0].created_at.empty());
  CHECK_EQ(store.attemptCount(child), 2);

  // checkpoint flag round-trips through spec_json too
  Attempt boss = attempt;
  boss.checkpoint = true;
  boss.tier = 7;
  store.logAttempt(boss);
  const PerformanceHistory with_boss = store.history(child);
  CHECK_EQ(with_boss.size(), static_cast<std::size_t>(3));
  CHECK(with_boss[2].checkpoint);
  CHECK_EQ(with_boss[2].tier, 7);

  // limit keeps the newest attempts, returned oldest-first
  const PerformanceHistory tail = store.history(child, 2);
  CHECK_EQ(tail.size(), static_cast<std::size_t>(2));
  CHECK(tail[1].checkpoint);

  // skill_state is an upsert
  SkillState state;
  state.skill = skills::Addition;
  state.difficulty = 0.42;
  state.tier = 5;
  state.alpha = 7.5;
  state.beta = 2.5;
  store.saveSkillState(child, state);
  state.difficulty = 0.55;
  state.tier = 6;
  state.alpha = 3.0;
  state.beta = 4.0;
  store.saveSkillState(child, state);

  const std::vector<SkillState> states = store.skillStates(child);
  CHECK_EQ(states.size(), static_cast<std::size_t>(1));
  CHECK_NEAR(states[0].alpha, 3.0, 1e-9);
  CHECK_NEAR(states[0].beta, 4.0, 1e-9);
  CHECK_NEAR(states[0].difficulty, 0.55, 1e-9);

  // progression is an upsert
  ProgressionState progress;
  progress.child_id = child;
  progress.altitude = 12;
  progress.zone_id = 1;
  store.saveProgression(progress);
  progress.altitude = 13;
  store.saveProgression(progress);

  const auto loaded = store.progression(child);
  CHECK(loaded.has_value());
  CHECK_EQ(loaded->altitude, 13);
  CHECK_EQ(loaded->zone_id, 1);

  // milestones are idempotent
  store.awardMilestone(child, "first_step");
  store.awardMilestone(child, "first_step");
  store.awardMilestone(child, "altitude_10");
  CHECK_EQ(store.milestones(child).size(), static_cast<std::size_t>(2));
  CHECK(store.hasMilestone(child, "altitude_10"));
  CHECK(!store.hasMilestone(child, "summit"));

  // analytics: the design document's 7-day success-rate query
  const std::vector<SkillSuccessRate> rates = store.successRates(child, 7);
  CHECK_EQ(rates.size(), static_cast<std::size_t>(1));
  CHECK_EQ(rates[0].n, 3);
  CHECK_NEAR(rates[0].success_rate, 2.0 / 3.0, 1e-6);
  CHECK_EQ(rates[0].skill.id, std::string("math.addition"));

  // anonymised export: numbers only, no child name
  const std::string csv = store.exportAttemptsCsv(child);
  CHECK(csv.find("sub_skill") != std::string::npos);
  CHECK(csv.find("Ada") == std::string::npos);

  // reset clears history but keeps the profile
  store.resetProgress(child);
  CHECK_EQ(store.attemptCount(child), 0);
  CHECK(!store.progression(child).has_value());
  CHECK(store.milestones(child).empty());
  CHECK(store.skillStates(child).empty());
  CHECK(store.child(child).has_value());

  store.deleteChild(child);
  CHECK(!store.child(child).has_value());
}

}  // namespace

SDP_TEST(store, memory_store_behaves_like_the_real_thing) {
  auto store = openMemoryStore();
  exerciseStore(*store);
}

SDP_TEST(store, sqlite_in_memory_database) {
  auto store = openSqliteStore(":memory:");
  exerciseStore(*store);
}

SDP_TEST(store, sqlite_file_database_survives_reopen) {
  const std::string path = "sdp100_test_store.db";
  std::remove(path.c_str());

  int child = 0;
  {
    auto store = openSqliteStore(path);
    child = store->createChild("Bo", 7);
    Attempt attempt;
    attempt.child_id = child;
    attempt.skill = skills::Patterns;
    attempt.domain = Domain::ProblemSolving;
    attempt.tier = 2;
    attempt.correct = true;
    attempt.response_ms = 1500;
    store->logAttempt(attempt);
    ProgressionState progress;
    progress.child_id = child;
    progress.altitude = 1;
    progress.zone_id = 0;
    store->saveProgression(progress);
  }

  {
    auto store = openSqliteStore(path);
    CHECK_EQ(store->attemptCount(child), 1);
    const PerformanceHistory history = store->history(child);
    CHECK_EQ(history.size(), static_cast<std::size_t>(1));
    CHECK_EQ(history[0].tier, 2);
    const auto progress = store->progression(child);
    CHECK(progress.has_value());
    CHECK_EQ(progress->altitude, 1);
    store->deleteChild(child);
  }

  std::remove(path.c_str());
}

SDP_TEST(store, seven_day_window_excludes_old_attempts) {
  auto store = openMemoryStore();
  const int child = store->createChild("Cy", 9);

  Attempt recent;
  recent.child_id = child;
  recent.skill = skills::Counting;
  recent.correct = true;
  store->logAttempt(recent);  // no timestamp => filled with "now"

  Attempt ancient;
  ancient.child_id = child;
  ancient.skill = skills::Counting;
  ancient.correct = false;
  ancient.created_at = "2001-01-01 00:00:00";
  store->logAttempt(ancient);

  CHECK_EQ(store->attemptCount(child), 2);
  const auto window = store->successRates(child, 7);
  CHECK_EQ(window.size(), static_cast<std::size_t>(1));
  CHECK_EQ(window[0].n, 1);
  CHECK_NEAR(window[0].success_rate, 1.0, 1e-9);

  const auto everything = store->successRates(child, 20000);
  CHECK_EQ(everything.size(), static_cast<std::size_t>(1));
  CHECK_EQ(everything[0].n, 2);

  CHECK_EQ(store->historyWithin(child, 7).size(), static_cast<std::size_t>(1));
  CHECK_EQ(store->history(child).size(), static_cast<std::size_t>(2));
}

SDP_TEST(store, family_report_keeps_each_climbers_activity_separate) {
  auto store = openMemoryStore();
  const int ada = store->createChild("Ada", 8);
  const int robin = store->createChild("Robin", 9);
  store->recordPlay(ada, "Willow Bay", 40);
  store->recordPlay(robin, "Glasswind Hollow", 80);
  store->awardMilestone(ada, "bay.pennant.raised");
  Attempt answer;
  answer.child_id = ada;
  answer.skill = skills::Counting;
  answer.correct = true;
  store->logAttempt(answer);
  answer.correct = false;
  store->logAttempt(answer);
  const auto report = trail::FamilyReport::read(*store, ada, {});
  CHECK_EQ(report.seconds, 40);
  CHECK_EQ(report.skills.size(), std::size_t(1));
  CHECK_EQ(report.skills.front().correct, 1);
  CHECK_EQ(report.attempts, 2);
  CHECK_EQ(report.events.size(), std::size_t(4));
  const auto sibling = trail::FamilyReport::read(*store, robin, {});
  CHECK_EQ(sibling.seconds, 80);
  CHECK_EQ(sibling.attempts, 0);
  CHECK_EQ(sibling.events.size(), std::size_t(1));
}
