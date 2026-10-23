#pragma once

#include <string>
#include <tuple>
#include <vector>

// Min-cost of a maximum s-t flow (successive shortest paths).
// Edge: u,v,cap,cost. Malformed -> -1.
int
test_mcmf_min_cost_cpp(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int, int>>& edges);

std::string
test_mcmf_min_cost_cpp_output(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int, int>>& edges);
