#pragma once

#include <string>

// Binomial C(n,k) for 0<=k<=n<=30 (fits i32). Invalid -> -1.
int
test_binomial_coefficient_cpp(int n, int k);

std::string
test_binomial_coefficient_cpp_output(int n, int k);
