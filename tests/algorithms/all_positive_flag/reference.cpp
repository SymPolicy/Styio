#include "reference.hpp"

#include <algorithm>
#include <string>

int
test_all_positive_flag_cpp(const std::vector<int>& values) {
  return std::all_of(values.begin(), values.end(), [](int value) {
    return value > 0;
  }) ? 1 : 0;
}

std::string
test_all_positive_flag_cpp_output(const std::vector<int>& values) {
  return std::to_string(test_all_positive_flag_cpp(values)) + "\n";
}
