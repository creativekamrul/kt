#include "sdp/generator.hpp"
#include "sdp/mountain.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace sdp {
namespace {

std::string itos(int v) { return std::to_string(v); }

int clampInt(int v, int lo, int hi) { return std::max(lo, std::min(hi, v)); }

/// Builds the multiple-choice options for a numeric answer.
void finalizeNumeric(Puzzle& puzzle, const LevelSpec& spec, int correct, Rng& rng, int lo, int hi) {
  const int want = std::max(1, spec.diff.optionCount());
  std::vector<int> wrong = LevelGenerator::makeDistractors(correct, want - 1, lo, hi, rng);
  if (static_cast<int>(wrong.size()) > want - 1) wrong.resize(static_cast<std::size_t>(want - 1));

  puzzle.options.clear();
  puzzle.options.push_back(PuzzleOption{itos(correct), true});
  for (int v : wrong) puzzle.options.push_back(PuzzleOption{itos(v), false});

  rng.shuffle(puzzle.options);
  puzzle.answer = itos(correct);
}

void finalizeText(Puzzle& puzzle, const LevelSpec& spec,
                  const std::vector<std::string>& wrong, Rng& rng, const std::string& correct) {
  const int want = std::max(2, spec.diff.optionCount());
  puzzle.options.clear();
  puzzle.options.push_back(PuzzleOption{correct, true});
  for (const auto& text : wrong) {
    if (static_cast<int>(puzzle.options.size()) >= want) break;
    if (text == correct) continue;
    bool duplicate = false;
    for (const auto& option : puzzle.options) {
      if (option.text == text) duplicate = true;
    }
    if (duplicate) continue;
    puzzle.options.push_back(PuzzleOption{text, false});
  }

  // Keep the "options == distractor_count + 1" contract even when a content
  // bank is too thin to supply enough distinct wrong answers.
  static const char* const kPadPool[] = {"none of these", "not sure", "all of them", "0"};
  for (const char* pad : kPadPool) {
    if (static_cast<int>(puzzle.options.size()) >= want) break;
    bool duplicate = false;
    for (const auto& option : puzzle.options) {
      if (option.text == pad) duplicate = true;
    }
    if (!duplicate) puzzle.options.push_back(PuzzleOption{pad, false});
  }

  rng.shuffle(puzzle.options);
  puzzle.answer = correct;
}

// -- templates --------------------------------------------------------------

Puzzle tmplFallback(const LevelSpec& spec, Rng& rng, const GeneratorContext&);

Puzzle tmplCounting(const LevelSpec& spec, Rng& rng, const GeneratorContext&) {
  Puzzle puzzle;
  const DifficultyParams& d = spec.diff;
  const int lo = std::max(0, d.min_value);
  const int hi = std::max(d.max_value, lo + 4);
  const bool boss = spec.kind != LevelKind::Practice;

  int answer = 0;
  if (d.difficulty < 0.34 && !boss) {
    const int n = rng.nextInt(lo, std::max(lo, hi - 1));
    puzzle.prompt = "Which number comes right after " + itos(n) + "?";
    answer = n + 1;
  } else if (d.difficulty < 0.70 && !boss) {
    const int a = rng.nextInt(lo, std::max(lo, hi - 3));
    puzzle.prompt = "Which number is missing? " + itos(a) + ", " + itos(a + 1) + ", __, " +
                    itos(a + 3);
    answer = a + 2;
  } else {
    const int step = clampInt(2 + static_cast<int>(std::lround(d.difficulty * 7.0)), 2, 10);
    const int a = rng.nextInt(lo, std::max(lo, hi - 3 * step));
    puzzle.prompt = "Skip counting by " + itos(step) + ": " + itos(a) + ", " + itos(a + step) +
                    ", " + itos(a + 2 * step) + ", __";
    answer = a + 3 * step;
  }

  puzzle.hint = "Count on carefully, one step at a time.";
  finalizeNumeric(puzzle, spec, answer, rng, std::max(0, answer - 12), answer + 12);
  return puzzle;
}

/// "a + b - c = ?" - the checkpoint (boss) form, mixing two sub-skills.
Puzzle tmplMixedExpression(const LevelSpec& spec, Rng& rng, const GeneratorContext&) {
  Puzzle puzzle;
  const int lo = std::max(1, spec.diff.min_value);
  const int hi = std::max(spec.diff.max_value, lo + 5);

  const int a = rng.nextInt(lo, hi);
  const int b = rng.nextInt(lo, hi);
  const int c = rng.nextInt(1, std::max(1, std::min(a + b, hi)));

  puzzle.prompt = itos(a) + " + " + itos(b) + " - " + itos(c) + " = ?";
  puzzle.hint = "Do it in two steps: add first, then take away.";
  finalizeNumeric(puzzle, spec, a + b - c, rng, 0, a + b + 20);
  return puzzle;
}

Puzzle tmplAddition(const LevelSpec& spec, Rng& rng, const GeneratorContext& ctx) {
  if (spec.kind == LevelKind::Checkpoint) return tmplMixedExpression(spec, rng, ctx);

  Puzzle puzzle;
  const DifficultyParams& d = spec.diff;
  const int lo = std::max(1, d.min_value);
  const int hi = std::max(d.max_value, lo + 3);
  const int operands = clampInt(d.operand_count, 2, 4);

  std::vector<int> terms;
  int total = 0;
  for (int i = 0; i < operands; ++i) {
    int value = rng.nextInt(lo, hi);
    // When carrying is allowed, make sure the units digits actually force one.
    if (d.allow_carry && i < 2 && (value % 10) < 5) value += 5 - (value % 10);
    terms.push_back(value);
    total += value;
  }

  puzzle.prompt = itos(terms[0]);
  for (std::size_t i = 1; i < terms.size(); ++i) puzzle.prompt += " + " + itos(terms[i]);
  puzzle.prompt += " = ?";
  puzzle.hint = "Start from the bigger number and count on.";

  finalizeNumeric(puzzle, spec, total, rng, 0, total + 20);
  return puzzle;
}

Puzzle tmplSubtraction(const LevelSpec& spec, Rng& rng, const GeneratorContext& ctx) {
  if (spec.kind == LevelKind::Checkpoint) return tmplMixedExpression(spec, rng, ctx);

  Puzzle puzzle;
  const DifficultyParams& d = spec.diff;
  const int lo = std::max(1, d.min_value);
  const int hi = std::max(d.max_value, lo + 6);

  int a = 0;
  int b = 0;
  if (d.allow_borrow) {
    // Construct a borrow directly: units digit of the minuend is smaller.
    const int units_a = rng.nextInt(0, 4);
    const int units_b = rng.nextInt(5, 9);
    const int tens_a = rng.nextInt(2, 9);
    const int tens_b = rng.nextInt(1, tens_a - 1);
    a = tens_a * 10 + units_a;
    b = tens_b * 10 + units_b;
  } else {
    a = rng.nextInt(lo + 1, hi);
    b = rng.nextInt(1, std::max(1, a - 1));
  }

  puzzle.prompt = itos(a) + " - " + itos(b) + " = ?";
  puzzle.hint = "Count back from " + itos(a) + ".";
  finalizeNumeric(puzzle, spec, a - b, rng, 0, a + 12);
  return puzzle;
}

Puzzle tmplPatterns(const LevelSpec& spec, Rng& rng, const GeneratorContext&) {
  Puzzle puzzle;
  const DifficultyParams& d = spec.diff;
  const bool boss = spec.kind != LevelKind::Practice;

  int step = clampInt(1 + static_cast<int>(std::lround(d.difficulty * 8.0)), 1, 9);
  int start = rng.nextInt(1, std::max(2, d.max_value));
  const int length = d.difficulty > 0.5 ? 4 : 3;

  if (boss) {
    step = -step;  // a descending sequence is a genuinely new reading of the rule
    start = rng.nextInt(std::abs(step) * length + 1, std::max(10, d.max_value * 2));
  }

  std::string sequence;
  int value = start;
  int last = start;
  for (int i = 0; i < length; ++i) {
    if (i > 0) sequence += ", ";
    sequence += itos(value);
    last = value;
    value += step;
  }
  const int answer = last + step;

  puzzle.prompt = "What comes next? " + sequence + ", __";
  puzzle.hint = boss ? "This one counts down, not up." : "Look at how much the numbers change.";
  finalizeNumeric(puzzle, spec, answer, rng, std::max(0, answer - 12), answer + 12);
  return puzzle;
}

Puzzle tmplOddOneOut(const LevelSpec& spec, Rng& rng, const GeneratorContext&) {
  Puzzle puzzle;
  const DifficultyParams& d = spec.diff;
  const int want = std::max(2, d.optionCount());
  const int factor = d.difficulty > 0.6 ? rng.nextInt(2, 5) : 2;

  // Members of the hidden rule are small multiples of "factor"; the outsider
  // is never one. Taking the first (want - 1) multipliers guarantees distinct
  // values and therefore an exact option count.
  std::vector<int> group;
  for (int multiplier = 2; multiplier <= 8 && static_cast<int>(group.size()) < want - 1;
       ++multiplier) {
    group.push_back(factor * multiplier);
  }
  rng.shuffle(group);
  while (static_cast<int>(group.size()) < want - 1) group.push_back(factor * (group.size() + 3));

  int outsider = group.front() + 1;
  while (std::find(group.begin(), group.end(), outsider) != group.end()) outsider += 1;

  std::vector<std::string> wrong;
  for (int value : group) wrong.push_back(itos(value));

  puzzle.prompt = "Which one does not belong?";
  puzzle.hint = "All but one share a secret rule.";
  finalizeText(puzzle, spec, wrong, rng, itos(outsider));
  return puzzle;
}

Puzzle tmplOrdering(const LevelSpec& spec, Rng& rng, const GeneratorContext&) {
  Puzzle puzzle;
  const DifficultyParams& d = spec.diff;
  const int want = clampInt(d.optionCount(), 2, 4);
  const int lo = std::max(1, d.min_value);
  const int hi = std::max(lo + want + 3, d.max_value);

  std::vector<int> values;
  int attempts = 0;
  while (static_cast<int>(values.size()) < want && attempts < 400) {
    ++attempts;
    const int value = rng.nextInt(lo, hi);
    if (std::find(values.begin(), values.end(), value) != values.end()) continue;
    values.push_back(value);
  }

  const bool biggest = rng.chance(0.5);
  int answer = values.front();
  for (int value : values) {
    if (biggest ? (value > answer) : (value < answer)) answer = value;
  }

  std::vector<std::string> wrong;
  for (int value : values) {
    if (value != answer) wrong.push_back(itos(value));
  }
  rng.shuffle(wrong);

  puzzle.prompt = biggest ? "Which number is the biggest?" : "Which number is the smallest?";
  puzzle.hint = "Compare the tens first, then the ones.";
  finalizeText(puzzle, spec, wrong, rng, itos(answer));
  return puzzle;
}

Puzzle tmplQuizBank(const LevelSpec& spec, Rng& rng, const GeneratorContext& ctx) {
  const SkillDef* def =
      ctx.curriculum != nullptr ? ctx.curriculum->skill(spec.skill.view()) : nullptr;
  const QuizBank* bank = nullptr;
  if (def != nullptr && ctx.banks != nullptr && !def->quiz_bank.empty()) {
    auto it = ctx.banks->find(def->quiz_bank);
    if (it != ctx.banks->end()) bank = &it->second;
  }

  std::vector<const QuizItem*> candidates;
  if (bank != nullptr) {
    for (const auto& item : *bank) {
      if (spec.diff.tier >= item.tier_min && spec.diff.tier <= item.tier_max) {
        candidates.push_back(&item);
      }
    }
    if (candidates.empty()) {
      for (const auto& item : *bank) candidates.push_back(&item);
    }
  }
  if (candidates.empty()) {
    // No bank shipped for this topic: degrade to a number question instead of
    // crashing a child's session.
    return tmplFallback(spec, rng, ctx);
  }

  const QuizItem& item = *candidates[static_cast<std::size_t>(
      rng.nextInt(0, static_cast<int>(candidates.size()) - 1))];

  Puzzle puzzle;
  puzzle.prompt = item.prompt;
  puzzle.hint = item.hint;

  std::vector<std::string> wrong = item.distractors;
  rng.shuffle(wrong);
  if (bank != nullptr) {
    for (const auto& other : *bank) {
      if (static_cast<int>(wrong.size()) >= std::max(1, spec.diff.optionCount()) - 1) break;
      if (other.answer == item.answer) continue;
      if (std::find(wrong.begin(), wrong.end(), other.answer) != wrong.end()) continue;
      wrong.push_back(other.answer);
    }
  }
  finalizeText(puzzle, spec, wrong, rng, item.answer);
  return puzzle;
}

Puzzle tmplFallback(const LevelSpec& spec, Rng& rng, const GeneratorContext&) {
  Puzzle puzzle;
  const int lo = std::max(1, spec.diff.min_value);
  const int hi = std::max(lo + 2, spec.diff.max_value);
  const int a = rng.nextInt(lo, hi);
  const int b = rng.nextInt(lo, hi);

  puzzle.prompt = itos(a) + " + " + itos(b) + " = ?";
  puzzle.hint = "Count on from the bigger number.";
  finalizeNumeric(puzzle, spec, a + b, rng, 0, a + b + 20);
  return puzzle;
}

}  // namespace

int Puzzle::correctIndex() const {
  for (std::size_t i = 0; i < options.size(); ++i) {
    if (options[i].correct) return static_cast<int>(i);
  }
  return -1;
}

bool Puzzle::wellFormed() const {
  if (prompt.empty() || options.size() < 2) return false;
  int correct_count = 0;
  for (const auto& option : options) {
    if (option.text.empty()) return false;
    if (option.correct) ++correct_count;
  }
  if (!route.empty()) {
    if (spec.skill!=skills::Navigation || correct_count!=0) return false;
    std::vector<bool> used(options.size(),false);
    for(int i:route){
      if(i<0 || i>=int(options.size()) || used[i]) return false;
      used[i]=true;
    }
    return true;
  }
  if (correct_count != 1) return false;

  for (std::size_t i = 0; i < options.size(); ++i) {
    for (std::size_t j = i + 1; j < options.size(); ++j) {
      if (options[i].text == options[j].text) return false;
    }
  }
  if (!answer.empty() && options[static_cast<std::size_t>(correctIndex())].text != answer) {
    return false;
  }
  if (spec.skill.empty()) return false;
  return true;
}

std::vector<int> LevelGenerator::makeDistractors(int correct, int count, int lo, int hi, Rng& rng) {
  std::vector<int> candidates;
  if (count <= 0) return candidates;

  const bool bounded_above = hi > lo;
  auto push = [&](int value) {
    if (value == correct) return;
    if (value < lo) return;
    if (bounded_above && value > hi) return;
    if (std::find(candidates.begin(), candidates.end(), value) != candidates.end()) return;
    candidates.push_back(value);
  };

  // The near-misses children actually make, in rough order of frequency.
  push(correct + 1);
  push(correct - 1);
  push(correct + 2);
  push(correct - 2);
  push(correct + 10);
  push(correct - 10);
  push(correct + 11);
  push(correct - 11);
  if (correct >= 10) {
    const int tens = (correct / 10) * 10;
    push(tens);       // dropped the units digit
    push(tens + 10);  // wrong ten
  }
  if (correct >= 100) push(correct + 100);

  rng.shuffle(candidates);
  if (static_cast<int>(candidates.size()) > count) candidates.resize(static_cast<std::size_t>(count));

  // Only widen the search if the near-miss pool was too small.
  for (int value = lo; value <= hi && static_cast<int>(candidates.size()) < count; ++value) {
    push(value);
  }
  int filler = 1;
  while (static_cast<int>(candidates.size()) < count) {
    push(correct + 1000 * filler);
    if (static_cast<int>(candidates.size()) < count) push(correct - 1000 * filler);
    push(correct + 1000 * filler);
    ++filler;
    if (filler > 1000) break;
  }
  return candidates;
}

LevelGenerator::LevelGenerator() : LevelGenerator(makeDefaultCurriculum()) {}

LevelGenerator::LevelGenerator(const Curriculum& curriculum) : owned_(curriculum) {
  curriculum_ = &owned_;
  registerTemplates();
}

void LevelGenerator::registerTemplates() {
  registerTemplate("math.counting", tmplCounting);
  registerTemplate("math.addition", tmplAddition);
  registerTemplate("math.subtraction", tmplSubtraction);
  registerTemplate("logic.patterns", tmplPatterns);
  registerTemplate("logic.odd_one_out", tmplOddOneOut);
  registerTemplate("logic.ordering", tmplOrdering);
  registerTemplate("quiz.bank", tmplQuizBank);
  registerTemplate("mixed.boss", tmplMixedExpression);
  registerTemplate("generic.fallback", tmplFallback);
}

void LevelGenerator::registerTemplate(const std::string& id, TemplateFn fn) {
  templates_[id] = std::move(fn);
}

void LevelGenerator::addQuizBank(const std::string& name, QuizBank bank) {
  banks_[name] = std::move(bank);
}

const QuizBank* LevelGenerator::quizBank(const std::string& name) const {
  auto it = banks_.find(name);
  return it == banks_.end() ? nullptr : &it->second;
}

std::vector<std::string> LevelGenerator::templateIds() const {
  std::vector<std::string> ids;
  for (const auto& entry : templates_) ids.push_back(entry.first);
  return ids;
}

GeneratorContext LevelGenerator::context() const {
  GeneratorContext ctx;
  ctx.curriculum = curriculum_;
  ctx.banks = &banks_;
  return ctx;
}

Puzzle LevelGenerator::generate(const LevelSpec& spec) const {
  const SkillDef* def =
      curriculum_ != nullptr ? curriculum_->skill(spec.skill.view()) : nullptr;
  std::string template_id = def != nullptr ? def->template_id : std::string{};

  auto it = templates_.find(template_id);
  if (it == templates_.end()) {
    // A content typo must never crash a child's session.
    it = templates_.find("generic.fallback");
  }
  if (it == templates_.end()) throw std::runtime_error("no templates registered");

  Rng rng(spec.seed);
  const GeneratorContext ctx = context();
  Puzzle puzzle;
  if (spec.skill == skills::Navigation) {
    const RouteRules rules=rulesForTrailTier(spec.diff.tier);
    std::vector<std::string> landmarks{"Pine","Moon","Sun","Bloom"};
    rng.shuffle(landmarks);
    for (int i=0; i<rules.anchors; ++i) {
      puzzle.route.push_back(i);
      puzzle.options.push_back({landmarks[i], false});
    }
    rng.shuffle(puzzle.route);
    puzzle.route.resize(rules.required);
    puzzle.prompt="Rig the safety line";
    puzzle.hint="Follow the trail blazes in order.";
  } else {
    puzzle=it->second(spec,rng,ctx);
  }
  puzzle.spec = spec;
  puzzle.spec_json = to_json(spec);
  puzzle.is_checkpoint = spec.kind == LevelKind::Checkpoint;
  return puzzle;
}

}  // namespace sdp
