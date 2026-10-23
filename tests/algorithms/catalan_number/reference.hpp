#pragma once

#include <string>

// nth Catalan number C_n (CLRS Ch.15 matrix-chain / parenthesization count).
// Malformed n<0 -> 0. Random/fixed tests keep n <= 15 (fits i32).
int
test_catalan_number_cpp(int n);

std::string
test_catalan_number_cpp_output(int n);
