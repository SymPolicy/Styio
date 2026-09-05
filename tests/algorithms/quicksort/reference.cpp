#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <utility>
#include <vector>

namespace {

std::size_t
partition_lomuto(std::vector<int>& values, std::size_t lo, std::size_t hi) {
  const int pivot = values[hi];
  std::size_t i = lo;
  for (std::size_t j = lo; j < hi; ++j) {
    if (values[j] <= pivot) {
      std::swap(values[i], values[j]);
      ++i;
    }
  }
  std::swap(values[i], values[hi]);
  return i;
}

void
quicksort_range(std::vector<int>& values, std::size_t lo, std::size_t hi) {
  if (lo >= hi || values.empty()) {
    return;
  }
  std::vector<std::pair<std::size_t, std::size_t>> stack;
  stack.emplace_back(lo, hi);
  while (!stack.empty()) {
    const auto [left, right] = stack.back();
    stack.pop_back();
    if (left >= right) {
      continue;
    }
    const std::size_t p = partition_lomuto(values, left, right);
    if (p > 0) {
      stack.emplace_back(left, p - 1);
    }
    stack.emplace_back(p + 1, right);
  }
}

} // namespace

std::vector<int>
test_quicksort_cpp(std::vector<int> values) {
  if (values.size() <= 1) {
    return values;
  }
  quicksort_range(values, 0, values.size() - 1);
  return values;
}

std::string
test_quicksort_cpp_output(const std::vector<int>& values) {
  return styio::testing::algorithms::format_i32_list(test_quicksort_cpp(values)) + "\n";
}
