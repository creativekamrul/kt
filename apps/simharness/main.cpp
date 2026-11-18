// ===========================================================================
// sdp_sim - synthetic-learner simulation.
//
// This is the evidence engine for the report. It runs a virtual population of
// children through the REAL game loop (SessionRunner -> AdaptiveEngine ->
// LevelGenerator -> DataStore -> ProgressionService). Nothing is reimplemented
// for the sake of the demo, so the charts describe the shipped code path.
//
// Design choices that make the comparison trustworthy:
//   * PAIRED population - every arm sees the identical children, constructed
//     from the same per-child seed, so arm-to-arm differences are caused by the
//     difficulty policy and nothing else.
//   * An honest(ish) learner: latent ability per sub-skill, guessing floor from
//     multiple choice, occasional slips, end-of-session fatigue, and learning
//     that is fastest when the child is inside the flow zone. Every simplifying
//     assumption is listed in docs/simulation.md.
//   * Deterministic: same seed => same CSV byte for byte.
//
// Usage:
//   sdp_sim --children 200 --attempts 200 --seed 20260925 --out ../out/sim
// ===========================================================================
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "sdp/content.hpp"
#include "sdp/engine.hpp"
#include "sdp/generator.hpp"
#include "sdp/progression.hpp"
#include "sdp/session.hpp"
#include "sdp/store.hpp"

namespace {

using namespace sdp;

struct SimConfig {
  int children = 200;
  int attempts = 200;
  int session_length = 10;
  std::uint32_t seed = 20260925u;
  std::string out_dir = "out/sim";
  std::string store = "memory";  // "memory" (fast) or "sqlite" (production path)

  // Shape of the virtual learner's response curve. Deliberately tunable so the
  // report can show a ROBUSTNESS run where the engine's assumed model differs
  // from the simulated child (the engine assumes slope 0.8; the default here is
  // 0.9). Using the same value would be self-flattering.
  double learner_slope = 0.9;
  double learner_guess = 0.25;
};

struct ArmSpec {
  std::string name;
  PolicyKind kind;
  int static_tier;
  std::string description;
};

const std::vector<ArmSpec>& arms() {
  static const std::vector<ArmSpec> kArms = {
      {"latent", PolicyKind::LatentThompson, -1,
       "shipped default: Thompson sampling over a latent-ability model"},
      {"thompson_arms", PolicyKind::ThompsonSatisficing, -1,
       "ablation: independent Beta arm per tier"},
      {"ucb1", PolicyKind::Ucb1Shaped, -1, "ablation: UCB1 on flow-zone reward"},
      {"greedy", PolicyKind::GreedyWindow, -1, "ablation: greedy, no exploration"},
      {"static", PolicyKind::Static, 5, "baseline: fixed middle difficulty, no adaptation"},
  };
  return kArms;
}

double clampd(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }

double logistic(double x) { return 1.0 / (1.0 + std::exp(-x)); }

// ---------------------------------------------------------------------------
// Virtual learner
// ---------------------------------------------------------------------------
class VirtualLearner {
 public:
  VirtualLearner(int index, const Curriculum& curriculum, Rng& rng, double slope, double guess)
      : slope_(slope), guess_(guess) {
    group_ = index % 3;  // interleaved, so every arm faces the same mixture
    static const char* kNames[] = {"weak", "mid", "strong"};
    group_name_ = kNames[group_];

    double lo = 0.5, hi = 3.5;
    if (group_ == 1) { lo = 3.0; hi = 7.0; }
    if (group_ == 2) { lo = 7.0; hi = 11.0; }
    base_ = rng.nextDouble(lo, hi);

    for (const auto& def : curriculum.skills()) {
      // Spiky profiles: a child is not uniformly able across sub-skills.
      ability_[def.skill.id] = clampd(base_ + sampleNormal(rng, 0.0, 1.2), 0.0, 11.5);
      ability_start_[def.skill.id] = ability_[def.skill.id];
    }
  }

  const std::string& groupName() const { return group_name_; }
  int group() const { return group_; }

  double abilityOf(const std::string& skill) const {
    auto it = ability_.find(skill);
    return it == ability_.end() ? base_ : it->second;
  }

  double baseAbility() const { return base_; }

  double meanAbility() const {
    double sum = 0.0;
    for (const auto& entry : ability_) sum += entry.second;
    return ability_.empty() ? base_ : sum / static_cast<double>(ability_.size());
  }

  double startMeanAbility() const {
    double sum = 0.0;
    for (const auto& entry : ability_start_) sum += entry.second;
    return ability_start_.empty() ? base_ : sum / static_cast<double>(ability_start_.size());
  }

  /// Probability of answering a tier-t item of this sub-skill correctly.
  double trueSuccess(const std::string& skill, int tier) const {
    const double theta = abilityOf(skill);
    // Offset chosen so that a child whose ability equals the tier answers
    // correctly about 75% of the time - the definition of "in the zone".
    const double p = guess_ + (1.0 - guess_) * logistic((theta - tier) * slope_ + kOffset);
    return clampd(p, 0.02, 0.98);
  }

  void learn(const std::string& skill, double p_true, bool correct) {
    // Learning is fastest inside the flow zone (desirable difficulty). This is
    // the mechanism that makes "difficulty climbs as ability grows" non-trivial:
    // an engine that parks a child at 100% success teaches it almost nothing.
    const bool in_zone = std::fabs(p_true - 0.75) <= 0.05;
    double rate = learning_rate_;
    if (!in_zone) rate *= 0.35;
    if (correct) rate *= 1.15;
    auto it = ability_.find(skill);
    if (it == ability_.end()) return;
    it->second = clampd(it->second + rate, 0.0, 11.5);
  }

  int latencyMs(double difficulty, double p_true, Rng& rng) const {
    const double ms = 900.0 + 1500.0 * difficulty + 1200.0 * (1.0 - p_true) +
                      sampleNormal(rng, 0.0, 220.0);
    return static_cast<int>(clampd(ms, 350.0, 30000.0));
  }

  /// A slip: the child knows the answer but taps the wrong button.
  bool slips(Rng& rng) const { return rng.nextDouble() < slip_; }

 private:
  static constexpr double kOffset = 0.693;  // logistic(0.693) = 0.667 => p = 0.75 at theta == tier
  double slope_ = 0.9;
  double guess_ = 0.25;  // four options => guessing floor
  static constexpr double slip_ = 0.04;
  static constexpr double learning_rate_ = 0.025;

  int group_ = 1;
  std::string group_name_ = "mid";
  double base_ = 5.0;
  std::map<std::string, double> ability_;
  std::map<std::string, double> ability_start_;
};

struct ChildMetrics {
  int child = 0;
  std::string group;
  double ability_start = 0.0;
  double ability_end = 0.0;
  int attempts = 0;
  int solved = 0;
  double success = 0.0;
  double success_last_third = 0.0;
  double in_zone_fraction = 0.0;
  double mean_tier = 0.0;
  double tier_first_third = 0.0;
  double tier_last_third = 0.0;
  double mean_response_ms = 0.0;
};

struct GroupAggregate {
  int children = 0;
  double success = 0.0;
  double success_last_third = 0.0;
  double in_zone_fraction = 0.0;
  double tier_first_third = 0.0;
  double tier_last_third = 0.0;
};

std::string armPath(const SimConfig& cfg, const std::string& arm, const std::string& suffix) {
  return cfg.out_dir + "/" + suffix + "_" + arm + ".csv";
}

int parseFlag(int argc, char** argv, const char* flag, int fallback) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], flag) == 0) return std::atoi(argv[i + 1]);
  }
  return fallback;
}

double parseDouble(int argc, char** argv, const char* flag, double fallback) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], flag) == 0) return std::atof(argv[i + 1]);
  }
  return fallback;
}

std::string parseString(int argc, char** argv, const char* flag, const std::string& fallback) {
  for (int i = 1; i + 1 < argc; ++i) {
    if (std::strcmp(argv[i], flag) == 0) return argv[i + 1];
  }
  return fallback;
}

// ---------------------------------------------------------------------------
// One arm
// ---------------------------------------------------------------------------
void runArm(const ArmSpec& arm, const SimConfig& cfg, const Curriculum& curriculum,
            const std::vector<VirtualLearner>& population) {
  std::filesystem::create_directories(cfg.out_dir);

  std::ofstream attempts_csv(armPath(cfg, arm.name, "attempts"));
  attempts_csv << "arm,child,group,index,skill,tier,difficulty,correct,p_true,response_ms,"
                  "altitude,checkpoint" << (char)10;

  std::ofstream children_csv(armPath(cfg, arm.name, "children"));
  children_csv << "arm,child,group,ability_start,ability_end,attempts,solved,success,"
                  "success_last_third,in_zone_fraction,mean_tier,tier_first_third,"
                  "tier_last_third,mean_response_ms" << (char)10;

  const std::string db_path = cfg.out_dir + "/" + arm.name + ".db";
  if (cfg.store == "sqlite") std::remove(db_path.c_str());

  std::vector<ChildMetrics> metrics;
  metrics.reserve(population.size());

  std::map<std::string, GroupAggregate> groups;
  double all_success = 0.0, all_success_tail = 0.0, all_in_zone = 0.0;
  long long total_attempts = 0, total_solved = 0;

  const int sessions =
      (cfg.attempts + cfg.session_length - 1) / std::max(1, cfg.session_length);

  for (int index = 0; index < static_cast<int>(population.size()); ++index) {
    VirtualLearner learner = population[static_cast<std::size_t>(index)];  // identical start
    Rng rng(deriveSeed({cfg.seed, hashString(arm.name), static_cast<std::uint64_t>(index)}));

    std::unique_ptr<IDataStore> store =
        cfg.store == "sqlite" ? openSqliteStore(db_path) : openMemoryStore();
    const int child_id = store->createChild("learner", 8);

    EngineConfig engine_config;
    engine_config.bandit.kind = arm.kind;
    engine_config.bandit.static_tier = arm.static_tier;

    AdaptiveEngine engine(engine_config, curriculum);
    LevelGenerator generator(curriculum);
    ProgressionService progression(curriculum);
    SessionConfig session_config;
    session_config.length = std::max(1, cfg.session_length);
    SessionRunner runner(engine, generator, progression, *store, session_config);

    std::vector<double> tiers;
    std::vector<int> correctness;
    std::vector<double> in_zone_flags;
    double latency_sum = 0.0;
    int attempt_index = 0;

    for (int session = 0; session < sessions && attempt_index < cfg.attempts; ++session) {
      runner.startSession(child_id);
      for (int k = 0; k < session_config.length && attempt_index < cfg.attempts; ++k) {
        const Puzzle puzzle = runner.nextPuzzle();
        const std::string skill = puzzle.spec.skill.id;
        const int tier = puzzle.spec.diff.tier;

        // End-of-session fatigue: the last few items of a day are harder.
        const double fatigue = (k >= session_config.length - 3) ? 0.94 : 1.0;
        const double p_true = clampd(learner.trueSuccess(skill, tier) * fatigue, 0.02, 0.99);

        bool correct = rng.nextDouble() < p_true;
        if (correct && learner.slips(rng)) correct = false;
        const int latency = learner.latencyMs(puzzle.spec.diff.difficulty, p_true, rng);

        int answer = puzzle.correctIndex();
        if (!correct) {
          const int count = static_cast<int>(puzzle.options.size());
          answer = count > 1 ? (answer + 1 + rng.nextInt(0, count - 2)) % count : -1;
        }

        const SessionResult result = runner.submitAnswer(puzzle, answer, latency);
        learner.learn(skill, p_true, result.correct);

        tiers.push_back(static_cast<double>(tier));
        correctness.push_back(result.correct ? 1 : 0);
        in_zone_flags.push_back(std::fabs(p_true - 0.75) <= 0.05 ? 1.0 : 0.0);
        latency_sum += latency;

        attempts_csv << arm.name << "," << index << "," << learner.groupName() << ","
                     << attempt_index << "," << skill << "," << tier << ","
                     << puzzle.spec.diff.difficulty << "," << (result.correct ? 1 : 0) << ","
                     << p_true << "," << latency << "," << result.altitude << ","
                     << (result.was_checkpoint ? 1 : 0) << (char)10;
        ++attempt_index;
      }
    }

    auto meanOf = [](const std::vector<double>& values, std::size_t from, std::size_t to) {
      if (to <= from) return 0.0;
      double sum = 0.0;
      for (std::size_t i = from; i < to; ++i) sum += values[i];
      return sum / static_cast<double>(to - from);
    };

    ChildMetrics row;
    row.child = index;
    row.group = learner.groupName();
    row.ability_start = learner.startMeanAbility();
    row.ability_end = learner.meanAbility();
    row.attempts = static_cast<int>(correctness.size());
    row.solved = 0;
    for (int value : correctness) row.solved += value;
    row.success = row.attempts > 0 ? static_cast<double>(row.solved) / row.attempts : 0.0;
    const std::size_t n = correctness.size();
    const std::size_t tail_from = n - n / 3;
    int tail_solved = 0;
    for (std::size_t i = tail_from; i < n; ++i) tail_solved += correctness[i];
    row.success_last_third = (n > tail_from) ? static_cast<double>(tail_solved) / (n - tail_from) : 0.0;
    row.in_zone_fraction = meanOf(in_zone_flags, 0, n);
    row.mean_tier = meanOf(tiers, 0, n);
    row.tier_first_third = meanOf(tiers, 0, n / 3);
    row.tier_last_third = meanOf(tiers, tail_from, n);
    row.mean_response_ms = row.attempts > 0 ? latency_sum / row.attempts : 0.0;

    children_csv << arm.name << "," << row.child << "," << row.group << "," << row.ability_start
                 << "," << row.ability_end << "," << row.attempts << "," << row.solved << ","
                 << row.success << "," << row.success_last_third << "," << row.in_zone_fraction
                 << "," << row.mean_tier << "," << row.tier_first_third << ","
                 << row.tier_last_third << "," << row.mean_response_ms << (char)10;

    GroupAggregate& bucket = groups[row.group];
    bucket.children += 1;
    bucket.success += row.success;
    bucket.success_last_third += row.success_last_third;
    bucket.in_zone_fraction += row.in_zone_fraction;
    bucket.tier_first_third += row.tier_first_third;
    bucket.tier_last_third += row.tier_last_third;

    all_success += row.success;
    all_success_tail += row.success_last_third;
    all_in_zone += row.in_zone_fraction;
    total_attempts += row.attempts;
    total_solved += row.solved;

    metrics.push_back(row);
  }

  const double count = static_cast<double>(metrics.size());
  nlohmann::json summary;
  summary["arm"] = arm.name;
  summary["description"] = arm.description;
  summary["policy"] = std::string(to_string(arm.kind));
  summary["static_tier"] = arm.static_tier;
  summary["children"] = static_cast<int>(metrics.size());
  summary["attempts_per_child"] = cfg.attempts;
  summary["total_attempts"] = total_attempts;
  summary["success_rate"] = count > 0 ? all_success / count : 0.0;
  summary["success_rate_last_third"] = count > 0 ? all_success_tail / count : 0.0;
  summary["in_flow_zone_fraction"] = count > 0 ? all_in_zone / count : 0.0;
  summary["target_success"] = 0.75;

  double spread = 0.0;
  for (const auto& row : metrics) {
    spread += std::fabs(row.success_last_third - 0.75);
  }
  summary["mean_abs_deviation_from_target"] = count > 0 ? spread / count : 0.0;

  nlohmann::json by_group = nlohmann::json::object();
  for (const auto& entry : groups) {
    const double g = static_cast<double>(entry.second.children);
    by_group[entry.first] = nlohmann::json{
        {"children", entry.second.children},
        {"success_rate", entry.second.success / g},
        {"success_rate_last_third", entry.second.success_last_third / g},
        {"in_flow_zone_fraction", entry.second.in_zone_fraction / g},
        {"mean_tier_first_third", entry.second.tier_first_third / g},
        {"mean_tier_last_third", entry.second.tier_last_third / g}};
  }
  summary["by_group"] = by_group;

  std::ofstream summary_file(cfg.out_dir + "/summary_" + arm.name + ".json");
  summary_file << summary.dump(2) << (char)10;

  std::printf("arm %-9s success=%.3f  tail=%.3f  in-zone=%.3f  |dev from 0.75|=%.3f\n",
              arm.name.c_str(), summary["success_rate"].get<double>(),
              summary["success_rate_last_third"].get<double>(),
              summary["in_flow_zone_fraction"].get<double>(),
              summary["mean_abs_deviation_from_target"].get<double>());
  for (const auto& entry : groups) {
    const double g = static_cast<double>(entry.second.children);
    std::printf("    %-7s n=%3d  success=%.3f  tail=%.3f  in-zone=%.3f  tier %.2f -> %.2f\n",
                entry.first.c_str(), entry.second.children, entry.second.success / g,
                entry.second.success_last_third / g, entry.second.in_zone_fraction / g,
                entry.second.tier_first_third / g, entry.second.tier_last_third / g);
  }
}

}  // namespace

int main(int argc, char** argv) {
  SimConfig cfg;
  cfg.children = parseFlag(argc, argv, "--children", cfg.children);
  cfg.attempts = parseFlag(argc, argv, "--attempts", cfg.attempts);
  cfg.session_length = parseFlag(argc, argv, "--session-length", cfg.session_length);
  cfg.seed = static_cast<std::uint32_t>(parseFlag(argc, argv, "--seed", static_cast<int>(cfg.seed)));
  cfg.out_dir = parseString(argc, argv, "--out", cfg.out_dir);
  cfg.store = parseString(argc, argv, "--store", cfg.store);
  cfg.learner_slope = parseDouble(argc, argv, "--learner-slope", cfg.learner_slope);
  cfg.learner_guess = parseDouble(argc, argv, "--learner-guess", cfg.learner_guess);

  if (cfg.children < 1 || cfg.attempts < 3) {
    std::fprintf(stderr, "usage: sdp_sim [--children N] [--attempts M] [--session-length K]"
                         " [--seed S] [--out DIR] [--store memory|sqlite]\n");
    return 2;
  }

  const Curriculum curriculum = makeDefaultCurriculum();
  std::printf("sdp_sim: %d children x %d attempts, sessions of %d, store=%s, seed=%u, "
              "learner(slope=%.2f guess=%.2f)\n",
              cfg.children, cfg.attempts, cfg.session_length, cfg.store.c_str(), cfg.seed,
              cfg.learner_slope, cfg.learner_guess);

  // One paired population, reused by every arm: the children are identical
  // across arms, so differences are attributable to the difficulty policy.
  std::vector<VirtualLearner> population;
  population.reserve(static_cast<std::size_t>(cfg.children));
  for (int index = 0; index < cfg.children; ++index) {
    Rng rng(deriveSeed({cfg.seed, 0x5D0100u, static_cast<std::uint64_t>(index)}));
    population.emplace_back(index, curriculum, rng, cfg.learner_slope, cfg.learner_guess);
  }

  for (const auto& arm : arms()) runArm(arm, cfg, curriculum, population);

  std::printf("wrote CSVs and summaries to %s\n", cfg.out_dir.c_str());
  return 0;
}
