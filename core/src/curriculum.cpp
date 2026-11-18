#include "sdp/curriculum.hpp"

#include <algorithm>
#include <cmath>

namespace sdp {
namespace {

struct TierShape {
  int min_value_lo = 1;
  int min_value_hi = 1;
  int max_value_lo = 10;
  int max_value_hi = 100;
  int operand_lo = 2;
  int operand_hi = 2;
  int distractor_lo = 3;
  int distractor_hi = 3;
  int carry_from = -1;       // tier index at which carrying is allowed
  int borrow_from = -1;      // tier index at which borrowing is allowed
  int timed_from = -1;       // tier index at which a time limit appears
};

int lerpInt(int lo, int hi, double t) {
  return static_cast<int>(std::lround(static_cast<double>(lo) + (hi - lo) * t));
}

std::vector<DifficultyParams> buildTiers(int count, const TierShape& shape) {
  std::vector<DifficultyParams> tiers;
  tiers.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    const double t = (count <= 1) ? 0.0 : static_cast<double>(i) / static_cast<double>(count - 1);
    DifficultyParams p;
    p.tier = i;
    p.difficulty = t;
    p.min_value = lerpInt(shape.min_value_lo, shape.min_value_hi, t);
    p.max_value = lerpInt(shape.max_value_lo, shape.max_value_hi, t);
    p.operand_count = lerpInt(shape.operand_lo, shape.operand_hi, t);
    p.distractor_count = lerpInt(shape.distractor_lo, shape.distractor_hi, t);
    p.allow_carry = shape.carry_from >= 0 && i >= shape.carry_from;
    p.allow_borrow = shape.borrow_from >= 0 && i >= shape.borrow_from;
    p.time_limit_ms = (shape.timed_from >= 0 && i >= shape.timed_from)
                          ? lerpInt(30000, 12000, t)
                          : 0;
    tiers.push_back(p);
  }
  return tiers;
}

SkillDef makeSkill(const SubSkill& skill, Domain domain, const char* template_id,
                   const std::vector<DifficultyParams>& tiers, std::string quiz_bank = {}) {
  SkillDef def;
  def.skill = skill;
  def.domain = domain;
  def.display_key = "skill." + skill.id;
  def.template_id = template_id;
  def.tiers = tiers;
  def.quiz_bank = std::move(quiz_bank);
  return def;
}

}  // namespace

const SkillDef* Curriculum::skill(std::string_view id) const {
  for (const auto& s : skills_) {
    if (s.skill.view() == id) return &s;
  }
  return nullptr;
}

const ZoneDef* Curriculum::zone(int id) const {
  for (const auto& z : zones_) {
    if (z.id == id) return &z;
  }
  return nullptr;
}

const ZoneDef* Curriculum::zoneForAltitude(int altitude) const {
  const ZoneDef* best = nullptr;
  for (const auto& z : zones_) {
    if (altitude >= z.altitude_start) {
      if (best == nullptr || z.altitude_start > best->altitude_start) best = &z;
    }
  }
  return best;
}

int Curriculum::zoneBoundaryAbove(int altitude) const {
  int best = -1;
  for (const auto& z : zones_) {
    if (z.altitude_start > altitude) {
      if (best < 0 || z.altitude_start < best) best = z.altitude_start;
    }
  }
  return best;
}

bool Curriculum::isZoneBoundary(int altitude) const {
  if (altitude <= 0) return false;
  for (const auto& z : zones_) {
    if (z.altitude_start == altitude) return true;
  }
  return false;
}

std::vector<SubSkill> Curriculum::unlockedAt(int altitude) const {
  std::vector<SubSkill> result;
  for (const auto& z : zones_) {
    if (altitude < z.altitude_start) continue;
    for (const auto& s : z.unlocks) {
      if (std::find(result.begin(), result.end(), s) == result.end()) result.push_back(s);
    }
  }
  return result;
}

int Curriculum::tierCount(std::string_view skill_id) const {
  const SkillDef* def = skill(skill_id);
  return def ? static_cast<int>(def->tiers.size()) : 0;
}

const DifficultyParams* Curriculum::paramsFor(std::string_view skill_id, int tier) const {
  const SkillDef* def = skill(skill_id);
  if (def == nullptr || def->tiers.empty()) return nullptr;
  const int clamped = std::max(0, std::min(tier, static_cast<int>(def->tiers.size()) - 1));
  return &def->tiers[static_cast<std::size_t>(clamped)];
}

int Curriculum::maxTier(std::string_view skill_id) const {
  const int n = tierCount(skill_id);
  return n > 0 ? n - 1 : 0;
}

void Curriculum::addSkill(SkillDef def) {
  for (auto& existing : skills_) {
    if (existing.skill == def.skill) {
      existing = std::move(def);
      return;
    }
  }
  skills_.push_back(std::move(def));
}

void Curriculum::addZone(ZoneDef def) { zones_.push_back(std::move(def)); }

bool Curriculum::validate(std::string* error) const {
  auto fail = [&](const std::string& message) {
    if (error != nullptr) *error = message;
    return false;
  };
  if (skills_.empty()) return fail("curriculum has no skills");
  if (zones_.empty()) return fail("curriculum has no zones");
  for (const auto& s : skills_) {
    if (s.skill.empty()) return fail("skill with empty id");
    if (s.tiers.empty()) return fail("skill '" + s.skill.id + "' has no difficulty tiers");
    if (s.template_id.empty()) return fail("skill '" + s.skill.id + "' has no template id");
  }
  for (const auto& z : zones_) {
    for (const auto& unlocked : z.unlocks) {
      if (skill(unlocked.view()) == nullptr) {
        return fail("zone '" + z.key + "' unlocks unknown skill '" + unlocked.id + "'");
      }
    }
  }
  int previous = -1;
  for (const auto& z : zones_) {
    if (z.altitude_start <= previous && previous != -1) {
      return fail("zone '" + z.key + "' does not start above the previous zone");
    }
    previous = z.altitude_start;
  }
  return true;
}

Curriculum makeDefaultCurriculum() {
  constexpr int kTiers = 12;
  Curriculum c;

  // ---- math.counting : number sense, ranges grow to 100
  TierShape counting;
  counting.min_value_lo = 1;
  counting.min_value_hi = 1;
  counting.max_value_lo = 10;
  counting.max_value_hi = 100;
  counting.distractor_lo = 3;
  counting.distractor_hi = 3;
  counting.timed_from = 8;

  // ---- math.addition
  TierShape addition;
  addition.min_value_lo = 1;
  addition.min_value_hi = 10;
  addition.max_value_lo = 5;
  addition.max_value_hi = 60;
  addition.operand_lo = 2;
  addition.operand_hi = 3;
  addition.distractor_lo = 3;
  addition.distractor_hi = 3;
  addition.carry_from = 4;
  addition.timed_from = 9;

  // ---- math.subtraction
  TierShape subtraction = addition;
  subtraction.min_value_hi = 10;
  subtraction.borrow_from = 5;

  // ---- logic.patterns
  TierShape patterns;
  patterns.min_value_lo = 1;
  patterns.min_value_hi = 2;
  patterns.max_value_lo = 10;
  patterns.max_value_hi = 50;
  patterns.distractor_lo = 3;
  patterns.distractor_hi = 4;
  patterns.timed_from = 9;

  // ---- logic.odd_one_out
  TierShape odd;
  odd.min_value_lo = 1;
  odd.min_value_hi = 1;
  odd.max_value_lo = 20;
  odd.max_value_hi = 100;
  odd.distractor_lo = 3;
  odd.distractor_hi = 4;
  odd.timed_from = 8;

  // ---- logic.ordering
  TierShape ordering = odd;
  ordering.distractor_lo = 2;
  ordering.distractor_hi = 4;

  // ---- quiz.*  (tiers select rows from the CSV bank via their tier window)
  TierShape quiz;
  quiz.min_value_lo = 1;
  quiz.min_value_hi = 1;
  quiz.max_value_lo = 1;
  quiz.max_value_hi = 1;
  quiz.distractor_lo = 3;
  quiz.distractor_hi = 3;

  c.addSkill(makeSkill(skills::Counting, Domain::Math, "math.counting",
                       buildTiers(kTiers, counting)));
  c.addSkill(makeSkill(skills::Addition, Domain::Math, "math.addition",
                       buildTiers(kTiers, addition)));
  c.addSkill(makeSkill(skills::Subtraction, Domain::Math, "math.subtraction",
                       buildTiers(kTiers, subtraction)));
  c.addSkill(makeSkill(skills::Patterns, Domain::ProblemSolving, "logic.patterns",
                       buildTiers(kTiers, patterns)));
  c.addSkill(makeSkill(skills::OddOneOut, Domain::ProblemSolving, "logic.odd_one_out",
                       buildTiers(kTiers, odd)));
  c.addSkill(makeSkill(skills::Ordering, Domain::ProblemSolving, "logic.ordering",
                       buildTiers(kTiers, ordering)));
  c.addSkill(makeSkill(skills::QuizAnimals, Domain::Quiz, "quiz.bank",
                       buildTiers(kTiers, quiz), "animals"));
  c.addSkill(makeSkill(skills::QuizScience, Domain::Quiz, "quiz.bank",
                       buildTiers(kTiers, quiz), "science"));
  c.addSkill(makeSkill(skills::QuizSafety, Domain::Quiz, "quiz.bank",
                       buildTiers(kTiers, quiz), "safety"));

  // ---- zones double as curriculum order (and as palette swaps)
  ZoneDef forest;
  forest.id = 0;
  forest.key = "forest";
  forest.display_key = "zone.forest";
  forest.altitude_start = 0;
  forest.altitude_end = 30;
  forest.palette = "forest";
  forest.unlocks = {skills::Counting, skills::Addition, skills::Patterns, skills::QuizAnimals};

  ZoneDef rocky;
  rocky.id = 1;
  rocky.key = "rocky";
  rocky.display_key = "zone.rocky";
  rocky.altitude_start = 30;
  rocky.altitude_end = 60;
  rocky.palette = "rocky";
  rocky.unlocks = {skills::Subtraction, skills::OddOneOut, skills::QuizScience};

  ZoneDef snow;
  snow.id = 2;
  snow.key = "snowline";
  snow.display_key = "zone.snowline";
  snow.altitude_start = 60;
  snow.altitude_end = 90;
  snow.palette = "snowline";
  snow.unlocks = {skills::Ordering, skills::QuizSafety};

  ZoneDef summit;
  summit.id = 3;
  summit.key = "summit";
  summit.display_key = "zone.summit";
  summit.altitude_start = 90;
  summit.altitude_end = 0;  // final zone: unbounded
  summit.palette = "summit";
  summit.unlocks = {};

  c.addZone(forest);
  c.addZone(rocky);
  c.addZone(snow);
  c.addZone(summit);

  c.setLandmarkInterval(10);
  c.setSessionLength(10);
  c.setReviewInterval(5);
  return c;
}

}  // namespace sdp
