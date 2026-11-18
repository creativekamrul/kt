#pragma once
// ===========================================================================
// sdp/content.hpp - human-editable content.
//
// State lives in SQLite; content lives in text files. Anything a ministry of
// education, a teacher or a translator might want to change is a data file:
// the curriculum, the difficulty tables, the quiz banks and the UI strings.
// Changing them requires no recompile - that is the localisation argument in
// the government-program pitch.
// ===========================================================================

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "sdp/curriculum.hpp"
#include "sdp/generator.hpp"
#include "sdp/types.hpp"

namespace sdp {

struct LoadReport {
  std::vector<std::string> errors;
  std::vector<std::string> warnings;

  bool ok() const { return errors.empty(); }
  void error(std::string message) { errors.push_back(std::move(message)); }
  void warn(std::string message) { warnings.push_back(std::move(message)); }
  std::string summary() const;
};

/// Localisation table. Every user-visible string is looked up by key with a
/// built-in fallback, so a missing translation degrades instead of breaking.
class StringTable {
 public:
  void set(std::string key, std::string value);
  std::string get(std::string_view key, std::string_view fallback) const;
  std::size_t size() const noexcept { return entries_.size(); }
  const std::map<std::string, std::string>& entries() const noexcept { return entries_; }

 private:
  std::map<std::string, std::string> entries_;
};

class ContentLoader {
 public:
  /// Resolved from the SDP_CONTENT_DIR compile definition, else "content".
  static std::string defaultContentDir();

  static Curriculum loadCurriculum(const std::string& path, LoadReport* report = nullptr);
  static QuizBank loadQuizBank(const std::string& path, LoadReport* report = nullptr);
  static StringTable loadStrings(const std::string& path, LoadReport* report = nullptr);

  /// Loads curriculum.json and every quiz bank it references into the
  /// generator. Falls back to the built-in curriculum when files are missing,
  /// so the game always starts.
  static Curriculum loadContentDirectory(const std::string& directory, LevelGenerator* generator,
                                         LoadReport* report = nullptr);

  /// Serialisation used by the sdp_content_export tool, which keeps the shipped
  /// content/curriculum.json in sync with the compiled-in defaults.
  static std::string writeCurriculumJson(const Curriculum& curriculum);
};

}  // namespace sdp
