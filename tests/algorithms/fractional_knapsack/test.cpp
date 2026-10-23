#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct FracInput {
  int capacity = 0;
  std::vector<std::pair<int, int>> items; // (weight, value)
};

FracInput
test_fractional_knapsack_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 8);
  std::uniform_int_distribution<int> wcap_dist(0, 40);
  std::uniform_int_distribution<int> w_dist(1, 15);
  std::uniform_int_distribution<int> v_dist(0, 30);
  FracInput input;
  input.capacity = wcap_dist(rng);
  const int n = n_dist(rng);
  input.items.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    input.items.push_back({w_dist(rng), v_dist(rng)});
  }
  return input;
}

std::filesystem::path
test_fractional_knapsack_styio() {
  return styio::testing::algorithms::styio_program("fractional_knapsack",
                                                   "fractional_knapsack.styio");
}

std::string
format_fractional_knapsack_input(const FracInput& input) {
  const int n = static_cast<int>(input.items.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(2 + 3 * n));
  encoded.push_back(n);
  encoded.push_back(input.capacity);
  for (const auto& [w, v] : input.items) {
    encoded.push_back(w);
  }
  for (const auto& [w, v] : input.items) {
    encoded.push_back(v);
  }
  for (int i = 0; i < n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_fractional_knapsack) {
  std::mt19937 rng(0xF7AC);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const FracInput input = test_fractional_knapsack_random_input(rng);
    const std::string expected =
      test_fractional_knapsack_cpp_output(input.capacity, input.items);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_fractional_knapsack_styio(), format_fractional_knapsack_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_fractional_knapsack_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_fractional_knapsack_fixed_cases) {
  // CLRS-style: (w,v)=(10,60),(20,100),(30,120), W=50 -> 240
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0,0]\n", "0\n" },
    { "[3,50,10,20,30,60,100,120,0,0,0]\n", "240\n" },
    { "[]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_fractional_knapsack_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
