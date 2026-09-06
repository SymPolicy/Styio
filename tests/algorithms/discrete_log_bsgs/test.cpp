#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

const std::vector<int> kPrimes = {3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47};

std::filesystem::path
test_discrete_log_bsgs_styio() {
  return styio::testing::algorithms::styio_program("discrete_log_bsgs", "discrete_log_bsgs.styio");
}

std::string
format_discrete_log_bsgs_input(int a, int b, int p) {
  return styio::testing::algorithms::format_i32_list({a, b, p}) + "\n";
}

long long
mod_pow(long long a, long long e, long long p) {
  long long r = 1 % p;
  a %= p;
  while (e > 0) {
    if (e & 1) {
      r = (r * a) % p;
    }
    a = (a * a) % p;
    e >>= 1;
  }
  return r;
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_discrete_log_bsgs) {
  std::mt19937 rng(0xD15C);
  std::uniform_int_distribution<int> pi(0, static_cast<int>(kPrimes.size()) - 1);
  for (int iteration = 0; iteration < 100; ++iteration) {
    const int p = kPrimes[static_cast<std::size_t>(pi(rng))];
    std::uniform_int_distribution<int> v(0, p - 1);
    const int a = v(rng);
    const int x = v(rng);
    const int b = static_cast<int>(mod_pow(a, x, p));
    const std::string expected = test_discrete_log_bsgs_cpp_output(a, b, p);
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_discrete_log_bsgs_styio(), format_discrete_log_bsgs_input(a, b, p));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << format_discrete_log_bsgs_input(a, b, p);
  }
}

TEST(StyioCppReferenceEquivalence, test_discrete_log_bsgs_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[2,1,5]\n", "0\n" },
    { "[2,3,5]\n", "3\n" },
    { "[0,0,7]\n", "1\n" },
    { "[0,1,7]\n", "0\n" },
    { "[3,4,0]\n", "-1\n" },
    { "[]\n", "-1\n" },
  };
  for (const auto& [stdin_text, expected] : cases) {
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_discrete_log_bsgs_styio(), stdin_text);
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected) << stdin_text;
  }
}
