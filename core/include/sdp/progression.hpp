#pragma once
// ===========================================================================
// sdp/progression.hpp - the visible climb.
//
// The one rule that matters: VISIBLE PROGRESS NEVER REGRESSES. A wrong answer
// does not move the child down the mountain, ever. The adaptive engine tunes
// difficulty invisibly; the world only ever rewards engagement.
// ===========================================================================

#include <string>
#include <vector>

#include "sdp/curriculum.hpp"
#include "sdp/types.hpp"

namespace sdp {

struct ProgressionEvent {
  bool solved = false;
  int altitude_before = 0;
  int altitude_after = 0;
  bool altitude_changed = false;
  bool zone_changed = false;
  int zone_id = 0;
  std::string zone_key;
  bool landmark_reached = false;
  std::string landmark_key;
  bool checkpoint = false;
  std::vector<std::string> new_milestones;
};

class ProgressionService {
 public:
  /// Owns a copy of the curriculum, for the same lifetime-safety reason as
  /// LevelGenerator: services must never dangle on a caller's temporary.
  explicit ProgressionService(const Curriculum& curriculum) : curriculum_(curriculum) {}

  /// One solved problem = one step up. Never more, never less.
  ProgressionEvent onSolved(ProgressionState& state, bool was_checkpoint);
  /// A wrong answer changes nothing at all: no altitude loss, no shame.
  ProgressionEvent onWrong(const ProgressionState& state) const;

  int zoneIdForAltitude(int altitude) const;
  bool isCheckpointAltitude(int altitude) const;
  bool isLandmarkAltitude(int altitude) const;

  /// Milestone keys earned by climbing from "before" to "after" (exclusive,
  /// inclusive). Deterministic, so replaying a log re-earns the same badges.
  std::vector<std::string> milestonesForGain(int before, int after) const;

  const Curriculum& curriculum() const noexcept { return curriculum_; }

 private:
  Curriculum curriculum_{};
};

}  // namespace sdp
