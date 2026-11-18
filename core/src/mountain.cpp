#include "sdp/mountain.hpp"

#include <algorithm>
#include <cmath>

namespace sdp {
RouteRules rulesForTrailTier(int tier) {
  const int anchors=std::clamp(tier+2,2,4);
  return {anchors,tier==2 ? 3 : anchors};
}
double RouteRules::randomSuccess() const {
  double p=1;
  for(int i=0;i<required;++i)p/=anchors-i;
  return p;
}
MountainTrail MountainTrail::generate(const Puzzle& puzzle) {
  MountainTrail trail;
  trail.seed = puzzle.spec.seed;
  Rng rng(trail.seed);
  trail.activity = puzzle.spec.domain == Domain::Quiz ? TrailActivity::Survey :
      puzzle.spec.skill == skills::Addition || puzzle.spec.skill == skills::Subtraction ?
      TrailActivity::Supplies : TrailActivity::Fork;
  trail.rope = {float(rng.nextInt(175,305)), 98};
  trail.start = {float(rng.nextInt(212,268)), 442};
  trail.crossings={rng.nextInt(100,170),rng.nextInt(300,378)};
  trail.camp = {trail.start.x, 385};
  for (int i=0; i<int(puzzle.options.size()); ++i) {
    float x = 55.f + (i+.5f)*370.f / puzzle.options.size();
    trail.markers.push_back({x+rng.nextInt(-8,8), float(rng.nextInt(219,280))});
  }
  return trail;
}

bool MountainTrail::near(TrailPoint a, TrailPoint b, float radius) const {
  float dx=a.x-b.x, dy=a.y-b.y;
  return dx*dx + dy*dy <= radius*radius;
}
int MountainTrail::nearest(TrailPoint p, float radius) const {
  int result=-1;
  for (int i=0; i<int(markers.size()); ++i) {
    if (!near(p,markers[i],radius)) continue;
    result=i;
    radius=std::hypot(p.x-markers[i].x,p.y-markers[i].y);
  }
  return result;
}
int MountainTrail::creekY(float x) const {
  return 350+int(6*std::sin(x*.023f+float(seed%19)));
}
TrailPoint MountainTrail::move(TrailPoint from, float dx, float dy, bool rope_open) const {
  float x=std::clamp(from.x+dx,42.f,438.f);
  if (from.y<170) x=std::clamp(x,rope.x-9,rope.x+9);
  const bool on_rope=rope_open && std::abs(x-rope.x)<=10;
  float y=std::clamp(from.y+dy,on_rope ? rope.y : 170.f,456.f);
  const bool crossing=std::abs(x-crossings[0])<=13 || std::abs(x-crossings[1])<=13;
  if (!crossing && std::abs(y-creekY(x))<9) {
    if (std::abs(from.y-creekY(from.x))<9) return from;
    y=float(creekY(x))+(from.y>=creekY(from.x) ? 9.f : -9.f);
  }
  return {x,y};
}
}  // namespace sdp
