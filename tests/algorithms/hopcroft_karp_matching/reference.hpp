#pragma once

#include <string>
#include <utility>
#include <vector>

// CLRS 26.3 companion: Hopcroft-Karp maximum cardinality bipartite matching.
// Left vertices 0..nl-1, right 0..nr-1. Edges (u,v). nl<=0 or nr<=0 -> 0.
int
test_hopcroft_karp_matching_cpp(
  int nl,
  int nr,
  const std::vector<std::pair<int, int>>& edges);

std::string
test_hopcroft_karp_matching_cpp_output(
  int nl,
  int nr,
  const std::vector<std::pair<int, int>>& edges);
