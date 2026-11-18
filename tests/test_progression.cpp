#include "sdp/progression.hpp"
#include "test_framework.hpp"

using namespace sdp;

namespace {

ProgressionState freshState() {
  ProgressionState state;
  state.child_id = 1;
  state.altitude = 0;
  state.zone_id = 0;
  return state;
}

}  // namespace

SDP_TEST(progression, one_solved_problem_is_exactly_one_step) {
  const Curriculum curriculum = makeDefaultCurriculum();
  ProgressionService progression(curriculum);
  ProgressionState state = freshState();

  const ProgressionEvent first = progression.onSolved(state, false);
  CHECK_EQ(state.altitude, 1);
  CHECK(first.altitude_changed);
  CHECK_EQ(first.altitude_before, 0);
  CHECK_EQ(first.altitude_after, 1);
  CHECK(!first.zone_changed);
  CHECK(first.solved);
}

SDP_TEST(progression, wrong_answers_are_a_complete_no_op) {
  const Curriculum curriculum = makeDefaultCurriculum();
  ProgressionService progression(curriculum);
  ProgressionState state = freshState();
  progression.onSolved(state, false);
  progression.onSolved(state, false);
  progression.onSolved(state, false);
  CHECK_EQ(state.altitude, 3);

  for (int i = 0; i < 50; ++i) {
    const ProgressionEvent event = progression.onWrong(state);
    CHECK(!event.altitude_changed);
    CHECK(!event.solved);
    CHECK_EQ(event.altitude_after, 3);
  }
  CHECK_EQ(state.altitude, 3);  // visible progress never regresses
}

SDP_TEST(progression, landmarks_appear_every_ten_steps) {
  const Curriculum curriculum = makeDefaultCurriculum();
  ProgressionService progression(curriculum);

  CHECK(!progression.isLandmarkAltitude(0));
  CHECK(!progression.isLandmarkAltitude(9));
  CHECK(progression.isLandmarkAltitude(10));
  CHECK(progression.isLandmarkAltitude(20));

  ProgressionState state = freshState();
  state.altitude = 9;
  const ProgressionEvent event = progression.onSolved(state, false);
  CHECK_EQ(state.altitude, 10);
  CHECK(event.landmark_reached);
  CHECK_EQ(event.landmark_key, std::string("landmark.10"));
}

SDP_TEST(progression, zone_boundaries_are_checkpoints_and_badges) {
  const Curriculum curriculum = makeDefaultCurriculum();
  ProgressionService progression(curriculum);

  CHECK(progression.isCheckpointAltitude(30));
  CHECK(!progression.isCheckpointAltitude(29));
  CHECK_EQ(progression.zoneIdForAltitude(0), 0);
  CHECK_EQ(progression.zoneIdForAltitude(30), 1);
  CHECK_EQ(progression.zoneIdForAltitude(95), 3);

  ProgressionState state = freshState();
  state.altitude = 29;
  const ProgressionEvent event = progression.onSolved(state, true);
  CHECK_EQ(state.altitude, 30);
  CHECK_EQ(state.zone_id, 1);
  CHECK(event.zone_changed);
  CHECK_EQ(event.zone_key, std::string("rocky"));

  bool has_zone_badge = false;
  for (const auto& key : event.new_milestones) {
    if (key == "zone_rocky") has_zone_badge = true;
  }
  CHECK(has_zone_badge);
}

SDP_TEST(progression, milestones_are_ordered_and_deduplicated) {
  const Curriculum curriculum = makeDefaultCurriculum();
  ProgressionService progression(curriculum);

  const std::vector<std::string> keys = progression.milestonesForGain(0, 30);
  CHECK(!keys.empty());
  CHECK_EQ(keys.front(), std::string("first_step"));

  int landmarks = 0;
  int zones = 0;
  for (const auto& key : keys) {
    if (key.rfind("altitude_", 0) == 0) ++landmarks;
    if (key.rfind("zone_", 0) == 0) ++zones;
  }
  CHECK_EQ(landmarks, 3);  // 10, 20, 30
  CHECK_EQ(zones, 1);      // entering the rocky slopes at 30
  CHECK(progression.milestonesForGain(7, 7).empty());
}
