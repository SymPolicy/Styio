#include "reference.hpp"

#include <functional>
#include <numeric>
#include <string>

int
test_transform_reduce_square_sum_cpp(const std::vector<int>& values) {
  return std::transform_reduce(
    values.begin(),
    values.end(),
    0,
    std::plus<>(),
    [](int value) { return value * value; });
}

std::string
test_transform_reduce_square_sum_cpp_output(const std::vector<int>& values) {
  return std::to_string(test_transform_reduce_square_sum_cpp(values)) + "\n";
}
