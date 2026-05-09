#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace {

struct MismatchInput
{
  std::vector<int> lhs;
  std::vector<int> rhs;
};

MismatchInput
test_mismatch_index_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> size_dist(0, 42);
  std::uniform_int_distribution<int> value_dist(-25, 25);
  std::uniform_int_distribution<int> delta_dist(-6, 6);
  std::bernoulli_distribution share_prefix_dist(0.55);

  std::vector<int> lhs(static_cast<std::size_t>(size_dist(rng)));
  for (int& value : lhs) {
    value = value_dist(rng);
  }

  const int rhs_size =
    std::max(0, static_cast<int>(lhs.size()) + delta_dist(rng));
  std::vector<int> rhs(static_cast<std::size_t>(rhs_size));
  for (int& value : rhs) {
    value = value_dist(rng);
  }

  if (share_prefix_dist(rng)) {
    const std::size_t limit = std::min(lhs.size(), rhs.size());
    for (std::size_t i = 0; i < limit; ++i) {
      rhs[i] = lhs[i];
    }
  }

  return { lhs, rhs };
}

std::filesystem::path
test_mismatch_index_styio() {
  return styio::testing::algorithms::styio_program(
    "mismatch_index", "mismatch_index.styio");
}

std::string
format_mismatch_index_input(const MismatchInput& input) {
  std::vector<int> encoded;
  encoded.reserve(input.lhs.size() + input.rhs.size() + 2);
  encoded.push_back(static_cast<int>(input.lhs.size()));
  encoded.push_back(static_cast<int>(input.rhs.size()));
  encoded.insert(encoded.end(), input.lhs.begin(), input.lhs.end());
  encoded.insert(encoded.end(), input.rhs.begin(), input.rhs.end());
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_mismatch_index) {
  std::mt19937 rng(0xA15A47C);

  for (int iteration = 0; iteration < 180; ++iteration) {
    const MismatchInput input = test_mismatch_index_random_input(rng);
    const std::string expected =
      test_mismatch_index_cpp_output(input.lhs, input.rhs);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_mismatch_index_styio(), format_mismatch_index_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_mismatch_index_input(input)
      << "stderr=" << actual.stderr_text;
  }
}
