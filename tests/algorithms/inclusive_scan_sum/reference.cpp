#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <numeric>
#include <string>

std::vector<int>
test_inclusive_scan_sum_cpp(std::vector<int> values) {
  std::vector<int> out(values.size());
  std::inclusive_scan(values.begin(), values.end(), out.begin());
  return out;
}

std::string
test_inclusive_scan_sum_cpp_output(const std::vector<int>& values) {
  return styio::testing::algorithms::format_i32_list(
    test_inclusive_scan_sum_cpp(values)) + "\n";
}
