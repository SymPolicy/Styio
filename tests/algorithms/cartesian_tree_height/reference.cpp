#include "reference.hpp"

#include <stack>
#include <string>
#include <vector>

int
test_cartesian_tree_height_cpp(const std::vector<int>& a) {
  const int n = static_cast<int>(a.size());
  if (n <= 0) {
    return 0;
  }
  std::vector<int> left(static_cast<std::size_t>(n), -1);
  std::vector<int> right(static_cast<std::size_t>(n), -1);
  std::vector<int> parent(static_cast<std::size_t>(n), -1);
  std::stack<int> st;
  for (int i = 0; i < n; ++i) {
    int last = -1;
    while (!st.empty() && a[static_cast<std::size_t>(st.top())] > a[static_cast<std::size_t>(i)]) {
      last = st.top();
      st.pop();
    }
    if (!st.empty()) {
      right[static_cast<std::size_t>(st.top())] = i;
      parent[static_cast<std::size_t>(i)] = st.top();
    }
    if (last != -1) {
      left[static_cast<std::size_t>(i)] = last;
      parent[static_cast<std::size_t>(last)] = i;
    }
    st.push(i);
  }
  int root = 0;
  while (parent[static_cast<std::size_t>(root)] != -1) {
    root = parent[static_cast<std::size_t>(root)];
  }
  // height via DFS stack
  std::vector<int> depth(static_cast<std::size_t>(n), 0);
  std::vector<int> stack = {root};
  int best = 0;
  while (!stack.empty()) {
    const int u = stack.back();
    stack.pop_back();
    if (depth[static_cast<std::size_t>(u)] > best) {
      best = depth[static_cast<std::size_t>(u)];
    }
    const int L = left[static_cast<std::size_t>(u)];
    const int R = right[static_cast<std::size_t>(u)];
    if (L != -1) {
      depth[static_cast<std::size_t>(L)] = depth[static_cast<std::size_t>(u)] + 1;
      stack.push_back(L);
    }
    if (R != -1) {
      depth[static_cast<std::size_t>(R)] = depth[static_cast<std::size_t>(u)] + 1;
      stack.push_back(R);
    }
  }
  return best;
}

std::string
test_cartesian_tree_height_cpp_output(const std::vector<int>& a) {
  return std::to_string(test_cartesian_tree_height_cpp(a)) + "\n";
}
