#pragma once

#include <string>
#include <tuple>
#include <vector>

// CLRS 24.1 Bellman-Ford: shortest path s->t with integer edge weights
// (may be negative). Output: distance; unreachable = 1000000000; malformed = -1.
// Harness inputs have no negative cycle reachable from s; if one is detected, -1.
int
test_bellman_ford_cpp(int n,
                      const std::vector<std::tuple<int, int, int>>& edges,
                      int s,
                      int t);

std::string
test_bellman_ford_cpp_output(int n,
                             const std::vector<std::tuple<int, int, int>>& edges,
                             int s,
                             int t);
