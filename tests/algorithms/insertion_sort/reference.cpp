#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

std::vector<int>
test_insertion_sort_cpp(std::vector<int> values) {
  for (std::size_t i = 1; i < values.size(); ++i) {
    const int key = values[i];
    std::size_t j = i;
    while (j > 0 && values[j - 1] > key) {
      values[j] = values[j - 1];
      --j;
    }
    values[j] = key;
  }
  return values;
}

std::string
test_insertion_sort_cpp_output(const std::vector<int>& values) {
  return styio::testing::algorithms::format_i32_list(test_insertion_sort_cpp(values)) + "\n";
}
