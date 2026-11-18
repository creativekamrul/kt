#include "sdp/rng.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sdp {
namespace {

constexpr double kPi = 3.14159265358979323846;

inline std::uint64_t rotl(std::uint64_t x, int k) noexcept {
  return (x << k) | (x >> (64 - k));
}

inline std::uint64_t splitmix64(std::uint64_t& state) noexcept {
  std::uint64_t z = (state += 0x9E3779B97F4A7C15ull);
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

}  // namespace

void Rng::seedWith(std::uint64_t seed) noexcept {
  std::uint64_t sm = seed;
  for (auto& slot : s_) slot = splitmix64(sm);
  if ((s_[0] | s_[1] | s_[2] | s_[3]) == 0) s_[0] = 1;  // xoshiro needs non-zero state
}

std::uint64_t Rng::nextU64() noexcept {
  const std::uint64_t result = rotl(s_[1] * 5, 7) * 9;
  const std::uint64_t t = s_[1] << 17;
  s_[2] ^= s_[0];
  s_[3] ^= s_[1];
  s_[1] ^= s_[2];
  s_[0] ^= s_[3];
  s_[2] ^= t;
  s_[3] = rotl(s_[3], 45);
  return result;
}

double Rng::nextDouble() noexcept {
  // 53 significant bits, exactly representable in a double.
  return static_cast<double>(nextU64() >> 11) * (1.0 / 9007199254740992.0);
}

double Rng::nextDouble(double lo, double hi) noexcept { return lo + (hi - lo) * nextDouble(); }

int Rng::nextInt(int lo, int hi) noexcept {
  if (hi <= lo) return lo;
  const std::uint64_t range = static_cast<std::uint64_t>(hi - lo) + 1ull;
  // Rejection sampling keeps the result unbiased and portable.
  const std::uint64_t threshold = (0ull - range) % range;
  for (;;) {
    const std::uint64_t r = nextU64();
    if (r >= threshold) return lo + static_cast<int>(r % range);
  }
}

std::uint64_t Rng::fingerprint() const noexcept {
  std::uint64_t h = 0x243F6A8885A308D3ull;
  for (auto v : s_) h = hashCombine(h, v);
  return h;
}

std::uint64_t hashCombine(std::uint64_t h, std::uint64_t value) noexcept {
  std::uint64_t z = h ^ (value + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2));
  z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
  z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
  return z ^ (z >> 31);
}

std::uint64_t hashString(std::string_view s) noexcept {
  std::uint64_t h = 0xCBF29CE484222325ull;  // FNV-1a
  for (unsigned char c : s) {
    h ^= static_cast<std::uint64_t>(c);
    h *= 0x100000001B3ull;
  }
  return hashCombine(h, s.size());
}

std::uint32_t deriveSeed(std::initializer_list<std::uint64_t> parts) noexcept {
  std::uint64_t h = 0x2545F4914F6CDD1Dull;
  for (auto p : parts) h = hashCombine(h, p);
  return static_cast<std::uint32_t>((h ^ (h >> 32)) & 0xFFFFFFFFull);
}

double sampleNormal(Rng& rng, double mean, double sd) noexcept {
  double u1 = rng.nextDouble();
  const double u2 = rng.nextDouble();
  if (u1 <= 0.0) u1 = std::numeric_limits<double>::min();
  const double z = std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * kPi * u2);
  return mean + sd * z;
}

double sampleGamma(Rng& rng, double shape, double scale) noexcept {
  if (!(shape > 0.0)) return 0.0;
  if (shape < 1.0) {
    // Boost: Gamma(a) = Gamma(a + 1) * U^(1/a)   (Marsaglia-Tsang, 2000)
    const double u = std::max(rng.nextDouble(), std::numeric_limits<double>::min());
    return sampleGamma(rng, shape + 1.0, scale) * std::pow(u, 1.0 / shape);
  }
  const double d = shape - 1.0 / 3.0;
  const double c = 1.0 / std::sqrt(9.0 * d);
  for (;;) {
    double x = 0.0;
    double v = 0.0;
    do {
      x = sampleNormal(rng, 0.0, 1.0);
      v = 1.0 + c * x;
    } while (v <= 0.0);
    v = v * v * v;
    const double u = rng.nextDouble();
    if (u < 1.0 - 0.0331 * x * x * x * x) return d * v * scale;
    const double lu = std::log(std::max(u, std::numeric_limits<double>::min()));
    if (lu < 0.5 * x * x + d * (1.0 - v + std::log(v))) return d * v * scale;
  }
}

double sampleBeta(Rng& rng, double a, double b) noexcept {
  const double x = sampleGamma(rng, a, 1.0);
  const double y = sampleGamma(rng, b, 1.0);
  const double total = x + y;
  if (!(total > 0.0)) return a / (a + b);  // degenerate safeguard
  return x / total;
}

double sampleExponential(Rng& rng, double rate) noexcept {
  const double u = std::max(rng.nextDouble(), std::numeric_limits<double>::min());
  return -std::log(u) / std::max(rate, std::numeric_limits<double>::min());
}

}  // namespace sdp
