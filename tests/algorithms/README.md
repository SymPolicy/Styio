# C++ Reference Equivalence Tests

Each directory under `tests/algorithms/` is one algorithm case. A case keeps
its C++ reference implementation, Styio implementation, and GoogleTest driver
together so new algorithms can be reviewed without changing a shared fixture.

Required case layout:

1. `reference.hpp` / `reference.cpp`: C++ reference implementation.
2. `<case>.styio`: Styio implementation under test.
3. `test.cpp`: deterministic random-input generation and output comparison.

Shared reference-equivalence runner code lives under `.common/` so only
algorithm case directories appear as normal siblings.

The C++ implementation is the reference oracle. The Styio program receives the
same randomized input through stdin, then the test compares exact stdout.

## Standard Library Oracle Matrix

This table tracks C++ standard-library algorithms and numeric algorithms that
fit value-style equivalence testing. "Implemented" means the C++ reference calls
the listed standard-library API and the Styio program re-implements the same
observable behavior.

| C++ reference API | Case | Output contract | Status |
|-------------------|------|-----------------|--------|
| `std::accumulate` | `accumulate_sum` | Sum of an `i32` list. | Implemented |
| `std::adjacent_find` | `adjacent_equal_index` | First adjacent-equal index, or `-1`. | Implemented |
| `std::all_of` | `all_positive_flag` | `1` when every element is `> 0`, else `0`. Empty input follows `std::all_of` and returns `1`. | Implemented |
| `std::any_of` | `any_negative_flag` | `1` when any element is `< 0`, else `0`. | Implemented |
| `std::count` | `count_value` | Count of `target` in `[target, values...]`. | Implemented |
| `std::find` | `linear_search` | First target index in `[target, values...]`, or `-1`. | Implemented |
| `std::inner_product` | `inner_product` | Dot product for `[len, lhs..., rhs...]`. | Implemented |
| `std::is_sorted` | `is_sorted_flag` | `1` for nondecreasing order, else `0`. | Implemented |
| `std::lower_bound` | `binary_search` | First matching index in `[target, sorted_values...]`, or `-1`. | Implemented |
| `std::max_element` | `max_element_value` | Maximum element, or `-1` for empty input. | Implemented |
| `std::min_element` | `min_element_value` | Minimum element, or `-1` for empty input. | Implemented |
| `std::none_of` | `none_zero_flag` | `1` when no element is `0`, else `0`. Empty input follows `std::none_of` and returns `1`. | Implemented |
| `std::partial_sum` | `prefix_sum` | Inclusive prefix-sum list. | Implemented |
| `std::equal` | `equal_flag` | Equality flag for two encoded sequences. | Implemented |
| `std::mismatch` | `mismatch_index` | First differing index, or `-1`. | Implemented |
| `std::lexicographical_compare` | `lexicographical_compare_flag` | `1` when the first encoded sequence compares less. | Implemented |
| `std::upper_bound` | `upper_bound_index` | First index greater than target in a sorted sequence. | Implemented |
| `std::equal_range` | `equal_range_bounds` | Lower/upper bound pair for a target. | Implemented |
| `std::search` | `subrange_search_index` | First subrange match index, or `-1`. | Implemented |
| `std::find_end` | `subrange_find_end_index` | Last subrange match index, or `-1`. | Implemented |
| `std::minmax_element` | `minmax_pair` | Minimum/maximum pair. | Implemented |
| `std::inclusive_scan` | `inclusive_scan_sum` | Inclusive scan list. | Implemented |
| `std::exclusive_scan` | `exclusive_scan_sum` | Exclusive scan list. | Implemented |
| `std::adjacent_difference` | `adjacent_difference` | Adjacent-difference list. | Implemented |
| `std::transform_reduce` | `transform_reduce_square_sum` | Sum of squared values. | Implemented |

Classical non-standard-algorithm cases such as `bubble_sort`,
`selection_sort`, `euclidean_gcd`, and `factorial` stay in this directory as
regression coverage, but they are separate from the standard-library oracle
matrix above.

## CLRS Classic Cases

Textbook classics from *Introduction to Algorithms* (CLRS), value-style stdin/stdout
equivalence (same harness as above):

| Case | Notes |
|------|--------|
| `insertion_sort` | CLRS insertion sort |
| `merge_sort` | C++ oracle: bottom-up merge sort; Styio: sort-correct via insertion control-flow until merge port is runtime-stable |
| `quicksort` | C++ oracle: Lomuto + iterative stack; Styio: sort-correct via insertion control-flow until stack port is runtime-stable |
| `heap_sort` | C++ oracle: in-place heapsort; Styio: sort-correct via insertion control-flow until sift-down port is runtime-stable |
| `bubble_sort` | Classical (already present) |
| `selection_sort` | Classical (already present) |
| `bfs_distance` | CLRS BFS: unweighted directed distance `s -> t` (or `-1`) |
| `dfs_reachable` | CLRS DFS reachability: `1` if `t` reachable from `s`, else `0` |
| `rod_cutting` | CLRS 15.1 rod-cutting maximum revenue |
| `lcs_length` | CLRS 15.4 LCS length |
| `knapsack_01` | Classic 0-1 knapsack maximum value (DP companion) |
| `counting_sort` | CLRS 8.2 counting sort (non-negative `list[i32]`) |
| `select_ith` | CLRS Ch.9 order statistic (0-based) |
| `bellman_ford` | CLRS 24.1 Bellman-Ford `s -> t` |
| `dijkstra` | CLRS 24.3 Dijkstra `s -> t` (non-negative weights) |
| `floyd_warshall` | CLRS 25.2 Floyd-Warshall `dist[s][t]` |
| `matrix_chain_cost` | CLRS 15.2 matrix-chain minimum cost |
| `edit_distance` | Levenshtein edit distance (DP companion) |
| `lis_length` | Longest increasing subsequence length |
| `activity_selection` | CLRS 16.1 activity-selection maximum count |
| `kruskal_mst_weight` | CLRS 23.2 Kruskal MST / forest total weight |
| `topo_order` | CLRS 22.4 Kahn topological sort (lex-smallest; multi-line ids) |
| `scc_count` | CLRS 22.5 Kosaraju SCC count (Styio: reachability + union-find) |
| `prim_mst_weight` | CLRS 23.2 Prim MST / forest total weight |
| `dag_shortest_path` | CLRS 24.2 DAG shortest paths `s -> t` |
| `bridges_count` | Undirected bridge count (Ch.22 exercises companion) |
| `coin_change_min` | Unbounded coin-change minimum coins (DP companion) |
| `subset_sum_flag` | Subset-sum decision flag (DP companion) |
| `fractional_knapsack` | CLRS 16.2 fractional knapsack (integer floor of optimum) |
| `huffman_cost` | CLRS 16.3 Huffman weighted external path length |
| `catalan_number` | nth Catalan number (Ch.15 parenthesization companion) |
| `extended_gcd` | CLRS 31.2 Extended-Euclid `(g,x,y)` |
| `mod_pow` | CLRS 31.6 modular exponentiation |

### Flat `list[i32]` stdin encodings (graphs + DP)

All algorithm cases ingest one Styio `@stdin: list[i32]` line shaped as
`[...]\n` (see `.common/format_i32_list`). Graph and DP cases use the following
**concrete encodings**. Where Styio needs mutable auxiliary storage and cannot
yet allocate fresh lists stably, the formatted stdin may append **trailing
workspace zeros**; the C++ oracle ignores those slots and computes from the
structured fields only.

#### Graphs (directed; vertices `0 .. n-1`)

| Case | Encoding | Output |
|------|----------|--------|
| `bfs_distance` | `[n, m, s, t, u1, v1, ..., um, vm, d0..d{n-1}]` with `n` trailing workspace zeros for Styio unit-weight relaxations | single `i32` distance, or `-1` if unreachable |
| `dfs_reachable` | `[n, m, s, t, u1, v1, ..., um, vm]` | `1` / `0` |

Notes:

- Edges are **directed** `ui -> vi`. Self-loops and parallel edges are allowed.
- `bfs_distance` C++ uses textbook BFS; Styio uses `(n-1)` rounds of `+1`
  edge relaxation over the trailing dist workspace (Bellman-Ford on unit
  weights), which matches BFS distances.
- `dfs_reachable` C++ uses iterative DFS; Styio uses an iterative bitset
  closure (`n <= 30` in random tests) with the same reachability relation.

#### DP

| Case | Encoding | Output |
|------|----------|--------|
| `rod_cutting` | `[n, p1, ..., pn, r0..rn]` with `(n+1)` trailing DP workspace zeros; `pi` is price of length `i` | maximum revenue |
| `lcs_length` | `[n, m, a1..an, b1..bm, then (n+1)*(m+1) zeros]` | LCS length |
| `knapsack_01` | `[n, W, w1..wn, v1..vn, then (W+1) zeros]` | maximum value |
| `matrix_chain_cost` | `[p, d0..dp, then (p+1)*(p+1) zeros]` (`p` matrices) | min scalar multiplications |
| `edit_distance` | `[n, m, a1..an, b1..bm, then (n+1)*(m+1) zeros]` | Levenshtein distance |
| `lis_length` | `[n, a1..an, then n zeros]` | LIS length |

#### Weighted graphs / greedy / order stats

| Case | Encoding | Output |
|------|----------|--------|
| `counting_sort` | `[a1..an]` non-negative integers | sorted `list[i32]` (C++: counting sort with `k=max`; Styio: insertion known-green) |
| `select_ith` | `[i, a1..an]` (`i` 0-based rank) | `a_{(i)}` or `-1` if invalid (C++: `nth_element`; Styio: sort-then-index) |
| `bellman_ford` | `[n, m, s, t, u1,v1,w1, ..., then n dist zeros]` | distance; unreachable `1000000000`; malformed/neg-cycle `-1` |
| `dijkstra` | same shape as `bellman_ford` (weights `w >= 0`) | distance; unreachable `1000000000`; malformed `-1` (Styio: BF relaxations) |
| `floyd_warshall` | `[n, m, s, t, u1,v1,w1, ..., then n*n matrix zeros]` | `dist[s][t]`; unreachable `1000000000`; malformed `-1` |
| `activity_selection` | `[n, s1..sn, f1..fn, then n used-flag zeros]` | max compatible count (Styio: repeated earliest-finish) |
| `kruskal_mst_weight` | `[n, m, u1,v1,w1, ..., then n parent + m taken zeros]` | MST/forest weight (undirected; Styio: min-edge + union-find) |
| `prim_mst_weight` | `[n, m, u1,v1,w1, ..., then n in_mst + n key zeros]` | MST/forest weight (undirected; Styio: dense Prim) |
| `dag_shortest_path` | same shape as `bellman_ford` | distance; unreachable `1000000000`; non-DAG/malformed `-1` (Styio: BF; C++: topo+relax) |
| `topo_order` | `[n, m, u1,v1, ..., then n indeg + n alive + n order zeros]` | one vertex id per line; cycle/malformed -> empty stdout |
| `scc_count` | `[n, m, u1,v1, ..., then n*n reach + n parent zeros]` (`n<=12`) | SCC count (Styio: Floyd reachability + UF) |
| `bridges_count` | `[n, m, u1,v1, ..., then n parent zeros]` (undirected) | bridge count via remove-one UF |

#### DP / greedy / number theory (batch3)

| Case | Encoding | Output |
|------|----------|--------|
| `coin_change_min` | `[n, amount, c1..cn, then (amount+1) zeros]` | min coins, or `-1` if impossible |
| `subset_sum_flag` | `[n, target, a1..an, then (target+1) zeros]` | `1` / `0` |
| `fractional_knapsack` | `[n, W, w1..wn, v1..vn, then n taken zeros]` | floor of fractional optimum (cross-multiply density) |
| `huffman_cost` | `[n, f1..fn, then n alive + n work zeros]` | Huffman merge cost (`n<=1` -> `0`) |
| `catalan_number` | `[n, then (n+1) zeros]` | `C_n` (`n<=15` in tests) |
| `extended_gcd` | `[a, b]` | three lines `g`, `x`, `y` with `a*x+b*y=g` |
| `mod_pow` | `[base, exp, mod]` | `(base^exp) mod mod`; malformed -> `-1` |

Notes:

- Weighted shortest-path cases use `1000000000` for unreachable so negative distances stay unambiguous (unlike unweighted `bfs_distance`, which keeps `-1`).
- `bellman_ford` / `floyd_warshall` random tests generate DAGs (forward edges on vertex ids) so negative cycles do not appear.
- `dijkstra` Styio matches Dijkstra distances via Bellman-Ford on non-negative weights; C++ uses a binary-heap Dijkstra.
- `dag_shortest_path` random tests generate DAGs (forward edges on vertex ids); Styio uses Bellman-Ford relaxations matching the topo oracle on DAGs.
- `topo_order` uses multi-line integer output (like `minmax_pair`), not a `[...]` list, because dynamic list construction is not yet a stable Styio port pattern.
- `scc_count` C++ is Kosaraju; Styio unions mutually reachable pairs after a boolean Floyd closure (`n<=12`).
- `fractional_knapsack` returns the integer floor of the classic real-valued greedy optimum when weights/values are integers.
- `extended_gcd` / `mod_pow` cover CLRS Ch.31 number-theoretic classics with clear `list[i32]` I/O.

Malformed / short inputs should fail closed to the documented empty/zero/`-1`
defaults in each case's fixed-case tests (same policy as `inner_product` /
`binary_search`).

