#include "reference.hpp"

#include <string>
#include <vector>

namespace {

long long
mod_mul(long long a, long long b, long long mod) {
  return (a * b) % mod;
}

long long
mod_pow(long long base, long long exp, long long mod) {
  long long result = 1 % mod;
  base %= mod;
  while (exp > 0) {
    if (exp & 1) {
      result = mod_mul(result, base, mod);
    }
    base = mod_mul(base, base, mod);
    exp >>= 1;
  }
  return result;
}

bool
witness(long long a, long long n, long long d, int s) {
  long long x = mod_pow(a, d, n);
  if (x == 1 || x == n - 1) {
    return false;
  }
  for (int i = 1; i < s; ++i) {
    x = mod_mul(x, x, n);
    if (x == n - 1) {
      return false;
    }
  }
  return true;
}

} // namespace

int
test_miller_rabin_prime_flag_cpp(int n) {
  if (n < 0) {
    return -1;
  }
  if (n < 2) {
    return 0;
  }
  if (n == 2 || n == 3 || n == 5 || n == 7 || n == 11 || n == 13 || n == 23) {
    return 1;
  }
  if ((n % 2) == 0) {
    return 0;
  }
  int s = 0;
  long long d = n - 1;
  while ((d % 2) == 0) {
    d /= 2;
    ++s;
  }
  const std::vector<int> bases = {2, 3, 5, 7, 11, 13, 23};
  for (int a : bases) {
    if (a % n == 0) {
      continue;
    }
    if (witness(a, n, d, s)) {
      return 0;
    }
  }
  return 1;
}

std::string
test_miller_rabin_prime_flag_cpp_output(int n) {
  return std::to_string(test_miller_rabin_prime_flag_cpp(n)) + "\n";
}
