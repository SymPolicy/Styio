#include "reference.hpp"

#include <algorithm>
#include <string>

int
test_mismatch_index_cpp(
  const std::vector<int>& lhs,
  const std::vector<int>& rhs) {
  const auto [lhs_it, rhs_it] =
    std::mismatch(lhs.begin(), lhs.end(), rhs.begin(), rhs.end());
  if (lhs_it == lhs.end() && rhs_it == rhs.end()) {
    return -1;
  }
  return static_cast<int>(lhs_it - lhs.begin());
}

std::string
test_mismatch_index_cpp_output(
  const std::vector<int>& lhs,
  const std::vector<int>& rhs) {
  return std::to_string(test_mismatch_index_cpp(lhs, rhs)) + "\n";
}
