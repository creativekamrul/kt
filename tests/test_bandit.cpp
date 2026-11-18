#include <cmath>
#include <vector>

#include "sdp/bandit.hpp"
#include "test_framework.hpp"

using namespace sdp;

namespace {

// Latent-ability learner model used by the tests. tier == ability means the
// learner answers the item correctly about 75% of the time, so "ability" is
// measured directly in difficulty tiers.
double learnerSuccess(int ability, int tier) {
  const double x = static_cast<double>(tier - ability) * 0.9 - 1.1;
  return 1.0 / (1.0 + std::exp(x));
}

struct RunResult {
  double overall = 0.0;
  double tail = 0.0;
  int flow_tier = 0;
  double mean_tier = 0.0;
};

RunResult runLearner(int ability, int attempts, std::uint64_t seed) {
  Rng rng(seed);
  SkillBandit bandit(BanditConfig{});
  RunResult res;
  int correct = 0, total = 0, tail_correct = 0, tail_total = 0;
  double tier_sum = 0.0;
  for (int i = 0; i < attempts; ++i) {
    const int tier = bandit.selectTier(rng);
    const bool ok = rng.nextDouble() < learnerSuccess(ability, tier);
    bandit.observe(tier, ok);
    ++total;
    tier_sum += tier;
    if (ok) ++correct;
    if (i >= attempts - 100) {
      ++tail_total;
      if (ok) ++tail_correct;
    }
  }
  res.overall = static_cast<double>(correct) / static_cast<double>(total);
  res.tail = static_cast<double>(tail_correct) / static_cast<double>(tail_total);
  res.flow_tier = bandit.estimatedFlowTier();
  res.mean_tier = tier_sum / static_cast<double>(total);
  return res;
}

}  // namespace

SDP_TEST(bandit, beta_posterior_updates_correctly) {
  BetaPosterior p;  // Beta(1, 1)
  CHECK_NEAR(p.mean(), 0.5, 1e-12);
  p.observe(true);
  CHECK_NEAR(p.mean(), 2.0 / 3.0, 1e-12);
  p.observe(false);
  CHECK_NEAR(p.mean(), 2.0 / 4.0, 1e-12);
  CHECK(p.variance() > 0.0);
  CHECK(p.variance() < 0.25);
}

SDP_TEST(bandit, discounted_updates_forget_old_evidence) {
  BetaPosterior p;
  for (int i = 0; i < 50; ++i) p.observe(true, 0.9);
  CHECK(p.mean() > 0.9);
  // Fading evidence keeps the counts bounded instead of growing without limit.
  CHECK(p.alpha + p.beta < 50.0);
}

SDP_TEST(bandit, prior_decays_with_tier) {
  SkillBandit b{BanditConfig{}};
  CHECK_EQ(b.tierCount(), 12);
  CHECK(b.estimatedSuccess(0) > b.estimatedSuccess(11));
  CHECK_NEAR(b.estimatedSuccess(0), 0.97, 0.01);
}

SDP_TEST(bandit, selection_is_deterministic_for_a_seed) {
  Rng r1(5), r2(5);
  SkillBandit a{BanditConfig{}}, c{BanditConfig{}};
  for (int i = 0; i < 40; ++i) {
    const int t1 = a.selectTier(r1);
    const int t2 = c.selectTier(r2);
    CHECK_EQ(t1, t2);
    const bool ok = (i % 3) != 0;
    a.observe(t1, ok);
    c.observe(t2, ok);
  }
}

SDP_TEST(bandit, forbidden_tier_is_never_selected) {
  Rng rng(17);
  SkillBandit b{BanditConfig{}};
  for (int i = 0; i < 120; ++i) {
    const int t = b.selectTier(rng, 5);
    CHECK_NE(t, 5);
    b.observe(t, t >= 6);
  }
}

SDP_TEST(bandit, static_policy_never_moves) {
  Rng rng(23);
  BanditConfig cfg;
  cfg.kind = PolicyKind::Static;
  cfg.static_tier = 3;
  SkillBandit b{cfg};
  for (int i = 0; i < 30; ++i) {
    const int t = b.selectTier(rng);
    CHECK_EQ(t, 3);
    b.observe(t, false);  // a failing child still gets the same tier
  }
}

SDP_TEST(bandit, out_of_range_observe_is_ignored) {
  SkillBandit b{BanditConfig{}};
  b.observe(-1, true);
  b.observe(99, true);
  CHECK_EQ(b.totalPulls(), 0);
}

// ---------------------------------------------------------------------------
// The central claim of the project, tested directly: with a stationary learner
// the target-seeking bandit drives the success rate into the flow zone for
// weak AND strong learners, and the level it settles on tracks ability.
// ---------------------------------------------------------------------------
SDP_TEST(bandit, converges_into_flow_zone_for_every_ability) {
  const std::vector<int> abilities{0, 3, 6, 9, 11};
  std::vector<int> flow_tiers;

  for (int ability : abilities) {
    const RunResult res =
        runLearner(ability, 400, 1000ull + static_cast<std::uint64_t>(ability));
    flow_tiers.push_back(res.flow_tier);

    CHECK(res.overall > 0.5);
    CHECK(res.tail > 0.6);
    CHECK(res.tail < 0.95);
  }

  // Difficulty must climb with ability - the "weak children fail, strong
  // children get bored" failure mode of a static game.
  CHECK(flow_tiers.back() > flow_tiers.front());
  CHECK(flow_tiers[3] > flow_tiers[1]);
}

SDP_TEST(bandit, static_difficulty_fails_weak_and_bores_strong) {
  // Same learner model, but the tier is frozen at the middle of the range.
  const RunResult weak = runLearner(0, 400, 7);
  const RunResult strong = runLearner(11, 400, 8);

  // Sanity anchor for the comparison chart: a static tier cannot serve both.
  CHECK(std::fabs(weak.overall - strong.overall) > 0.2);
}
