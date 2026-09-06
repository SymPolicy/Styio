#pragma once

#include <string>
#include <utility>
#include <vector>

// CLRS 22.4 Topological sort (Kahn): lexicographically smallest order on a DAG.
// Output: one vertex id per line. On cycle or malformed n<=0 -> empty stdout.
std::vector<int>
test_topo_order_cpp(int n, const std::vector<std::pair<int, int>>& edges);

std::string
test_topo_order_cpp_output(int n, const std::vector<std::pair<int, int>>& edges);
