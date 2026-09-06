#include "reference.hpp"

#include <algorithm>
#include <numeric>
#include <string>
#include <vector>

int
test_suffix_array_lcp_cpp(const std::vector<int>& s, int i, int j) {
  const int n = static_cast<int>(s.size());
  if (n <= 0 || i < 0 || j < 0 || i >= n || j >= n) {
    return -1;
  }
  if (i == j) {
    return n - i;
  }
  std::vector<int> sa(static_cast<std::size_t>(n));
  std::iota(sa.begin(), sa.end(), 0);
  std::sort(sa.begin(), sa.end(), [&](int a, int b) {
    for (int k = 0; a + k < n && b + k < n; ++k) {
      if (s[static_cast<std::size_t>(a + k)] != s[static_cast<std::size_t>(b + k)]) {
        return s[static_cast<std::size_t>(a + k)] < s[static_cast<std::size_t>(b + k)];
      }
    }
    return a > b; // longer suffix first when prefix-equal? standard: shorter ends first
  });
  // Fix comparator for proper suffix order: when one is prefix of other, shorter < longer
  std::sort(sa.begin(), sa.end(), [&](int a, int b) {
    int ka = 0;
    int kb = 0;
    while (a + ka < n && b + kb < n) {
      if (s[static_cast<std::size_t>(a + ka)] != s[static_cast<std::size_t>(b + kb)]) {
        return s[static_cast<std::size_t>(a + ka)] < s[static_cast<std::size_t>(b + kb)];
      }
      ++ka;
      ++kb;
    }
    return (n - a) < (n - b);
  });
  std::vector<int> rank(static_cast<std::size_t>(n));
  for (int r = 0; r < n; ++r) {
    rank[static_cast<std::size_t>(sa[static_cast<std::size_t>(r)])] = r;
  }
  // Kasai LCP between adjacent SA entries
  std::vector<int> lcp(static_cast<std::size_t>(n), 0);
  int h = 0;
  for (int x = 0; x < n; ++x) {
    const int r = rank[static_cast<std::size_t>(x)];
    if (r == 0) {
      continue;
    }
    const int y = sa[static_cast<std::size_t>(r - 1)];
    while (x + h < n && y + h < n &&
           s[static_cast<std::size_t>(x + h)] == s[static_cast<std::size_t>(y + h)]) {
      ++h;
    }
    lcp[static_cast<std::size_t>(r)] = h;
    if (h > 0) {
      --h;
    }
  }
  int ri = rank[static_cast<std::size_t>(i)];
  int rj = rank[static_cast<std::size_t>(j)];
  if (ri > rj) {
    std::swap(ri, rj);
  }
  int ans = lcp[static_cast<std::size_t>(ri + 1)];
  for (int t = ri + 2; t <= rj; ++t) {
    ans = std::min(ans, lcp[static_cast<std::size_t>(t)]);
  }
  return ans;
}

std::string
test_suffix_array_lcp_cpp_output(const std::vector<int>& s, int i, int j) {
  return std::to_string(test_suffix_array_lcp_cpp(s, i, j)) + "\n";
}
