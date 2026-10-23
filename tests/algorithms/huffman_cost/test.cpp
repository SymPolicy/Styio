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
test_huffman_cost_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 10);
  std::uniform_int_distribution<int> f_dist(1, 20);
  const int n = n_dist(rng);
  std::vector<int> freqs;
  freqs.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    freqs.push_back(f_dist(rng));
  }
  return freqs;
}

std::filesystem::path
test_huffman_cost_styio() {
  return styio::testing::algorithms::styio_program("huffman_cost", "huffman_cost.styio");
}

std::string
format_huffman_cost_input(const std::vector<int>& freqs) {
  const int n = static_cast<int>(freqs.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(1 + 3 * n));
  encoded.push_back(n);
  for (int f : freqs) {
    encoded.push_back(f);
  }
  for (int i = 0; i < n; ++i) {
    encoded.push_back(0);
  }
  for (int i = 0; i < n; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_huffman_cost) {
  std::mt19937 rng(0xA11FF);

  for (int iteration = 0; iteration < 160; ++iteration) {
    const auto input = test_huffman_cost_random_input(rng);
    const std::string expected = test_huffman_cost_cpp_output(input);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_huffman_cost_styio(), format_huffman_cost_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_huffman_cost_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_huffman_cost_fixed_cases) {
  const std::vector<int> clrs = {45, 13, 12, 16, 9, 5};
  const std::string clrs_in = format_huffman_cost_input(clrs);
  const std::string clrs_out = test_huffman_cost_cpp_output(clrs);

  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0]\n", "0\n" },
    { "[1,7,0,0]\n", "0\n" },
    { clrs_in, clrs_out },
    { "[]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_huffman_cost_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
