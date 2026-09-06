#pragma once

#include <string>
#include <utility>
#include <vector>

// Maximum cardinality bipartite matching (Kuhn DFS / Hopcroft companion).
// Left vertices [0, nl), right [0, nr). Directed left->right edges.
// Output matching size. Malformed -> 0.
int
test_bipartite_matching_cpp(
  int nl,
  int nr,
  const std::vector<std::pair<int, int>>& edges);

std::string
test_bipartite_matching_cpp_output(
  int nl,
  int nr,
  const std::vector<std::pair<int, int>>& edges);
