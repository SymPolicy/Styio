#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct InnerProductInput
{
  std::vector<int> lhs;
  std::vector<int> rhs;
};

InnerProductInput
test_inner_product_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> size_dist(0, 36);
  std::uniform_int_distribution<int> value_dist(-30, 30);

  const auto size = static_cast<std::size_t>(size_dist(rng));
  std::vector<int> lhs(size);
  std::vector<int> rhs(size);
  for (std::size_t i = 0; i < size; ++i) {
    lhs[i] = value_dist(rng);
    rhs[i] = value_dist(rng);
  }

  return { lhs, rhs };
}

std::filesystem::path
test_inner_product_styio() {
  return styio::testing::algorithms::styio_program(
    "inner_product", "inner_product.styio");
}

std::string
format_inner_product_input(const InnerProductInput& input) {
  std::vector<int> encoded;
  encoded.reserve(input.lhs.size() + input.rhs.size() + 1);
  encoded.push_back(static_cast<int>(input.lhs.size()));
  encoded.insert(encoded.end(), input.lhs.begin(), input.lhs.end());
  encoded.insert(encoded.end(), input.rhs.begin(), input.rhs.end());
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_inner_product) {
  std::mt19937 rng(0x1E470D);

  for (int iteration = 0; iteration < 180; ++iteration) {
    const InnerProductInput input = test_inner_product_random_input(rng);
    const std::string expected =
      test_inner_product_cpp_output(input.lhs, input.rhs);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_inner_product_styio(), format_inner_product_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_inner_product_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_inner_product_malformed_input) {
  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[]\n", "0\n" },
    { "[0]\n", "0\n" },
    { "[2,1,2,3]\n", "0\n" },
    { "[2,1,2,3,4]\n", "11\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_inner_product_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
