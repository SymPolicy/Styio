#include "reference.hpp"

#include <map>
#include <string>
#include <vector>

int
test_sam_distinct_substrings_cpp(const std::vector<int>& s) {
  struct State {
    int len = 0;
    int link = -1;
    std::map<int, int> next;
  };
  std::vector<State> st(1);
  int last = 0;
  auto sa_extend = [&](int c) {
    const int cur = static_cast<int>(st.size());
    st.push_back({});
    st[static_cast<std::size_t>(cur)].len = st[static_cast<std::size_t>(last)].len + 1;
    int p = last;
    while (p != -1 && !st[static_cast<std::size_t>(p)].next.count(c)) {
      st[static_cast<std::size_t>(p)].next[c] = cur;
      p = st[static_cast<std::size_t>(p)].link;
    }
    if (p == -1) {
      st[static_cast<std::size_t>(cur)].link = 0;
    } else {
      const int q = st[static_cast<std::size_t>(p)].next[c];
      if (st[static_cast<std::size_t>(p)].len + 1 == st[static_cast<std::size_t>(q)].len) {
        st[static_cast<std::size_t>(cur)].link = q;
      } else {
        const int clone = static_cast<int>(st.size());
        st.push_back(st[static_cast<std::size_t>(q)]);
        st[static_cast<std::size_t>(clone)].len = st[static_cast<std::size_t>(p)].len + 1;
        while (p != -1 && st[static_cast<std::size_t>(p)].next[c] == q) {
          st[static_cast<std::size_t>(p)].next[c] = clone;
          p = st[static_cast<std::size_t>(p)].link;
        }
        st[static_cast<std::size_t>(q)].link = clone;
        st[static_cast<std::size_t>(cur)].link = clone;
      }
    }
    last = cur;
  };
  for (int c : s) {
    sa_extend(c);
  }
  long long ans = 0;
  for (std::size_t i = 1; i < st.size(); ++i) {
    ans += st[i].len - st[static_cast<std::size_t>(st[i].link)].len;
  }
  return static_cast<int>(ans);
}

std::string
test_sam_distinct_substrings_cpp_output(const std::vector<int>& s) {
  return std::to_string(test_sam_distinct_substrings_cpp(s)) + "\n";
}
