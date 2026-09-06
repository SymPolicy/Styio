#include "reference.hpp"

#include <cstdlib>
#include <string>
#include <tuple>

std::tuple<int, int, int>
test_extended_gcd_cpp(int a, int b) {
  int aa = std::abs(a);
  int bb = std::abs(b);
  int old_r = aa;
  int r = bb;
  int old_s = 1;
  int s = 0;
  int old_t = 0;
  int t = 1;
  while (r != 0) {
    const int q = old_r / r;
    const int next_r = old_r - q * r;
    old_r = r;
    r = next_r;
    const int next_s = old_s - q * s;
    old_s = s;
    s = next_s;
    const int next_t = old_t - q * t;
    old_t = t;
    t = next_t;
  }
  // old_r = gcd, old_s*aa + old_t*bb = gcd
  // Map back if original a/b were negative.
  if (a < 0) {
    old_s = -old_s;
  }
  if (b < 0) {
    old_t = -old_t;
  }
  return {old_r, old_s, old_t};
}

std::string
test_extended_gcd_cpp_output(int a, int b) {
  const auto [g, x, y] = test_extended_gcd_cpp(a, b);
  return std::to_string(g) + "\n" + std::to_string(x) + "\n" + std::to_string(y) + "\n";
}
