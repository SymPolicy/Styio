#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct EditDistanceInput {
  std::vector<int> a;
  std::vector<int> b;
};

EditDistanceInput
test_edit_distance_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> len_dist(0, 10);
  std::uniform_int_distribution<int> val_dist(0, 5);
  EditDistanceInput input;
  input.a.resize(static_cast<std::size_t>(len_dist(rng)));
  input.b.resize(static_cast<std::size_t>(len_dist(rng)));
  for (int& v : input.a) {
    v = val_dist(rng);
  }
  for (int& v : input.b) {
    v = val_dist(rng);
  }
  return input;
}

std::filesystem::path
test_edit_distance_styio() {
  return styio::testing::algorithms::styio_program("edit_distance", "edit_distance.styio");
}

std::string
format_edit_distance_input(const EditDistanceInput& input) {
  const int n = static_cast<int>(input.a.size());
  const int m = static_cast<int>(input.b.size());
  std::vector<int> encoded;
  encoded.reserve(static_cast<std::size_t>(2 + n + m + (n + 1) * (m + 1)));
  encoded.push_back(n);
  encoded.push_back(m);
  encoded.insert(encoded.end(), input.a.begin(), input.a.end());
  encoded.insert(encoded.end(), input.b.begin(), input.b.end());
  for (int i = 0; i < (n + 1) * (m + 1); ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_edit_distance) {
  std::mt19937 rng(0xED17D1);

  for (int iteration = 0; iteration < 120; ++iteration) {
    const EditDistanceInput input = test_edit_distance_random_input(rng);
    const std::string expected = test_edit_distance_cpp_output(input.a, input.b);

    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(
        test_edit_distance_styio(), format_edit_distance_input(input));

    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << format_edit_distance_input(input)
      << "stderr=" << actual.stderr_text;
  }
}

TEST(StyioCppReferenceEquivalence, test_edit_distance_fixed_cases) {
  // kitten/sitting style on ints: [1,2,3] vs [1,4,3,5] ; n=3,m=4; workspace 4*5=20
  std::string enc = "[3,4,1,2,3,1,4,3,5";
  for (int i = 0; i < 20; ++i) {
    enc += ",0";
  }
  enc += "]\n";

  const std::vector<std::pair<std::string, std::string>> cases = {
    { "[0,0,0]\n", "0\n" },
    { enc, "2\n" },
    { "[]\n", "0\n" },
  };

  for (const auto& [stdin_text, expected] : cases) {
    const styio::testing::algorithms::CommandResult actual =
      styio::testing::algorithms::run_styio_program(test_edit_distance_styio(), stdin_text);

    ASSERT_EQ(actual.exit_code, 0)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
    EXPECT_EQ(actual.stdout_text, expected)
      << "input=" << stdin_text << "stderr=" << actual.stderr_text;
  }
}
