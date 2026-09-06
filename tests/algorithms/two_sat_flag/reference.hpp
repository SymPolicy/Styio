#pragma once

#include <string>
#include <utility>
#include <vector>

// 2-SAT satisfiability via implication graph SCCs.
// Variables 1..nv. Each clause is a pair of literals (+/- var ids).
// Output 1 satisfiable / 0 unsat; nv<=0 -> 1; malformed lit -> 0.
int
test_two_sat_flag_cpp(
  int nv, const std::vector<std::pair<int, int>>& clauses);

std::string
test_two_sat_flag_cpp_output(
  int nv, const std::vector<std::pair<int, int>>& clauses);
