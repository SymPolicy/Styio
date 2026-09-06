#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct CpInput {
  std::vector<std::pair<int, int>> pts;
};

CpInput
test_closest_pair_dist_sq_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(2, 20);
  std::uniform_int_distribution<int> c_dist(-30, 30);
  CpInput input;
  const int n = n_dist(rng);
  input.pts.resize(static_cast<std::size_t>(n));
  for (auto& p : input.pts) {
    p = {c_dist(rng), c_dist(rng)};
  }
  return input;
}

std::filesystem::path
test_closest_pair_dist_sq_styio() {
  return styio::testing::algorithms::styio_program(
    "closest_pair_dist_sq", "closest_pair_dist_sq.styio");
}

std::string
format_closest_pair_dist_sq_input(const CpInput& input) {
  const int n = static_cast<int>(input.pts.size());
  std::vector<int> encoded;
  encoded.push_back(n);
  for (const auto& [x, y] : input.pts) {
    encoded.push_back(x);
    encoded.push_back(y);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_closest_pair_dist_sq) {
  std::mt19937 rng(0xC105);
  for (int iteration = 0; iteration < 100; ++iteration) {
    const CpInput input = test_closest_pair_dist_sq_random_input(rng);
    const std::string expected = test_closest_pair_dist_sq_cpp_output(input.pts);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_closest_pair_dist_sq_styio(), format_closest_pair_dist_sq_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_closest_pair_dist_sq_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_closest_pair_dist_sq_fixed_cases) {
  const std::string classic = "[3,0,0,3,4,1,0]\n"; // dist^2 between (0,0)-(1,0)=1
  const std::vector<std::pair<std::string, std::string>> cases = {
    { classic, "1\n" },
    { "[1,0,0]\n", "-1\n" },
    { "[]\n", "-1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_closest_pair_dist_sq_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
