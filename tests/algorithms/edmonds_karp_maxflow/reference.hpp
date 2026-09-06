#pragma once

#include <string>
#include <tuple>
#include <vector>

// CLRS 26.2 Edmonds-Karp (Ford-Fulkerson + BFS): max s-t flow value.
// Directed edges with capacity >= 0. Parallel edges accumulate.
// Malformed n<=0 / bad s,t -> -1.
int
test_edmonds_karp_maxflow_cpp(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int>>& edges);

std::string
test_edmonds_karp_maxflow_cpp_output(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int>>& edges);
