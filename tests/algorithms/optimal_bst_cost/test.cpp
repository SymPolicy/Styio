#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

std::filesystem::path
test_optimal_bst_cost_styio() {
  return styio::testing::algorithms::styio_program("optimal_bst_cost",
                                                   "optimal_bst_cost.styio");
}

std::string
format_optimal_bst_cost_input(const std::vector<int>& freq) {
  const int n = static_cast<int>(freq.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(1 + n + n * n + n * n));
  encoded.push_back(n);
  for (int f : freq) {
    encoded.push_back(f);
  }
  for (int i = 0; i < n * n; ++i) {
    encoded.push_back(0);
  }
  for (int i = 0; i < n * n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_optimal_bst_cost) {
  std::mt19937 rng(0x0B57);
  std::uniform_int_distribution<int> n_dist(0, 7);
  std::uniform_int_distribution<int> f_dist(1, 9);

  for (int iteration = 0; iteration < 100; ++iteration) {
    const int n = n_dist(rng);
    std::vector<int> freq;
    freq.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
      freq.push_back(f_dist(rng));
    }
    const std::string expected = test_optimal_bst_cost_cpp_output(freq);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_optimal_bst_cost_styio(), format_optimal_bst_cost_input(freq));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_optimal_bst_cost_input(freq)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_optimal_bst_cost_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0]\n", "0\n" },
    { "[1,5,0,0]\n", "5\n" },
    { "[2,4,2,0,0,0,0,0,0,0,0]\n", "8\n" },
    { "[4,4,2,6,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]\n",
      "26\n" },
    { "[]\n", "-1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_optimal_bst_cost_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
