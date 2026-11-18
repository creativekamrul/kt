#pragma once

#include <array>
#include <optional>
#include <string>
#include <vector>

#include "sdp/generator.hpp"
#include "sdp/store.hpp"

namespace sdp {

struct TrailTask {
  const char* prompt;
  SubSkill skill;
  std::array<int, 6> deck{};
};

struct Expedition {
  const char* id;
  const char* title;
  const char* badge;
  int altitude;
  std::array<TrailTask, 3> tasks;
};

const std::array<Expedition, 4>& expeditions();

struct BadgeProgress {
  std::string title;
  std::string goal;
  int count;
  int required;
  bool earned() const { return count >= required; }
};

struct TrailJournal {
  std::string rank;
  std::string next_rank;
  int steps_to_rank = 0;
  int altitude = 0;
  std::array<int, 4> stages{};
  std::vector<BadgeProgress> badges;
};

struct ExpeditionRound {
  int level;
  int stage;
  Puzzle puzzle;
};

enum class ExpeditionOutcome { TryAgain, Placed, Advanced, Completed, Unavailable };

struct BridgeSpan {
  int stage;
  std::array<int, 6> deck;
  std::array<int, 3> rack;
  std::array<bool, 2> filled{};
  static constexpr std::array<int, 2> gaps{1, 4};
  bool fit(int gap, int value);
  bool complete() const { return filled[0] && filled[1]; }
};

// Expedition stages are optional, untimed field tasks. Each solved stage has
// one durable milestone key; the last key doubles as the completion badge.
class TrailCampaign {
 public:
  TrailCampaign(IDataStore& store, int child) : store_(store), child_(child) {}
  TrailJournal journal(const PerformanceHistory& history) const;
  std::optional<ExpeditionRound> next(int level) const;
  std::optional<BridgeSpan> bridgeSpan(int stage, bool replay = false) const;
  ExpeditionOutcome placePlank(BridgeSpan& span, int gap, int value, bool replay = false);
  ExpeditionOutcome answer(int level, int stage, int option);

 private:
  int altitude() const;
  int cleared(const Expedition& level) const;
  IDataStore& store_;
  int child_;
};

}  // namespace sdp
