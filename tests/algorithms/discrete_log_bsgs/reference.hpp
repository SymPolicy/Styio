#pragma once

#include <string>

// Baby-step giant-step: smallest x>=0 with a^x ≡ b (mod p). No solution -> -1.
// Assumes p is prime in random tests; a,b in [0,p).
int
test_discrete_log_bsgs_cpp(int a, int b, int p);

std::string
test_discrete_log_bsgs_cpp_output(int a, int b, int p);
