#pragma once
// ===========================================================================
// sdp/curriculum.hpp - the content-driven curriculum.
//
// Zones double as curriculum ordering (forest -> rocky -> snowline -> summit),
// exactly as the design document specifies: a zone gates a set of sub-skills,
// and the visible climb and the teaching order are the same object.
//
// Everything here is data. The shipped defaults below let the game run with no
// content files at all; content/curriculum.json overrides them so a ministry of
// education can re-align the curriculum without a recompile.
// ===========================================================================

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "sdp/types.hpp"

namespace sdp {

struct SkillDef {
  SubSkill skill{};
  Domain domain = Domain::Math;
  std::string display_key;    // localization key, e.g. "skill.math.addition"
  std::string template_id;    // generator template that builds items for it
  std::vector<DifficultyParams> tiers;  // tier index -> concrete parameters
  std::string quiz_bank;      // CSV bank name (Quiz domain only)
};

struct ZoneDef {
  int id = 0;
  std::string key;          // "forest"
  std::string display_key;  // "zone.forest"
  int altitude_start = 0;   // inclusive
  int altitude_end = 0;     // exclusive (unused for the final zone)
  std::vector<SubSkill> unlocks;
  std::string palette;  // visual palette id - the cheap-asset trick from the doc
};

class Curriculum {
 public:
  const SkillDef* skill(std::string_view id) const;
  const ZoneDef* zone(int id) const;
  const ZoneDef* zoneForAltitude(int altitude) const;

  int zoneCount() const noexcept { return static_cast<int>(zones_.size()); }
  const std::vector<ZoneDef>& zones() const noexcept { return zones_; }
  const std::vector<SkillDef>& skills() const noexcept { return skills_; }

  /// Altitude at which the zone after the one containing altitude starts,
  /// or -1 when there is none. Used for checkpoint placement.
  int zoneBoundaryAbove(int altitude) const;
  bool isZoneBoundary(int altitude) const;

  /// Sub-skills a child may practise at this altitude (all unlocks of all
  /// zones reached so far). Empty when the curriculum has no zones/content.
  std::vector<SubSkill> unlockedAt(int altitude) const;

  int tierCount(std::string_view skill) const;
  const DifficultyParams* paramsFor(std::string_view skill, int tier) const;
  /// Highest tier index that exists (0 when the skill is unknown).
  int maxTier(std::string_view skill) const;

  int landmarkInterval() const noexcept { return landmark_interval_; }
  int sessionLength() const noexcept { return session_length_; }
  int reviewInterval() const noexcept { return review_interval_; }
  void setLandmarkInterval(int v) noexcept { landmark_interval_ = v; }
  void setSessionLength(int v) noexcept { session_length_ = v; }
  void setReviewInterval(int v) noexcept { review_interval_ = v; }

  void addSkill(SkillDef def);
  void addZone(ZoneDef def);

  /// Structural check used by the content loader and by tests.
  bool validate(std::string* error) const;

 private:
  std::vector<SkillDef> skills_;
  std::vector<ZoneDef> zones_;
  int landmark_interval_ = 10;
  int session_length_ = 10;
  int review_interval_ = 5;
};

/// Shipped defaults: 4 zones, 9 sub-skills (3 per domain), 12 difficulty tiers
/// each. Mirrored by content/curriculum.json.
Curriculum makeDefaultCurriculum();

}  // namespace sdp
