#pragma once

#include <string>
#include <utility>
#include <vector>

// CLRS BFS: unweighted shortest-path distance from s to t on a directed graph.
// Vertices are 0 .. n-1. Returns -1 when t is unreachable from s.
int
test_bfs_distance_cpp(int n,
                      const std::vector<std::pair<int, int>>& edges,
                      int s,
                      int t);

std::string
test_bfs_distance_cpp_output(int n,
                             const std::vector<std::pair<int, int>>& edges,
                             int s,
                             int t);
