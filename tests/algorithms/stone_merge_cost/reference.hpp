#pragma once

#include <string>
#include <vector>

// Classic interval-DP stone merging minimum cost.
// Cost of merging piles i..j is sum(a[i..j]) + min over splits.
int
test_stone_merge_cost_cpp(const std::vector<int>& a);

std::string
test_stone_merge_cost_cpp_output(const std::vector<int>& a);
