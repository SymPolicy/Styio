#include "reference.hpp"

#include <algorithm>
#include <string>

int
test_is_sorted_flag_cpp(const std::vector<int>& values) {
  return std::is_sorted(values.begin(), values.end()) ? 1 : 0;
}

std::string
test_is_sorted_flag_cpp_output(const std::vector<int>& values) {
  return std::to_string(test_is_sorted_flag_cpp(values)) + "\n";
}
