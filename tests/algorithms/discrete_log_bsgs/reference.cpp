#include "reference.hpp"

#include <cmath>
#include <string>
#include <unordered_map>

int
test_discrete_log_bsgs_cpp(int a, int b, int p) {
  if (p <= 0) {
    return -1;
  }
  a %= p;
  if (a < 0) {
    a += p;
  }
  b %= p;
  if (b < 0) {
    b += p;
  }
  if (p == 1) {
    return 0;
  }
  if (b == 1 % p) {
    return 0;
  }
  if (a == 0) {
    return b == 0 ? 1 : -1;
  }
  const int m = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(p)))) + 1;
  std::unordered_map<int, int> baby;
  long long cur = 1;
  for (int j = 0; j < m; ++j) {
    if (!baby.count(static_cast<int>(cur))) {
      baby[static_cast<int>(cur)] = j;
    }
    cur = (cur * a) % p;
  }
  // factor = a^{-m} mod p
  long long base = 1;
  for (int i = 0; i < m; ++i) {
    base = (base * a) % p;
  }
  auto mod_pow = [&](long long x, long long e) {
    long long r = 1 % p;
    while (e > 0) {
      if (e & 1) {
        r = (r * x) % p;
      }
      x = (x * x) % p;
      e >>= 1;
    }
    return r;
  };
  // inverse of base via Fermat (p prime) or extended Euclid fallback
  long long inv = mod_pow(base, p - 2);
  long long gamma = b;
  for (int i = 0; i < m; ++i) {
    auto it = baby.find(static_cast<int>(gamma));
    if (it != baby.end()) {
      const long long ans = 1LL * i * m + it->second;
      if (ans < p) {
        return static_cast<int>(ans);
      }
    }
    gamma = (gamma * inv) % p;
  }
  return -1;
}

std::string
test_discrete_log_bsgs_cpp_output(int a, int b, int p) {
  return std::to_string(test_discrete_log_bsgs_cpp(a, b, p)) + "\n";
}
