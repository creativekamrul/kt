#pragma once
// ===========================================================================
// sdp/rng.hpp - deterministic, platform-independent randomness.
//
// Level generation must be reproducible from a seed: "same spec, same puzzle",
// on any compiler, any OS, forever. That is what makes generated content
// debuggable and replayable straight from the attempts log. std::mt19937 plus
// std::uniform_*_distribution does NOT guarantee it, because the distribution
// algorithms are implementation-defined. Hence our own PRNG.
//
// Algorithm: xoshiro256** for streams, SplitMix64 for seeding.
// ===========================================================================

#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vector>

namespace sdp {

class Rng {
 public:
  Rng() : Rng(0x9E3779B97F4A7C15ull) {}
  explicit Rng(std::uint64_t seed) { seedWith(seed); }

  void seedWith(std::uint64_t seed) noexcept;

  std::uint64_t nextU64() noexcept;
  std::uint32_t nextU32() noexcept { return static_cast<std::uint32_t>(nextU64() >> 32); }

  double nextDouble() noexcept;                      // [0, 1)
  double nextDouble(double lo, double hi) noexcept;  // [lo, hi)
  int nextInt(int lo, int hi) noexcept;              // inclusive at both ends
  bool chance(double p) noexcept { return nextDouble() < p; }

  template <typename T>
  void shuffle(std::vector<T>& v) noexcept {
    if (v.size() < 2) return;
    for (std::size_t i = v.size() - 1; i > 0; --i) {
      const std::size_t j = static_cast<std::size_t>(nextInt(0, static_cast<int>(i)));
      if (i != j) std::swap(v[i], v[j]);
    }
  }

  /// Deterministic fingerprint of the internal state - used by tests to assert
  /// that two independently constructed generators agree.
  std::uint64_t fingerprint() const noexcept;

 private:
  std::uint64_t s_[4]{};
};

// ---------------------------------------------------------------------------
// Seed derivation: mix several integers (child id, attempt index, skill hash,
// tier, level kind ...) into one 32-bit seed for LevelSpec.
// ---------------------------------------------------------------------------
std::uint64_t hashCombine(std::uint64_t h, std::uint64_t value) noexcept;
std::uint64_t hashString(std::string_view s) noexcept;
std::uint32_t deriveSeed(std::initializer_list<std::uint64_t> parts) noexcept;

// ---------------------------------------------------------------------------
// Distributions (hand-rolled for determinism).
// ---------------------------------------------------------------------------
double sampleNormal(Rng& rng, double mean, double sd) noexcept;            // Box-Muller
double sampleGamma(Rng& rng, double shape, double scale = 1.0) noexcept;   // Marsaglia-Tsang
double sampleBeta(Rng& rng, double a, double b) noexcept;                  // ratio of gammas
double sampleExponential(Rng& rng, double rate) noexcept;

}  // namespace sdp
