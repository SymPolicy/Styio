#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <numeric>
#include <string>

std::vector<int>
test_prefix_sum_cpp(std::vector<int> values) {
  std::partial_sum(values.begin(), values.end(), values.begin());
  return values;
}

std::string
test_prefix_sum_cpp_output(const std::vector<int>& values) {
  return styio::testing::algorithms::format_i32_list(
    test_prefix_sum_cpp(values)) + "\n";
}
