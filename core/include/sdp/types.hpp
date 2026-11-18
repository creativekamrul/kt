#pragma once
// ===========================================================================
// sdp/types.hpp - core value types shared by every module.
//
// Nothing in this header (or any header under sdp/) depends on graphics,
// windowing, audio or SQLite. That is what makes the adaptive core portable,
// headless-testable and reusable by any future shell.
// ===========================================================================

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sdp {

// ===========================================================================
// Domain - fixed set of three content domains.
// Persisted as TEXT ('math' | 'problem_solving' | 'quiz') to match the schema,
// so a fourth domain would need no migration.
// ===========================================================================
enum class Domain { Math, ProblemSolving, Quiz };

std::string_view to_string(Domain d) noexcept;
std::optional<Domain> domain_from_string(std::string_view s) noexcept;
std::string_view display_name(Domain d) noexcept;

// ===========================================================================
// SubSkill - stable string identity, e.g. "math.addition".
//
// Why not an enum? The curriculum (content/curriculum.json) defines which
// sub-skills exist, their difficulty tiers and their zone ordering. Curriculum
// alignment and translation therefore never require a recompile, which is a
// headline point of the government-program pitch. The SQLite schema already
// stores sub_skill as TEXT, so nothing is lost at the storage layer.
// ===========================================================================
struct SubSkill {
  std::string id;

  SubSkill() = default;
  explicit SubSkill(std::string s) : id(std::move(s)) {}

  bool empty() const noexcept { return id.empty(); }
  std::string_view view() const noexcept { return id; }
  std::string_view domainPrefix() const noexcept;  // before the first '.'
  std::string_view topic() const noexcept;         // after the first '.'

  friend bool operator==(const SubSkill& a, const SubSkill& b) noexcept { return a.id == b.id; }
  friend bool operator!=(const SubSkill& a, const SubSkill& b) noexcept { return !(a == b); }
  friend bool operator<(const SubSkill& a, const SubSkill& b) noexcept { return a.id < b.id; }
};

// Well-known ids. Content files may add more; code treats these as the
// canonical names for the templates shipped with the game.
namespace skills {
inline const SubSkill Navigation{"trail.navigation"};
inline const SubSkill Counting{"math.counting"};
inline const SubSkill Addition{"math.addition"};
inline const SubSkill Subtraction{"math.subtraction"};
inline const SubSkill Patterns{"logic.patterns"};
inline const SubSkill OddOneOut{"logic.odd_one_out"};
inline const SubSkill Ordering{"logic.ordering"};
inline const SubSkill QuizAnimals{"quiz.animals"};
inline const SubSkill QuizScience{"quiz.science"};
inline const SubSkill QuizSafety{"quiz.safety"};
}  // namespace skills

// ===========================================================================
// DifficultyParams - the knobs the adaptive engine tunes. Concrete values come
// from the content difficulty table for the selected tier, so designers can
// retune difficulty without touching code.
// ===========================================================================
struct DifficultyParams {
  int tier = 0;             // discrete bandit arm index (0 = easiest)
  double difficulty = 0.0;  // normalised 0..1, persisted to attempts.difficulty
  int min_value = 1;
  int max_value = 10;
  int operand_count = 2;
  int distractor_count = 3;
  int time_limit_ms = 0;  // 0 = untimed
  bool allow_carry = false;
  bool allow_borrow = false;

  int optionCount() const noexcept { return distractor_count + 1; }
};

// ===========================================================================
// LevelSpec - the engine-to-generator contract (see the design document).
// The seed makes generation a pure function of the spec: reproducible,
// replayable and debuggable from the attempts log alone.
// ===========================================================================
enum class LevelKind { Practice, Review, Checkpoint };
std::string_view to_string(LevelKind k) noexcept;

struct LevelSpec {
  Domain domain = Domain::Math;
  SubSkill skill{};
  DifficultyParams diff{};
  std::uint32_t seed = 0;
  LevelKind kind = LevelKind::Practice;
  int child_id = 0;
  int attempt_index = 0;
  std::vector<SubSkill> review_skills;  // mixed sub-skills for Review/Checkpoint
};

// ===========================================================================
// Telemetry
// ===========================================================================
struct Attempt {
  int child_id = 0;
  Domain domain = Domain::Math;
  SubSkill skill{};
  double difficulty = 0.0;
  int tier = 0;
  bool correct = false;
  int response_ms = 0;
  bool checkpoint = false;
  std::string spec_json;   // exact LevelSpec used (replay/debug)
  std::string created_at;  // UTC ISO-8601; filled by the store when empty
};

// ---------------------------------------------------------------------------
// PerformanceHistory - the append-only view of one child's attempts.
// This is the sole input to AdaptiveEngine::nextLevel(), which keeps every
// engine decision a pure function of the persisted log: no hidden state that
// could drift away from the database.
// ---------------------------------------------------------------------------
class PerformanceHistory {
 public:
  PerformanceHistory() = default;
  explicit PerformanceHistory(int child_id) : child_id_(child_id) {}

  void add(Attempt a);
  void addAll(const std::vector<Attempt>& items);

  const std::vector<Attempt>& attempts() const noexcept { return attempts_; }
  std::size_t size() const noexcept { return attempts_.size(); }
  bool empty() const noexcept { return attempts_.empty(); }
  int childId() const noexcept { return child_id_; }

  const Attempt& operator[](std::size_t i) const { return attempts_[i]; }
  const Attempt& back() const { return attempts_.back(); }

  // Iteration support: "for (const Attempt& a : history)" reads better than
  // index arithmetic in the places that fold the log (engine, reports).
  const Attempt* begin() const noexcept { return attempts_.data(); }
  const Attempt* end() const noexcept { return attempts_.data() + attempts_.size(); }

  // "1 solved problem = 1 step up", so altitude is exactly the number of
  // correct answers. Progression applies the same rule, which keeps the visible
  // climb and the adaptive engine perfectly consistent.
  int correctCount() const noexcept;
  int attemptIndex() const noexcept { return static_cast<int>(attempts_.size()); }

  int countFor(std::string_view skill) const noexcept;
  int correctFor(std::string_view skill) const noexcept;
  double successRateFor(std::string_view skill, int last_n = 0) const noexcept;
  int attemptsSinceSkill(std::string_view skill) const noexcept;
  const Attempt* lastFor(std::string_view skill) const noexcept;

 private:
  int child_id_ = 0;
  std::vector<Attempt> attempts_;
};

// ===========================================================================
// Persisted state types
// ===========================================================================
struct ChildProfile {
  int id = 0;
  std::string name;
  int age = 0;
  std::string created_at;
};

struct SkillState {
  SubSkill skill{};
  double difficulty = 0.0;
  int tier = 0;
  double alpha = 1.0;  // Beta posterior for Thompson sampling at this tier
  double beta = 1.0;
  std::string updated_at;
};

struct ProgressionState {
  int child_id = 0;
  int altitude = 0;
  int zone_id = 0;
  std::string updated_at;
};

struct Milestone {
  std::string name;
  std::string earned_at;
};

struct SkillSuccessRate {
  SubSkill skill{};
  double success_rate = 0.0;
  int n = 0;
};

// ===========================================================================
// JSON helpers (implemented in json_io.cpp so nlohmann/json stays an
// implementation detail of the core library)
// ===========================================================================
std::string to_json(const DifficultyParams& d);
std::string to_json(const LevelSpec& s);
std::optional<LevelSpec> level_spec_from_json(std::string_view text) noexcept;

/// Guarantees an attempt always carries parseable spec JSON, because the fixed
/// schema stores the difficulty tier and the checkpoint flag inside it.
std::string attempt_spec_json(const Attempt& attempt);

}  // namespace sdp
