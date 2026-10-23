#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Knapsack01Input
{
  int capacity = 0;
  std::vector<int> weights;
  std::vector<int> values;
};

Knapsack01Input
test_knapsack_01_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 10);
  std::uniform_int_distribution<int> w_dist(1, 8);
  std::uniform_int_distribution<int> v_dist(0, 20);
  std::uniform_int_distribution<int> c_dist(0, 24);

  Knapsack01Input input;
  input.capacity = c_dist(rng);
  const int n = n_dist(rng);
  input.weights.resize(static_cast<std::size_t>(n));
  input.values.resize(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    input.weights[static_cast<std::size_t>(i)] = w_dist(rng);
    input.values[static_cast<std::size_t>(i)] = v_dist(rng);
  }
  return input;
}

std::filesystem::path
test_knapsack_01_styio() {
  return styio::testing::algorithms::styio_program("knapsack_01", "knapsack_01.styio");
}

std::string
format_knapsack_01_input(const Knapsack01Input& input) {
  // Encoding: [n, W, w1..wn, v1..vn, then (W+1) DP workspace zeros]
  const int n = static_cast<int>(input.weights.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(2 + n + n + input.capacity + 1));
  encoded.push_back(n);
  encoded.push_back(input.capacity);
  encoded.insert(encoded.end(), input.weights.begin(), input.weights.end());
  encoded.insert(encoded.end(), input.values.begin(), input.values.end());
  for (int i = 0; i <= input.capacity; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_knapsack_01) {
  std::mt19937 rng(0xA5A001);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const Knapsack01Input input = test_knapsack_01_random_input(rng);
    const std::string expected =
      test_knapsack_01_cpp_output(input.capacity, input.weights, input.values);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_knapsack_01_styio(), format_knapsack_01_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_knapsack_01_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_knapsack_01_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0,0,0]\n", "0\n" },
    // n=3,W=5, w=2,3,4 v=3,4,5 -> opt 7 (2+3); workspace 6 zeros
    { "[3,5,2,3,4,3,4,5,0,0,0,0,0,0]\n", "7\n" },
    { "[]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_knapsack_01_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
