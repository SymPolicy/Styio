#include "reference.hpp"

#include <algorithm>
#include <string>

int
test_upper_bound_index_cpp(const std::vector<int>& values, int target) {
  const auto it = std::upper_bound(values.begin(), values.end(), target);
  return static_cast<int>(it - values.begin());
}

std::string
test_upper_bound_index_cpp_output(
  const std::vector<int>& values,
  int target) {
  return std::to_string(test_upper_bound_index_cpp(values, target)) + "\n";
}
