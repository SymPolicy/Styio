#pragma once

#include <string>
#include <vector>

// CLRS 15.2 Matrix-chain multiplication: minimum scalar multiplications.
// dims has length n+1 for n matrices A_i with dims[i-1] x dims[i].
// n < 1 -> 0.
int
test_matrix_chain_cost_cpp(const std::vector<int>& dims);

std::string
test_matrix_chain_cost_cpp_output(const std::vector<int>& dims);
