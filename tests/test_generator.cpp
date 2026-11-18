#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "sdp/generator.hpp"
#include "test_framework.hpp"

using namespace sdp;

namespace {

LevelSpec specFor(const Curriculum& curriculum, const SubSkill& skill, int tier,
                  std::uint32_t seed, LevelKind kind = LevelKind::Practice) {
  LevelSpec spec;
  const SkillDef* def = curriculum.skill(skill.view());
  if (def == nullptr) return spec;
  const int tiers = static_cast<int>(def->tiers.size());
  const int clamped = std::max(0, std::min(tier, tiers - 1));
  spec.domain = def->domain;
  spec.skill = def->skill;
  spec.diff = def->tiers[static_cast<std::size_t>(clamped)];
  spec.diff.tier = clamped;
  spec.kind = kind;
  spec.seed = seed;
  spec.child_id = 1;
  return spec;
}

std::vector<LevelSpec> sampleSpecs(const Curriculum& curriculum, int per_skill,
                                   std::uint32_t base_seed) {
  std::vector<LevelSpec> specs;
  std::uint32_t rolling = base_seed;
  for (const auto& def : curriculum.skills()) {
    const int tiers = static_cast<int>(def.tiers.size());
    for (int i = 0; i < per_skill; ++i) {
      const int tier = tiers <= 1 ? 0 : (i % tiers);
      specs.push_back(specFor(curriculum, def.skill, tier,
                              deriveSeed({base_seed, ++rolling, hashString(def.skill.view())})));
    }
  }
  return specs;
}

std::vector<int> numbersIn(const std::string& text) {
  std::vector<int> out;
  std::string current;
  for (char ch : text) {
    if (ch >= '0' && ch <= '9') {
      current += ch;
    } else if (!current.empty()) {
      out.push_back(std::stoi(current));
      current.clear();
    }
  }
  if (!current.empty()) out.push_back(std::stoi(current));
  return out;
}

std::string flatten(const Puzzle& puzzle) {
  std::string text = puzzle.prompt + "|" + puzzle.answer;
  for (const auto& option : puzzle.options) {
    text += "|" + option.text + (option.correct ? "*" : "");
  }
  return text;
}

}  // namespace

SDP_TEST(generator, every_sampled_spec_is_well_formed) {
  const Curriculum curriculum = makeDefaultCurriculum();
  LevelGenerator generator(curriculum);

  int generated = 0;
  for (const auto& spec : sampleSpecs(curriculum, 12, 4242)) {
    const Puzzle puzzle = generator.generate(spec);
    CHECK(puzzle.wellFormed());
    CHECK_EQ(puzzle.options.size(), static_cast<std::size_t>(spec.diff.optionCount()));
    CHECK_EQ(puzzle.correctIndex(),
             [&] {
               for (std::size_t i = 0; i < puzzle.options.size(); ++i) {
                 if (puzzle.options[i].correct) return static_cast<int>(i);
               }
               return -1;
             }());
    CHECK_EQ(puzzle.spec.seed, spec.seed);
    CHECK(!puzzle.spec_json.empty());
    ++generated;
  }
  CHECK_EQ(generated, 9 * 12);
}

SDP_TEST(generator, generation_is_deterministic) {
  const Curriculum curriculum = makeDefaultCurriculum();
  LevelGenerator first(curriculum);
  LevelGenerator second(curriculum);

  for (const auto& spec : sampleSpecs(curriculum, 6, 777)) {
    const Puzzle a = first.generate(spec);
    const Puzzle b = first.generate(spec);   // same generator, twice
    const Puzzle c = second.generate(spec);  // independent generator
    CHECK_EQ(flatten(a), flatten(b));
    CHECK_EQ(flatten(a), flatten(c));
  }
}

SDP_TEST(generator, different_seeds_give_different_puzzles) {
  const Curriculum curriculum = makeDefaultCurriculum();
  LevelGenerator generator(curriculum);

  std::set<std::string> distinct;
  for (int i = 0; i < 12; ++i) {
    const LevelSpec spec = specFor(curriculum, skills::Addition, 3, 1000u + i * 13u);
    distinct.insert(flatten(generator.generate(spec)));
  }
  CHECK(distinct.size() >= 6);  // a seed that does nothing would give 1
}

SDP_TEST(generator, addition_answers_are_arithmetically_correct) {
  const Curriculum curriculum = makeDefaultCurriculum();
  LevelGenerator generator(curriculum);

  for (int i = 0; i < 200; ++i) {
    const LevelSpec spec = specFor(curriculum, skills::Addition, i % 12, 500u + i);
    const Puzzle puzzle = generator.generate(spec);
    const std::vector<int> terms = numbersIn(puzzle.prompt);
    CHECK(terms.size() >= 2);

    int sum = 0;
    for (int term : terms) sum += term;
    CHECK_EQ(std::stoi(puzzle.answer), sum);
    CHECK_EQ(puzzle.options[static_cast<std::size_t>(puzzle.correctIndex())].text, puzzle.answer);
  }
}

SDP_TEST(generator, subtraction_answers_are_arithmetically_correct) {
  const Curriculum curriculum = makeDefaultCurriculum();
  LevelGenerator generator(curriculum);

  for (int i = 0; i < 200; ++i) {
    const LevelSpec spec = specFor(curriculum, skills::Subtraction, i % 12, 900u + i);
    const Puzzle puzzle = generator.generate(spec);
    const std::vector<int> numbers = numbersIn(puzzle.prompt);
    CHECK_EQ(numbers.size(), static_cast<std::size_t>(2));
    CHECK(numbers[0] >= numbers[1]);  // no negative results for 6-10 year olds
    CHECK_EQ(std::stoi(puzzle.answer), numbers[0] - numbers[1]);
  }
}

SDP_TEST(generator, patterns_follow_an_arithmetic_rule) {
  const Curriculum curriculum = makeDefaultCurriculum();
  LevelGenerator generator(curriculum);

  for (int i = 0; i < 120; ++i) {
    const LevelSpec spec = specFor(curriculum, skills::Patterns, i % 12, 1300u + i);
    const Puzzle puzzle = generator.generate(spec);
    const std::vector<int> terms = numbersIn(puzzle.prompt);
    CHECK(terms.size() >= 3);

    const int step = terms[1] - terms[0];
    for (std::size_t k = 1; k < terms.size(); ++k) {
      CHECK_EQ(terms[k] - terms[k - 1], step);
    }
    CHECK_EQ(std::stoi(puzzle.answer), terms.back() + step);
  }
}

SDP_TEST(generator, checkpoint_bosses_mix_addition_and_subtraction) {
  const Curriculum curriculum = makeDefaultCurriculum();
  LevelGenerator generator(curriculum);

  int checked = 0;
  for (int i = 0; i < 30; ++i) {
    const LevelSpec spec =
        specFor(curriculum, skills::Addition, 6, 2100u + i, LevelKind::Checkpoint);
    const Puzzle puzzle = generator.generate(spec);
    CHECK(puzzle.is_checkpoint);
    CHECK(puzzle.prompt.find('+') != std::string::npos);
    CHECK(puzzle.prompt.find('-') != std::string::npos);
    ++checked;
  }
  CHECK_EQ(checked, 30);
}

SDP_TEST(generator, quiz_bank_rows_are_used) {
  const Curriculum curriculum = makeDefaultCurriculum();
  LevelGenerator generator(curriculum);

  QuizBank bank;
  bank.push_back(QuizItem{"Which animal says moo?", "cow", {"dog", "cat", "duck"}, "It gives milk.",
                          0, 5});
  bank.push_back(QuizItem{"Which animal is the largest?", "whale", {"ant", "mouse", "cat"},
                          "It lives in the sea.", 6, 11});
  bank.push_back(QuizItem{"How many legs does a spider have?", "8", {"6", "4", "10"},
                          "Count them!", 6, 11});
  bank.push_back(QuizItem{"What do bees make?", "honey", {"milk", "bread", "juice"},
                          "It is sweet.", 0, 5});
  generator.addQuizBank("animals", bank);

  std::set<std::string> prompts;
  for (int i = 0; i < 40; ++i) {
    const LevelSpec spec = specFor(curriculum, skills::QuizAnimals, i % 12, 3300u + i);
    const Puzzle puzzle = generator.generate(spec);
    CHECK(puzzle.wellFormed());
    prompts.insert(puzzle.prompt);

    bool known = false;
    for (const auto& item : bank) {
      if (item.prompt == puzzle.prompt && item.answer == puzzle.answer) known = true;
    }
    CHECK(known);
  }
  CHECK(prompts.size() >= 2);  // more than one row is reachable
}

SDP_TEST(generator, unknown_template_degrades_gracefully) {
  Curriculum curriculum = makeDefaultCurriculum();
  SkillDef broken;
  broken.skill = SubSkill{"math.broken"};
  broken.domain = Domain::Math;
  broken.template_id = "does.not.exist";
  broken.tiers = curriculum.skill("math.addition")->tiers;
  curriculum.addSkill(broken);

  LevelGenerator generator(curriculum);
  const Puzzle puzzle = generator.generate(specFor(curriculum, broken.skill, 2, 4711));
  CHECK(puzzle.wellFormed());
  CHECK_EQ(puzzle.options.size(), static_cast<std::size_t>(4));
}

SDP_TEST(generator, text_options_never_duplicate_the_answer) {
  const Curriculum curriculum = makeDefaultCurriculum();
  LevelGenerator generator(curriculum);

  // A thin bank forces the padding path of finalizeText.
  QuizBank thin;
  thin.push_back(QuizItem{"Pick the red one.", "red", {}, "", 0, 99});
  generator.addQuizBank("safety", thin);

  for (int i = 0; i < 40; ++i) {
    const LevelSpec spec = specFor(curriculum, skills::QuizSafety, i % 12, 6100u + i);
    const Puzzle puzzle = generator.generate(spec);
    CHECK(puzzle.wellFormed());
    CHECK_EQ(puzzle.options.size(), static_cast<std::size_t>(spec.diff.optionCount()));
    CHECK_EQ(puzzle.answer, std::string("red"));
  }
}
