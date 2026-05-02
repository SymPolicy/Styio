#include "reference.hpp"

#include <numeric>
#include <string>

int
test_inner_product_cpp(
  const std::vector<int>& lhs,
  const std::vector<int>& rhs) {
  return std::inner_product(lhs.begin(), lhs.end(), rhs.begin(), 0);
}

std::string
test_inner_product_cpp_output(
  const std::vector<int>& lhs,
  const std::vector<int>& rhs) {
  return std::to_string(test_inner_product_cpp(lhs, rhs)) + "\n";
}
