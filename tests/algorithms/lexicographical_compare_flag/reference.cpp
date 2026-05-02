#include "reference.hpp"

#include <algorithm>
#include <string>

int
test_lexicographical_compare_flag_cpp(
  const std::vector<int>& lhs,
  const std::vector<int>& rhs) {
  return std::lexicographical_compare(
    lhs.begin(), lhs.end(), rhs.begin(), rhs.end()) ? 1 : 0;
}

std::string
test_lexicographical_compare_flag_cpp_output(
  const std::vector<int>& lhs,
  const std::vector<int>& rhs) {
  return std::to_string(test_lexicographical_compare_flag_cpp(lhs, rhs)) + "\n";
}
