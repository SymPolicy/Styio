#pragma once

#include <string>
#include <tuple>
#include <vector>

// CLRS 25.2 Floyd-Warshall: all-pairs; return dist[s][t].
// Output: distance; unreachable = 1000000000; malformed = -1.
// No negative-cycle inputs in the harness.
int
test_floyd_warshall_cpp(int n,
                        const std::vector<std::tuple<int, int, int>>& edges,
                        int s,
                        int t);

std::string
test_floyd_warshall_cpp_output(int n,
                               const std::vector<std::tuple<int, int, int>>& edges,
                               int s,
                               int t);
