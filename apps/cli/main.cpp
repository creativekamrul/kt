// ===========================================================================
// sdp_cli - the console shell.
//
// This is a real shell, not a throwaway: it drives the same SessionRunner,
// AdaptiveEngine and SqliteStore the graphical build will use. Having two shells
// over one core (this and the optional SFML one) is the concrete proof of the
// "engine-agnostic core" claim, and it is the only way to verify the whole game
// loop on a machine with no display.
//
// Usage:
//   sdp_cli --db sdp100.db --name Ada          # interactive
//   sdp_cli --db sdp100.db --demo 12           # scripted session (CI friendly)
//   sdp_cli --db sdp100.db --list
// ===========================================================================
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "sdp/content.hpp"
#include "sdp/engine.hpp"
#include "sdp/generator.hpp"
#include "sdp/progression.hpp"
#include "sdp/session.hpp"
#include "sdp/store.hpp"
#include "sdp/view.hpp"

namespace {

using namespace sdp;

struct Options {
  std::string db_path = "sdp100.db";
  std::string name = "Player";
  std::string content_dir;
  int session_length = 10;
  int demo = 0;
  bool list = false;
  bool reset = false;
};

void printBanner() {
  std::cout << (char)10
            << "  SDP 100 - adaptive learning game (console shell)" << (char)10
            << "  Climb the mountain one solved puzzle at a time." << (char)10;
}

/// A deliberately crude ASCII mountain. The point is the marker moving up; the
/// SFML shell replaces this with the real parallax art.
void renderMountain(const Curriculum& curriculum, const ProgressionState& state,
                    const StringTable& strings, std::ostream& out) {
  constexpr int kRows = 7;
  const ZoneDef* zone = curriculum.zone(state.zone_id);
  const std::string zone_name =
      zone != nullptr ? strings.get(zone->display_key, zone->key) : std::string("the mountain");

  // Shared with the graphical shell, and unit tested: see sdp/view.hpp.
  const int marker_row = markerRow(state.altitude, zone, kRows);

  out << (char)10;
  for (int row = 0; row < kRows; ++row) {
    const int width = 2 * row + 1;
    std::string line(static_cast<std::size_t>(std::max(0, kRows - row)), ' ');
    line += "/";
    line.append(static_cast<std::size_t>(width), row == 0 ? '_' : ' ');
    line += (char)92;  // backslash, written numerically to survive editors
    if (row == marker_row) line += "   <-- you are here";
    out << line << (char)10;
  }
  out << "  altitude " << state.altitude << " - " << zone_name << (char)10;
}

void renderPuzzle(const Puzzle& puzzle, std::ostream& out) {
  out << (char)10 << "  " << puzzle.prompt << (char)10 << (char)10;
  for (std::size_t i = 0; i < puzzle.options.size(); ++i) {
    out << "    " << (i + 1) << ") " << puzzle.options[i].text << (char)10;
  }
}

void printResult(const SessionResult& result, std::ostream& out) {
  out << (char)10 << "  " << result.feedback << (char)10;
  for (const auto& milestone : result.new_milestones) {
    out << "  * new badge: " << milestone << (char)10;
  }
  if (result.landmark_reached) {
    out << "  * landmark reached at altitude " << result.altitude << (char)10;
  }
}

int runInteractive(SessionRunner& runner, const Curriculum& curriculum,
                   const StringTable& strings, const Options& options) {
  while (!runner.complete()) {
    renderMountain(curriculum, runner.progressionState(), strings, std::cout);
    const Puzzle puzzle = runner.nextPuzzle();
    renderPuzzle(puzzle, std::cout);
    if (puzzle.is_checkpoint) std::cout << "  (checkpoint: this one mixes what you know)" << (char)10;

    std::cout << (char)10 << "  your answer (1-" << puzzle.options.size() << "): ";
    int choice = 0;
    if (!(std::cin >> choice)) {
      std::cout << (char)10 << "  input closed - stopping the session here." << (char)10;
      break;
    }
    const SessionResult result = runner.submitAnswer(choice - 1, 3000);
    printResult(result, std::cout);
  }

  const SessionSummary summary = runner.summary();
  std::cout << (char)10 << "  === " << summary.headline << " ===" << (char)10
            << "  solved " << summary.solved << " of " << summary.attempted << ", altitude "
            << summary.altitude_start << " -> " << summary.altitude_end << (char)10;
  if (!summary.new_milestones.empty()) {
    std::cout << "  badges today:";
    for (const auto& milestone : summary.new_milestones) std::cout << " " << milestone;
    std::cout << (char)10;
  }
  (void)options;
  return 0;
}

/// Scripted session: answers with a fixed success rate so the whole loop can be
/// exercised without a human (and without a display) - used by CI and by anyone
/// verifying the build.
int runDemo(SessionRunner& runner, const Curriculum& curriculum, const StringTable& strings,
            int puzzles, std::uint32_t seed) {
  Rng rng(seed);
  std::cout << "  demo session: " << puzzles << " puzzles, scripted answers" << (char)10;

  int index = 0;
  while (!runner.complete() && index < puzzles) {
    const Puzzle puzzle = runner.nextPuzzle();
    const bool wants_correct = rng.nextDouble() < 0.72;
    int answer = puzzle.correctIndex();
    if (!wants_correct && puzzle.options.size() > 1) {
      answer = (answer + 1) % static_cast<int>(puzzle.options.size());
    }
    const SessionResult result = runner.submitAnswer(puzzle, answer, 1200 + index * 37);
    std::cout << "    #" << (index + 1) << " " << puzzle.spec.skill.id
              << " tier " << puzzle.spec.diff.tier << " -> "
              << (result.correct ? "correct" : "wrong") << ", altitude " << result.altitude
              << (result.was_checkpoint ? " [checkpoint]" : "") << (char)10;
    ++index;
  }

  const SessionSummary summary = runner.summary();
  renderMountain(curriculum, runner.progressionState(), strings, std::cout);
  std::cout << "  " << summary.headline << (char)10
            << "  success rate " << summary.success_rate << (char)10;
  return 0;
}

int listChildren(IDataStore& store, const Curriculum& curriculum) {
  const std::vector<ChildProfile> children = store.children();
  if (children.empty()) {
    std::cout << "  no children in this database yet." << (char)10;
    return 0;
  }
  std::cout << "  id  name            age  altitude  zone" << (char)10;
  for (const auto& child : children) {
    const auto progress = store.progression(child.id);
    const int altitude = progress.has_value() ? progress->altitude : 0;
    const ZoneDef* zone = curriculum.zoneForAltitude(altitude);
    std::cout << "  " << child.id << "   " << child.name << "   " << child.age << "   " << altitude
              << "   " << (zone != nullptr ? zone->key : "forest") << (char)10;
  }
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  Options options;

  for (int i = 1; i < argc; ++i) {
    const std::string flag = argv[i];
    auto next = [&](const char* fallback) -> std::string {
      return (i + 1 < argc) ? std::string(argv[++i]) : std::string(fallback);
    };
    if (flag == "--db") {
      options.db_path = next("sdp100.db");
    } else if (flag == "--name") {
      options.name = next("Player");
    } else if (flag == "--content") {
      options.content_dir = next("");
    } else if (flag == "--length") {
      options.session_length = std::atoi(next("10").c_str());
    } else if (flag == "--demo") {
      options.demo = std::atoi(next("10").c_str());
    } else if (flag == "--list") {
      options.list = true;
    } else if (flag == "--reset") {
      options.reset = true;
    } else if (flag == "--help" || flag == "-h") {
      std::cout << "usage: sdp_cli [--db PATH] [--name NAME] [--length N] [--demo N]"
                   " [--content DIR] [--list] [--reset]" << (char)10;
      return 0;
    }
  }

  const std::string content_dir =
      options.content_dir.empty() ? ContentLoader::defaultContentDir() : options.content_dir;

  LoadReport report;
  LevelGenerator generator;
  const Curriculum curriculum = ContentLoader::loadContentDirectory(content_dir, &generator, &report);
  const StringTable strings = ContentLoader::loadStrings(content_dir + "/strings.json", nullptr);
  if (!report.ok()) {
    std::cerr << "content problems (falling back to built-in defaults):" << (char)10
              << report.summary() << (char)10;
  }

  printBanner();

  std::unique_ptr<IDataStore> store = openSqliteStore(options.db_path);

  if (options.list) {
    return listChildren(*store, curriculum);
  }

  int child_id = 0;
  for (const auto& child : store->children()) {
    if (child.name == options.name) child_id = child.id;
  }
  if (child_id == 0) {
    child_id = store->createChild(options.name, 8);
    std::cout << "  created a new climber: " << options.name << " (id " << child_id << ")"
              << (char)10;
  }
  if (options.reset) {
    store->resetProgress(child_id);
    std::cout << "  progress reset for " << options.name << (char)10;
  }

  EngineConfig engine_config;
  AdaptiveEngine engine(engine_config, curriculum);
  ProgressionService progression(curriculum);
  SessionConfig session_config;
  session_config.length = std::max(1, options.session_length);
  SessionRunner runner(engine, generator, progression, *store, session_config, &strings);

  runner.startSession(child_id);
  if (options.demo > 0) {
    return runDemo(runner, curriculum, strings, options.demo, 987654321u);
  }
  return runInteractive(runner, curriculum, strings, options);
}
