#include "reference.hpp"

#include <string>

int
test_factorial_cpp(int n) {
  int answer = 1;
  for (int i = 2; i <= n; ++i) {
    answer *= i;
  }
  return answer;
}

std::string
test_factorial_cpp_output(int n) {
  return std::to_string(test_factorial_cpp(n)) + "\n";
}
