#pragma once

#include <string>
#include <utility>
#include <vector>

// CLRS 22.5 Strongly connected components (Kosaraju): return the number of SCCs
// on a directed graph. Malformed n<=0 -> 0.
int
test_scc_count_cpp(int n, const std::vector<std::pair<int, int>>& edges);

std::string
test_scc_count_cpp_output(int n, const std::vector<std::pair<int, int>>& edges);
