#pragma once

#include <string>
#include <utility>
#include <vector>

// CLRS DFS reachability: 1 if t is reachable from s on a directed graph, else 0.
// Vertices are 0 .. n-1.
int
test_dfs_reachable_cpp(int n,
                       const std::vector<std::pair<int, int>>& edges,
                       int s,
                       int t);

std::string
test_dfs_reachable_cpp_output(int n,
                              const std::vector<std::pair<int, int>>& edges,
                              int s,
                              int t);
