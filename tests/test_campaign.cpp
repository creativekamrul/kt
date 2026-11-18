#include "sdp/campaign.hpp"
#include "test_framework.hpp"

using namespace sdp;

SDP_TEST(campaign, levels_unlock_resume_and_award_once_per_player) {
  auto store = openSqliteStore(":memory:");
  const int player = store->createChild("Scout", 8);
  const int friend_id = store->createChild("Guide", 9);
  TrailCampaign campaign(*store, player);
  CHECK(!campaign.next(-1));
  CHECK(!campaign.next(int(expeditions().size())));
  for (int i = 1; i < int(expeditions().size()); ++i) {
    const auto& level = expeditions()[i];
    ProgressionState progress;
    progress.child_id = player;
    if (level.altitude > 0) {
      progress.altitude = level.altitude - 1;
      store->saveProgression(progress);
      CHECK(!campaign.next(i));
      CHECK(campaign.answer(i, 0, 0) == ExpeditionOutcome::Unavailable);
    }
    progress.altitude = level.altitude;
    store->saveProgression(progress);
    for (int stage = 0; stage < int(level.tasks.size()); ++stage) {
      TrailCampaign resumed(*store, player);
      const auto round = resumed.next(i);
      CHECK(round.has_value());
      CHECK_EQ(round->stage, stage);
      CHECK(round->puzzle.wellFormed());
      CHECK_EQ(round->puzzle.options.size(), std::size_t(4));
      const int right = round->puzzle.correctIndex();
      CHECK(resumed.answer(i, stage, -1) == ExpeditionOutcome::Unavailable);
      CHECK(resumed.answer(i, stage, (right+1)%4) == ExpeditionOutcome::TryAgain);
      CHECK_EQ(campaign.next(i)->puzzle.correctIndex(), right);
      const bool last = stage+1 == int(level.tasks.size());
      CHECK(campaign.answer(i, stage, right) == (last ? ExpeditionOutcome::Completed : ExpeditionOutcome::Advanced));
      CHECK(resumed.answer(i, stage, right) == ExpeditionOutcome::Unavailable);
      CHECK_EQ(campaign.journal(store->history(player)).stages[i], stage+1);
    }
    CHECK(!campaign.next(i));
    CHECK(campaign.journal(store->history(player)).badges.at(i+4).earned());
    CHECK_EQ(store->progression(player)->altitude, level.altitude);
  }
  CHECK_EQ(store->attemptCount(player), 0);
  CHECK_EQ(store->milestones(player).size(), std::size_t(9));
  TrailCampaign other(*store, friend_id);
  CHECK_EQ(other.bridgeSpan(0)->stage, 0);
  CHECK(!other.journal(store->history(friend_id)).badges.at(4).earned());
}

SDP_TEST(campaign, journal_recognises_existing_climbs_and_checkpoint_success) {
  auto store = openSqliteStore(":memory:");
  const int child = store->createChild("Climber",8);
  TrailCampaign campaign(*store, child);
  ProgressionState progress;
  progress.child_id = child;
  progress.altitude = 60;
  store->saveProgression(progress);
  for (const auto& skill : {skills::Counting, skills::Addition, skills::Counting, skills::Patterns}) {
    Attempt attempt;
    attempt.child_id = child;
    attempt.skill = skill;
    attempt.correct = true;
    store->logAttempt(attempt);
  }
  Attempt checkpoint;
  checkpoint.child_id = child;
  checkpoint.skill = skills::Subtraction;
  checkpoint.checkpoint = true;
  store->logAttempt(checkpoint);
  auto journal = campaign.journal(store->history(child));
  CHECK_EQ(journal.rank, "Alpine Guide");
  CHECK_EQ(journal.steps_to_rank, 30);
  CHECK(journal.badges[0].earned());
  CHECK_EQ(journal.badges[1].count, 3);
  CHECK(!journal.badges[2].earned());
  CHECK(!journal.badges[3].earned());
  checkpoint.correct = true;
  store->logAttempt(checkpoint);
  progress.altitude = 90;
  store->saveProgression(progress);
  journal = campaign.journal(store->history(child));
  CHECK(journal.next_rank.empty());
  CHECK_EQ(journal.rank, "Summit Keeper");
  for (int i = 0; i < 4; ++i) CHECK(journal.badges[i].earned());
}

SDP_TEST(campaign, bridge_resumes_old_stages_and_saves_each_new_plank) {
  auto store = openSqliteStore(":memory:");
  const int child = store->createChild("Builder", 8);
  store->awardMilestone(child, "expedition.bridge.1"); // completed in the old quiz
  TrailCampaign campaign(*store, child);
  CHECK(!campaign.bridgeSpan(0));
  CHECK(!campaign.bridgeSpan(0,true));
  CHECK(!campaign.next(0));
  for (int stage : {1,2}) {
    auto span = campaign.bridgeSpan(stage);
    CHECK(span.has_value());
    CHECK(!span->filled[0] && !span->filled[1]);
    CHECK(campaign.placePlank(*span,1,999) == ExpeditionOutcome::TryAgain);
    CHECK(campaign.placePlank(*span,1,span->deck[span->gaps[1]]) == ExpeditionOutcome::Placed);
    auto restored = campaign.bridgeSpan(stage);
    CHECK(restored->filled[1] && !restored->filled[0]);
    CHECK(campaign.placePlank(*restored,1,restored->deck[restored->gaps[1]]) == ExpeditionOutcome::TryAgain);
    CHECK_EQ(campaign.journal(store->history(child)).stages[0],stage);
    const auto outcome = campaign.placePlank(*restored,0,restored->deck[restored->gaps[0]]);
    CHECK(outcome == (stage==2 ? ExpeditionOutcome::Completed : ExpeditionOutcome::Advanced));
    CHECK(!campaign.bridgeSpan(stage));
  }
  CHECK(campaign.journal(store->history(child)).badges[4].earned());
  auto saved = store->milestones(child).size();
  for (int stage=0; stage<3; ++stage) {
    auto replay = campaign.bridgeSpan(stage,true);
    CHECK(replay.has_value());
    CHECK(campaign.placePlank(*replay,0,replay->deck[replay->gaps[0]],true) == ExpeditionOutcome::Placed);
    campaign.placePlank(*replay,1,replay->deck[replay->gaps[1]],true);
    CHECK(replay->complete());
  }
  CHECK_EQ(store->milestones(child).size(),saved);
  CHECK_EQ(store->attemptCount(child),0);
}

SDP_TEST(campaign, original_partial_bridge_keeps_its_numbers_after_upgrade) {
  auto store=openSqliteStore(":memory:");
  const int player=store->createChild("First climber",8);
  store->awardMilestone(player,"expedition.bridge.1.plank.1");
  TrailCampaign game(*store,player);
  auto span=game.bridgeSpan(0);
  CHECK(span.has_value());
  CHECK(span->deck==expeditions()[0].tasks[0].deck);
  CHECK(span->filled[1] && !span->filled[0]);
  CHECK(game.placePlank(*span,0,4)==ExpeditionOutcome::Advanced);
  CHECK_EQ(game.journal(store->history(player)).stages[0],1);
}
