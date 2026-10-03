#include "reference.hpp"

#include <queue>
#include <string>
#include <vector>

std::vector<int>
test_gale_shapley_matching_cpp(
  int n,
  const std::vector<std::vector<int>>& men_pref,
  const std::vector<std::vector<int>>& women_pref) {
  if (n <= 0) {
    return {};
  }
  // women_rank[w][m] = preference rank (lower better)
  std::vector<std::vector<int>> women_rank(
    static_cast<std::size_t>(n), std::vector<int>(static_cast<std::size_t>(n), n));
  for (int w = 0; w < n; ++w) {
    if (static_cast<int>(women_pref[static_cast<std::size_t>(w)].size()) != n) {
      return {};
    }
    for (int r = 0; r < n; ++r) {
      const int m = women_pref[static_cast<std::size_t>(w)][static_cast<std::size_t>(r)];
      if (m < 0 || m >= n) {
        return {};
      }
      women_rank[static_cast<std::size_t>(w)][static_cast<std::size_t>(m)] = r;
    }
  }
  for (int m = 0; m < n; ++m) {
    if (static_cast<int>(men_pref[static_cast<std::size_t>(m)].size()) != n) {
      return {};
    }
  }
  std::vector<int> next_prop(static_cast<std::size_t>(n), 0);
  std::vector<int> wife(static_cast<std::size_t>(n), -1);
  std::vector<int> husband(static_cast<std::size_t>(n), -1);
  std::queue<int> free_men;
  for (int m = 0; m < n; ++m) {
    free_men.push(m);
  }
  while (!free_men.empty()) {
    const int m = free_men.front();
    free_men.pop();
    if (next_prop[static_cast<std::size_t>(m)] >= n) {
      continue;
    }
    const int w =
      men_pref[static_cast<std::size_t>(m)]
              [static_cast<std::size_t>(next_prop[static_cast<std::size_t>(m)]++)];
    if (w < 0 || w >= n) {
      free_men.push(m);
      continue;
    }
    if (husband[static_cast<std::size_t>(w)] == -1) {
      husband[static_cast<std::size_t>(w)] = m;
      wife[static_cast<std::size_t>(m)] = w;
    } else {
      const int m2 = husband[static_cast<std::size_t>(w)];
      if (women_rank[static_cast<std::size_t>(w)][static_cast<std::size_t>(m)] <
          women_rank[static_cast<std::size_t>(w)][static_cast<std::size_t>(m2)]) {
        husband[static_cast<std::size_t>(w)] = m;
        wife[static_cast<std::size_t>(m)] = w;
        wife[static_cast<std::size_t>(m2)] = -1;
        free_men.push(m2);
      } else {
        free_men.push(m);
      }
    }
  }
  return wife;
}

std::string
test_gale_shapley_matching_cpp_output(
  int n,
  const std::vector<std::vector<int>>& men_pref,
  const std::vector<std::vector<int>>& women_pref) {
  const std::vector<int> wife =
    test_gale_shapley_matching_cpp(n, men_pref, women_pref);
  std::string out;
  for (int w : wife) {
    out += std::to_string(w);
    out += "\n";
  }
  return out;
}
