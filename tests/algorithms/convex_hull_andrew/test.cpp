#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace {

struct HullInput {
  std::vector<std::pair<int, int>> pts;
};

HullInput
test_convex_hull_andrew_random_input(std::mt19937& rng) {
  std::uniform_int_distribution<int> n_dist(0, 12);
  std::uniform_int_distribution<int> c_dist(-8, 8);
  HullInput input;
  const int n = n_dist(rng);
  input.pts.resize(static_cast<std::size_t>(n));
  for (auto& p : input.pts) {
    p = {c_dist(rng), c_dist(rng)};
  }
  return input;
}

std::filesystem::path
test_convex_hull_andrew_styio() {
  return styio::testing::algorithms::styio_program(
    "convex_hull_andrew", "convex_hull_andrew.styio");
}

std::string
format_convex_hull_andrew_input(const HullInput& input) {
  const int n = static_cast<int>(input.pts.size());
  std::vector<int> encoded;
  encoded.push_back(n);
  for (const auto& [x, y] : input.pts) {
    encoded.push_back(x);
    encoded.push_back(y);
  }
  for (int i = 0; i < n * 3; ++i) {
    encoded.push_back(0);
  }
  return styio::testing::algorithms::format_i32_list(encoded) + "\n";
}

// Canonicalize hull rotation so Andrew vs Jarvis start points match.
std::string
canonicalize_hull_output(const std::string& raw) {
  std::vector<std::pair<int, int>> pts;
  {
    std::size_t pos = 0;
    while (pos < raw.size()) {
      const std::size_t nl1 = raw.find('\n', pos);
      if (nl1 == std::string::npos) {
        break;
      }
      const int x = std::stoi(raw.substr(pos, nl1 - pos));
      pos = nl1 + 1;
      const std::size_t nl2 = raw.find('\n', pos);
      if (nl2 == std::string::npos) {
        break;
      }
      const int y = std::stoi(raw.substr(pos, nl2 - pos));
      pos = nl2 + 1;
      pts.push_back({x, y});
    }
  }
  if (pts.empty()) {
    return "";
  }
  std::size_t start = 0;
  for (std::size_t i = 1; i < pts.size(); ++i) {
    if (pts[i] < pts[start]) {
      start = i;
    }
  }
  std::rotate(pts.begin(), pts.begin() + static_cast<std::ptrdiff_t>(start), pts.end());
  // Prefer CCW: if clockwise, reverse the tail.
  if (pts.size() >= 3) {
    long long area2 = 0;
    for (std::size_t i = 0; i < pts.size(); ++i) {
      const auto& a = pts[i];
      const auto& b = pts[(i + 1) % pts.size()];
      area2 += 1LL * a.first * b.second - 1LL * a.second * b.first;
    }
    if (area2 < 0) {
      std::reverse(pts.begin() + 1, pts.end());
    }
  }
  std::string out;
  for (const auto& [x, y] : pts) {
    out += std::to_string(x);
    out += "\n";
    out += std::to_string(y);
    out += "\n";
  }
  return out;
}

} // namespace

TEST(StyioCppReferenceEquivalence, test_convex_hull_andrew) {
  std::mt19937 rng(0xC0A1);
  for (int iteration = 0; iteration < 100; ++iteration) {
    const HullInput input = test_convex_hull_andrew_random_input(rng);
    const std::string expected = canonicalize_hull_output(
      test_convex_hull_andrew_cpp_output(input.pts));
    const auto actual = styio::testing::algorithms::run_styio_program(
      test_convex_hull_andrew_styio(), format_convex_hull_andrew_input(input));
    ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
    EXPECT_EQ(canonicalize_hull_output(actual.stdout_text), expected)
      << format_convex_hull_andrew_input(input);
  }
}

TEST(StyioCppReferenceEquivalence, test_convex_hull_andrew_fixed_cases) {
  // square (0,0),(0,1),(1,0),(1,1) -> 4 hull verts
  const std::string classic = "[4,0,0,0,1,1,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0]\n";
  const auto actual = styio::testing::algorithms::run_styio_program(
    test_convex_hull_andrew_styio(), classic);
  ASSERT_EQ(actual.exit_code, 0) << actual.stderr_text;
  EXPECT_EQ(canonicalize_hull_output(actual.stdout_text),
            canonicalize_hull_output(test_convex_hull_andrew_cpp_output(
              {{0, 0}, {0, 1}, {1, 0}, {1, 1}})));
  const auto empty = styio::testing::algorithms::run_styio_program(
    test_convex_hull_andrew_styio(), "[]\n");
  ASSERT_EQ(empty.exit_code, 0);
  EXPECT_EQ(empty.stdout_text, "");
}
