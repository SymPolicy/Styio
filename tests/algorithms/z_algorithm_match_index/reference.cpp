#include <algorithm>
#include "reference.hpp"

#include <string>
#include <vector>

int
test_z_algorithm_match_index_cpp(const std::vector<int>& text, const std::vector<int>& pattern) {
  const int m = static_cast<int>(pattern.size());
  const int n = static_cast<int>(text.size());
  if (m == 0) {
    return 0;
  }
  if (m > n) {
    return -1;
  }
  // concat = pattern + sentinel + text; sentinel = 1000000007 unused in alphabet tests
  const int sentinel = 1000000007;
  std::vector<int> s;
  s.reserve(static_cast<std::size_t>(m + 1 + n));
  s.insert(s.end(), pattern.begin(), pattern.end());
  s.push_back(sentinel);
  s.insert(s.end(), text.begin(), text.end());
  const int len = static_cast<int>(s.size());
  std::vector<int> z(static_cast<std::size_t>(len), 0);
  int l = 0;
  int r = 0;
  for (int i = 1; i < len; ++i) {
    if (i < r) {
      z[static_cast<std::size_t>(i)] =
        std::min(r - i, z[static_cast<std::size_t>(i - l)]);
    }
    while (i + z[static_cast<std::size_t>(i)] < len &&
           s[static_cast<std::size_t>(z[static_cast<std::size_t>(i)])] ==
             s[static_cast<std::size_t>(i + z[static_cast<std::size_t>(i)])]) {
      ++z[static_cast<std::size_t>(i)];
    }
    if (i + z[static_cast<std::size_t>(i)] > r) {
      l = i;
      r = i + z[static_cast<std::size_t>(i)];
    }
  }
  for (int i = m + 1; i < len; ++i) {
    if (z[static_cast<std::size_t>(i)] == m) {
      return i - (m + 1);
    }
  }
  return -1;
}

std::string
test_z_algorithm_match_index_cpp_output(
  const std::vector<int>& text, const std::vector<int>& pattern) {
  return std::to_string(test_z_algorithm_match_index_cpp(text, pattern)) + "\n";
}
