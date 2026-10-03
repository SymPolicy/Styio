#pragma once

#include <string>
#include <tuple>
#include <vector>

// CLRS 23.2 Prim: total weight of an MST grown from vertex 0 on an undirected
// graph. Disconnected -> forest weight of the component of 0 only? We match
// Kruskal's forest semantics by summing Prim trees over all components
// (restart from next unused vertex). Malformed n<=0 -> 0.
int
test_prim_mst_weight_cpp(int n, std::vector<std::tuple<int, int, int>> edges);

std::string
test_prim_mst_weight_cpp_output(int n, const std::vector<std::tuple<int, int, int>>& edges);
