#pragma once
// ===========================================================================
// sdp/view.hpp - shell-agnostic presentation logic.
//
// This is deliberately NOT in a shell. Everything here is pure geometry and
// state - where the climber sits on the mountain, which screen comes next -
// which means it can be unit tested without a window, and both shells (console
// and SFML) are guaranteed to agree. The heaviest bug risk in a graphical shell
// is exactly this logic, so none of it lives in the shell.
// ===========================================================================

#include <string_view>
#include <vector>

#include "sdp/curriculum.hpp"
#include "sdp/session.hpp"
#include "sdp/types.hpp"

namespace sdp {

struct Rect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;

  int right() const noexcept { return x + width; }
  int bottom() const noexcept { return y + height; }
  bool contains(int px, int py) const noexcept {
    return px >= x && px < right() && py >= y && py < bottom();
  }
  bool intersects(const Rect& other) const noexcept {
    return x < other.right() && other.x < right() && y < other.bottom() && other.y < bottom();
  }
};

enum class Scene { ProfileSelect, Mountain, Puzzle, TrailFeedback, RetryPrompt, CampSummary };
std::string_view to_string(Scene scene) noexcept;

/// How far through the current zone the child is, 0..1. Altitudes outside the
/// zone clamp, so a malformed zone table cannot produce a marker off-screen.
double zoneProgress(int altitude, const ZoneDef* zone) noexcept;

/// Row of a mountain drawn with rows rows that represents this altitude, where
/// row 0 is the summit. Shared by every shell so the two agree exactly.
int markerRow(int altitude, const ZoneDef* zone, int rows) noexcept;

/// Vertical stack layout for answer buttons: centred, equal height, with gaps.
std::vector<Rect> stackLayout(int count, int x, int y, int width, int item_height, int gap);

/// The game's scene flow. Kept free of rendering so the flow is testable.
class SceneMachine {
 public:
  SceneMachine() = default;

  Scene scene() const noexcept { return scene_; }
  int childId() const noexcept { return child_id_; }
  int puzzlesServed() const noexcept { return puzzles_served_; }
  int sessionLength() const noexcept { return session_length_; }
  bool canAnswer() const noexcept { return scene_ == Scene::Puzzle; }
  bool atSummary() const noexcept { return scene_ == Scene::CampSummary; }

  void selectProfile(int child_id);
  void startClimbing(int session_length);
  /// After each answered puzzle. session_complete comes from SessionRunner.
  void onResult(const SessionResult& result, bool session_complete);
  /// After the retry prompt: either way the child answers another puzzle, or the
  /// day ends if the session was already complete.
  void onRetryDecision(bool accepted, bool session_complete);
  void continueTrail(bool session_complete);
  void leaveSummary();

 private:
  Scene scene_ = Scene::ProfileSelect;
  int child_id_ = 0;
  int puzzles_served_ = 0;
  int session_length_ = 10;
};

}  // namespace sdp
