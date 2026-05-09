#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

namespace {

struct SubrangeFindEndInput
{
  std::vector<int> haystack;
  std::vector<int> needle;
};

SubrangeFindEndInput
test_subrange_find_end_index_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> hay_size_dist(0, 34);
  std::uniform_int_distribution<int> needle_size_dist(0, 8);
  std::uniform_int_distribution<int> value_dist(-5, 5);
  std::bernoulli_distribution present_dist(0.60);

  std::vector<int> haystack(static_cast<std::size_t>(hay_size_dist(rng)));
  for (int& value : haystack) {
    value = value_dist(rng);
  }

  std::vector<int> needle(static_cast<std::size_t>(needle_size_dist(rng)));
  for (int& value : needle) {
    value = value_dist(rng);
  }

  if (!haystack.empty() && present_dist(rng)) {
    std::uniform_int_distribution<std::size_t> index_dist(0, haystack.size() - 1);
    const std::size_t start = index_dist(rng);
    const std::size_t count =
      std::min<std::size_t>(needle.size(), haystack.size() - start);
    needle.assign(haystack.begin() + static_cast<std::ptrdiff_t>(start),
                  haystack.begin() + static_cast<std::ptrdiff_t>(start + count));
  }

  return { haystack, needle };
}

std::filesystem::path
test_subrange_find_end_index_styio() {
  return styio::testing::algorithms::styio_program(
    "subrange_find_end_index", "subrange_find_end_index.styio");
}

std::string
format_subrange_find_end_index_input(const SubrangeFindEndInput& input) {
  std::vector<int> encoded;
  encoded.reserve(input.haystack.size() + input.needle.size() + 2);
  encoded.push_back(static_cast<int>(input.haystack.size()));
  encoded.push_back(static_cast<int>(input.needle.size()));
  encoded.insert(encoded.end(), input.haystack.begin(), input.haystack.end());
  encoded.insert(encoded.end(), input.needle.begin(), input.needle.end());
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_subrange_find_end_index) {
  std::mt19937 rng(0xF1ADE0D);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const SubrangeFindEndInput input =
      test_subrange_find_end_index_random_input(rng);
    const std::string expected =
      test_subrange_find_end_index_cpp_output(input.haystack, input.needle);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_subrange_find_end_index_styio(),
        format_subrange_find_end_index_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_subrange_find_end_index_input(input)
      << "stderr=" << actual.stderr_text;
  }
}
