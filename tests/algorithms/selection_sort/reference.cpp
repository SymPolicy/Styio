#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <algorithm>
#include <string>

std::vector<int>
test_selection_sort_cpp(std::vector<int> values) {
  for (std::size_t i = 0; i < values.size(); ++i) {
    std::size_t min_index = i;
    for (std::size_t j = i + 1; j < values.size(); ++j) {
      if (values[j] < values[min_index]) {
        min_index = j;
      }
    }
    if (min_index != i) {
      std::swap(values[i], values[min_index]);
    }
  }
  return values;
}

std::string
test_selection_sort_cpp_output(const std::vector<int>& values) {
  return styio::testing::algorithms::format_i32_list(
    test_selection_sort_cpp(values)) + "\n";
}
