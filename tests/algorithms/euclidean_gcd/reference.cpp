#include "reference.hpp"

#include <cstdlib>
#include <numeric>
#include <string>

int
test_euclidean_gcd_cpp(int a, int b) {
  return std::gcd(std::abs(a), std::abs(b));
}

std::string
test_euclidean_gcd_cpp_output(int a, int b) {
  return std::to_string(test_euclidean_gcd_cpp(a, b)) + "\n";
}
