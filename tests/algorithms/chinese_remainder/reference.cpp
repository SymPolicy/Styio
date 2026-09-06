#include "reference.hpp"

#include <cstdlib>
#include <string>

namespace {

int
egcd(int aa, int bb, int& x, int& y) {
  if (bb == 0) {
    x = 1;
    y = 0;
    return aa;
  }
  int x1 = 0;
  int y1 = 0;
  const int g = egcd(bb, aa % bb, x1, y1);
  x = y1;
  y = x1 - (aa / bb) * y1;
  return g;
}

} // namespace

int
test_chinese_remainder_cpp(int a, int m, int b, int n) {
  if (m <= 0 || n <= 0) {
    return -1;
  }
  // Normalize residues into [0, mod)
  a %= m;
  if (a < 0) {
    a += m;
  }
  b %= n;
  if (b < 0) {
    b += n;
  }
  int x = 0;
  int y = 0;
  const int g = egcd(m, n, x, y);
  const int diff = b - a;
  if (diff % g != 0) {
    return -1;
  }
  // x0 = a + m * ((diff/g) * (m/g)^{-1} mod (n/g))
  const int m1 = m / g;
  const int n1 = n / g;
  int inv = 0;
  int tmp = 0;
  egcd(m1, n1, inv, tmp);
  inv %= n1;
  if (inv < 0) {
    inv += n1;
  }
  long long k = (static_cast<long long>(diff / g) % n1) * inv % n1;
  if (k < 0) {
    k += n1;
  }
  long long ans = a + m * k;
  const long long lcm = static_cast<long long>(m1) * n;
  ans %= lcm;
  if (ans < 0) {
    ans += lcm;
  }
  return static_cast<int>(ans);
}

std::string
test_chinese_remainder_cpp_output(int a, int m, int b, int n) {
  return std::to_string(test_chinese_remainder_cpp(a, m, b, n)) + "\n";
}
