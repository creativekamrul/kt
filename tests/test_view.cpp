#include <set>

#include "sdp/view.hpp"
#include "test_framework.hpp"

using namespace sdp;

namespace {

const ZoneDef* zoneAt(const Curriculum& curriculum, int altitude) {
  return curriculum.zoneForAltitude(altitude);
}

}  // namespace

SDP_TEST(view, marker_starts_at_the_bottom_of_a_zone_and_climbs) {
  const Curriculum curriculum = makeDefaultCurriculum();
  const ZoneDef* forest = zoneAt(curriculum, 0);
  CHECK(forest != nullptr);
  CHECK_EQ(forest->altitude_start, 0);

  constexpr int kRows = 7;
  CHECK_EQ(markerRow(0, forest, kRows), kRows - 1);   // bottom row at the zone start
  CHECK_EQ(markerRow(30, forest, kRows), 0);          // top row at the zone end

  int previous = kRows;
  for (int altitude = 0; altitude <= 30; ++altitude) {
    const int row = markerRow(altitude, forest, kRows);
    CHECK(row >= 0 && row < kRows);
    CHECK(row <= previous);  // the marker must never move down as altitude rises
    previous = row;
  }

  // Out-of-zone altitudes clamp instead of running off the screen.
  CHECK_EQ(markerRow(-5, forest, kRows), kRows - 1);
  CHECK_EQ(markerRow(1000, forest, kRows), 0);
  // An unknown zone means "no progress yet", i.e. the bottom of the mountain -
  // never a marker floating mid-screen for a child with no data.
  CHECK_EQ(markerRow(15, nullptr, kRows), kRows - 1);
}

SDP_TEST(view, progress_is_monotone_across_every_zone) {
  const Curriculum curriculum = makeDefaultCurriculum();
  double previous = -1.0;
  for (const auto& zone : curriculum.zones()) {
    for (int altitude = zone.altitude_start; altitude <= zone.altitude_start + 40; ++altitude) {
      const double progress = zoneProgress(altitude, &zone);
      CHECK(progress >= 0.0 && progress <= 1.0);
      if (altitude > zone.altitude_start) CHECK(progress >= previous - 1e-12);
      previous = progress;
    }
    previous = -1.0;
  }
}

SDP_TEST(view, the_final_zone_still_moves) {
  const Curriculum curriculum = makeDefaultCurriculum();
  const ZoneDef* summit = zoneAt(curriculum, 120);
  CHECK(summit != nullptr);
  CHECK_EQ(summit->altitude_end, 0);  // open ended
  CHECK(zoneProgress(90, summit) < 0.001);
  CHECK(zoneProgress(105, summit) > 0.4);
  CHECK(zoneProgress(120, summit) >= 0.99);
}

SDP_TEST(view, button_layout_is_ordered_and_never_overlaps) {
  const std::vector<Rect> rects = stackLayout(4, 100, 200, 300, 60, 16);
  CHECK_EQ(rects.size(), static_cast<std::size_t>(4));
  for (std::size_t i = 0; i < rects.size(); ++i) {
    CHECK_EQ(rects[i].width, 300);
    CHECK_EQ(rects[i].height, 60);
    CHECK_EQ(rects[i].x, 100);
    for (std::size_t j = i + 1; j < rects.size(); ++j) CHECK(!rects[i].intersects(rects[j]));
  }
  CHECK(rects[0].contains(250, 230));
  CHECK(!rects[0].contains(250, 260));  // in the gap
  CHECK(!rects[0].contains(50, 230));
  CHECK(stackLayout(0, 0, 0, 10, 10, 2).empty());
}

SDP_TEST(view, scene_flow_follows_the_session) {
  SceneMachine machine;
  CHECK(machine.scene() == Scene::ProfileSelect);
  CHECK(!machine.canAnswer());

  machine.selectProfile(7);
  CHECK_EQ(machine.childId(), 7);
  CHECK(machine.scene() == Scene::Mountain);

  machine.startClimbing(3);
  CHECK_EQ(machine.sessionLength(), 3);
  CHECK(machine.canAnswer());

  SessionResult correct;
  correct.correct = true;
  machine.onResult(correct, false);
  CHECK(machine.scene() == Scene::TrailFeedback);
  machine.onResult(correct, false);
  CHECK_EQ(machine.puzzlesServed(), 1);
  machine.continueTrail(false);
  CHECK(machine.scene() == Scene::Puzzle);

  SessionResult wrong;
  wrong.correct = false;
  wrong.offer_retry = true;
  machine.onResult(wrong, false);
  CHECK(machine.scene() == Scene::RetryPrompt);
  CHECK(!machine.canAnswer());  // the shell must ask before serving another item

  machine.onRetryDecision(true, false);
  CHECK(machine.scene() == Scene::Puzzle);

  machine.onResult(correct, true);
  CHECK(machine.scene() == Scene::TrailFeedback);
  machine.continueTrail(true);
  CHECK(machine.atSummary());
  machine.leaveSummary();
  CHECK(machine.scene() == Scene::Mountain);
  machine.startClimbing(3);
  CHECK(machine.canAnswer());
  CHECK_EQ(machine.puzzlesServed(), 0);
}

SDP_TEST(view, retry_decision_at_the_end_of_a_session_ends_the_day) {
  SceneMachine machine;
  machine.selectProfile(1);
  machine.startClimbing(1);

  SessionResult wrong;
  wrong.correct = false;
  wrong.offer_retry = true;
  machine.onResult(wrong, false);
  CHECK(machine.scene() == Scene::RetryPrompt);

  machine.onRetryDecision(false, true);
  CHECK(machine.atSummary());
}

SDP_TEST(view, scene_names_are_stable_and_unique) {
  const Scene scenes[] = {Scene::ProfileSelect, Scene::Mountain, Scene::Puzzle, Scene::TrailFeedback, Scene::RetryPrompt,
                          Scene::CampSummary};
  std::set<std::string> names;
  for (Scene scene : scenes) {
    const std::string name(to_string(scene));
    CHECK(!name.empty());
    CHECK(names.insert(name).second);  // unique
  }
  CHECK_EQ(to_string(Scene::Puzzle), std::string("puzzle"));
}
