#pragma once

#include <string>

// CLRS 31.5 Chinese Remainder Theorem for two congruences:
//   x ≡ a (mod m), x ≡ b (mod n)
// Return the unique solution in [0, lcm(m,n)), or -1 if no solution /
// moduli non-positive. Uses generalized CRT (moduli need not be coprime).
int
test_chinese_remainder_cpp(int a, int m, int b, int n);

std::string
test_chinese_remainder_cpp_output(int a, int m, int b, int n);
