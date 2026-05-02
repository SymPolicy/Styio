#include "reference.hpp"

#include <numeric>
#include <string>

int
test_accumulate_sum_cpp(const std::vector<int>& values) {
  return std::accumulate(values.begin(), values.end(), 0);
}

std::string
test_accumulate_sum_cpp_output(const std::vector<int>& values) {
  return std::to_string(test_accumulate_sum_cpp(values)) + "\n";
}
