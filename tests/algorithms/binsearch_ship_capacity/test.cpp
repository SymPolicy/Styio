#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct ShipInput {
  int days = 1;
  std::vector<int> weights;
};

ShipInput
test_binsearch_ship_capacity_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(1, 12);
  std::uniform_int_distribution<int> d_dist(1, 10);
  std::uniform_int_distribution<int> w_dist(1, 15);
  ShipInput input;
  const int n = n_dist(rng);
  input.days = d_dist(rng);
  input.weights.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    input.weights.push_back(w_dist(rng));
  }
  return input;
}

std::filesystem::path
test_binsearch_ship_capacity_styio() {
  return styio::testing::algorithms::styio_program(
    "binsearch_ship_capacity", "binsearch_ship_capacity.styio");
}

std::string
format_binsearch_ship_capacity_input(const ShipInput& input) {
  const int n = static_cast<int>(input.weights.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(2 + n));
  encoded.push_back(n);
  encoded.push_back(input.days);
  for (int w : input.weights) {
    encoded.push_back(w);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_binsearch_ship_capacity) {
  std::mt19937 rng(0x51C0);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const ShipInput input = test_binsearch_ship_capacity_random_input(rng);
    const std::string expected =
      test_binsearch_ship_capacity_cpp_output(input.days, input.weights);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_binsearch_ship_capacity_styio(),
        format_binsearch_ship_capacity_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_binsearch_ship_capacity_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_binsearch_ship_capacity_fixed_cases) {
  // weights 1,2,3,4,5 days=3 => capacity 6 (classic)
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[5,3,1,2,3,4,5]\n", "6\n" },
    { "[1,1,10]\n", "10\n" },
    { "[0,3]\n", "-1\n" },
    { "[2,0,1,1]\n", "-1\n" },
    { "[]\n", "-1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_binsearch_ship_capacity_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
