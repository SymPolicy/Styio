#pragma once

#include <string>
#include <utility>
#include <vector>

// Union-Find with path compression + union by size: size of component of x
// after processing undirected unions. Malformed n<=0 / bad x -> -1.
int
test_uf_component_size_cpp(
  int n,
  int x,
  const std::vector<std::pair<int, int>>& edges);

std::string
test_uf_component_size_cpp_output(
  int n,
  int x,
  const std::vector<std::pair<int, int>>& edges);
