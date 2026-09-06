#include "reference.hpp"

#include <cstdlib>
#include <string>

int
test_modular_inverse_cpp(int a, int m) {
  if (m <= 0) {
    return -1;
  }
  int aa = std::abs(a);
  int old_r = aa;
  int r = m;
  int old_s = 1;
  int s = 0;
  while (r != 0) {
    const int q = old_r / r;
    const int next_r = old_r - q * r;
    old_r = r;
    r = next_r;
    const int next_s = old_s - q * s;
    old_s = s;
    s = next_s;
  }
  if (old_r != 1) {
    return -1;
  }
  // old_s * aa ≡ 1 (mod m); map sign of a
  int inv = old_s;
  if (a < 0) {
    inv = -inv;
  }
  inv %= m;
  if (inv < 0) {
    inv += m;
  }
  return inv;
}

std::string
test_modular_inverse_cpp_output(int a, int m) {
  return std::to_string(test_modular_inverse_cpp(a, m)) + "\n";
}
