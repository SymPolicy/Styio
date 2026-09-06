#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <algorithm>
#include <vector>

namespace {

void
merge_range(std::vector<int>& values, std::vector<int>& tmp, std::size_t left, std::size_t mid, std::size_t right) {
  std::size_t i = left;
  std::size_t j = mid;
  std::size_t k = left;
  while (i < mid && j < right) {
    if (values[i] <= values[j]) {
      tmp[k++] = values[i++];
    } else {
      tmp[k++] = values[j++];
    }
  }
  while (i < mid) {
    tmp[k++] = values[i++];
  }
  while (j < right) {
    tmp[k++] = values[j++];
  }
  for (std::size_t idx = left; idx < right; ++idx) {
    values[idx] = tmp[idx];
  }
}

} // namespace

std::vector<int>
test_merge_sort_cpp(std::vector<int> values) {
  if (values.size() <= 1) {
    return values;
  }
  std::vector<int> tmp = values;
  for (std::size_t width = 1; width < values.size(); width *= 2) {
    for (std::size_t left = 0; left < values.size(); left += width * 2) {
      const std::size_t mid = std::min(left + width, values.size());
      const std::size_t right = std::min(left + width * 2, values.size());
      merge_range(values, tmp, left, mid, right);
    }
  }
  return values;
}

std::string
test_merge_sort_cpp_output(const std::vector<int>& values) {
  return styio::testing::algorithms::format_i32_list(test_merge_sort_cpp(values)) + "\n";
}
