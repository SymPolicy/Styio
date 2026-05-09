#include "reference.hpp"

#include <algorithm>
#include <string>

int
test_subrange_search_index_cpp(
  const std::vector<int>& haystack,
  const std::vector<int>& needle) {
  const auto it =
    std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end());
  if (it == haystack.end() && !needle.empty()) {
    return -1;
  }
  return static_cast<int>(it - haystack.begin());
}

std::string
test_subrange_search_index_cpp_output(
  const std::vector<int>& haystack,
  const std::vector<int>& needle) {
  return std::to_string(test_subrange_search_index_cpp(haystack, needle)) + "\n";
}
