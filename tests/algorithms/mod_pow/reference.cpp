#include "reference.hpp"

#include <string>

int
test_mod_pow_cpp(int base, int exp, int mod) {
  if (mod <= 0 || exp < 0) {
    return -1;
  }
  long long result = 1 % mod;
  long long b = base % mod;
  if (b < 0) {
    b += mod;
  }
  int e = exp;
  while (e > 0) {
    if (e % 2 == 1) {
      result = (result * b) % mod;
    }
    b = (b * b) % mod;
    e /= 2;
  }
  return static_cast<int>(result);
}

std::string
test_mod_pow_cpp_output(int base, int exp, int mod) {
  return std::to_string(test_mod_pow_cpp(base, exp, mod)) + "\n";
}
