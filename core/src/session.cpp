#include "sdp/session.hpp"

#include <stdexcept>

namespace sdp {
namespace {

constexpr const char* kCorrectKeys[] = {"feedback.correct.0", "feedback.correct.1",
                                        "feedback.correct.2", "feedback.correct.3"};
constexpr const char* kCorrectFallbacks[] = {"Nice climbing!", "You did it!", "Great work!",
                                             "Keep going!"};
constexpr int kCorrectCount = 4;

}  // namespace

SessionRunner::SessionRunner(AdaptiveEngine& engine, LevelGenerator& generator,
                             ProgressionService& progression, IDataStore& store,
                             SessionConfig config, const StringTable* strings)
    : engine_(engine),
      generator_(generator),
      progression_(progression),
      store_(store),
      config_(config),
      strings_(strings) {}

std::string SessionRunner::text(std::string_view key, std::string_view fallback) const {
  return strings_ != nullptr ? strings_->get(key, fallback) : std::string(fallback);
}

void SessionRunner::startSession(int child_id) {
  child_id_ = child_id;
  puzzles_served_ = 0;
  attempted_ = 0;
  solved_ = 0;
  retry_pending_ = false;
  current_is_retry_ = false;
  has_current_ = false;
  last_practice_tier_ = -1;
  session_milestones_.clear();

  auto stored = store_.progression(child_id);
  if (!stored.has_value()) {
    ProgressionState fresh;
    fresh.child_id = child_id;
    fresh.altitude = 0;
    fresh.zone_id = progression_.zoneIdForAltitude(0);
    store_.saveProgression(fresh);
    stored = fresh;
  }
  state_ = *stored;
  altitude_start_ = state_.altitude;

  // Resume exactly: fold the persisted log, so the engine's posteriors are a
  // function of the database rather than of anything held in memory.
  history_ = store_.history(child_id);
  engine_.replay(history_);
}

Puzzle SessionRunner::nextPuzzle() {
  if (!active()) throw std::logic_error("startSession() must be called first");

  if (has_current_) return current_;

  LevelSpec spec;
  if (retry_pending_) {
    // One retry only, always measured from the PRACTICE tier rather than from
    // the previous attempt. Without this bound a struggling child cascades
    // downward (tier-1, tier-2, ...) and the difficulty policy is silently
    // overridden - a bug the simulation harness exposed, see docs/simulation.md.
    const int cap = std::max(0, last_practice_tier_ - 1);
    spec = last_skill_ == skills::Navigation ? engine_.navigationLevel(history_,cap) :
        engine_.nextLevelForSkill(history_, last_skill_, cap);
    retry_pending_ = false;
    current_is_retry_ = true;
  } else {
    if (config_.mountain_routes && history_.attemptIndex()%3 != 2 &&
        !progression_.isCheckpointAltitude(state_.altitude))
      spec=engine_.navigationLevel(history_);
    else spec = engine_.nextLevel(history_);
    current_is_retry_ = false;
  }

  current_ = generator_.generate(spec);
  has_current_ = true;
  ++puzzles_served_;
  return current_;
}

SessionResult SessionRunner::submitAnswer(const Puzzle& puzzle, int chosen_index,
                                          int response_ms) {
  if (!has_current_ || puzzle.spec.seed != current_.spec.seed) {
    throw std::logic_error("submitAnswer() must answer the puzzle returned by nextPuzzle()");
  }
  return submitAnswer(chosen_index, response_ms);
}

SessionResult SessionRunner::submitAnswer(int chosen_index, int response_ms) {
  if (!has_current_) throw std::logic_error("no puzzle is awaiting an answer");

  if (!current_.route.empty()) throw std::logic_error("wayfinding requires a visited route");
  const int correct_index = current_.correctIndex();
  const bool correct = chosen_index >= 0 && chosen_index == correct_index;

  return recordAttempt(correct,response_ms);
}

SessionResult SessionRunner::submitRoute(const std::vector<int>& visited,int response_ms) {
  if (!has_current_ || current_.route.empty()) throw std::logic_error("no route is awaiting completion");
  return recordAttempt(visited==current_.route,response_ms);
}

SessionResult SessionRunner::recordAttempt(bool correct,int response_ms) {
  Attempt attempt;
  attempt.child_id = child_id_;
  attempt.domain = current_.spec.domain;
  attempt.skill = current_.spec.skill;
  attempt.difficulty = current_.spec.diff.difficulty;
  attempt.tier = current_.spec.diff.tier;
  attempt.correct = correct;
  attempt.response_ms = response_ms;
  attempt.checkpoint = current_.spec.kind == LevelKind::Checkpoint;
  attempt.spec_json = current_.spec_json;

  // Telemetry first: the log is the source of truth, and the engine's update
  // must be reproducible from it. The attempt, the progression row and the
  // skill state are written in ONE transaction (see IDataStore::beginBatch).
  history_.add(attempt);
  engine_.observe(attempt);
  store_.beginBatch();
  store_.logAttempt(attempt);
  ++attempted_;

  SessionResult result;
  result.correct = correct;
  result.was_checkpoint = attempt.checkpoint;
  result.retry = current_is_retry_;

  if (correct) {
    ++solved_;
    const ProgressionEvent event = progression_.onSolved(state_, attempt.checkpoint);
    result.landmark_reached = event.landmark_reached;
    result.landmark_key = event.landmark_key;
    store_.saveProgression(state_);

    for (const auto& key : event.new_milestones) {
      if (store_.hasMilestone(child_id_, key)) continue;
      store_.awardMilestone(child_id_, key);
      result.new_milestones.push_back(key);
      session_milestones_.push_back(key);
    }
    persistSkillState(attempt.skill);

    const int variant = attempted_ % kCorrectCount;
    result.feedback = text(kCorrectKeys[variant], kCorrectFallbacks[variant]);
    store_.endBatch();
    if (event.landmark_reached) {
      result.feedback += " You reached a new landmark!";
    }
  } else {
    const ProgressionEvent event = progression_.onWrong(state_);  // a no-op, by design
    (void)event;
    store_.endBatch();
    // A retry is offered once per mistake, and never a second time in a row:
    // two failures in front of a six-year-old is where frustration starts.
    result.offer_retry = config_.allow_retry && !complete() && !current_is_retry_;
    retry_pending_ = result.offer_retry;
    last_skill_ = attempt.skill;
    if (current_is_retry_) {
      result.feedback = text("feedback.wrong_again", "That one was tricky - let us try something else.");
    } else {
      result.feedback = text("feedback.wrong", "Not quite - let us try this one again.");
    }
  }

  if (!current_is_retry_) last_practice_tier_ = attempt.tier;

  result.altitude_gained = correct;
  result.altitude = state_.altitude;
  result.zone_id = state_.zone_id;
  if (const ZoneDef* zone = progression_.curriculum().zone(state_.zone_id)) {
    result.zone_key = zone->key;
  }

  has_current_ = false;
  return result;
}

void SessionRunner::persistSkillState(const SubSkill& skill) {
  for (const auto& state : engine_.skillStates()) {
    if (state.skill == skill) {
      store_.saveSkillState(child_id_, state);
      return;
    }
  }
}

SessionSummary SessionRunner::summary() const {
  SessionSummary summary;
  summary.child_id = child_id_;
  summary.attempted = attempted_;
  summary.solved = solved_;
  summary.success_rate = attempted_ > 0 ? static_cast<double>(solved_) / attempted_ : 0.0;
  summary.altitude_start = altitude_start_;
  summary.altitude_end = state_.altitude;
  summary.new_milestones = session_milestones_;

  const ZoneDef* zone = progression_.curriculum().zone(state_.zone_id);
  summary.zone_key = zone != nullptr ? zone->key : std::string("forest");
  const std::string zone_text =
      zone != nullptr ? text(zone->display_key, zone->key) : std::string("the mountain");

  summary.headline = "You reached " + zone_text + " today! (" + std::to_string(solved_) +
                     " of " + std::to_string(attempted_) + " puzzles solved)";
  return summary;
}

}  // namespace sdp
