#pragma once

#include <string>
#include <tuple>
#include <vector>

// CLRS 23.2 Kruskal: total weight of an MST on an undirected connected graph.
// If the graph is disconnected, return the forest weight (sum of MSTs of components).
// Malformed n<=0 -> 0.
int
test_kruskal_mst_weight_cpp(int n, std::vector<std::tuple<int, int, int>> edges);

std::string
test_kruskal_mst_weight_cpp_output(int n, const std::vector<std::tuple<int, int, int>>& edges);
