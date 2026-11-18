#pragma once
// ===========================================================================
// sdp/generator.hpp - LevelSpec -> concrete puzzle.
//
// Determinism is the whole point: generation is a pure function of
// (LevelSpec, template). Same spec, same puzzle, on every machine. That is what
// lets a session be replayed from the attempts log during debugging, and what
// makes the content pipeline testable.
//
// Templates are "parametrized templates" in the sense of the design document:
// arithmetic is generated from DifficultyParams, and quiz content comes from
// human-editable CSV banks. Three templates per domain, done well, beats ten
// done badly.
// ===========================================================================

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "sdp/curriculum.hpp"
#include "sdp/rng.hpp"
#include "sdp/types.hpp"

namespace sdp {

struct PuzzleOption {
  std::string text;
  bool correct = false;
};

struct Puzzle {
  std::string prompt;
  std::vector<PuzzleOption> options;
  std::string answer;  // canonical answer text, equal to the correct option
  std::vector<int> route;  // ordered anchor indices for a physical wayfinding encounter
  std::string hint;
  LevelSpec spec{};
  std::string spec_json;  // persisted to attempts.spec_json for replay/debug
  bool is_checkpoint = false;

  int correctIndex() const;
  /// Invariant check used by the tests: exactly one correct option, no
  /// duplicate option text, and the marked answer matches the canonical one.
  bool wellFormed() const;
};

/// One row of a quiz bank (content/quiz/*.csv).
struct QuizItem {
  std::string prompt;
  std::string answer;
  std::vector<std::string> distractors;
  std::string hint;
  int tier_min = 0;
  int tier_max = 999;
};

using QuizBank = std::vector<QuizItem>;

struct GeneratorContext {
  const Curriculum* curriculum = nullptr;
  const std::map<std::string, QuizBank>* banks = nullptr;
};

class LevelGenerator {
 public:
  using TemplateFn = std::function<Puzzle(const LevelSpec&, Rng&, const GeneratorContext&)>;

  LevelGenerator();  // uses makeDefaultCurriculum()
  /// The generator OWNS A COPY of the curriculum. Holding a pointer to the
  /// caller's object looked tempting and was a use-after-free waiting to happen
  /// (passing a temporary, e.g. makeDefaultCurriculum(), dangles immediately).
  explicit LevelGenerator(const Curriculum& curriculum);

  void registerTemplate(const std::string& id, TemplateFn fn);
  void addQuizBank(const std::string& name, QuizBank bank);
  const QuizBank* quizBank(const std::string& name) const;
  std::vector<std::string> templateIds() const;

  Puzzle generate(const LevelSpec& spec) const;

  /// Plausible wrong answers: the near-misses children actually produce
  /// (off-by-one, off-by-ten, dropped units digit), not random noise. Error
  /// patterns are then visible in the telemetry.
  static std::vector<int> makeDistractors(int correct, int count, int lo, int hi, Rng& rng);

 private:
  GeneratorContext context() const;
  void registerTemplates();

  Curriculum owned_{};          // the generator's own copy, always valid
  const Curriculum* curriculum_ = nullptr;
  std::map<std::string, TemplateFn> templates_;
  std::map<std::string, QuizBank> banks_;
};

}  // namespace sdp
