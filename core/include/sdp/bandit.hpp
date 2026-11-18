#pragma once
// ===========================================================================
// sdp/bandit.hpp - per-sub-skill difficulty policies (multi-armed bandits).
//
// WHY BANDITS AND NOT DEEP RL
// A child produces roughly 30-60 answers per session. Deep RL needs millions of
// episodes, so it is the wrong tool at this data scale (and impossible to
// defend ethically in a school pilot). Bandits need no training data, run in
// microseconds, and their behaviour is analysable - exactly what a course
// report and a funding board want to see.
//
// THE OBJECTIVE: SATISFICING, NOT REWARD MAXIMISATION
// Arms are difficulty tiers of one sub-skill; the goal is to keep the child in
// the flow zone (target success probability, default 0.75). Thompson sampling
// therefore acts greedily on the target objective using a parameter drawn from
// the posterior - it does not maximise "number of correct answers", which would
// trivially pick the easiest tier.
//
// FOUR POLICIES BEHIND ONE INTERFACE (so the report can compare them):
//
//  LatentThompson (default, the one that ships)
//      Thompson sampling over a LATENT ABILITY parameter theta with a monotone
//      response model p(correct | tier, theta) = guess + (1-guess)*logistic(
//      (theta - tier)*slope + offset). The posterior over theta is a discrete
//      grid updated exactly by Bayes. This is a structured (contextual) bandit:
//      it exploits the fact that difficulty ladders are monotone, so ONE piece
//      of evidence moves the estimate for every tier at once.
//
//  ThompsonSatisficing (independent Beta arms)
//      The textbook naive formulation: a separate Beta posterior per tier and
//      the sampled success probability nearest the target wins. Kept as an
//      ablation - and it is the reason the structured version exists, because
//      independent arms cannot propagate information across tiers and therefore
//      stall for children who are far from the initial prior.
//
//  Ucb1Shaped        deterministic exploration on the shaped reward
//  GreedyWindow      pure exploitation (no exploration at all)
//  Static            a fixed tier - the "no adaptation" baseline
// ===========================================================================

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "sdp/rng.hpp"

namespace sdp {

// ---------------------------------------------------------------------------
// Beta posterior - used by the independent-arms policies.
// ---------------------------------------------------------------------------
struct BetaPosterior {
  double alpha = 1.0;
  double beta = 1.0;

  double mean() const noexcept { return alpha / (alpha + beta); }
  double variance() const noexcept;
  double sample(Rng& rng) const noexcept { return sampleBeta(rng, alpha, beta); }

  /// Bayesian update with optional exponential forgetting (discounted Thompson
  /// sampling): with discount < 1 old evidence fades, which lets a policy track
  /// a child who is improving, tiring or switching content.
  void observe(bool correct, double discount = 1.0, double floor_value = 0.05) noexcept;
};

enum class PolicyKind {
  LatentThompson,       // default: structured Thompson sampling (monotone model)
  ThompsonSatisficing,  // ablation: independent Beta arms
  Ucb1Shaped,           // deterministic-exploration alternative
  GreedyWindow,         // ablation: no exploration
  Static,               // baseline: fixed tier
};

std::string_view to_string(PolicyKind k) noexcept;

struct BanditConfig {
  PolicyKind kind = PolicyKind::LatentThompson;
  int tier_count = 12;
  double target_success = 0.75;  // flow-zone centre
  double target_band = 0.05;     // plus/minus band counted as "in the zone"
  double discount = 1.0;         // 1.0 = exact Bayesian updates (Beta policies)
  int min_pulls_per_tier = 0;    // forced sweep before sampling (0 = none)

  // ---- priors for the independent-arms policies ----
  double prior_strength = 4.0;
  double prior_start_success = 0.97;
  double prior_end_success = 0.12;

  // ---- prior for the latent-ability policy ----
  // A new child starts near the bottom of the mountain: the game's zones put
  // counting and small additions first, and a first item that is far too hard
  // is the fastest way to lose a six-year-old. The posterior moves quickly once
  // evidence arrives, so a strong child is only mildly under-challenged at the
  // start (documented in docs/adaptive-engine.md).
  double latent_prior_mean = 1.0;   // in tier units
  double latent_prior_sd = 1.3;
  bool ordered_route = false;  // use chance of tying the whole ordered set of anchors
  double latent_guess = 0.25;       // multiple-choice guessing floor (4 options)
  double latent_slope = 0.8;        // assumed steepness of the response curve
  double latent_temper = 0.012;     // posterior floor: keeps tracking a growing child

  int static_tier = -1;  // for PolicyKind::Static (-1 => middle tier)
};

// ---------------------------------------------------------------------------
// One interface, several policies - this is what makes the simulation's
// arm comparison a like-for-like swap rather than a rewrite.
// ---------------------------------------------------------------------------
class IDifficultyPolicy {
 public:
  virtual ~IDifficultyPolicy() = default;

  /// Chooses the next difficulty tier.
  ///   forbidden_tier - excluded outright (the tier that just failed)
  ///   max_tier       - hard ceiling; the retry rule uses it so a second attempt
  ///                    at the same skill is never HARDER than the first.
  virtual int selectTier(Rng& rng, int forbidden_tier = -1, int max_tier = -1) = 0;
  virtual void observe(int tier, bool correct) = 0;

  virtual int tierCount() const = 0;
  virtual int pulls(int tier) const = 0;
  virtual int totalPulls() const = 0;
  virtual const BanditConfig& config() const = 0;

  virtual double estimatedSuccess(int tier) const = 0;
  /// Tier whose estimated success sits nearest the target: the policy's current
  /// estimate of the child's "just right" level for this sub-skill.
  virtual int estimatedFlowTier() const = 0;

  /// Beta-style summary of the current belief, used to persist skill_state
  /// (whose schema stores alpha/beta).
  virtual std::pair<double, double> evidenceAt(int tier) const = 0;
  /// Resume support; see AdaptiveEngine::loadSkillStates.
  virtual void setPosterior(int tier, double alpha, double beta) = 0;
  virtual void setPulls(int tier, int n) = 0;
  virtual void setLastTier(int t) = 0;
  virtual int lastTier() const = 0;

  virtual std::unique_ptr<IDifficultyPolicy> clone() const = 0;
};

// ---------------------------------------------------------------------------
// Independent Beta arms (ThompsonSatisficing / Ucb1Shaped / GreedyWindow /
// Static).
// ---------------------------------------------------------------------------
class SkillBandit final : public IDifficultyPolicy {
 public:
  SkillBandit() : SkillBandit(BanditConfig{}) {}
  explicit SkillBandit(BanditConfig cfg);

  int selectTier(Rng& rng, int forbidden_tier = -1, int max_tier = -1) override;
  void observe(int tier, bool correct) override;

  int tierCount() const noexcept override { return cfg_.tier_count; }
  int pulls(int tier) const noexcept override;
  int totalPulls() const noexcept override { return total_pulls_; }
  const BanditConfig& config() const noexcept override { return cfg_; }
  const BetaPosterior& posterior(int tier) const;

  double estimatedSuccess(int tier) const override;
  int estimatedFlowTier() const override;
  std::pair<double, double> evidenceAt(int tier) const override;
  void setPosterior(int tier, double alpha, double beta) override;
  void setPulls(int tier, int n) override;
  void setLastTier(int t) noexcept override { last_tier_ = t; }
  int lastTier() const noexcept override { return last_tier_; }
  std::unique_ptr<IDifficultyPolicy> clone() const override;

 private:
  double shapedReward(bool correct) const noexcept;

  BanditConfig cfg_{};
  std::vector<BetaPosterior> arms_;
  std::vector<int> pulls_;
  std::vector<double> reward_sum_;
  int total_pulls_ = 0;
  int last_tier_ = -1;
};

// ---------------------------------------------------------------------------
// Structured Thompson sampling over a latent ability parameter.
//
// Instead of one posterior per arm, there is ONE posterior over the child's
// ability theta (in tier units), discretised on a grid and updated exactly by
// Bayes through a monotone response model. Acting "greedily" on a theta drawn
// from that posterior is proper Thompson sampling for this structured bandit,
// and it has the property the naive version lacks: evidence gathered at one
// tier immediately re-prices every other tier.
// ---------------------------------------------------------------------------
class LatentThompsonBandit final : public IDifficultyPolicy {
 public:
  LatentThompsonBandit(BanditConfig cfg, int tier_count);

  int selectTier(Rng& rng, int forbidden_tier = -1, int max_tier = -1) override;
  void observe(int tier, bool correct) override;

  int tierCount() const noexcept override { return tier_count_; }
  int pulls(int tier) const noexcept override;
  int totalPulls() const noexcept override { return total_pulls_; }
  const BanditConfig& config() const noexcept override { return cfg_; }

  /// Predicted success probability at a tier under the posterior mean ability.
  double estimatedSuccess(int tier) const override;
  int estimatedFlowTier() const override;
  std::pair<double, double> evidenceAt(int tier) const override;
  void setPosterior(int tier, double alpha, double beta) override;
  void setPulls(int tier, int n) override;
  void setLastTier(int t) noexcept override { last_tier_ = t; }
  int lastTier() const noexcept override { return last_tier_; }
  std::unique_ptr<IDifficultyPolicy> clone() const override;

  // Diagnostics (tests, reports).
  double thetaMean() const;
  double thetaSd() const;
  double successGiven(double theta, int tier) const;

 private:
  void normaliseTempered();
  double sampleTheta(Rng& rng) const;

  BanditConfig cfg_{};
  int tier_count_ = 12;
  std::vector<double> grid_;
  std::vector<double> weights_;  // unnormalised posterior over grid_
  std::vector<int> pulls_;
  int total_pulls_ = 0;
  int last_tier_ = -1;
  double offset_ = 0.693;  // logistic(0.693) = 2/3 => p = target at theta == tier
};

/// Factory: builds the policy named by cfg.kind.
std::unique_ptr<IDifficultyPolicy> makeDifficultyPolicy(const BanditConfig& cfg, int tier_count);

}  // namespace sdp
