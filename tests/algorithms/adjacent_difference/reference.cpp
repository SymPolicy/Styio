#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <numeric>
#include <string>

std::vector<int>
test_adjacent_difference_cpp(std::vector<int> values) {
  std::vector<int> out(values.size());
  std::adjacent_difference(values.begin(), values.end(), out.begin());
  return out;
}

std::string
test_adjacent_difference_cpp_output(const std::vector<int>& values) {
  return styio::testing::algorithms::format_i32_list(
    test_adjacent_difference_cpp(values)) + "\n";
}
