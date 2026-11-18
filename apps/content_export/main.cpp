// ===========================================================================
// sdp_content_export - writes the compiled-in default curriculum as the
// editable content/curriculum.json.
//
// Why this exists: the shipped content file is generated from the same code the
// game falls back to, so the two can never silently drift apart. Teachers and
// translators then edit the JSON, and content/README explains the format.
//
//   sdp_content_export > content/curriculum.json
// ===========================================================================
#include <cstdio>
#include <string>

#include "sdp/content.hpp"

int main(int argc, char** argv) {
  const sdp::Curriculum curriculum = sdp::makeDefaultCurriculum();

  std::string validation;
  if (!curriculum.validate(&validation)) {
    std::fprintf(stderr, "default curriculum is invalid: %s\n", validation.c_str());
    return 1;
  }

  const std::string json = sdp::ContentLoader::writeCurriculumJson(curriculum);
  std::fputs(json.c_str(), stdout);
  std::fputc('\n', stdout);

  if (argc > 2) {
    std::fprintf(stderr, "usage: sdp_content_export [> curriculum.json]\n");
  }
  return 0;
}
