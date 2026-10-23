#pragma once

#include <string>
#include <utility>
#include <vector>

// Andrew monotone-chain convex hull. Returns CCW hull vertices (no repeated close).
// Degenerate / n<=0 -> empty. Collinear points on edge are dropped (strict).
std::vector<std::pair<int, int>>
test_convex_hull_andrew_cpp(const std::vector<std::pair<int, int>>& pts);

std::string
test_convex_hull_andrew_cpp_output(const std::vector<std::pair<int, int>>& pts);
