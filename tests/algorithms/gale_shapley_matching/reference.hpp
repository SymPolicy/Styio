#pragma once

#include <string>
#include <vector>

// Gale-Shapley stable marriage (proposing-side): for each man i, partner woman.
// Prefs: men_pref[i] = ranking list of women (permutation);
//        women_pref[j] = ranking list of men (permutation).
// n<=0 -> empty output. Output: n lines, partner of man 0..n-1.
std::vector<int>
test_gale_shapley_matching_cpp(
  int n,
  const std::vector<std::vector<int>>& men_pref,
  const std::vector<std::vector<int>>& women_pref);

std::string
test_gale_shapley_matching_cpp_output(
  int n,
  const std::vector<std::vector<int>>& men_pref,
  const std::vector<std::vector<int>>& women_pref);
