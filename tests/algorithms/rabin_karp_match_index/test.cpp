#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct RkInput {
  std::vector<int> text;
  std::vector<int> pattern;
};

RkInput
test_rabin_karp_match_index_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 20);
  std::uniform_int_distribution<int> m_dist(0, 8);
  std::uniform_int_distribution<int> a_dist(0, 4);
  RkInput input;
  const int n = n_dist(rng);
  const int m = m_dist(rng);
  input.text.reserve(static_cast<std::size_t>(n));
  input.pattern.reserve(static_cast<std::size_t>(m));
  for (int i = 0; i < n; ++i) {
    input.text.push_back(a_dist(rng));
  }
  for (int i = 0; i < m; ++i) {
    input.pattern.push_back(a_dist(rng));
  }
  return input;
}

std::filesystem::path
test_rabin_karp_match_index_styio() {
  return styio::testing::algorithms::styio_program(
    "rabin_karp_match_index", "rabin_karp_match_index.styio");
}

std::string
format_rabin_karp_match_index_input(const RkInput& input) {
  const int n = static_cast<int>(input.text.size());
  const int m = static_cast<int>(input.pattern.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(2 + n + m));
  encoded.push_back(n);
  encoded.push_back(m);
  for (int v : input.text) {
    encoded.push_back(v);
  }
  for (int v : input.pattern) {
    encoded.push_back(v);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_rabin_karp_match_index) {
  std::mt19937 rng(0xCB33);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const RkInput input = test_rabin_karp_match_index_random_input(rng);
    const std::string expected =
      test_rabin_karp_match_index_cpp_output(input.text, input.pattern);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_rabin_karp_match_index_styio(),
        format_rabin_karp_match_index_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_rabin_karp_match_index_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_rabin_karp_match_index_fixed_cases) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[5,3,1,2,1,2,3,1,2,3]\n", "2\n" },
    { "[3,3,1,2,3,1,2,4]\n", "-1\n" },
    { "[0,0]\n", "0\n" },
    { "[2,0,9,8]\n", "0\n" },
    { "[]\n", "-1\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_rabin_karp_match_index_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
