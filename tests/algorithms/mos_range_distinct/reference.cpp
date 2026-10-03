#include "reference.hpp"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

int
test_mos_range_distinct_cpp(
  const std::vector<int>& a, const std::vector<std::pair<int, int>>& queries) {
  const int n = static_cast<int>(a.size());
  const int q = static_cast<int>(queries.size());
  if (n < 0) {
    return 0;
  }
  struct Q {
    int l, r, id;
  };
  std::vector<Q> qs;
  qs.reserve(static_cast<std::size_t>(q));
  for (int i = 0; i < q; ++i) {
    int l = queries[static_cast<std::size_t>(i)].first;
    int r = queries[static_cast<std::size_t>(i)].second;
    if (l < 0 || r < 0 || l >= n || r >= n || l > r) {
      continue;
    }
    qs.push_back({l, r, i});
  }
  const int block = std::max(1, static_cast<int>(std::sqrt(n == 0 ? 1 : n)));
  std::sort(qs.begin(), qs.end(), [&](const Q& x, const Q& y) {
    const int bx = x.l / block;
    const int by = y.l / block;
    if (bx != by) {
      return bx < by;
    }
    return (bx & 1) ? (x.r > y.r) : (x.r < y.r);
  });
  // coordinate compress values into [0..n)
  std::vector<int> vals = a;
  std::sort(vals.begin(), vals.end());
  vals.erase(std::unique(vals.begin(), vals.end()), vals.end());
  std::vector<int> b(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    b[static_cast<std::size_t>(i)] = static_cast<int>(
      std::lower_bound(vals.begin(), vals.end(), a[static_cast<std::size_t>(i)]) - vals.begin());
  }
  std::vector<int> freq(vals.size() + 1, 0);
  int cur_l = 0;
  int cur_r = -1;
  int distinct = 0;
  auto add = [&](int idx) {
    const int x = b[static_cast<std::size_t>(idx)];
    if (freq[static_cast<std::size_t>(x)] == 0) {
      ++distinct;
    }
    ++freq[static_cast<std::size_t>(x)];
  };
  auto remove = [&](int idx) {
    const int x = b[static_cast<std::size_t>(idx)];
    --freq[static_cast<std::size_t>(x)];
    if (freq[static_cast<std::size_t>(x)] == 0) {
      --distinct;
    }
  };
  long long sum = 0;
  for (const Q& qq : qs) {
    while (cur_l > qq.l) {
      add(--cur_l);
    }
    while (cur_r < qq.r) {
      add(++cur_r);
    }
    while (cur_l < qq.l) {
      remove(cur_l++);
    }
    while (cur_r > qq.r) {
      remove(cur_r--);
    }
    sum += distinct;
  }
  return static_cast<int>(sum);
}

std::string
test_mos_range_distinct_cpp_output(
  const std::vector<int>& a, const std::vector<std::pair<int, int>>& queries) {
  return std::to_string(test_mos_range_distinct_cpp(a, queries)) + "\n";
}
