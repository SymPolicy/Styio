#include "reference.hpp"

#include <algorithm>
#include <string>

int
test_adjacent_equal_index_cpp(const std::vector<int>& values) {
  const auto it = std::adjacent_find(values.begin(), values.end());
  if (it == values.end()) {
    return -1;
  }
  return static_cast<int>(it - values.begin());
}

std::string
test_adjacent_equal_index_cpp_output(const std::vector<int>& values) {
  return std::to_string(test_adjacent_equal_index_cpp(values)) + "\n";
}
