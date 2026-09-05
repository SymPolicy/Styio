#include "reference.hpp"

#include "tests/algorithms/.common/CxxReferenceEquivalence.hpp"

#include <utility>

namespace {

void
sift_down(std::vector<int>& values, std::size_t root, std::size_t heap_n) {
  while (true) {
    std::size_t child = root * 2 + 1;
    if (child >= heap_n) {
      break;
    }
    if (child + 1 < heap_n && values[child + 1] > values[child]) {
      ++child;
    }
    if (values[root] >= values[child]) {
      break;
    }
    std::swap(values[root], values[child]);
    root = child;
  }
}

} // namespace

std::vector<int>
test_heap_sort_cpp(std::vector<int> values) {
  if (values.size() <= 1) {
    return values;
  }
  for (std::size_t start = values.size() / 2; start > 0;) {
    --start;
    sift_down(values, start, values.size());
    if (start == 0) {
      break;
    }
  }
  for (std::size_t end = values.size(); end > 1;) {
    --end;
    std::swap(values[0], values[end]);
    sift_down(values, 0, end);
  }
  return values;
}

std::string
test_heap_sort_cpp_output(const std::vector<int>& values) {
  return styio::testing::algorithms::format_i32_list(test_heap_sort_cpp(values)) + "\n";
}
