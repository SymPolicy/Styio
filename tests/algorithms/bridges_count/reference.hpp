#pragma once

#include <string>
#include <utility>
#include <vector>

// Undirected bridge count (CLRS 22.5 / articulation exercises companion).
// A bridge is an edge whose removal increases the number of connected components.
// Parallel edges: only a unique critical connection counts as a bridge.
// Malformed n<=0 -> 0.
int
test_bridges_count_cpp(int n, const std::vector<std::pair<int, int>>& edges);

std::string
test_bridges_count_cpp_output(int n, const std::vector<std::pair<int, int>>& edges);
