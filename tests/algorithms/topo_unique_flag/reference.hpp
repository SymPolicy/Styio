#pragma once

#include <string>
#include <utility>
#include <vector>

// 1 if the DAG has a unique topological order; 0 if non-unique or cyclic/malformed.
int
test_topo_unique_flag_cpp(int n, const std::vector<std::pair<int, int>>& edges);

std::string
test_topo_unique_flag_cpp_output(int n, const std::vector<std::pair<int, int>>& edges);
