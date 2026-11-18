#pragma once
// ===========================================================================
// sdp/session.hpp - one "day of climbing".
//
// This is the headless game loop: engine -> generator -> child answer ->
// telemetry -> bandit update -> SQLite -> progression. The graphical shell
// renders what this produces; it contains no learning logic of its own.
//
// Wrapping a session in a class (rather than looping in main) is what lets the
// simulation harness, the unit tests and the shell all drive the identical code
// path. The numbers in the report therefore describe the real game.
// ===========================================================================

#include <string>
#include <vector>

#include "sdp/content.hpp"
#include "sdp/curriculum.hpp"
#include "sdp/engine.hpp"
#include "sdp/generator.hpp"
#include "sdp/progression.hpp"
#include "sdp/store.hpp"
#include "sdp/types.hpp"

namespace sdp {

struct SessionConfig {
  int length = 10;       // puzzles in one day of climbing
  bool allow_retry = true;
  bool mountain_routes = false;
};

struct SessionResult {
  bool correct = false;
  bool altitude_gained = false;
  int altitude = 0;
  int zone_id = 0;
  std::string zone_key;
  bool landmark_reached = false;
  std::string landmark_key;
  bool was_checkpoint = false;
  bool offer_retry = false;
  bool retry = false;  // this attempt WAS the offered retry
  std::string feedback;
  std::vector<std::string> new_milestones;
};

struct SessionSummary {
  int child_id = 0;
  int attempted = 0;
  int solved = 0;
  double success_rate = 0.0;
  int altitude_start = 0;
  int altitude_end = 0;
  std::string zone_key;
  std::vector<std::string> new_milestones;
  std::string headline;
};

class SessionRunner {
 public:
  SessionRunner(AdaptiveEngine& engine, LevelGenerator& generator,
                ProgressionService& progression, IDataStore& store,
                SessionConfig config = SessionConfig{}, const StringTable* strings = nullptr);

  void startSession(int child_id);
  bool active() const noexcept { return child_id_ != 0; }
  int childId() const noexcept { return child_id_; }

  Puzzle nextPuzzle();
  SessionResult submitAnswer(int chosen_index, int response_ms);
  /// Convenience overload for shells and tests: answers the puzzle it is given
  /// and asserts it is the one that was served.
  SessionResult submitAnswer(const Puzzle& puzzle, int chosen_index, int response_ms);

  SessionResult submitRoute(const std::vector<int>& visited, int response_ms);

  bool retryPending() const noexcept { return retry_pending_; }
  /// Declines an offered retry, so the next puzzle is a normal practice item.
  /// A child who says "no" must not be given the easier variant anyway.
  void cancelRetry() noexcept { retry_pending_ = false; }
  int puzzlesServed() const noexcept { return puzzles_served_; }
  bool complete() const noexcept { return puzzles_served_ >= config_.length; }
  const Puzzle& currentPuzzle() const { return current_; }
  const LevelSpec& currentSpec() const { return current_.spec; }
  const ProgressionState& progressionState() const noexcept { return state_; }
  const PerformanceHistory& history() const noexcept { return history_; }
  SessionSummary summary() const;

 private:
  std::string text(std::string_view key, std::string_view fallback) const;
  SessionResult recordAttempt(bool correct, int response_ms);
  void persistSkillState(const SubSkill& skill);

  AdaptiveEngine& engine_;
  LevelGenerator& generator_;
  ProgressionService& progression_;
  IDataStore& store_;
  SessionConfig config_{};
  const StringTable* strings_ = nullptr;

  int child_id_ = 0;
  int altitude_start_ = 0;
  int puzzles_served_ = 0;
  int attempted_ = 0;
  int solved_ = 0;
  bool retry_pending_ = false;
  bool current_is_retry_ = false;
  bool has_current_ = false;
  SubSkill last_skill_{};
  int last_practice_tier_ = -1;

  PerformanceHistory history_{};
  ProgressionState state_{};
  Puzzle current_{};
  std::vector<std::string> session_milestones_{};
};

}  // namespace sdp
