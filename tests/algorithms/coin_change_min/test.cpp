#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct CoinInput {
  int amount = 0;
  std::vector<int> coins;
};

CoinInput
test_coin_change_min_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 6);
  std::uniform_int_distribution<int> a_dist(0, 40);
  std::uniform_int_distribution<int> c_dist(1, 12);
  CoinInput input;
  input.amount = a_dist(rng);
  const int n = n_dist(rng);
  input.coins.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    input.coins.push_back(c_dist(rng));
  }
  return input;
}

std::filesystem::path
test_coin_change_min_styio() {
  return styio::testing::algorithms::styio_program("coin_change_min", "coin_change_min.styio");
}

std::string
format_coin_change_min_input(const CoinInput& input) {
  const int n = static_cast<int>(input.coins.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(2 + n + input.amount + 1));
  encoded.push_back(n);
  encoded.push_back(input.amount);
  for (int c : input.coins) {
    encoded.push_back(c);
  }
  for (int i = 0; i <= input.amount; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_coin_change_min) {
  std::mt19937 rng(0xC01A);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const CoinInput input = test_coin_change_min_random_input(rng);
    const std::string expected =
      test_coin_change_min_cpp_output(input.amount, input.coins);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_coin_change_min_styio(), format_coin_change_min_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_coin_change_min_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_coin_change_min_fixed_cases) {
  std::vector<int> enc = {4, 30, 1, 5, 10, 25};
  for (int i = 0; i <= 30; ++i) {
    enc.push_back(0);
  }
  const std::string thirty = styio::testing::algorithms::format_i32_list(enc) + "\n";

  const std::vector<std::pair<std::string, std::string>> fixed = {
    { "[0,0,0]\n", "0\n" },
    { thirty, "2\n" },
    { "[1,3,2,0,0,0,0]\n", "-1\n" },
    { "[]\n", "-1\n" },
  };

  for (const auto& [stdin_text, expected] : fixed) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_coin_change_min_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
