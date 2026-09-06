#pragma once

#include <string>
#include <tuple>
#include <vector>

// Push-relabel (relabel-to-front style) max s-t flow value.
// Malformed -> -1.
int
test_push_relabel_maxflow_cpp(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int>>& edges);

std::string
test_push_relabel_maxflow_cpp_output(
  int n,
  int s,
  int t,
  const std::vector<std::tuple<int, int, int>>& edges);
