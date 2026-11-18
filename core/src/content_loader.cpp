#include "sdp/content.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace sdp {
namespace {

/// Directory the running executable lives in, so a portable install with a
/// "content" folder beside the binary works without any configuration.
std::string executableDirectory() {
#if defined(_WIN32)
  char buffer[4096] = {};
  const DWORD length = GetModuleFileNameA(nullptr, buffer, sizeof(buffer));
  if (length == 0 || length >= sizeof(buffer)) return {};
  return std::filesystem::path(std::string(buffer, length)).parent_path().string();
#elif defined(__linux__)
  std::error_code error;
  const auto resolved = std::filesystem::read_symlink("/proc/self/exe", error);
  return error ? std::string{} : resolved.parent_path().string();
#else
  return {};
#endif
}

bool hasCurriculum(const std::filesystem::path& directory) {
  std::error_code error;
  return std::filesystem::exists(directory / "curriculum.json", error);
}

std::string readFile(const std::string& path, LoadReport* report, const char* what) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    if (report != nullptr) report->error(std::string("cannot open ") + what + ": " + path);
    return {};
  }
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}

/// Minimal RFC-4180-ish CSV parser: quotes, doubled quotes and embedded commas.
std::vector<std::vector<std::string>> parseCsv(const std::string& text) {
  std::vector<std::vector<std::string>> rows;
  std::vector<std::string> row;
  std::string field;
  bool quoted = false;

  for (std::size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (quoted) {
      if (c == '"') {
        if (i + 1 < text.size() && text[i + 1] == '"') {
          field += '"';
          ++i;
        } else {
          quoted = false;
        }
      } else {
        field += c;
      }
    } else if (c == '"') {
      quoted = true;
    } else if (c == ',') {
      row.push_back(field);
      field.clear();
    } else if (c == '\n') {
      row.push_back(field);
      field.clear();
      rows.push_back(row);
      row.clear();
    } else if (c != '\r') {
      field += c;
    }
  }
  if (!field.empty() || !row.empty()) {
    row.push_back(field);
    rows.push_back(row);
  }
  return rows;
}

std::string trim(const std::string& text) {
  const auto first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const auto last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

int toInt(const std::string& text, int fallback) {
  try {
    return std::stoi(trim(text));
  } catch (...) {
    return fallback;
  }
}

DifficultyParams paramsFromJson(const nlohmann::json& j, int index) {
  DifficultyParams p;
  p.tier = j.value("tier", index);
  p.difficulty = j.value("difficulty", 0.0);
  p.min_value = j.value("min_value", 1);
  p.max_value = j.value("max_value", 10);
  p.operand_count = j.value("operand_count", 2);
  p.distractor_count = j.value("distractor_count", 3);
  p.time_limit_ms = j.value("time_limit_ms", 0);
  p.allow_carry = j.value("allow_carry", false);
  p.allow_borrow = j.value("allow_borrow", false);
  return p;
}

}  // namespace

std::string LoadReport::summary() const {
  std::ostringstream out;
  out << errors.size() << " error(s), " << warnings.size() << " warning(s)";
  for (const auto& message : errors) out << (char)10 << "  error: " << message;
  for (const auto& message : warnings) out << (char)10 << "  warning: " << message;
  return out.str();
}

void StringTable::set(std::string key, std::string value) { entries_[std::move(key)] = std::move(value); }

std::string StringTable::get(std::string_view key, std::string_view fallback) const {
  auto it = entries_.find(std::string(key));
  return it == entries_.end() ? std::string(fallback) : it->second;
}

std::string ContentLoader::defaultContentDir() {
  // Resolution order, most explicit first. SDP_CONTENT_DIR is a RUNTIME
  // environment variable, not a compile-time constant: baking a build-machine
  // path into the library would break the portable, offline install that is
  // the whole point of the deployment story.
  //
  //   1. SDP_CONTENT_DIR environment variable (tests, CI, custom installs)
  //   2. "content" beside the executable (portable build layout)
  //   3. "content" relative to the working directory (running from a checkout)
  if (const char* override_dir = std::getenv("SDP_CONTENT_DIR")) {
    if (override_dir[0] != 0) return std::string(override_dir);
  }

  const std::string exe_dir = executableDirectory();
  if (!exe_dir.empty()) {
    const std::filesystem::path base(exe_dir);
    const std::filesystem::path candidates[] = {
        base / "content",
        base.parent_path() / "content",                       // build/apps -> build/content
        base.parent_path().parent_path() / "content",         // build/apps -> repo/content
    };
    for (const auto& candidate : candidates) {
      if (hasCurriculum(candidate)) return candidate.string();
    }
  }

  if (hasCurriculum(std::filesystem::path("content"))) return std::string("content");
  return std::string("content");
}

Curriculum ContentLoader::loadCurriculum(const std::string& path, LoadReport* report) {
  const std::string text = readFile(path, report, "curriculum");
  if (trim(text).empty()) {
    if (report != nullptr && report->errors.empty()) {
      report->error("curriculum file is empty or unreadable: " + path);
    }
    return makeDefaultCurriculum();
  }

  Curriculum curriculum;
  try {
    const auto root = nlohmann::json::parse(text);

    curriculum.setLandmarkInterval(root.value("landmark_interval", 10));
    curriculum.setSessionLength(root.value("session_length", 10));
    curriculum.setReviewInterval(root.value("review_interval", 5));

    for (const auto& j : root.value("skills", nlohmann::json::array())) {
      SkillDef def;
      def.skill = SubSkill{j.value("id", std::string{})};
      const auto domain = domain_from_string(j.value("domain", std::string("math")));
      def.domain = domain.value_or(Domain::Math);
      if (!domain.has_value() && report != nullptr) {
        report->warn("skill '" + def.skill.id + "' has unknown domain, defaulting to math");
      }
      def.display_key = j.value("display_key", "skill." + def.skill.id);
      def.template_id = j.value("template", std::string{});
      def.quiz_bank = j.value("quiz_bank", std::string{});

      int index = 0;
      for (const auto& tier : j.value("tiers", nlohmann::json::array())) {
        def.tiers.push_back(paramsFromJson(tier, index));
        ++index;
      }
      curriculum.addSkill(std::move(def));
    }

    for (const auto& j : root.value("zones", nlohmann::json::array())) {
      ZoneDef zone;
      zone.id = j.value("id", 0);
      zone.key = j.value("key", std::string{});
      zone.display_key = j.value("display_key", "zone." + zone.key);
      zone.altitude_start = j.value("altitude_start", 0);
      zone.altitude_end = j.value("altitude_end", 0);
      zone.palette = j.value("palette", zone.key);
      for (const auto& unlock : j.value("unlocks", nlohmann::json::array())) {
        if (unlock.is_string()) zone.unlocks.push_back(SubSkill{unlock.get<std::string>()});
      }
      curriculum.addZone(std::move(zone));
    }
  } catch (const std::exception& error) {
    if (report != nullptr) report->error(std::string("curriculum JSON is invalid: ") + error.what());
    return makeDefaultCurriculum();
  }

  std::string validation;
  if (!curriculum.validate(&validation)) {
    if (report != nullptr) report->error("curriculum failed validation: " + validation);
    return makeDefaultCurriculum();
  }
  return curriculum;
}

QuizBank ContentLoader::loadQuizBank(const std::string& path, LoadReport* report) {
  QuizBank bank;
  const std::string text = readFile(path, report, "quiz bank");
  if (text.empty()) return bank;

  const auto rows = parseCsv(text);
  bool first_content_row = true;
  for (const auto& row : rows) {
    if (row.empty()) continue;
    const std::string head = trim(row[0]);
    if (head.empty() || head[0] == '#') continue;
    if (first_content_row) {
      first_content_row = false;
      if (head == "prompt") continue;  // header line
    }

    QuizItem item;
    item.prompt = trim(row[0]);
    if (row.size() > 1) item.answer = trim(row[1]);
    for (std::size_t i = 2; i < row.size() && i <= 4; ++i) {
      const std::string option = trim(row[i]);
      if (!option.empty() && option != item.answer) item.distractors.push_back(option);
    }
    if (row.size() > 5) item.hint = trim(row[5]);
    if (row.size() > 6) item.tier_min = toInt(row[6], 0);
    if (row.size() > 7) item.tier_max = toInt(row[7], 999);

    if (item.prompt.empty() || item.answer.empty()) {
      if (report != nullptr) report->warn("skipping incomplete quiz row in " + path);
      continue;
    }
    if (item.distractors.size() < 2 && report != nullptr) {
      report->warn("quiz row '" + item.prompt + "' has fewer than two distractors");
    }
    bank.push_back(std::move(item));
  }
  return bank;
}

StringTable ContentLoader::loadStrings(const std::string& path, LoadReport* report) {
  StringTable table;
  const std::string text = readFile(path, report, "strings");
  if (text.empty()) return table;
  try {
    const auto root = nlohmann::json::parse(text);
    for (auto it = root.begin(); it != root.end(); ++it) {
      if (it.value().is_string()) table.set(it.key(), it.value().get<std::string>());
    }
  } catch (const std::exception& error) {
    if (report != nullptr) report->error(std::string("strings JSON is invalid: ") + error.what());
  }
  return table;
}

Curriculum ContentLoader::loadContentDirectory(const std::string& directory,
                                               LevelGenerator* generator,
                                               LoadReport* report) {
  Curriculum curriculum = loadCurriculum(directory + "/curriculum.json", report);

  if (generator != nullptr) {
    StringTable ignored;  // banks are the only content the generator needs
    (void)ignored;
    for (const auto& def : curriculum.skills()) {
      if (def.quiz_bank.empty()) continue;
      const QuizBank bank = loadQuizBank(directory + "/quiz/" + def.quiz_bank + ".csv", report);
      if (!bank.empty()) {
        generator->addQuizBank(def.quiz_bank, bank);
      } else if (report != nullptr) {
        report->warn("quiz bank '" + def.quiz_bank + "' is empty or missing");
      }
    }
  }
  return curriculum;
}

std::string ContentLoader::writeCurriculumJson(const Curriculum& curriculum) {
  nlohmann::json root;
  root["schema"] = 1;
  root["landmark_interval"] = curriculum.landmarkInterval();
  root["session_length"] = curriculum.sessionLength();
  root["review_interval"] = curriculum.reviewInterval();

  nlohmann::json skills = nlohmann::json::array();
  for (const auto& def : curriculum.skills()) {
    nlohmann::json j;
    j["id"] = def.skill.id;
    j["domain"] = std::string(to_string(def.domain));
    j["display_key"] = def.display_key;
    j["template"] = def.template_id;
    j["quiz_bank"] = def.quiz_bank;

    nlohmann::json tiers = nlohmann::json::array();
    for (const auto& tier : def.tiers) {
      tiers.push_back(nlohmann::json{{"tier", tier.tier},
                                     {"difficulty", tier.difficulty},
                                     {"min_value", tier.min_value},
                                     {"max_value", tier.max_value},
                                     {"operand_count", tier.operand_count},
                                     {"distractor_count", tier.distractor_count},
                                     {"time_limit_ms", tier.time_limit_ms},
                                     {"allow_carry", tier.allow_carry},
                                     {"allow_borrow", tier.allow_borrow}});
    }
    j["tiers"] = tiers;
    skills.push_back(j);
  }
  root["skills"] = skills;

  nlohmann::json zones = nlohmann::json::array();
  for (const auto& zone : curriculum.zones()) {
    nlohmann::json unlocks = nlohmann::json::array();
    for (const auto& skill : zone.unlocks) unlocks.push_back(skill.id);
    zones.push_back(nlohmann::json{{"id", zone.id},
                                   {"key", zone.key},
                                   {"display_key", zone.display_key},
                                   {"altitude_start", zone.altitude_start},
                                   {"altitude_end", zone.altitude_end},
                                   {"palette", zone.palette},
                                   {"unlocks", unlocks}});
  }
  root["zones"] = zones;
  return root.dump(2);
}

}  // namespace sdp
