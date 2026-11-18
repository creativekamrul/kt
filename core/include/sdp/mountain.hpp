#pragma once

#include <array>
#include "sdp/generator.hpp"

namespace sdp {

struct RouteRules {
  int anchors, required;
  double randomSuccess() const;
};
RouteRules rulesForTrailTier(int tier);

struct TrailPoint { float x = 0, y = 0; };
enum class TrailActivity { Fork, Supplies, Survey };

// Coordinates are in map pixels. The renderer, collision checks and interactions
// all use the same seeded landmarks.
struct MountainTrail {
  TrailActivity activity = TrailActivity::Fork;
  std::vector<TrailPoint> markers;
  TrailPoint camp, rope, start;
  std::array<int,2> crossings{};
  int creekY(float x) const;
  unsigned seed = 0;
  static MountainTrail generate(const Puzzle& puzzle);
  TrailPoint move(TrailPoint from, float dx, float dy, bool rope_open) const;
  int nearest(TrailPoint point, float radius = 33.f) const;
  bool near(TrailPoint a, TrailPoint b, float radius) const;
};

}  // namespace sdp
