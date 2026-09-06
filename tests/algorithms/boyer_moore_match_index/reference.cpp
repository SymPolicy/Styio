#include "reference.hpp"

#include <string>
#include <unordered_map>
#include <vector>

int
test_boyer_moore_match_index_cpp(
  const std::vector<int>& text, const std::vector<int>& pattern) {
  const int n = static_cast<int>(text.size());
  const int m = static_cast<int>(pattern.size());
  if (m == 0) {
    return 0;
  }
  if (m > n) {
    return -1;
  }
  std::unordered_map<int, int> shift;
  for (int i = 0; i < m - 1; ++i) {
    shift[pattern[static_cast<std::size_t>(i)]] = m - 1 - i;
  }
  int i = 0;
  while (i <= n - m) {
    int j = m - 1;
    while (j >= 0 &&
           text[static_cast<std::size_t>(i + j)] ==
             pattern[static_cast<std::size_t>(j)]) {
      --j;
    }
    if (j < 0) {
      return i;
    }
    const int c = text[static_cast<std::size_t>(i + m - 1)];
    const auto it = shift.find(c);
    const int s = (it == shift.end()) ? m : it->second;
    i += s;
  }
  return -1;
}

std::string
test_boyer_moore_match_index_cpp_output(
  const std::vector<int>& text, const std::vector<int>& pattern) {
  return std::to_string(test_boyer_moore_match_index_cpp(text, pattern)) + "\n";
}
