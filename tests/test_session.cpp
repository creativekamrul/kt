#include <memory>
#include <string>
#include <vector>

#include "sdp/content.hpp"
#include "sdp/engine.hpp"
#include "sdp/generator.hpp"
#include "sdp/progression.hpp"
#include "sdp/session.hpp"
#include "sdp/mountain.hpp"
#include "sdp/store.hpp"
#include "test_framework.hpp"

using namespace sdp;

namespace {

/// One assembled game, wired exactly as apps/cli and apps/simharness wire it.
struct Harness {
  Curriculum curriculum;
  LevelGenerator generator;
  AdaptiveEngine engine;
  std::unique_ptr<IDataStore> store;
  ProgressionService progression;
  SessionRunner runner;

  Harness(const Curriculum& c, std::unique_ptr<IDataStore> s)
      : curriculum(c),
        generator(c),
        engine(EngineConfig{}, c),
        store(std::move(s)),
        progression(curriculum),
        runner(engine, generator, progression, *store) {}

  int newChild() { return store->createChild("Kid", 8); }

  static int wrongIndex(const Puzzle& puzzle) {
    const int correct = puzzle.correctIndex();
    const int count = static_cast<int>(puzzle.options.size());
    return count > 1 ? (correct + 1) % count : -1;
  }
};

/// A puzzle must always be answerable: at least two options, exactly one correct.
void CHECK_CONSTRAINT(const Puzzle& puzzle) { CHECK(puzzle.wellFormed()); }

double successRate(const PerformanceHistory& history) {
  if (history.empty()) return 0.0;
  return static_cast<double>(history.correctCount()) / static_cast<double>(history.size());
}

}  // namespace

SDP_TEST(session, a_day_of_climbing_logs_every_attempt) {
  Harness harness(makeDefaultCurriculum(), openMemoryStore());
  const int child = harness.newChild();

  AdaptiveEngine engine(EngineConfig{}, harness.curriculum);
  LevelGenerator generator(harness.curriculum);
  ProgressionService progression(harness.curriculum);
  SessionConfig config;
  config.length = 12;
  SessionRunner runner(engine, generator, progression, *harness.store, config);

  runner.startSession(child);
  int wrong = 0;
  for (int i = 0; i < 12; ++i) {
    const Puzzle puzzle = runner.nextPuzzle();
    const bool make_wrong = (i % 3) == 1;
    if (make_wrong) ++wrong;
    runner.submitAnswer(make_wrong ? Harness::wrongIndex(puzzle) : puzzle.correctIndex(), 2000 + i);
  }

  CHECK_EQ(harness.store->attemptCount(child), 12);
  const auto progress = harness.store->progression(child);
  CHECK(progress.has_value());
  CHECK_EQ(progress->altitude, 12 - wrong);

  CHECK(runner.complete());
  const SessionSummary summary = runner.summary();
  CHECK_EQ(summary.attempted, 12);
  CHECK_EQ(summary.solved, 12 - wrong);
  CHECK_NEAR(summary.success_rate, static_cast<double>(12 - wrong) / 12.0, 1e-9);
  CHECK_EQ(summary.altitude_end, 12 - wrong);
  CHECK(!summary.headline.empty());
  CHECK_EQ(summary.child_id, child);

  CHECK(!harness.store->skillStates(child).empty());
  CHECK(harness.store->hasMilestone(child, "first_step"));

  const PerformanceHistory log = harness.store->history(child);
  CHECK_EQ(log.size(), static_cast<std::size_t>(12));
  CHECK_EQ(log.correctCount(), 12 - wrong);
  for (const auto& attempt : log) {
    CHECK(!attempt.spec_json.empty());
    CHECK(!attempt.created_at.empty());
    CHECK(attempt.response_ms >= 2000);
  }
}

SDP_TEST(session, wrong_answers_never_lower_altitude) {
  Harness harness(makeDefaultCurriculum(), openMemoryStore());
  const int child = harness.newChild();
  harness.runner.startSession(child);

  for (int i = 0; i < 8; ++i) {
    const Puzzle puzzle = harness.runner.nextPuzzle();
    harness.runner.submitAnswer(puzzle.correctIndex(), 1500);
  }
  const int altitude_after_successes = harness.store->progression(child)->altitude;
  CHECK_EQ(altitude_after_successes, 8);

  for (int i = 0; i < 15; ++i) {
    const Puzzle puzzle = harness.runner.nextPuzzle();
    const SessionResult result = harness.runner.submitAnswer(Harness::wrongIndex(puzzle), 1500);
    CHECK(!result.correct);
    CHECK_EQ(result.altitude, altitude_after_successes);
    CHECK(!result.altitude_gained);
  }
  CHECK_EQ(harness.store->progression(child)->altitude, altitude_after_successes);
}

SDP_TEST(session, a_wrong_answer_offers_an_easier_retry_on_the_same_skill) {
  Harness harness(makeDefaultCurriculum(), openMemoryStore());
  const int child = harness.newChild();
  harness.runner.startSession(child);

  const Puzzle puzzle = harness.runner.nextPuzzle();
  const SubSkill skill = puzzle.spec.skill;
  const int failed_tier = puzzle.spec.diff.tier;

  const SessionResult result = harness.runner.submitAnswer(Harness::wrongIndex(puzzle), 4000);
  CHECK(!result.correct);
  CHECK(result.offer_retry);
  CHECK(harness.runner.retryPending());

  const Puzzle retry = harness.runner.nextPuzzle();
  CHECK_EQ(retry.spec.skill.id, skill.id);          // same sub-skill
  CHECK(retry.spec.diff.tier < failed_tier);        // strictly gentler
  CHECK(!harness.runner.retryPending());

  const SessionResult second = harness.runner.submitAnswer(retry.correctIndex(), 3000);
  CHECK(second.correct);
  CHECK_EQ(harness.store->progression(child)->altitude, 1);
}

SDP_TEST(session, retries_never_cascade) {
  // Failing everything used to drive the tier down one step per attempt,
  // because each retry became the baseline for the next one. The retry is now
  // anchored to the practice tier, so two retries can never be adjacent.
  Harness harness(makeDefaultCurriculum(), openMemoryStore());
  const int child = harness.newChild();
  harness.runner.startSession(child);

  bool previous_was_retry = false;
  int retries = 0;
  for (int i = 0; i < 40; ++i) {
    const Puzzle puzzle = harness.runner.nextPuzzle();
    const SessionResult result = harness.runner.submitAnswer(Harness::wrongIndex(puzzle), 900);
    CHECK(!(result.retry && previous_was_retry));
    if (result.retry) ++retries;
    previous_was_retry = result.retry;
  }
  CHECK(retries > 0);        // the retry path is exercised
  CHECK(retries <= 20);      // at most one retry per practice item
}

SDP_TEST(session, an_offered_retry_can_be_declined) {
  Harness harness(makeDefaultCurriculum(), openMemoryStore());
  const int child = harness.newChild();
  harness.runner.startSession(child);

  const Puzzle puzzle = harness.runner.nextPuzzle();
  harness.runner.submitAnswer(Harness::wrongIndex(puzzle), 1000);
  CHECK(harness.runner.retryPending());

  harness.runner.cancelRetry();  // the child chose "no, let us move on"
  CHECK(!harness.runner.retryPending());

  const Puzzle next = harness.runner.nextPuzzle();
  CHECK(next.wellFormed());
  CHECK_CONSTRAINT(next);
}

SDP_TEST(session, retries_can_be_disabled) {
  Harness harness(makeDefaultCurriculum(), openMemoryStore());
  const int child = harness.newChild();

  AdaptiveEngine engine(EngineConfig{}, harness.curriculum);
  LevelGenerator generator(harness.curriculum);
  ProgressionService progression(harness.curriculum);
  SessionConfig config;
  config.length = 6;
  config.allow_retry = false;
  SessionRunner runner(engine, generator, progression, *harness.store, config);

  runner.startSession(child);
  const Puzzle puzzle = runner.nextPuzzle();
  const SessionResult result = runner.submitAnswer(Harness::wrongIndex(puzzle), 1000);
  CHECK(!result.offer_retry);
  CHECK(!runner.retryPending());
}

SDP_TEST(session, whole_sessions_are_reproducible) {
  // Two independent games, same content and same answers, must produce
  // byte-identical logs. This is the integration-level proof of the
  // "deterministic and replayable" claim that the report leans on.
  Harness first(makeDefaultCurriculum(), openMemoryStore());
  Harness second(makeDefaultCurriculum(), openMemoryStore());
  const int child_a = first.newChild();
  const int child_b = second.newChild();
  CHECK_EQ(child_a, child_b);

  first.runner.startSession(child_a);
  second.runner.startSession(child_b);

  Rng rng_a(4242u);
  Rng rng_b(4242u);

  for (int i = 0; i < 30; ++i) {
    const Puzzle puzzle_a = first.runner.nextPuzzle();
    const Puzzle puzzle_b = second.runner.nextPuzzle();

    CHECK_EQ(puzzle_a.prompt, puzzle_b.prompt);
    CHECK_EQ(puzzle_a.answer, puzzle_b.answer);
    CHECK_EQ(puzzle_a.spec.skill.id, puzzle_b.spec.skill.id);
    CHECK_EQ(puzzle_a.spec.diff.tier, puzzle_b.spec.diff.tier);
    CHECK_EQ(puzzle_a.spec.seed, puzzle_b.spec.seed);

    const bool want_correct = rng_a.nextDouble() < 0.7;
    (void)rng_b.nextDouble();
    const int answer_a = want_correct ? puzzle_a.correctIndex() : Harness::wrongIndex(puzzle_a);
    const int answer_b = want_correct ? puzzle_b.correctIndex() : Harness::wrongIndex(puzzle_b);

    const SessionResult result_a = first.runner.submitAnswer(answer_a, 1000 + i);
    const SessionResult result_b = second.runner.submitAnswer(answer_b, 1000 + i);
    CHECK_EQ(result_a.correct, result_b.correct);
    CHECK_EQ(result_a.altitude, result_b.altitude);
  }

  const PerformanceHistory log_a = first.store->history(child_a);
  const PerformanceHistory log_b = second.store->history(child_b);
  CHECK_EQ(log_a.size(), log_b.size());
  CHECK_EQ(log_a.size(), static_cast<std::size_t>(30));
  for (std::size_t i = 0; i < log_a.size(); ++i) {
    CHECK_EQ(log_a[i].skill.id, log_b[i].skill.id);
    CHECK_EQ(log_a[i].tier, log_b[i].tier);
    CHECK_EQ(log_a[i].correct, log_b[i].correct);
    CHECK_EQ(log_a[i].spec_json, log_b[i].spec_json);
  }
  CHECK_NEAR(successRate(log_a), successRate(log_b), 1e-12);
}

SDP_TEST(session, a_session_resumes_from_sqlite_after_a_restart) {
  const std::string path = "sdp100_session_test.db";
  std::remove(path.c_str());

  Curriculum curriculum = makeDefaultCurriculum();
  int child = 0;

  {
    auto store = openSqliteStore(path);
    child = store->createChild("Ada", 8);
    LevelGenerator generator(curriculum);
    AdaptiveEngine engine(EngineConfig{}, curriculum);
    ProgressionService progression(curriculum);
    SessionConfig config;
    config.length = 8;
    SessionRunner runner(engine, generator, progression, *store, config);

    runner.startSession(child);
    for (int i = 0; i < 8; ++i) {
      const Puzzle puzzle = runner.nextPuzzle();
      runner.submitAnswer(puzzle.correctIndex(), 1500);
    }
    CHECK_EQ(store->progression(child)->altitude, 8);
  }

  {
    // A brand-new process: nothing in memory, everything from SQLite.
    auto store = openSqliteStore(path);
    LevelGenerator generator(curriculum);
    AdaptiveEngine engine(EngineConfig{}, curriculum);
    ProgressionService progression(curriculum);
    SessionConfig config;
    config.length = 8;
    SessionRunner runner(engine, generator, progression, *store, config);

    runner.startSession(child);
    CHECK_EQ(runner.progressionState().altitude, 8);
    CHECK_EQ(runner.progressionState().zone_id, 0);

    const Puzzle puzzle = runner.nextPuzzle();
    CHECK(puzzle.wellFormed());
    CHECK_EQ(puzzle.spec.child_id, child);
    CHECK_EQ(puzzle.spec.attempt_index, 8);  // the log drove the decision
    CHECK(puzzle.spec.diff.tier >= 0);
    CHECK(puzzle.spec.diff.tier < curriculum.tierCount(puzzle.spec.skill.view()));

    const SessionResult result = runner.submitAnswer(puzzle, puzzle.correctIndex(), 1200);
    CHECK(result.correct);
    CHECK_EQ(store->progression(child)->altitude, 9);
    CHECK_EQ(store->attemptCount(child), 9);
    store->deleteChild(child);
  }

  std::remove(path.c_str());
}

SDP_TEST(session, warmup_gestures_are_gentle_for_a_brand_new_child) {
  // The first item a six-year-old ever sees must not be tier 11.
  Harness harness(makeDefaultCurriculum(), openMemoryStore());
  const int child = harness.newChild();
  harness.runner.startSession(child);

  const Puzzle first = harness.runner.nextPuzzle();
  CHECK(first.spec.diff.tier <= 3);
  CHECK(first.wellFormed());
  CHECK(first.options.size() >= 2);
}

SDP_TEST(session, a_display_refresh_keeps_the_puzzle_and_a_new_day_keeps_altitude) {
  Harness game(makeDefaultCurriculum(), openMemoryStore());
  const int player = game.newChild();
  game.runner.startSession(player);
  const Puzzle first = game.runner.nextPuzzle();
  CHECK_EQ(game.runner.nextPuzzle().spec_json, first.spec_json);
  CHECK_EQ(game.runner.puzzlesServed(), 1);
  while (!game.runner.complete()) {
    const Puzzle puzzle = game.runner.nextPuzzle();
    game.runner.submitAnswer(puzzle, puzzle.correctIndex(), 500);
  }
  const auto day = game.runner.summary();
  game.runner.startSession(player);
  CHECK_EQ(game.runner.puzzlesServed(), 0);
  CHECK_EQ(game.runner.summary().altitude_start, day.altitude_end);
  const Puzzle next = game.runner.nextPuzzle();
  const auto result = game.runner.submitAnswer(next, Harness::wrongIndex(next), 500);
  CHECK(!result.landmark_reached);
  CHECK_EQ(result.altitude, day.altitude_end);
}

SDP_TEST(session, a_generated_mountain_campaign_reaches_the_summit_and_restores_navigation) {
  Harness h(makeDefaultCurriculum(),openSqliteStore(":memory:"));
  const int child=h.newChild();
  SessionRunner runner(h.engine,h.generator,h.progression,*h.store,{10,true,true});
  int routes=0, academic=0, checkpoints=0;
  while (!h.store->progression(child) || h.store->progression(child)->altitude<90) {
    runner.startSession(child);
    while(!runner.complete()) {
      const auto p=runner.nextPuzzle();
      CHECK(p.wellFormed());
      const auto terrain=MountainTrail::generate(p);
      CHECK_EQ(terrain.markers.size(),p.options.size());
      const float bank=terrain.creekY(210)+9.f;
      CHECK_NEAR(terrain.move({210,bank},0,-2,false).y,bank,.01);
      const float log=float(terrain.crossings.front());
      CHECK(terrain.move({log,terrain.creekY(log)+9.f},0,-2,false).y < terrain.creekY(log)+9.f);
      CHECK_EQ(terrain.move({terrain.rope.x,170},0,-4,false).y,170.f);
      CHECK(terrain.move({terrain.rope.x,170},0,-4,true).y<170);
      if(p.route.empty()){
        ++academic;
        if(p.is_checkpoint)++checkpoints;
        CHECK(runner.submitAnswer(p.correctIndex(),600).correct);
      } else {
        ++routes;
        CHECK(runner.submitRoute(p.route,950).correct);
      }
    }
  }
  CHECK(routes>0 && academic>0 && checkpoints>0);
  const int count=h.engine.policyFor(skills::Navigation.id)->totalPulls();
  runner.startSession(child);
  CHECK_EQ(h.engine.policyFor(skills::Navigation.id)->totalPulls(),count);
  CHECK_EQ(runner.progressionState().altitude,90);
  CHECK(h.store->hasMilestone(child,"zone_summit"));
  CHECK_NEAR(rulesForTrailTier(0).randomSuccess(),0.5,1e-12);
  CHECK_NEAR(rulesForTrailTier(3).randomSuccess(),1.0/24,1e-12);
}
SDP_TEST(session, a_mistied_anchor_keeps_altitude_and_offers_a_gentle_route) {
  Harness h(makeDefaultCurriculum(),openMemoryStore());
  SessionRunner runner(h.engine,h.generator,h.progression,*h.store,{10,true,true});
  runner.startSession(h.newChild());
  const auto p=runner.nextPuzzle();
  CHECK(p.route.size()>=2);
  const auto result=runner.submitRoute({p.route.back()},1100);
  CHECK(!result.correct && result.offer_retry);
  CHECK_EQ(result.altitude,0);
  const auto retry=runner.nextPuzzle();
  CHECK(retry.spec.skill==skills::Navigation);
  CHECK(retry.spec.diff.tier<=p.spec.diff.tier);
  CHECK(runner.submitRoute(retry.route,2000).correct);
}
