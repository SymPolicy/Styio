#include "reference.hpp"

#include <string>
#include <vector>

std::vector<int>
test_counting_sort_cpp(const std::vector<int>& values) {
  if (values.empty()) {
    return {};
  }
  int k = 0;
  for (int v : values) {
    if (v < 0) {
      return {};
    }
    if (v > k) {
      k = v;
    }
  }
  std::vector<int> count(static_cast<std::size_t>(k) + 1, 0);
  for (int v : values) {
    ++count[static_cast<std::size_t>(v)];
  }
  for (int i = 1; i <= k; ++i) {
    count[static_cast<std::size_t>(i)] += count[static_cast<std::size_t>(i - 1)];
  }
  std::vector<int> out(values.size(), 0);
  for (int i = static_cast<int>(values.size()) - 1; i >= 0; --i) {
    const int v = values[static_cast<std::size_t>(i)];
    const int pos = count[static_cast<std::size_t>(v)] - 1;
    out[static_cast<std::size_t>(pos)] = v;
    --count[static_cast<std::size_t>(v)];
  }
  return out;
}

std::string
test_counting_sort_cpp_output(const std::vector<int>& values) {
  const std::vector<int> sorted = test_counting_sort_cpp(values);
  std::string out = "[";
  for (std::size_t i = 0; i < sorted.size(); ++i) {
    if (i != 0) {
      out += ',';
    }
    out += std::to_string(sorted[i]);
  }
  out += "]\n";
  return out;
}
