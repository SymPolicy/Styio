#pragma once

#include <string>
#include <tuple>
#include <vector>

// CLRS 25.3 Johnson APSP: return dist[s][t] after reweighting + Dijkstra.
// Negative weights allowed; no negative cycle in random tests.
// Unreachable -> 1000000000; malformed -> -1.
int
test_johnson_apsp_cpp(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int>>& edges);

std::string
test_johnson_apsp_cpp_output(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int>>& edges);
