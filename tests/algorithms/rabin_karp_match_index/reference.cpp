#include "reference.hpp"

#include <string>
#include <vector>

int
test_rabin_karp_match_index_cpp(const std::vector<int>& text,
                                const std::vector<int>& pattern) {
  const int n = static_cast<int>(text.size());
  const int m = static_cast<int>(pattern.size());
  if (m == 0) {
    return 0;
  }
  if (m > n) {
    return -1;
  }
  constexpr int base = 256;
  constexpr int mod = 101;
  long long h = 1;
  for (int i = 0; i < m - 1; ++i) {
    h = (h * base) % mod;
  }
  long long p = 0;
  long long t = 0;
  for (int i = 0; i < m; ++i) {
    // Map possibly-negative alphabet values into a non-neg residue first.
    const int pv = pattern[static_cast<std::size_t>(i)] % mod;
    const int tv = text[static_cast<std::size_t>(i)] % mod;
    p = (base * p + (pv < 0 ? pv + mod : pv)) % mod;
    t = (base * t + (tv < 0 ? tv + mod : tv)) % mod;
  }
  for (int s = 0; s <= n - m; ++s) {
    if (p == t) {
      bool ok = true;
      for (int j = 0; j < m; ++j) {
        if (text[static_cast<std::size_t>(s + j)] !=
            pattern[static_cast<std::size_t>(j)]) {
          ok = false;
          break;
        }
      }
      if (ok) {
        return s;
      }
    }
    if (s < n - m) {
      int outv = text[static_cast<std::size_t>(s)] % mod;
      if (outv < 0) {
        outv += mod;
      }
      int inv = text[static_cast<std::size_t>(s + m)] % mod;
      if (inv < 0) {
        inv += mod;
      }
      t = (base * (t - outv * h) + inv) % mod;
      if (t < 0) {
        t += mod;
      }
    }
  }
  return -1;
}

std::string
test_rabin_karp_match_index_cpp_output(const std::vector<int>& text,
                                       const std::vector<int>& pattern) {
  return std::to_string(test_rabin_karp_match_index_cpp(text, pattern)) + "\n";
}
