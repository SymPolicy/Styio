#include "reference.hpp"

#include <string>
#include <vector>

namespace {

std::vector<int>
prefix_function(const std::vector<int>& pattern) {
  const int m = static_cast<int>(pattern.size());
  std::vector<int> pi(static_cast<std::size_t>(m), 0);
  int k = 0;
  for (int q = 1; q < m; ++q) {
    while (k > 0 && pattern[static_cast<std::size_t>(k)] !=
                      pattern[static_cast<std::size_t>(q)]) {
      k = pi[static_cast<std::size_t>(k - 1)];
    }
    if (pattern[static_cast<std::size_t>(k)] ==
        pattern[static_cast<std::size_t>(q)]) {
      ++k;
    }
    pi[static_cast<std::size_t>(q)] = k;
  }
  return pi;
}

} // namespace

int
test_kmp_match_index_cpp(const std::vector<int>& text,
                         const std::vector<int>& pattern) {
  const int n = static_cast<int>(text.size());
  const int m = static_cast<int>(pattern.size());
  if (m == 0) {
    return 0;
  }
  if (m > n) {
    return -1;
  }
  const std::vector<int> pi = prefix_function(pattern);
  int q = 0;
  for (int i = 0; i < n; ++i) {
    while (q > 0 && pattern[static_cast<std::size_t>(q)] !=
                      text[static_cast<std::size_t>(i)]) {
      q = pi[static_cast<std::size_t>(q - 1)];
    }
    if (pattern[static_cast<std::size_t>(q)] ==
        text[static_cast<std::size_t>(i)]) {
      ++q;
    }
    if (q == m) {
      return i - m + 1;
    }
  }
  return -1;
}

std::string
test_kmp_match_index_cpp_output(const std::vector<int>& text,
                                const std::vector<int>& pattern) {
  return std::to_string(test_kmp_match_index_cpp(text, pattern)) + "\n";
}
