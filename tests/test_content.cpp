#include <cstdio>
#include <fstream>
#include <string>

#include "sdp/content.hpp"
#include "test_framework.hpp"

using namespace sdp;

namespace {

std::string contentDir() { return ContentLoader::defaultContentDir(); }

/// Fails with the loader's own diagnostics, so a broken content file reports
/// WHICH file and WHY (missing, bad JSON, failed validation) instead of a bare
/// boolean.
void requireOk(const LoadReport& report, const char* file, int line) {
  if (!report.ok()) sdptest::fail("content report not ok: " + report.summary(), file, line);
}

}  // namespace

#define REQUIRE_OK(report) requireOk((report), __FILE__, __LINE__)

SDP_TEST(content, shipped_curriculum_loads_and_validates) {
  LoadReport report;
  const Curriculum curriculum =
      ContentLoader::loadCurriculum(contentDir() + "/curriculum.json", &report);

  // The shipped file must be loadable: if this fails, the product would ship
  // with broken content and silently fall back.
  REQUIRE_OK(report);
  std::string error;
  CHECK(curriculum.validate(&error));

  CHECK_EQ(curriculum.skills().size(), static_cast<std::size_t>(9));
  CHECK_EQ(curriculum.zoneCount(), 4);
  CHECK_EQ(curriculum.tierCount("math.addition"), 12);
  CHECK_EQ(curriculum.landmarkInterval(), 10);
  CHECK_EQ(curriculum.sessionLength(), 10);
  CHECK_EQ(curriculum.zoneForAltitude(95)->key, std::string("summit"));

  const DifficultyParams* easy = curriculum.paramsFor("math.addition", 0);
  const DifficultyParams* hard = curriculum.paramsFor("math.addition", 11);
  CHECK(easy != nullptr && hard != nullptr);
  CHECK(hard->max_value > easy->max_value);
  CHECK(hard->allow_carry);
}

SDP_TEST(content, quiz_banks_load_from_csv) {
  LoadReport report;
  const QuizBank bank = ContentLoader::loadQuizBank(contentDir() + "/quiz/animals.csv", &report);
  REQUIRE_OK(report);
  CHECK(bank.size() >= 10);

  int easy = 0;
  int hard = 0;
  for (const auto& item : bank) {
    CHECK(!item.prompt.empty());
    CHECK(!item.answer.empty());
    CHECK(item.distractors.size() >= 3);
    for (const auto& distractor : item.distractors) CHECK_NE(distractor, item.answer);
    if (item.tier_min <= 3) ++easy;
    if (item.tier_min >= 8) ++hard;
  }
  CHECK(easy >= 2);  // every difficulty band is covered
  CHECK(hard >= 2);
}

SDP_TEST(content, csv_parser_handles_quotes_commas_and_comments) {
  const std::string path = "sdp100_csv_edge_test.csv";
  {
    std::ofstream out(path);
    const char q = 34;
    const char nl = 10;
    out << "prompt,answer,distractor1,distractor2,distractor3,hint,tier_min,tier_max" << nl;
    out << q << "Which one is red, orange or yellow?" << q << ",red,blue,green,purple," << q
        << "Warm colours." << q << ",0,3" << nl;
    out << "# a comment line" << nl;
    out << nl;
    out << "Pick the biggest.,9,1,2,3," << q << "Count up." << q << ",4,7" << nl;
    out << q << "He said " << q << q << "hello" << q << q << " loudly." << q << ",yes,no,maybe,"
        << "never,,8,11" << nl;
  }

  LoadReport report;
  const QuizBank bank = ContentLoader::loadQuizBank(path, &report);
  std::remove(path.c_str());

  CHECK(report.ok());
  CHECK_EQ(bank.size(), static_cast<std::size_t>(3));
  CHECK_EQ(bank[0].prompt, std::string("Which one is red, orange or yellow?"));
  CHECK_EQ(bank[0].hint, std::string("Warm colours."));
  CHECK_EQ(bank[0].distractors.size(), static_cast<std::size_t>(3));
  CHECK_EQ(bank[1].tier_min, 4);
  CHECK_EQ(bank[2].prompt, std::string("He said \"hello\" loudly."));
}

SDP_TEST(content, missing_files_degrade_to_shipped_defaults) {
  LoadReport report;
  const Curriculum curriculum = ContentLoader::loadCurriculum("no/such/curriculum.json", &report);
  CHECK(!report.ok());  // the failure is reported, not hidden
  std::string error;
  CHECK(curriculum.validate(&error));
  CHECK_EQ(curriculum.skills().size(), static_cast<std::size_t>(9));
}

SDP_TEST(content, content_directory_wires_quiz_banks_into_the_generator) {
  LevelGenerator generator;
  LoadReport report;
  const Curriculum curriculum =
      ContentLoader::loadContentDirectory(contentDir(), &generator, &report);
  REQUIRE_OK(report);
  CHECK(generator.quizBank("animals") != nullptr);
  CHECK(generator.quizBank("science") != nullptr);
  CHECK(generator.quizBank("safety") != nullptr);

  const SkillDef* def = curriculum.skill("quiz.animals");
  CHECK(def != nullptr);
  LevelSpec spec;
  spec.domain = def->domain;
  spec.skill = def->skill;
  spec.diff = def->tiers[3];
  spec.diff.tier = 3;
  spec.seed = 12345u;
  spec.child_id = 1;

  const Puzzle puzzle = generator.generate(spec);
  CHECK(puzzle.wellFormed());

  bool from_bank = false;
  for (const auto& item : *generator.quizBank("animals")) {
    if (item.prompt == puzzle.prompt && item.answer == puzzle.answer) from_bank = true;
  }
  CHECK(from_bank);
}

SDP_TEST(content, strings_table_uses_keys_with_fallbacks) {
  const StringTable table = ContentLoader::loadStrings(contentDir() + "/strings.json");
  CHECK(table.size() > 5);
  CHECK_EQ(table.get("zone.forest", "?"), std::string("the forest"));
  CHECK_EQ(table.get("skill.math.addition", "?"), std::string("Adding"));
  CHECK_EQ(table.get("does.not.exist", "fallback"), std::string("fallback"));
}

SDP_TEST(content, exported_curriculum_round_trips) {
  const Curriculum original = makeDefaultCurriculum();
  const std::string json = ContentLoader::writeCurriculumJson(original);

  const std::string path = "sdp100_roundtrip_test.json";
  {
    std::ofstream out(path);
    out << json;
  }

  LoadReport report;
  const Curriculum loaded = ContentLoader::loadCurriculum(path, &report);
  std::remove(path.c_str());

  CHECK(report.ok());
  CHECK_EQ(loaded.skills().size(), original.skills().size());
  CHECK_EQ(loaded.zoneCount(), original.zoneCount());
  CHECK_EQ(loaded.tierCount("math.addition"), original.tierCount("math.addition"));
  CHECK_EQ(loaded.paramsFor("math.addition", 11)->max_value,
           original.paramsFor("math.addition", 11)->max_value);
  CHECK_EQ(loaded.paramsFor("math.addition", 11)->allow_carry, true);
  CHECK_EQ(loaded.sessionLength(), original.sessionLength());
  CHECK_EQ(loaded.zones().back().key, original.zones().back().key);
  CHECK_EQ(loaded.skills().back().quiz_bank, original.skills().back().quiz_bank);
}
