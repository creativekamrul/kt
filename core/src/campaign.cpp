#include "sdp/campaign.hpp"

#include <algorithm>
#include <set>

namespace sdp {

const std::array<Expedition, 4>& expeditions() {
  static const std::array<Expedition, 4> levels{{
      {"bridge", "Willow Bridge", "Bridge Builder", 0, {{
        {"Count up by twos.", skills::Counting, {2,4,6,8,10,12}},
        {"Count back by twos.", skills::Subtraction, {16,14,12,10,8,6}},
        {"Count up by threes.", skills::Patterns, {3,6,9,12,15,18}}}}},
      {"supplies", "Pack the Lookout", "Quartermaster", 10, {{
        {"", skills::Counting},
        {"", skills::Counting},
        {"", skills::Subtraction}}}},
      {"cairns", "Cairn Pass", "Pathfinder", 30, {{
        {"", skills::Patterns},
        {"", skills::Patterns},
        {"", skills::Addition}}}},
      {"beacon", "Snowline Beacon", "Beacon Keeper", 60, {{
        {"", skills::Counting},
        {"", skills::Subtraction},
        {"", skills::Patterns}}}}
  }};
  return levels;
}

namespace {
std::string stageKey(const Expedition& level, int stage) {
  return std::string("expedition.") + level.id + "." + std::to_string(stage + 1);
}
}

int TrailCampaign::altitude() const {
  const auto state = store_.progression(child_);
  return state ? state->altitude : 0;
}

int TrailCampaign::cleared(const Expedition& level) const {
  int stage = 0;
  while (stage < static_cast<int>(level.tasks.size()) &&
         store_.hasMilestone(child_, stageKey(level, stage))) ++stage;
  return stage;
}

TrailJournal TrailCampaign::journal(const PerformanceHistory& history) const {
  TrailJournal result;
  result.altitude = altitude();
  struct Rank { int steps; const char* title; };
  constexpr Rank ranks[] = {{0,"Trail Scout"},{10,"Lookout Guide"},{30,"Ridge Walker"},
                            {60,"Alpine Guide"},{90,"Summit Keeper"}};
  for (auto rank : ranks) {
    if (result.altitude < rank.steps) {
      result.next_rank = rank.title;
      result.steps_to_rank = rank.steps - result.altitude;
      break;
    }
    result.rank = rank.title;
  }

  std::set<SubSkill> explored;
  int checkpoints = 0;
  if (history.childId() != child_) throw std::invalid_argument("Journal history belongs to another player");
  for (const auto& attempt : history.attempts()) {
    if (!attempt.correct) continue;
    explored.insert(attempt.skill);
    if (attempt.checkpoint) ++checkpoints;
    if (checkpoints && explored.size() >= 4) break;
  }
  result.badges = {
      {"First Footprint", "Climb your first step", std::min(1,result.altitude), 1},
      {"Field Scholar", "Solve puzzles in four different skills", std::min(4,int(explored.size())), 4},
      {"Pass Keeper", "Solve a checkpoint challenge", std::min(1,checkpoints), 1},
      {"Summit Flag", "Reach altitude 90", std::min(90,result.altitude), 90}};
  for (std::size_t i = 0; i < expeditions().size(); ++i) {
    const auto& level = expeditions()[i];
    result.stages[i] = cleared(level);
    result.badges.push_back({level.badge, level.title, result.stages[i], int(level.tasks.size())});
  }
  return result;
}

std::optional<ExpeditionRound> TrailCampaign::next(int index) const {
  if (index <= 0 || index >= int(expeditions().size())) return std::nullopt;
  const auto& level = expeditions()[index];
  if (altitude() < level.altitude) return std::nullopt;
  const int stage = cleared(level);
  if (stage == int(level.tasks.size())) return std::nullopt;
  const auto& task = level.tasks[stage];
  Puzzle puzzle;
  puzzle.spec.child_id = child_;
  puzzle.spec.skill = task.skill;
  puzzle.spec.domain = task.skill.domainPrefix() == "logic" ? Domain::ProblemSolving : Domain::Math;
  puzzle.spec.seed = deriveSeed({hashString(level.id), static_cast<unsigned>(child_), static_cast<unsigned>(stage)});
  Rng rng(puzzle.spec.seed);
  const int units=rng.nextInt(2,5), groups=rng.nextInt(2,5);
  const int total=units*groups;
  auto n=[](int value){ return std::to_string(value); };
  int solution=0;
  if (index==1) {
    if(stage==0){ solution=total; puzzle.prompt="Pack "+n(groups)+" bags of "+n(units)+" apples. Bring the total."; }
    if(stage==1){ solution=units; puzzle.prompt="Share "+n(total)+" biscuits among "+n(groups)+" hikers. Bring one share."; }
    if(stage==2){ solution=total-units; puzzle.prompt="Camp needs "+n(total)+" cups and has "+n(units)+". Bring the rest."; }
    puzzle.hint="Picture the supplies in equal groups.";
  } else if(index==2) {
    const int step=stage==1 ? -units : units;
    const int first=stage==1 ? total+4*units : groups;
    solution=first+3*step;
    puzzle.prompt="Follow the cairns: "+n(first)+", "+n(first+step)+", "+n(first+2*step)+". Find the next marker.";
    puzzle.hint="Compare two neighboring cairns.";
  } else {
    if(stage==0){ solution=total; puzzle.prompt="The beacon flashes "+n(units)+" times in each of "+n(groups)+" groups. Find the total."; }
    if(stage==1){ solution=total-units; puzzle.prompt="Of "+n(total)+" beacon cells, "+n(units)+" went dark. Find the working count."; }
    if(stage==2){ solution=units*8; puzzle.prompt="Trace the beacon code: "+n(units)+", "+n(units*2)+", "+n(units*4)+". Find the next signal."; }
    puzzle.hint="Follow the signal one group at a time.";
  }
  puzzle.answer=n(solution);
  puzzle.options.push_back({puzzle.answer,true});
  for (int n : LevelGenerator::makeDistractors(solution, 3, 0, solution + 10, rng))
    puzzle.options.push_back({std::to_string(n), false});
  rng.shuffle(puzzle.options);
  return ExpeditionRound{index, stage, std::move(puzzle)};
}

bool BridgeSpan::fit(int gap, int value) {
  if (gap < 0 || gap >= int(gaps.size()) || filled[gap] || deck[gaps[gap]] != value) return false;
  filled[gap] = true;
  return true;
}

std::optional<BridgeSpan> TrailCampaign::bridgeSpan(int stage, bool replay) const {
  const auto& level = expeditions().front();
  if (stage < 0 || stage >= int(level.tasks.size())) return std::nullopt;
  const int saved = cleared(level);
  if (replay ? saved != int(level.tasks.size()) : saved != stage) return std::nullopt;
  auto deck = level.tasks[stage].deck;
  const std::string key=stageKey(level,stage);
  const bool legacyPartial=!replay && (store_.hasMilestone(child_,key+".plank.0") ||
                                        store_.hasMilestone(child_,key+".plank.1"));
  if(!legacyPartial) {
    Rng layout(deriveSeed({hashString("willow-span"),static_cast<unsigned>(child_),static_cast<unsigned>(stage)}));
    const int offset=layout.nextInt(0,6);
    for(int& value:deck) value+=offset;
  }
  BridgeSpan span{stage, deck, {deck[BridgeSpan::gaps[0]], deck[BridgeSpan::gaps[1]], deck.back()+1}};
  Rng rng(deriveSeed({hashString(level.id), static_cast<unsigned>(child_), static_cast<unsigned>(stage)}));
  for (int i = int(span.rack.size()) - 1; i > 0; --i) std::swap(span.rack[i], span.rack[rng.nextInt(0,i)]);
  if (!replay) {
    for (int gap = 0; gap < int(span.gaps.size()); ++gap)
      span.filled[gap] = store_.hasMilestone(child_, key + (legacyPartial ? ".plank." : ".socket.") + std::to_string(gap));
  }
  return span;
}

ExpeditionOutcome TrailCampaign::placePlank(BridgeSpan& span, int gap, int value, bool replay) {
  // Serialize reading the open sockets and publishing the stage milestone.
  // A second window may also be finishing the same span.
  StoreBatch batch(store_);
  auto current = bridgeSpan(span.stage, replay);
  if (!current) return ExpeditionOutcome::Unavailable;
  if (replay) current->filled = span.filled;
  const auto& level = expeditions().front();
  const auto key = stageKey(level, span.stage);
  bool fitted = current->fit(gap, value);
  span = *current;
  if (!fitted) return ExpeditionOutcome::TryAgain;
  if (!span.complete()) {
    if (!replay) store_.awardMilestone(child_, key + ".socket." + std::to_string(gap));
    return ExpeditionOutcome::Placed;
  }
  if (!replay) store_.awardMilestone(child_, key);
  return span.stage + 1 == int(level.tasks.size()) ? ExpeditionOutcome::Completed : ExpeditionOutcome::Advanced;
}

ExpeditionOutcome TrailCampaign::answer(int level, int stage, int option) {
  const auto round = next(level);
  if (!round || round->stage != stage || option < 0 || option >= int(round->puzzle.options.size()))
    return ExpeditionOutcome::Unavailable;
  if (option != round->puzzle.correctIndex()) return ExpeditionOutcome::TryAgain;
  const auto& def = expeditions()[level];
  store_.awardMilestone(child_, stageKey(def, stage));
  return stage + 1 == int(def.tasks.size()) ? ExpeditionOutcome::Completed : ExpeditionOutcome::Advanced;
}

}  // namespace sdp
