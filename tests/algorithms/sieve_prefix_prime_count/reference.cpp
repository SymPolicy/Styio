#include "reference.hpp"

#include <string>
#include <vector>

int
test_sieve_prefix_prime_count_cpp(int n) {
  if (n <= 1) {
    return 0;
  }
  std::vector<char> is_prime(static_cast<std::size_t>(n) + 1, 1);
  is_prime[0] = 0;
  is_prime[1] = 0;
  for (int p = 2; 1LL * p * p <= n; ++p) {
    if (is_prime[static_cast<std::size_t>(p)]) {
      for (int x = p * p; x <= n; x += p) {
        is_prime[static_cast<std::size_t>(x)] = 0;
      }
    }
  }
  int count = 0;
  for (int i = 2; i <= n; ++i) {
    count += is_prime[static_cast<std::size_t>(i)] ? 1 : 0;
  }
  return count;
}

std::string
test_sieve_prefix_prime_count_cpp_output(int n) {
  return std::to_string(test_sieve_prefix_prime_count_cpp(n)) + "\n";
}
