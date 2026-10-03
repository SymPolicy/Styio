#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

std::vector<int>
test_rod_cutting_random_prices(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 14);
  std::uniform_int_distribution<int> p_dist(0, 40);
  const int n = n_dist(rng);
  std::vector<int> prices(static_cast<std::size_t>(n));
  for (int& p : prices) {
    p = p_dist(rng);
  }
  return prices;
}

std::filesystem::path
test_rod_cutting_styio() {
  return styio::testing::algorithms::styio_program("rod_cutting", "rod_cutting.styio");
}

std::string
format_rod_cutting_input(const std::vector<int>& prices) {
  // Encoding: [n, p1, p2, ..., pn, then (n+1) workspace slots for Styio DP]
  const int n = static_cast<int>(prices.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(1 + n + n + 1));
  encoded.push_back(n);
  encoded.insert(encoded.end(), prices.begin(), prices.end());
  for (int i = 0; i <= n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_rod_cutting) {
  std::mt19937 rng(0xC07C07);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const std::vector<int> prices = test_rod_cutting_random_prices(rng);
    const std::string expected = test_rod_cutting_cpp_output(prices);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_rod_cutting_styio(), format_rod_cutting_input(prices));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_rod_cutting_input(prices)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_rod_cutting_fixed_cases) {
  // CLRS example-ish: prices 1..4 = 1,5,8,9 -> opt 10 (2+2)
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0,0]\n", "0\n" },
    { "[1,1,0,0]\n", "1\n" },
    { "[4,1,5,8,9,0,0,0,0,0]\n", "10\n" },
    { "[]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_rod_cutting_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
