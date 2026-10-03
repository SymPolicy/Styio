#include "reference.hpp"

#include <cstdlib>
#include <string>

namespace {

long long
mul_mod(long long a, long long b, long long mod) {
  return (a * b) % mod;
}

long long
pollard_f(long long x, long long c, long long mod) {
  return (mul_mod(x, x, mod) + c) % mod;
}

long long
pollard_rho(long long n) {
  if (n % 2 == 0) {
    return 2;
  }
  long long x = 2;
  long long y = 2;
  long long d = 1;
  long long c = 1;
  while (d == 1) {
    x = pollard_f(x, c, n);
    y = pollard_f(pollard_f(y, c, n), c, n);
    long long diff = x > y ? x - y : y - x;
    // gcd
    long long a = diff;
    long long b = n;
    while (b) {
      long long t = a % b;
      a = b;
      b = t;
    }
    d = a;
    if (d == n) {
      c += 1;
      x = 2;
      y = 2;
      d = 1;
      if (c > 32) {
        return n;
      }
    }
  }
  return d;
}

bool
is_prime_trial(long long n) {
  if (n < 2) {
    return false;
  }
  for (long long p = 2; p * p <= n; ++p) {
    if (n % p == 0) {
      return false;
    }
  }
  return true;
}

int
least_prime_factor(int n) {
  if (n <= 1) {
    return -1;
  }
  if (is_prime_trial(n)) {
    return n;
  }
  long long f = pollard_rho(n);
  if (f == n || f <= 1) {
    // fallback trial
    for (int p = 2; 1LL * p * p <= n; ++p) {
      if (n % p == 0) {
        return p;
      }
    }
    return n;
  }
  // peel to LPF
  int left = static_cast<int>(f);
  int right = n / left;
  int a = least_prime_factor(left);
  int b = least_prime_factor(right);
  if (a < 0) {
    return b;
  }
  if (b < 0) {
    return a;
  }
  return a < b ? a : b;
}

} // namespace

int
test_pollard_rho_factor_cpp(int n) {
  return least_prime_factor(n);
}

std::string
test_pollard_rho_factor_cpp_output(int n) {
  return std::to_string(test_pollard_rho_factor_cpp(n)) + "\n";
}
