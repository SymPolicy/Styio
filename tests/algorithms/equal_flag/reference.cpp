#include "reference.hpp"

#include <algorithm>
#include <string>

int
test_equal_flag_cpp(
  const std::vector<int>& lhs,
  const std::vector<int>& rhs) {
  return std::equal(lhs.begin(), lhs.end(), rhs.begin(), rhs.end()) ? 1 : 0;
}

std::string
test_equal_flag_cpp_output(
  const std::vector<int>& lhs,
  const std::vector<int>& rhs) {
  return std::to_string(test_equal_flag_cpp(lhs, rhs)) + "\n";
}
