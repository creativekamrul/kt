#include <algorithm>
#include <cmath>
#include <set>
#include <vector>

#include "sdp/rng.hpp"
#include "test_framework.hpp"

using namespace sdp;

SDP_TEST(rng, same_seed_same_stream) {
  Rng a(12345), b(12345);
  for (int i = 0; i < 64; ++i) CHECK_EQ(a.nextU64(), b.nextU64());
  CHECK_EQ(Rng(1).fingerprint(), Rng(1).fingerprint());
  CHECK_NE(Rng(1).fingerprint(), Rng(2).fingerprint());
}

SDP_TEST(rng, doubles_stay_in_unit_interval) {
  Rng r(7);
  for (int i = 0; i < 20000; ++i) {
    const double d = r.nextDouble();
    CHECK(d >= 0.0 && d < 1.0);
  }
}

SDP_TEST(rng, double_uniformity) {
  Rng r(99);
  double sum = 0.0;
  const int n = 50000;
  for (int i = 0; i < n; ++i) sum += r.nextDouble();
  CHECK_NEAR(sum / n, 0.5, 0.01);
}

SDP_TEST(rng, int_bounds_are_inclusive_and_covered) {
  Rng r(3);
  std::set<int> seen;
  for (int i = 0; i < 5000; ++i) {
    const int v = r.nextInt(2, 5);
    CHECK(v >= 2 && v <= 5);
    seen.insert(v);
  }
  CHECK_EQ(seen.size(), static_cast<std::size_t>(4));
  CHECK_EQ(r.nextInt(4, 4), 4);  // degenerate range
}

SDP_TEST(rng, shuffle_is_a_permutation) {
  Rng r(11);
  std::vector<int> v;
  for (int i = 0; i < 50; ++i) v.push_back(i);
  r.shuffle(v);
  std::vector<int> sorted = v;
  std::sort(sorted.begin(), sorted.end());
  for (int i = 0; i < 50; ++i) CHECK_EQ(sorted[static_cast<std::size_t>(i)], i);
  CHECK_NE(v[0], 0);  // practically certain for 50 elements, and fixed by seed
}

SDP_TEST(rng, gamma_mean_matches_shape_times_scale) {
  Rng r(21);
  double sum = 0.0;
  const int n = 20000;
  for (int i = 0; i < n; ++i) sum += sampleGamma(r, 3.0, 2.0);
  CHECK_NEAR(sum / n, 6.0, 0.2);
}

SDP_TEST(rng, gamma_supports_small_shapes) {
  // alpha < 1 is what discounted Thompson sampling produces, so the boosted
  // branch of Marsaglia-Tsang must be exercised.
  Rng r(22);
  double sum = 0.0;
  const int n = 20000;
  for (int i = 0; i < n; ++i) sum += sampleGamma(r, 0.4, 1.0);
  CHECK_NEAR(sum / n, 0.4, 0.08);
}

SDP_TEST(rng, beta_mean_and_support) {
  Rng r(31);
  double sum = 0.0;
  const int n = 20000;
  for (int i = 0; i < n; ++i) {
    const double b = sampleBeta(r, 2.0, 6.0);
    CHECK(b >= 0.0 && b <= 1.0);
    sum += b;
  }
  CHECK_NEAR(sum / n, 2.0 / 8.0, 0.02);
}

SDP_TEST(rng, derive_seed_is_stable_and_sensitive) {
  const std::uint32_t s1 = deriveSeed({1, 2, 3});
  CHECK_EQ(s1, deriveSeed({1, 2, 3}));
  CHECK_NE(s1, deriveSeed({1, 2, 4}));
  CHECK_NE(hashString("math.addition"), hashString("math.subtraction"));
}
