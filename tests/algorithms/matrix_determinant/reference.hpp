#pragma once

#include <string>
#include <vector>

// Exact integer determinant via Bareiss algorithm. n<=0 -> 0; n>6 -> 0.
int
test_matrix_determinant_cpp(const std::vector<std::vector<int>>& a);

std::string
test_matrix_determinant_cpp_output(const std::vector<std::vector<int>>& a);
