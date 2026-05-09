#include "reference.hpp"

#include <algorithm>
#include <string>

int
test_linear_search_cpp(const std::vector<int>& values, int target) {
  const auto it = std::find(values.begin(), values.end(), target);
  if (it == values.end()) {
    return -1;
  }
  return static_cast<int>(it - values.begin());
}

std::string
test_linear_search_cpp_output(const std::vector<int>& values, int target) {
  return std::to_string(test_linear_search_cpp(values, target)) + "\n";
}
