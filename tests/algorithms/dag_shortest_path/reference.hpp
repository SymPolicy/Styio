#pragma once

#include <string>
#include <tuple>
#include <vector>

// CLRS 24.2 DAG shortest paths: single-source s->t on a directed acyclic graph.
// Random tests generate DAGs (forward edges). Output distance; unreachable
// 1000000000; malformed -1. C++ uses topo order + one-pass relax.
int
test_dag_shortest_path_cpp(int n,
                           const std::vector<std::tuple<int, int, int>>& edges,
                           int s,
                           int t);

std::string
test_dag_shortest_path_cpp_output(int n,
                                  const std::vector<std::tuple<int, int, int>>& edges,
                                  int s,
                                  int t);
