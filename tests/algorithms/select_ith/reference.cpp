#include "reference.hpp"

#include <algorithm>
#include <string>
#include <vector>

int
test_select_ith_cpp(std::vector<int> values, int i) {
  if (values.empty() || i < 0 || i >= static_cast<int>(values.size())) {
    return -1;
  }
  // Textbook randomized-select equivalent via nth_element (partition-based).
  std::nth_element(values.begin(), values.begin() + i, values.end());
  return values[static_cast<std::size_t>(i)];
}

std::string
test_select_ith_cpp_output(const std::vector<int>& values, int i) {
  return std::to_string(test_select_ith_cpp(values, i)) + "\n";
}
