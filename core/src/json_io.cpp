#include <nlohmann/json.hpp>

#include "sdp/curriculum.hpp"
#include "sdp/types.hpp"

namespace sdp {

std::string to_json(const DifficultyParams& d) {
  nlohmann::json j{
      {"tier", d.tier},              {"difficulty", d.difficulty},
      {"min_value", d.min_value},    {"max_value", d.max_value},
      {"operand_count", d.operand_count},
      {"distractor_count", d.distractor_count},
      {"time_limit_ms", d.time_limit_ms},
      {"allow_carry", d.allow_carry},
      {"allow_borrow", d.allow_borrow}};
  return j.dump();
}

std::string to_json(const LevelSpec& s) {
  nlohmann::json j{
      {"domain", std::string(to_string(s.domain))},
      {"skill", s.skill.id},
      {"kind", std::string(to_string(s.kind))},
      {"seed", s.seed},
      {"child_id", s.child_id},
      {"attempt_index", s.attempt_index},
      {"diff", nlohmann::json::parse(to_json(s.diff))}};
  nlohmann::json review = nlohmann::json::array();
  for (const auto& skill : s.review_skills) review.push_back(skill.id);
  j["review_skills"] = review;
  return j.dump();
}

std::string attempt_spec_json(const Attempt& attempt) {
  if (!attempt.spec_json.empty()) return attempt.spec_json;
  // The fixed schema has no tier or checkpoint column, so a minimal spec is
  // synthesised here whenever the caller did not supply the full one.
  nlohmann::json j{
      {"domain", std::string(to_string(attempt.domain))},
      {"skill", attempt.skill.id},
      {"kind", attempt.checkpoint ? "checkpoint" : "practice"},
      {"diff", {{"tier", attempt.tier}, {"difficulty", attempt.difficulty}}}};
  return j.dump();
}

std::optional<LevelSpec> level_spec_from_json(std::string_view text) noexcept {
  try {
    const auto j = nlohmann::json::parse(text.begin(), text.end());
    LevelSpec spec;

    const auto domain = domain_from_string(j.value("domain", std::string("math")));
    if (!domain.has_value()) return std::nullopt;
    spec.domain = *domain;

    spec.skill = SubSkill{j.value("skill", std::string{})};
    if (spec.skill.empty()) return std::nullopt;

    const std::string kind = j.value("kind", std::string("practice"));
    if (kind == "checkpoint") {
      spec.kind = LevelKind::Checkpoint;
    } else if (kind == "review") {
      spec.kind = LevelKind::Review;
    } else {
      spec.kind = LevelKind::Practice;
    }

    spec.seed = j.value("seed", 0u);
    spec.child_id = j.value("child_id", 0);
    spec.attempt_index = j.value("attempt_index", 0);

    const auto diff = j.value("diff", nlohmann::json::object());
    spec.diff.tier = diff.value("tier", 0);
    spec.diff.difficulty = diff.value("difficulty", 0.0);
    spec.diff.min_value = diff.value("min_value", 1);
    spec.diff.max_value = diff.value("max_value", 10);
    spec.diff.operand_count = diff.value("operand_count", 2);
    spec.diff.distractor_count = diff.value("distractor_count", 3);
    spec.diff.time_limit_ms = diff.value("time_limit_ms", 0);
    spec.diff.allow_carry = diff.value("allow_carry", false);
    spec.diff.allow_borrow = diff.value("allow_borrow", false);

    if (j.contains("review_skills") && j["review_skills"].is_array()) {
      for (const auto& entry : j["review_skills"]) {
        if (entry.is_string()) spec.review_skills.push_back(SubSkill{entry.get<std::string>()});
      }
    }
    return spec;
  } catch (...) {
    return std::nullopt;
  }
}

}  // namespace sdp
