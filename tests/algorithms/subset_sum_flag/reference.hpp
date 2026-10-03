#pragma once

#include <string>
#include <vector>

// Subset-sum decision: 1 if some subset of `values` sums to `target`, else 0.
// Classic DP (CLRS NP-completeness / DP exercises). target<0 -> 0; target==0 -> 1.
int
test_subset_sum_flag_cpp(int target, const std::vector<int>& values);

std::string
test_subset_sum_flag_cpp_output(int target, const std::vector<int>& values);
