#include "reference.hpp"

#include <algorithm>
#include <string>

int
test_binary_search_cpp(const std::vector<int>& values, int target) {
  const auto it = std::lower_bound(values.begin(), values.end(), target);
  if (it == values.end() || *it != target) {
    return -1;
  }
  return static_cast<int>(it - values.begin());
}

std::string
test_binary_search_cpp_output(const std::vector<int>& values, int target) {
  return std::to_string(test_binary_search_cpp(values, target)) + "\n";
}
