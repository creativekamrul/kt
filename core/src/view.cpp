#include "sdp/view.hpp"

#include <algorithm>
#include <cmath>

namespace sdp {

std::string_view to_string(Scene scene) noexcept {
  switch (scene) {
    case Scene::ProfileSelect: return "profile_select";
    case Scene::Mountain: return "mountain";
    case Scene::Puzzle: return "puzzle";
    case Scene::TrailFeedback: return "trail_feedback";
    case Scene::RetryPrompt: return "retry_prompt";
    case Scene::CampSummary: return "camp_summary";
  }
  return "profile_select";
}

double zoneProgress(int altitude, const ZoneDef* zone) noexcept {
  if (zone == nullptr) return 0.0;
  const int span = zone->altitude_end - zone->altitude_start;
  if (span <= 0) {
    // Final, open-ended zone: express progress as the fraction of the standard
    // 30-step zone that has been climbed, so the marker still moves.
    const int climbed = altitude - zone->altitude_start;
    return std::clamp(static_cast<double>(climbed) / 30.0, 0.0, 1.0);
  }
  const double raw = static_cast<double>(altitude - zone->altitude_start) / static_cast<double>(span);
  return std::clamp(raw, 0.0, 1.0);
}

int markerRow(int altitude, const ZoneDef* zone, int rows) noexcept {
  if (rows <= 1) return 0;
  const double progress = zoneProgress(altitude, zone);
  const int row = rows - 1 - static_cast<int>(std::lround(progress * (rows - 1)));
  return std::clamp(row, 0, rows - 1);
}

std::vector<Rect> stackLayout(int count, int x, int y, int width, int item_height, int gap) {
  std::vector<Rect> rects;
  if (count <= 0) return rects;
  rects.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    Rect rect;
    rect.x = x;
    rect.y = y + i * (item_height + gap);
    rect.width = width;
    rect.height = item_height;
    rects.push_back(rect);
  }
  return rects;
}

void SceneMachine::selectProfile(int child_id) {
  child_id_ = child_id;
  puzzles_served_ = 0;
  scene_ = child_id > 0 ? Scene::Mountain : Scene::ProfileSelect;
}

void SceneMachine::startClimbing(int session_length) {
  session_length_ = std::max(1, session_length);
  puzzles_served_ = 0;
  scene_ = child_id_ > 0 ? Scene::Puzzle : Scene::ProfileSelect;
}

void SceneMachine::onResult(const SessionResult& result, bool session_complete) {
  if (!canAnswer()) return;
  ++puzzles_served_;
  scene_ = result.offer_retry && !session_complete ? Scene::RetryPrompt : Scene::TrailFeedback;
}

void SceneMachine::onRetryDecision(bool accepted, bool session_complete) {
  (void)accepted;  // accepting serves the easier variant, declining moves on -
                   // either way SessionRunner is about to serve a puzzle.
  if (scene_ != Scene::RetryPrompt) return;
  scene_ = session_complete ? Scene::CampSummary : Scene::Puzzle;
}

void SceneMachine::continueTrail(bool session_complete) {
  if (scene_ == Scene::TrailFeedback) {
    scene_ = session_complete ? Scene::CampSummary : Scene::Puzzle;
  }
}

void SceneMachine::leaveSummary() { scene_ = child_id_ > 0 ? Scene::Mountain : Scene::ProfileSelect; }

}  // namespace sdp
