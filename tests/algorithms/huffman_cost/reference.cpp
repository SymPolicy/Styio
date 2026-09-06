#include "reference.hpp"

#include <algorithm>
#include <queue>
#include <string>
#include <vector>

int
test_huffman_cost_cpp(std::vector<int> freqs) {
  freqs.erase(std::remove_if(freqs.begin(), freqs.end(),
                             [](int f) { return f <= 0; }),
              freqs.end());
  if (freqs.size() <= 1) {
    return 0;
  }
  std::priority_queue<int, std::vector<int>, std::greater<int>> pq(
    freqs.begin(), freqs.end());
  int cost = 0;
  while (pq.size() > 1) {
    const int a = pq.top();
    pq.pop();
    const int b = pq.top();
    pq.pop();
    const int merged = a + b;
    cost += merged;
    pq.push(merged);
  }
  return cost;
}

std::string
test_huffman_cost_cpp_output(const std::vector<int>& freqs) {
  return std::to_string(test_huffman_cost_cpp(freqs)) + "\n";
}
