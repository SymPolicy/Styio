#pragma once

#include <string>
#include <tuple>
#include <vector>

// CLRS 24.3 Dijkstra: non-negative weighted s->t distance.
// Output: distance; unreachable = 1000000000; malformed = -1.
int
test_dijkstra_cpp(int n,
                  const std::vector<std::tuple<int, int, int>>& edges,
                  int s,
                  int t);

std::string
test_dijkstra_cpp_output(int n,
                         const std::vector<std::tuple<int, int, int>>& edges,
                         int s,
                         int t);
