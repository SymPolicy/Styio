#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <utility>

std::vector<int>
test_bubble_sort_cpp(std::vector<int> values) {
  for (std::size_t i = 0; i < values.size(); ++i) {
    for (std::size_t j = 0; j + 1 < values.size() - i; ++j) {
      if (values[j] > values[j + 1]) {
        std::swap(values[j], values[j + 1]);
      }
    }
  }
  return values;
}

std::string
test_bubble_sort_cpp_output(const std::vector<int>& values) {
  return styio::testing::algorithms::format_i32_list(test_bubble_sort_cpp(values)) + "\n";
}
