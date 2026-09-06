#pragma once

#include <string>
#include <vector>

// Offline LCA via binary lifting on a rooted tree.
// parent[0] must be -1 (root); parent[i] in [0,n) for i>0.
// Malformed / disconnected climb -> -1.
int
test_lca_binary_lifting_cpp(const std::vector<int>& parent, int u, int v);

std::string
test_lca_binary_lifting_cpp_output(const std::vector<int>& parent, int u, int v);
