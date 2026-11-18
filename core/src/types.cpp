#include "sdp/types.hpp"

#include <algorithm>

namespace sdp {
namespace {

std::string_view before_dot(std::string_view s) noexcept {
  const auto pos = s.find('.');
  return pos == std::string_view::npos ? s : s.substr(0, pos);
}

std::string_view after_dot(std::string_view s) noexcept {
  const auto pos = s.find('.');
  return pos == std::string_view::npos ? s : s.substr(pos + 1);
}

}  // namespace

std::string_view SubSkill::domainPrefix() const noexcept { return before_dot(id); }
std::string_view SubSkill::topic() const noexcept { return after_dot(id); }

std::string_view to_string(Domain d) noexcept {
  switch (d) {
    case Domain::Math: return "math";
    case Domain::ProblemSolving: return "problem_solving";
    case Domain::Quiz: return "quiz";
  }
  return "math";
}

std::optional<Domain> domain_from_string(std::string_view s) noexcept {
  if (s == "math") return Domain::Math;
  if (s == "problem_solving" || s == "problem-solving" || s == "problemsolving") {
    return Domain::ProblemSolving;
  }
  if (s == "quiz") return Domain::Quiz;
  return std::nullopt;
}

std::string_view display_name(Domain d) noexcept {
  switch (d) {
    case Domain::Math: return "Math";
    case Domain::ProblemSolving: return "Problem solving";
    case Domain::Quiz: return "Quiz";
  }
  return "Math";
}

std::string_view to_string(LevelKind k) noexcept {
  switch (k) {
    case LevelKind::Practice: return "practice";
    case LevelKind::Review: return "review";
    case LevelKind::Checkpoint: return "checkpoint";
  }
  return "practice";
}

// ---------------------------------------------------------------- history
void PerformanceHistory::add(Attempt a) {
  if (child_id_ == 0) child_id_ = a.child_id;
  attempts_.push_back(std::move(a));
}

void PerformanceHistory::addAll(const std::vector<Attempt>& items) {
  for (const auto& a : items) add(a);
}

int PerformanceHistory::correctCount() const noexcept {
  int n = 0;
  for (const auto& a : attempts_) {
    if (a.correct) ++n;
  }
  return n;
}

int PerformanceHistory::countFor(std::string_view skill) const noexcept {
  int n = 0;
  for (const auto& a : attempts_) {
    if (a.skill.view() == skill) ++n;
  }
  return n;
}

int PerformanceHistory::correctFor(std::string_view skill) const noexcept {
  int n = 0;
  for (const auto& a : attempts_) {
    if (a.skill.view() == skill && a.correct) ++n;
  }
  return n;
}

double PerformanceHistory::successRateFor(std::string_view skill, int last_n) const noexcept {
  int seen = 0, good = 0;
  for (auto it = attempts_.rbegin(); it != attempts_.rend(); ++it) {
    if (it->skill.view() != skill) continue;
    ++seen;
    if (it->correct) ++good;
    if (last_n > 0 && seen >= last_n) break;
  }
  if (seen == 0) return 0.0;
  return static_cast<double>(good) / static_cast<double>(seen);
}

int PerformanceHistory::attemptsSinceSkill(std::string_view skill) const noexcept {
  int since = 0;
  for (auto it = attempts_.rbegin(); it != attempts_.rend(); ++it) {
    if (it->skill.view() == skill) return since;
    ++since;
  }
  return since;  // never practised: counts as "long ago"
}

const Attempt* PerformanceHistory::lastFor(std::string_view skill) const noexcept {
  for (auto it = attempts_.rbegin(); it != attempts_.rend(); ++it) {
    if (it->skill.view() == skill) return &(*it);
  }
  return nullptr;
}

}  // namespace sdp
