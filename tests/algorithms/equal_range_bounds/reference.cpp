#include "reference.hpp"

#include <algorithm>
#include <string>
#include <utility>

std::pair<int, int>
test_equal_range_bounds_cpp(const std::vector<int>& values, int target) {
  const auto [lower, upper] =
    std::equal_range(values.begin(), values.end(), target);
  return {
    static_cast<int>(lower - values.begin()),
    static_cast<int>(upper - values.begin()),
  };
}

std::string
test_equal_range_bounds_cpp_output(
  const std::vector<int>& values,
  int target) {
  const auto [lower, upper] = test_equal_range_bounds_cpp(values, target);
  return std::to_string(lower) + "\n" + std::to_string(upper) + "\n";
}
