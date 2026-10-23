#pragma once

#include <string>

// CLRS 31.2 / Ch.31 modular multiplicative inverse.
// Return a^{-1} mod m in [0, m), or -1 if gcd(|a|, m) != 1 or m <= 0.
int
test_modular_inverse_cpp(int a, int m);

std::string
test_modular_inverse_cpp_output(int a, int m);
