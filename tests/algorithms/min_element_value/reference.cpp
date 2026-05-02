#include "reference.hpp"

#include <algorithm>
#include <string>

int
test_min_element_value_cpp(const std::vector<int>& values) {
  const auto it = std::min_element(values.begin(), values.end());
  if (it == values.end()) {
    return -1;
  }
  return *it;
}

std::string
test_min_element_value_cpp_output(const std::vector<int>& values) {
  return std::to_string(test_min_element_value_cpp(values)) + "\n";
}
