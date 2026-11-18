#include "sdp/bandit.hpp"
#include "sdp/mountain.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace sdp {
namespace {

double clampd(double v, double lo, double hi) noexcept { return std::max(lo, std::min(hi, v)); }

double logistic(double x) noexcept { return 1.0 / (1.0 + std::exp(-x)); }

}  // namespace

std::string_view to_string(PolicyKind k) noexcept {
  switch (k) {
    case PolicyKind::LatentThompson: return "latent_thompson";
    case PolicyKind::ThompsonSatisficing: return "thompson_satisficing";
    case PolicyKind::Ucb1Shaped: return "ucb1_shaped";
    case PolicyKind::GreedyWindow: return "greedy_window";
    case PolicyKind::Static: return "static";
  }
  return "latent_thompson";
}

double BetaPosterior::variance() const noexcept {
  const double s = alpha + beta;
  return (alpha * beta) / (s * s * (s + 1.0));
}

void BetaPosterior::observe(bool correct, double discount, double floor_value) noexcept {
  if (discount > 0.0 && discount < 1.0) {
    alpha *= discount;
    beta *= discount;
  }
  alpha = std::max(alpha, floor_value);
  beta = std::max(beta, floor_value);
  if (correct) {
    alpha += 1.0;
  } else {
    beta += 1.0;
  }
}

// ===========================================================================
// Independent Beta arms
// ===========================================================================
SkillBandit::SkillBandit(BanditConfig cfg) : cfg_(cfg) {
  if (cfg_.tier_count < 1) cfg_.tier_count = 1;
  cfg_.target_success = clampd(cfg_.target_success, 0.05, 0.95);
  if (cfg_.kind == PolicyKind::LatentThompson) {
    // This class implements the Beta-arm policies; keep it usable rather than
    // undefined if it is handed the latent config (the factory would normally
    // have routed to LatentThompsonBandit).
    cfg_.kind = PolicyKind::ThompsonSatisficing;
  }

  const auto n = static_cast<std::size_t>(cfg_.tier_count);
  arms_.resize(n);
  pulls_.assign(n, 0);
  reward_sum_.assign(n, 0.0);

  for (int i = 0; i < cfg_.tier_count; ++i) {
    const double t = (cfg_.tier_count == 1)
                         ? 0.0
                         : static_cast<double>(i) / static_cast<double>(cfg_.tier_count - 1);
    const double mean = clampd(cfg_.prior_start_success +
                                   (cfg_.prior_end_success - cfg_.prior_start_success) * t,
                               0.02, 0.98);
    const double strength = std::max(cfg_.prior_strength, 0.1);
    arms_[static_cast<std::size_t>(i)].alpha = mean * strength;
    arms_[static_cast<std::size_t>(i)].beta = (1.0 - mean) * strength;
  }
}

double SkillBandit::shapedReward(bool correct) const noexcept {
  const double outcome = correct ? 1.0 : 0.0;
  const double denom = std::max(cfg_.target_success, 1.0 - cfg_.target_success);
  return 1.0 - std::fabs(outcome - cfg_.target_success) / denom;
}

int SkillBandit::selectTier(Rng& rng, int forbidden_tier, int max_tier) {
  const int n = cfg_.tier_count;
  const bool has_forbidden = forbidden_tier >= 0 && forbidden_tier < n;
  const bool has_cap = max_tier >= 0 && max_tier < n - 1;
  auto allowed = [&](int i) {
    if (has_forbidden && i == forbidden_tier) return false;
    if (has_cap && i > max_tier) return false;
    return true;
  };

  if (n == 1) {
    last_tier_ = 0;
    return 0;
  }

  switch (cfg_.kind) {
    case PolicyKind::Static: {
      int t = cfg_.static_tier >= 0 ? cfg_.static_tier : n / 2;
      if (has_cap) t = std::min(t, max_tier);
      t = std::max(0, std::min(n - 1, t));
      if (!allowed(t)) {
        for (int i = t; i >= 0; --i) {
          if (allowed(i)) { t = i; break; }
        }
      }
      if (!allowed(t)) {
        for (int i = t; i < n; ++i) {
          if (allowed(i)) { t = i; break; }
        }
      }
      last_tier_ = t;
      return t;
    }

    case PolicyKind::Ucb1Shaped: {
      for (int i = 0; i < n; ++i) {
        if (allowed(i) && pulls_[static_cast<std::size_t>(i)] == 0) {
          last_tier_ = i;
          return i;
        }
      }
      const double log_n = std::log(static_cast<double>(std::max(1, total_pulls_)));
      int best = -1;
      double best_score = -std::numeric_limits<double>::infinity();
      for (int i = 0; i < n; ++i) {
        if (!allowed(i)) continue;
        const auto p = static_cast<double>(pulls_[static_cast<std::size_t>(i)]);
        const double m = reward_sum_[static_cast<std::size_t>(i)] / p;
        const double score = m + std::sqrt(2.0 * log_n / p);
        if (score > best_score + 1e-12) {
          best_score = score;
          best = i;
        }
      }
      if (best < 0) best = 0;
      last_tier_ = best;
      return best;
    }

    case PolicyKind::GreedyWindow: {
      int best = -1;
      double best_d = std::numeric_limits<double>::infinity();
      for (int i = 0; i < n; ++i) {
        if (!allowed(i)) continue;
        const double d = std::fabs(arms_[static_cast<std::size_t>(i)].mean() - cfg_.target_success);
        if (d < best_d - 1e-12) {
          best_d = d;
          best = i;
        }
      }
      if (best < 0) best = 0;
      last_tier_ = best;
      return best;
    }

    case PolicyKind::ThompsonSatisficing:
    case PolicyKind::LatentThompson:
    default: {
      for (int i = 0; i < n; ++i) {
        if (allowed(i) && pulls_[static_cast<std::size_t>(i)] < cfg_.min_pulls_per_tier) {
          last_tier_ = i;
          return i;
        }
      }
      int best = -1;
      double best_d = std::numeric_limits<double>::infinity();
      for (int i = 0; i < n; ++i) {
        if (!allowed(i)) continue;
        const double s = arms_[static_cast<std::size_t>(i)].sample(rng);
        const double d = std::fabs(s - cfg_.target_success);
        if (d < best_d - 1e-12) {
          best_d = d;
          best = i;
        }
      }
      if (best < 0) best = has_cap ? max_tier : 0;
      last_tier_ = best;
      return best;
    }
  }
}

void SkillBandit::observe(int tier, bool correct) {
  if (tier < 0 || tier >= cfg_.tier_count) return;
  const auto idx = static_cast<std::size_t>(tier);
  arms_[idx].observe(correct, cfg_.discount);
  pulls_[idx] += 1;
  reward_sum_[idx] += shapedReward(correct);
  ++total_pulls_;
  last_tier_ = tier;
}

int SkillBandit::pulls(int tier) const noexcept {
  if (tier < 0 || tier >= cfg_.tier_count) return 0;
  return pulls_[static_cast<std::size_t>(tier)];
}

const BetaPosterior& SkillBandit::posterior(int tier) const {
  static const BetaPosterior kFallback{};
  if (tier < 0 || tier >= cfg_.tier_count) return kFallback;
  return arms_[static_cast<std::size_t>(tier)];
}

double SkillBandit::estimatedSuccess(int tier) const { return posterior(tier).mean(); }

int SkillBandit::estimatedFlowTier() const {
  int best = 0;
  double best_d = std::numeric_limits<double>::infinity();
  for (int i = 0; i < cfg_.tier_count; ++i) {
    const double d = std::fabs(arms_[static_cast<std::size_t>(i)].mean() - cfg_.target_success);
    if (d < best_d - 1e-12) {
      best_d = d;
      best = i;
    }
  }
  return best;
}

std::pair<double, double> SkillBandit::evidenceAt(int tier) const {
  const BetaPosterior& arm = posterior(tier);
  return {arm.alpha, arm.beta};
}

void SkillBandit::setPosterior(int tier, double alpha, double beta) {
  if (tier < 0 || tier >= cfg_.tier_count) return;
  auto& arm = arms_[static_cast<std::size_t>(tier)];
  arm.alpha = std::max(alpha, 1e-3);
  arm.beta = std::max(beta, 1e-3);
}

void SkillBandit::setPulls(int tier, int n) {
  if (tier < 0 || tier >= cfg_.tier_count) return;
  const int previous = pulls_[static_cast<std::size_t>(tier)];
  const int updated = std::max(0, n);
  pulls_[static_cast<std::size_t>(tier)] = updated;
  total_pulls_ += updated - previous;
}

std::unique_ptr<IDifficultyPolicy> SkillBandit::clone() const {
  return std::unique_ptr<IDifficultyPolicy>(new SkillBandit(*this));
}

// ===========================================================================
// Structured Thompson sampling over a latent ability parameter
// ===========================================================================
LatentThompsonBandit::LatentThompsonBandit(BanditConfig cfg, int tier_count)
    : cfg_(cfg), tier_count_(std::max(1, tier_count)) {
  cfg_.target_success = clampd(cfg_.target_success, 0.05, 0.95);
  cfg_.tier_count = tier_count_;
  cfg_.latent_guess = clampd(cfg_.latent_guess, 0.0, 0.6);
  cfg_.latent_slope = std::max(0.1, cfg_.latent_slope);
  cfg_.latent_temper = clampd(cfg_.latent_temper, 0.0, 0.5);
  cfg_.latent_prior_sd = std::max(0.2, cfg_.latent_prior_sd);

  // Four grid points per tier is plenty of resolution for a 6-10 year old.
  const int steps = 4 * tier_count_;
  grid_.reserve(static_cast<std::size_t>(steps) + 1);
  for (int i = 0; i <= steps; ++i) {
    grid_.push_back(static_cast<double>(i) * static_cast<double>(tier_count_) /
                    static_cast<double>(steps));
  }
  pulls_.assign(static_cast<std::size_t>(tier_count_), 0);

  // Initial belief: a new child starts near the bottom of the mountain, with
  // enough spread that the first few answers move the estimate a long way.
  weights_.resize(grid_.size());
  const double mean = cfg_.latent_prior_mean;
  const double sd = cfg_.latent_prior_sd;
  for (std::size_t i = 0; i < grid_.size(); ++i) {
    const double z = (grid_[i] - mean) / sd;
    weights_[i] = std::exp(-0.5 * z * z);
  }
  normaliseTempered();
}

void LatentThompsonBandit::normaliseTempered() {
  double total = 0.0;
  for (double w : weights_) total += w;
  if (!(total > 0.0)) {
    const double uniform = 1.0 / static_cast<double>(weights_.size());
    std::fill(weights_.begin(), weights_.end(), uniform);
    return;
  }
  const double n = static_cast<double>(weights_.size());
  const double temper = cfg_.latent_temper;
  for (double& w : weights_) {
    w = w / total;
    // Keep a small uniform floor: the child's ability GROWS during a session,
    // so a posterior that collapsed onto one grid point could never follow it.
    w = (1.0 - temper) * w + temper / n;
  }
}

double LatentThompsonBandit::successGiven(double theta, int tier) const {
  const double guess=cfg_.ordered_route ? rulesForTrailTier(tier).randomSuccess() : cfg_.latent_guess;
  const double offset=cfg_.ordered_route ? std::log(std::max(.001,cfg_.target_success-guess)/(1-cfg_.target_success)) : offset_;
  const double x = (theta - static_cast<double>(tier)) * cfg_.latent_slope + offset;
  const double p = guess + (1.0 - guess) * logistic(x);
  return clampd(p, 0.02, 0.995);
}

double LatentThompsonBandit::sampleTheta(Rng& rng) const {
  const double draw = rng.nextDouble();
  double cumulative = 0.0;
  for (std::size_t i = 0; i < weights_.size(); ++i) {
    cumulative += weights_[i];
    if (draw <= cumulative) return grid_[i];
  }
  return grid_.back();
}

int LatentThompsonBandit::selectTier(Rng& rng, int forbidden_tier, int max_tier) {
  const int n = tier_count_;
  const bool has_forbidden = forbidden_tier >= 0 && forbidden_tier < n;
  const bool has_cap = max_tier >= 0 && max_tier < n - 1;
  auto allowed = [&](int i) {
    if (has_forbidden && i == forbidden_tier) return false;
    if (has_cap && i > max_tier) return false;
    return true;
  };

  if (n == 1) {
    last_tier_ = 0;
    return 0;
  }

  // Thompson sampling: draw an ability from the posterior, then choose the tier
  // that predicted success says is closest to the flow-zone target.
  const double theta = sampleTheta(rng);
  int best = -1;
  double best_d = std::numeric_limits<double>::infinity();
  for (int i = 0; i < n; ++i) {
    if (!allowed(i)) continue;
    const double d = std::fabs(successGiven(theta, i) - cfg_.target_success);
    if (d < best_d - 1e-12) {
      best_d = d;
      best = i;
    }
  }
  if (best < 0) best = has_cap ? max_tier : 0;
  last_tier_ = best;
  return best;
}

void LatentThompsonBandit::observe(int tier, bool correct) {
  if (tier < 0 || tier >= tier_count_) return;

  // Exact Bayes update of the grid posterior through the response model: one
  // observation at one tier re-prices EVERY tier at once. That is the property
  // the independent-arms ablation lacks, and it is why a strong child is not
  // parked at a tier that is far too easy.
  for (std::size_t i = 0; i < weights_.size(); ++i) {
    const double p = successGiven(grid_[i], tier);
    weights_[i] *= correct ? p : (1.0 - p);
    if (!(weights_[i] > 1e-300)) weights_[i] = 1e-300;
  }
  normaliseTempered();

  pulls_[static_cast<std::size_t>(tier)] += 1;
  ++total_pulls_;
  last_tier_ = tier;
}

int LatentThompsonBandit::pulls(int tier) const noexcept {
  if (tier < 0 || tier >= tier_count_) return 0;
  return pulls_[static_cast<std::size_t>(tier)];
}

double LatentThompsonBandit::thetaMean() const {
  double sum = 0.0;
  for (std::size_t i = 0; i < grid_.size(); ++i) sum += grid_[i] * weights_[i];
  return sum;
}

double LatentThompsonBandit::thetaSd() const {
  const double mean = thetaMean();
  double variance = 0.0;
  for (std::size_t i = 0; i < grid_.size(); ++i) {
    const double delta = grid_[i] - mean;
    variance += delta * delta * weights_[i];
  }
  return std::sqrt(std::max(0.0, variance));
}

double LatentThompsonBandit::estimatedSuccess(int tier) const {
  return successGiven(thetaMean(), tier);
}

int LatentThompsonBandit::estimatedFlowTier() const {
  const double theta = thetaMean();
  int best = 0;
  double best_d = std::numeric_limits<double>::infinity();
  for (int i = 0; i < tier_count_; ++i) {
    const double d = std::fabs(successGiven(theta, i) - cfg_.target_success);
    if (d < best_d - 1e-12) {
      best_d = d;
      best = i;
    }
  }
  return best;
}

std::pair<double, double> LatentThompsonBandit::evidenceAt(int tier) const {
  // The persisted schema stores Beta parameters, so the latent belief is
  // summarised as an equivalent Beta at this tier. The exact representation is
  // the attempts log, which is why replay() (not this) is the fidelity path.
  const double p = clampd(estimatedSuccess(tier), 0.01, 0.99);
  const double strength = 8.0;
  return {p * strength, (1.0 - p) * strength};
}

void LatentThompsonBandit::setPosterior(int tier, double alpha, double beta) {
  // Reconstruct a belief centred on the persisted tier. alpha/beta are used only
  // for their implied mean, because the schema cannot store a grid posterior.
  const double sum = alpha + beta;
  const double p = sum > 0.0 ? clampd(alpha / sum, 0.02, 0.98) : cfg_.target_success;
  const double centre = clampd(static_cast<double>(tier), 0.0, static_cast<double>(tier_count_));
  // Invert the response model approximately: p = target at theta = centre.
  const double inferred = centre + (cfg_.target_success - p) / std::max(0.2, cfg_.latent_slope);
  const double sd = 1.0;
  for (std::size_t i = 0; i < grid_.size(); ++i) {
    const double z = (grid_[i] - inferred) / sd;
    weights_[i] = std::exp(-0.5 * z * z);
  }
  normaliseTempered();
}

void LatentThompsonBandit::setPulls(int tier, int n) {
  if (tier < 0 || tier >= tier_count_) return;
  const int previous = pulls_[static_cast<std::size_t>(tier)];
  const int updated = std::max(0, n);
  pulls_[static_cast<std::size_t>(tier)] = updated;
  total_pulls_ += updated - previous;
}

std::unique_ptr<IDifficultyPolicy> LatentThompsonBandit::clone() const {
  return std::unique_ptr<IDifficultyPolicy>(new LatentThompsonBandit(*this));
}

std::unique_ptr<IDifficultyPolicy> makeDifficultyPolicy(const BanditConfig& cfg, int tier_count) {
  BanditConfig local = cfg;
  local.tier_count = std::max(1, tier_count);
  if (local.kind == PolicyKind::LatentThompson) {
    return std::unique_ptr<IDifficultyPolicy>(new LatentThompsonBandit(local, local.tier_count));
  }
  return std::unique_ptr<IDifficultyPolicy>(new SkillBandit(local));
}

}  // namespace sdp
