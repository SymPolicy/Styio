#pragma once

#include <string>

// Least prime factor of n via Pollard Rho + trial. n<=1 -> -1; primes return n.
int
test_pollard_rho_factor_cpp(int n);

std::string
test_pollard_rho_factor_cpp_output(int n);
