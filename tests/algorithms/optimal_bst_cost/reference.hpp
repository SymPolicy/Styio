#pragma once

#include <string>
#include <vector>

// CLRS 15.5 Optimal BST expected-cost companion with key frequencies only
// (dummy probabilities q_i = 0). Return min weighted path cost for keys
// 1..n with frequencies f[i]. Empty -> 0. Malformed -> -1.
int
test_optimal_bst_cost_cpp(const std::vector<int>& freq);

std::string
test_optimal_bst_cost_cpp_output(const std::vector<int>& freq);
