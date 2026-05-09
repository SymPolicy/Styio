#include "reference.hpp"

#include <algorithm>
#include <string>

int
test_count_value_cpp(const std::vector<int>& values, int target) {
  return static_cast<int>(std::count(values.begin(), values.end(), target));
}

std::string
test_count_value_cpp_output(const std::vector<int>& values, int target) {
  return std::to_string(test_count_value_cpp(values, target)) + "\n";
}
