#pragma once

#include <string>

// Deterministic Miller-Rabin primality for n in [0, 46340] (i32-safe squares).
// Bases {2,3,5,7,11,13,23}. Output 1 prime / 0 composite; n<2 -> 0; n<0 -> -1.
int
test_miller_rabin_prime_flag_cpp(int n);

std::string
test_miller_rabin_prime_flag_cpp_output(int n);
