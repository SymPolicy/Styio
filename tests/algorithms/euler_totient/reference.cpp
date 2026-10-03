#include "reference.hpp"

#include <string>

int
test_euler_totient_cpp(int n) {
  if (n <= 0) {
    return -1;
  }
  int result = n;
  int x = n;
  for (int p = 2; 1LL * p * p <= x; ++p) {
    if (x % p == 0) {
      while (x % p == 0) {
        x /= p;
      }
      result -= result / p;
    }
  }
  if (x > 1) {
    result -= result / x;
  }
  return result;
}

std::string
test_euler_totient_cpp_output(int n) {
  return std::to_string(test_euler_totient_cpp(n)) + "\n";
}
