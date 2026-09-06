#pragma once

#include <string>
#include <tuple>
#include <vector>

// CLRS 26.2 companion: Dinic blocking-flow max s-t flow value.
// Directed edges with capacity >= 0. Parallel edges accumulate.
// Malformed n<=0 / bad s,t / s==t -> -1.
int
test_dinic_maxflow_cpp(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int>>& edges);

std::string
test_dinic_maxflow_cpp_output(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int>>& edges);
