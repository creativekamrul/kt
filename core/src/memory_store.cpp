#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "sdp/rng.hpp"  // deriveSeed: stable participant codes for CSV export
#include "sdp/store.hpp"
#include "sdp/types.hpp"
#include "time_util.hpp"

namespace sdp {
namespace {

/// Non-persistent store used by the tests and the simulation harness. It
/// implements exactly the same semantics as SqliteStore, which is what makes it
/// a legitimate stand-in: the simulation can run thousands of children without
/// touching a disk and without changing a line of engine code.
class MemoryStore final : public IDataStore {
 public:
  int createChild(const std::string& name, int age) override {
    ChildProfile profile;
    profile.id = next_child_id_++;
    profile.name = name;
    profile.age = age;
    profile.created_at = utcNowIso();
    children_[profile.id] = profile;
    return profile.id;
  }

  std::optional<ChildProfile> child(int child_id) const override {
    auto it = children_.find(child_id);
    if (it == children_.end()) return std::nullopt;
    return it->second;
  }

  std::vector<ChildProfile> children() const override {
    std::vector<ChildProfile> result;
    for (const auto& entry : children_) result.push_back(entry.second);
    return result;
  }

  void deleteChild(int child_id) override {
    children_.erase(child_id);
    resetProgress(child_id);
  }

  void logAttempt(const Attempt& attempt) override {
    Attempt stored = attempt;
    if (stored.created_at.empty()) stored.created_at = utcNowIso();
    stored.spec_json = attempt_spec_json(attempt);
    attempts_.push_back(std::move(stored));
  }

  PerformanceHistory history(int child_id, int limit) const override {
    return loadHistory(child_id, -1, limit);
  }

  PerformanceHistory historyWithin(int child_id, int window_days, int limit) const override {
    return loadHistory(child_id, window_days, limit);
  }

  int attemptCount(int child_id) const override {
    int count = 0;
    for (const auto& attempt : attempts_) {
      if (attempt.child_id == child_id) ++count;
    }
    return count;
  }

  void saveSkillState(int child_id, const SkillState& state) override {
    SkillState stored = state;
    stored.updated_at = utcNowIso();
    skill_states_[{child_id, state.skill.id}] = std::move(stored);
  }

  std::vector<SkillState> skillStates(int child_id) const override {
    std::vector<SkillState> result;
    for (const auto& entry : skill_states_) {
      if (entry.first.first == child_id) result.push_back(entry.second);
    }
    return result;
  }

  void saveProgression(const ProgressionState& state) override {
    ProgressionState stored = state;
    stored.updated_at = utcNowIso();
    progress_[state.child_id] = std::move(stored);
  }

  std::optional<ProgressionState> progression(int child_id) const override {
    auto it = progress_.find(child_id);
    if (it == progress_.end()) return std::nullopt;
    return it->second;
  }

  void awardMilestone(int child_id, std::string_view milestone) override {
    const auto key = std::make_pair(child_id, std::string(milestone));
    if (milestones_.count(key) != 0) return;  // idempotent, like INSERT OR IGNORE
    milestones_[key] = utcNowIso();
  }

  std::vector<Milestone> milestones(int child_id) const override {
    std::vector<Milestone> result;
    for (const auto& entry : milestones_) {
      if (entry.first.first != child_id) continue;
      Milestone milestone;
      milestone.name = entry.first.second;
      milestone.earned_at = entry.second;
      result.push_back(std::move(milestone));
    }
    std::sort(result.begin(), result.end(),
              [](const Milestone& a, const Milestone& b) { return a.name < b.name; });
    return result;
  }

  bool hasMilestone(int child_id, std::string_view milestone) const override {
    return milestones_.count({child_id, std::string(milestone)}) != 0;
  }

  void saveCamp(int child_id, const std::string& camp) override { camps_[child_id]=camp; }
  std::string savedCamp(int child_id) const override {
    auto it=camps_.find(child_id); return it==camps_.end() ? "" : it->second;
  }
  void recordPlay(int child_id,const std::string& area,int seconds) override {
    if(seconds<=0)return;
    auto& row=play_[child_id][area];
    row.area=area;row.seconds+=seconds;row.last_played=utcNowIso();
  }
  std::vector<AreaPlay> areaPlay(int child_id) const override {
    std::vector<AreaPlay> rows;
    auto it=play_.find(child_id);
    if(it!=play_.end())for(const auto& entry:it->second)rows.push_back(entry.second);
    return rows;
  }
  std::vector<SkillSuccessRate> successRates(int child_id, int window_days) const override {
    std::map<std::string, std::pair<int, int>> aggregate;  // skill -> (correct, total)
    for (const auto& attempt : attempts_) {
      if (attempt.child_id != child_id) continue;
      if (!isWithinLastDays(attempt.created_at, window_days)) continue;
      auto& bucket = aggregate[attempt.skill.id];
      bucket.second += 1;
      if (attempt.correct) bucket.first += 1;
    }
    std::vector<SkillSuccessRate> result;
    for (const auto& entry : aggregate) {
      SkillSuccessRate rate;
      rate.skill = SubSkill{entry.first};
      rate.n = entry.second.second;
      rate.success_rate = entry.second.second > 0
                              ? static_cast<double>(entry.second.first) / entry.second.second
                              : 0.0;
      result.push_back(std::move(rate));
    }
    return result;
  }

  std::string exportAttemptsCsv(int child_id) const override {
    std::ostringstream out;
    const std::string code =
        "P" + std::to_string(deriveSeed({static_cast<std::uint64_t>(child_id)}) % 100000u);
    out << "participant,sub_skill,domain,difficulty,correct,response_ms,created_at" << (char)10;
    for (const auto& attempt : attempts_) {
      if (attempt.child_id != child_id) continue;
      out << code << "," << attempt.skill.id << "," << to_string(attempt.domain) << ","
          << attempt.difficulty << "," << (attempt.correct ? 1 : 0) << "," << attempt.response_ms
          << "," << attempt.created_at << (char)10;
    }
    return out.str();
  }

  void resetProgress(int child_id) override {
    attempts_.erase(std::remove_if(attempts_.begin(), attempts_.end(),
                                   [&](const Attempt& a) { return a.child_id == child_id; }),
                    attempts_.end());
    for (auto it = skill_states_.begin(); it != skill_states_.end();) {
      it = (it->first.first == child_id) ? skill_states_.erase(it) : std::next(it);
    }
    progress_.erase(child_id);
    camps_.erase(child_id);
    play_.erase(child_id);
    for (auto it = milestones_.begin(); it != milestones_.end();) {
      it = (it->first.first == child_id) ? milestones_.erase(it) : std::next(it);
    }
  }

 private:
  PerformanceHistory loadHistory(int child_id, int window_days, int limit) const {
    std::vector<const Attempt*> matches;
    for (const auto& attempt : attempts_) {
      if (attempt.child_id != child_id) continue;
      if (window_days > 0 && !isWithinLastDays(attempt.created_at, window_days)) continue;
      matches.push_back(&attempt);
    }
    if (limit > 0 && static_cast<int>(matches.size()) > limit) {
      matches.erase(matches.begin(), matches.end() - limit);
    }
    PerformanceHistory history(child_id);
    for (const Attempt* attempt : matches) {
      Attempt copy = *attempt;
      if (const auto spec = level_spec_from_json(copy.spec_json)) {
        copy.tier = spec->diff.tier;
        copy.checkpoint = spec->kind == LevelKind::Checkpoint;
      }
      history.add(std::move(copy));
    }
    return history;
  }

  std::map<int,std::string> camps_;
  std::map<int,std::map<std::string,AreaPlay>> play_;
  int next_child_id_ = 1;
  std::map<int, ChildProfile> children_;
  std::vector<Attempt> attempts_;
  std::map<std::pair<int, std::string>, SkillState> skill_states_;
  std::map<int, ProgressionState> progress_;
  std::map<std::pair<int, std::string>, std::string> milestones_;
};

}  // namespace

std::unique_ptr<IDataStore> openMemoryStore() {
  return std::unique_ptr<IDataStore>(new MemoryStore());
}

}  // namespace sdp
