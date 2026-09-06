#include "reference.hpp"

#include <algorithm>
#include <string>
#include <vector>

int
test_manacher_palindrome_length_cpp(const std::vector<int>& a) {
  const int n = static_cast<int>(a.size());
  if (n == 0) {
    return 0;
  }
  // Transform with sentinels: ^ # a0 # a1 # ... # $
  // Use odd-length Manacher on expanded array of size 2n+1 with separators -2.
  const int sep = -2;
  std::vector<int> t;
  t.reserve(static_cast<std::size_t>(2 * n + 3));
  t.push_back(-3); // left guard
  for (int x : a) {
    t.push_back(sep);
    t.push_back(x);
  }
  t.push_back(sep);
  t.push_back(-4); // right guard
  const int m = static_cast<int>(t.size());
  std::vector<int> p(static_cast<std::size_t>(m), 0);
  int c = 0;
  int r = 0;
  int best = 0;
  for (int i = 1; i < m - 1; ++i) {
    const int mirror = 2 * c - i;
    if (i < r) {
      p[static_cast<std::size_t>(i)] =
        std::min(r - i, p[static_cast<std::size_t>(mirror)]);
    }
    while (t[static_cast<std::size_t>(i + 1 + p[static_cast<std::size_t>(i)])] ==
           t[static_cast<std::size_t>(i - 1 - p[static_cast<std::size_t>(i)])]) {
      ++p[static_cast<std::size_t>(i)];
    }
    if (i + p[static_cast<std::size_t>(i)] > r) {
      c = i;
      r = i + p[static_cast<std::size_t>(i)];
    }
    best = std::max(best, p[static_cast<std::size_t>(i)]);
  }
  return best;
}

std::string
test_manacher_palindrome_length_cpp_output(const std::vector<int>& a) {
  return std::to_string(test_manacher_palindrome_length_cpp(a)) + "\n";
}
