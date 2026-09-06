#include "reference.hpp"

#include <queue>
#include <string>
#include <vector>

int
test_aho_corasick_match_count_cpp(
  const std::vector<int>& text, const std::vector<std::vector<int>>& patterns) {
  constexpr int SIG = 64;
  auto enc = [](int c) -> int {
    if (c < 0) {
      c = -c;
    }
    return c % SIG;
  };
  struct Node {
    int next[SIG];
    int fail = 0;
    int out = 0;
    Node() {
      for (int i = 0; i < SIG; ++i) {
        next[i] = -1;
      }
    }
  };
  std::vector<Node> t(1);
  for (const auto& pat : patterns) {
    if (pat.empty()) {
      continue;
    }
    int v = 0;
    for (int ch : pat) {
      const int c = enc(ch);
      if (t[static_cast<std::size_t>(v)].next[c] == -1) {
        t[static_cast<std::size_t>(v)].next[c] = static_cast<int>(t.size());
        t.emplace_back();
      }
      v = t[static_cast<std::size_t>(v)].next[c];
    }
    t[static_cast<std::size_t>(v)].out += 1;
  }
  std::queue<int> q;
  for (int c = 0; c < SIG; ++c) {
    int& nxt = t[0].next[c];
    if (nxt != -1) {
      t[static_cast<std::size_t>(nxt)].fail = 0;
      q.push(nxt);
    } else {
      nxt = 0;
    }
  }
  while (!q.empty()) {
    const int v = q.front();
    q.pop();
    for (int c = 0; c < SIG; ++c) {
      const int u = t[static_cast<std::size_t>(v)].next[c];
      if (u == -1) {
        t[static_cast<std::size_t>(v)].next[c] =
          t[static_cast<std::size_t>(t[static_cast<std::size_t>(v)].fail)].next[c];
        continue;
      }
      t[static_cast<std::size_t>(u)].fail =
        t[static_cast<std::size_t>(t[static_cast<std::size_t>(v)].fail)].next[c];
      t[static_cast<std::size_t>(u)].out +=
        t[static_cast<std::size_t>(t[static_cast<std::size_t>(u)].fail)].out;
      q.push(u);
    }
  }
  int state = 0;
  int hits = 0;
  for (int ch : text) {
    state = t[static_cast<std::size_t>(state)].next[enc(ch)];
    hits += t[static_cast<std::size_t>(state)].out;
  }
  return hits;
}

std::string
test_aho_corasick_match_count_cpp_output(
  const std::vector<int>& text, const std::vector<std::vector<int>>& patterns) {
  return std::to_string(test_aho_corasick_match_count_cpp(text, patterns)) + "\n";
}
