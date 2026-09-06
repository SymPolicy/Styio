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
| `articulation_points_count` | Undirected articulation-point count (Ch.22 companion) |
| `edmonds_karp_maxflow` | CLRS 26.2 Edmonds-Karp max s-t flow value |
| `kmp_match_index` | CLRS 32.4 KMP first match index |
| `rabin_karp_match_index` | CLRS 32.2 Rabin-Karp first match index |
| `integer_partition_count` | Unrestricted partition count p(n) (DP companion) |
| `modular_inverse` | Modular multiplicative inverse via Extended-Euclid |
| `chinese_remainder` | CLRS 31.5 CRT for two congruences (generalized) |
| `bipartite_matching` | Max cardinality bipartite matching (Kuhn / flow) |
| `interval_chromatic` | Interval-graph chromatic number (= max overlap) |
| `binsearch_ship_capacity` | Binary search on answer: min ship capacity in D days |
| `optimal_bst_cost` | CLRS 15.5 BST expected-search-cost DP (key frequencies; q_i=0) |
| `dinic_maxflow` | Dinic blocking-flow max s-t flow value (Ch.26 companion) |
| `hopcroft_karp_matching` | Hopcroft-Karp bipartite matching size (Ch.26 companion) |
| `johnson_apsp` | CLRS 25.3 Johnson APSP `dist[s][t]` |
| `z_algorithm_match_index` | Z-algorithm first match index (Ch.32 companion) |
| `manacher_palindrome_length` | Manacher longest palindromic subarray length |
| `fft_poly_multiply` | FFT polynomial multiply (integer coeffs; multi-line) |
| `miller_rabin_prime_flag` | Deterministic Miller-Rabin primality for small n |
| `fenwick_range_sum` | Fenwick / BIT inclusive range sum (DS exercise) |
| `uf_component_size` | Union-Find component size after unions |
| `boyer_moore_match_index` | Boyer-Moore-Horspool first match index |
| `two_sat_flag` | 2-SAT satisfiability via implication SCCs |
| `gale_shapley_matching` | Gale-Shapley stable marriage (proposing-side partners) |
| `segment_tree_range_sum` | Segment tree inclusive range sum (DS companion) |
| `sparse_table_rmq` | Sparse-table RMQ range minimum |
| `lca_binary_lifting` | Offline LCA via binary lifting (tree parents) |
| `suffix_array_lcp` | Suffix array + Kasai LCP of two suffix starts |
| `convex_hull_andrew` | Andrew monotone-chain hull points (multi-line x,y) |
| `closest_pair_dist_sq` | Closest pair squared Euclidean distance (Ch.33) |
| `hungarian_assignment_cost` | Hungarian min assignment cost |
| `binomial_coefficient` | Binomial C(n,k) (n<=30) |
| `euler_totient` | Euler totient φ(n) |
| `push_relabel_maxflow` | CLRS 26.4 push-relabel max s-t flow value |
| `matrix_determinant` | Exact integer matrix determinant (Bareiss) |
| `topo_unique_flag` | Unique topological order flag (Kahn companion) |
| `sieve_prefix_prime_count` | Eratosthenes prefix prime count |
| `pollard_rho_factor` | Pollard Rho least prime factor |
| `discrete_log_bsgs` | Baby-step giant-step discrete log |
| `aho_corasick_match_count` | Aho-Corasick multi-pattern match count |
| `stone_merge_cost` | Interval-DP stone merging min cost |
| `tree_diameter_length` | Tree/forest diameter (edges) |
| `tree_mis_size` | Max independent set size on a forest |
| `stoer_wagner_mincut` | Stoer-Wagner global min-cut value |
| `mcmf_min_cost` | Min-cost of a maximum s-t flow |
| `sam_distinct_substrings` | Suffix automaton distinct-substring count |
| `mos_range_distinct` | Mo's algorithm range-distinct query sum |
| `cartesian_tree_height` | Cartesian tree height (min-heap) |

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

#### Graphs / strings / DP / number theory (batch4)

| Case | Encoding | Output |
|------|----------|--------|
| `articulation_points_count` | `[n, m, u1,v1, ..., then n parent zeros]` (undirected) | articulation-point count via remove-vertex UF |
| `edmonds_karp_maxflow` | `[n, m, s, t, u1,v1,c1, ..., then n*n residual + n parent + n queue zeros]` (`n<=6`) | max flow value; malformed -> `-1` |
| `kmp_match_index` | `[n, m, t1..tn, p1..pm, then m pi zeros]` | first match index, or `-1`; empty pattern -> `0` |
| `rabin_karp_match_index` | `[n, m, t1..tn, p1..pm]` (base 256, mod 101, verify hits) | first match index, or `-1`; empty pattern -> `0` |
| `integer_partition_count` | `[n, then (n+1) DP zeros]` | `p(n)`; `p(0)=1`; `n<0` -> `-1` |
| `modular_inverse` | `[a, m]` | `a^{-1} mod m` in `[0,m)`, or `-1` |
| `chinese_remainder` | `[a, m, b, n]` | unique `x` in `[0,lcm)`, or `-1` |
| `bipartite_matching` | `[nl, nr, m, u1,v1, ..., then N*N residual + N parent + N queue]` (`N=nl+nr+2`) | matching size (Styio: EK flow; C++: Kuhn) |
| `interval_chromatic` | `[n, s1..sn, f1..fn]` half-open `[s,f)` | chromatic number (= max overlap) |
| `binsearch_ship_capacity` | `[n, days, w1..wn]` | min capacity, or `-1` if impossible/malformed |
| `optimal_bst_cost` | `[n, f1..fn, then n*n dp + n*n sum zeros]` | min weighted BST search cost (`q_i=0` companion) |

Notes:

- Weighted shortest-path cases use `1000000000` for unreachable so negative distances stay unambiguous (unlike unweighted `bfs_distance`, which keeps `-1`).
- `bellman_ford` / `floyd_warshall` random tests generate DAGs (forward edges on vertex ids) so negative cycles do not appear.
- `dijkstra` Styio matches Dijkstra distances via Bellman-Ford on non-negative weights; C++ uses a binary-heap Dijkstra.
- `dag_shortest_path` random tests generate DAGs (forward edges on vertex ids); Styio uses Bellman-Ford relaxations matching the topo oracle on DAGs.
- `topo_order` uses multi-line integer output (like `minmax_pair`), not a `[...]` list, because dynamic list construction is not yet a stable Styio port pattern.
- `scc_count` C++ is Kosaraju; Styio unions mutually reachable pairs after a boolean Floyd closure (`n<=12`).
- `fractional_knapsack` returns the integer floor of the classic real-valued greedy optimum when weights/values are integers.
- `extended_gcd` / `mod_pow` cover CLRS Ch.31 number-theoretic classics with clear `list[i32]` I/O.
- `articulation_points_count` mirrors `bridges_count`: C++/Styio both use remove-one UF vs baseline components.
- `edmonds_karp_maxflow` C++ and Styio both run BFS augmenting paths on a residual matrix (`n<=6`).
- `kmp_match_index` / `rabin_karp_match_index` treat sequences as an `i32` alphabet; empty pattern matches at index `0`.
- `bipartite_matching` Styio reduces to unit-capacity Edmonds-Karp on a source-left-right-sink network; C++ uses Kuhn DFS.
- `interval_chromatic` Styio probes each interval start and counts covering half-open intervals (equals event-sweep max depth).
- `binsearch_ship_capacity` is the classic parametric-search "ship packages within D days" exercise.
- `optimal_bst_cost` uses the frequency-only DP (`dp[i][i]=f[i]`); full CLRS `q_i` dummies are deferred.
- `chinese_remainder` implements the generalized two-modulus CRT (moduli need not be coprime).

#### Flow / matching / APSP / strings / number theory / DS (batch5)

| Case | Encoding | Output |
|------|----------|--------|
| `dinic_maxflow` | same as `edmonds_karp_maxflow` (`n<=6`) | max flow value; malformed -> `-1` (C++ Dinic; Styio: EK residual BFS) |
| `hopcroft_karp_matching` | same as `bipartite_matching` | matching size (C++ Hopcroft-Karp; Styio: unit-capacity EK) |
| `johnson_apsp` | same as `floyd_warshall` (`n<=12`) | `dist[s][t]`; unreachable `1000000000`; malformed/neg-cycle `-1` (C++ Johnson; Styio: FW) |
| `z_algorithm_match_index` | `[n, m, t1..tn, p1..pm, then (m+1+n) zeros]` | first match index, or `-1`; empty pattern -> `0` (C++ Z; Styio: naive scan) |
| `manacher_palindrome_length` | `[n, a1..an]` | longest palindromic subarray length (C++ Manacher; Styio: expand-around-center) |
| `fft_poly_multiply` | `[n, m, a0..a{n-1}, b0..b{m-1}, then (n+m) zeros]` | one coefficient per line (`n+m-1` lines); empty factor -> empty stdout (C++ FFT; Styio: schoolbook) |
| `miller_rabin_prime_flag` | `[n]` (`0<=n<=20000` in tests) | `1` prime / `0` composite; `n<2` -> `0`; `n<0` -> `-1` |
| `fenwick_range_sum` | `[n, L, R, a0..a{n-1}, then (n+1) zeros]` (0-based inclusive) | range sum; malformed -> `0` (C++ Fenwick; Styio: prefix sums) |
| `uf_component_size` | `[n, m, x, u1,v1, ..., then n parent + n size zeros]` | size of component containing `x`; malformed -> `-1` |
| `boyer_moore_match_index` | `[n, m, t1..tn, p1..pm]` | first match index, or `-1`; empty pattern -> `0` (C++ BMH; Styio: naive scan) |
| `two_sat_flag` | `[nv, m, lit1a,lit1b, ..., then (2nv)^2 reach zeros]` (`nv<=8`) | `1` / `0` satisfiable (C++ Kosaraju; Styio: Floyd mutual reachability) |
| `gale_shapley_matching` | `[n, men_pref n*n, women_pref n*n, then n next + n wife + n husband + n*n rank zeros]` (`n<=8`) | one partner woman id per man (proposing side); `n<=0` -> empty stdout |

Notes (batch5):

- `dinic_maxflow` / `hopcroft_karp_matching` preserve the same numeric value as Edmonds-Karp / Kuhn while the C++ oracle uses the named textbook algorithm.
- `johnson_apsp` random tests use forward DAG edges so negative cycles do not appear; Styio matches via Floyd-Warshall.
- `fft_poly_multiply` keeps coefficients small (`[-3,3]`) so complex FFT rounding is exact; output is multi-line like `topo_order`.
- `miller_rabin_prime_flag` uses deterministic bases `{2,3,5,7,11,13,23}` with `n` small enough for `i32` modular squares.
- `gale_shapley_matching` returns the proposing-side stable matching as multi-line woman ids.


#### DS / geometry / strings / NT / flow (batch6)

| Case | Encoding | Output |
|------|----------|--------|
| `segment_tree_range_sum` | same as `fenwick_range_sum` | range sum; malformed -> `0` (C++ segment tree; Styio: prefix) |
| `sparse_table_rmq` | `[n, L, R, a0..a{n-1}]` (0-based inclusive) | range minimum; malformed -> `0` (C++ sparse table; Styio: scan) |
| `lca_binary_lifting` | `[n, u, v, parent0..parent{n-1}, then n depth zeros]` with `parent[0]=-1` | LCA id; malformed -> `-1` (C++ binary lifting; Styio: parent climb) |
| `suffix_array_lcp` | `[n, i, j, s0..s{n-1}]` | LCP of suffixes `i`,`j`; `i==j` -> `n-i`; malformed -> `-1` (C++ SA+Kasai; Styio: naive) |
| `convex_hull_andrew` | `[n, x1,y1,...,xn,yn, then n used + n out-x + n out-y zeros]` | multi-line `x`/`y` hull vertices CCW; empty -> empty (C++ Andrew; Styio: Jarvis) |
| `closest_pair_dist_sq` | `[n, x1,y1,...,xn,yn]` | min squared distance; `n<2` -> `-1` (C++ divide-conquer; Styio: O(n²)) |
| `hungarian_assignment_cost` | `[n, c00..c{n-1}{n-1}, then (1<<n) DP zeros]` (`n<=6`) | min assignment cost; `n<=0` -> `0` (C++ Hungarian; Styio: bit-DP) |
| `binomial_coefficient` | `[n, k, then (k+1) row zeros]` (`n<=30`) | `C(n,k)`; invalid -> `-1` |
| `euler_totient` | `[n]` | `φ(n)`; `n<=0` -> `-1` |
| `push_relabel_maxflow` | same as `edmonds_karp_maxflow` (`n<=6`) | max flow value; malformed -> `-1` (C++ push-relabel; Styio: EK) |
| `matrix_determinant` | `[n, a00..a{n-1}{n-1}, then n perm + n used zeros]` (`n<=5`) | det; empty/`n<=0` -> `0` (C++ Bareiss; Styio: Leibniz) |
| `topo_unique_flag` | `[n, m, u1,v1, ..., then n indeg + n alive zeros]` | `1` if unique topo order, else `0` |

Notes (batch6):

- `segment_tree_range_sum` / `sparse_table_rmq` mirror the Fenwick range contract with textbook DS oracles.
- `lca_binary_lifting` random tests build random trees via `parent[i] ∈ [0,i)`.
- `suffix_array_lcp` answers arbitrary suffix pairs by RMQ over Kasai adjacent heights.
- `convex_hull_andrew` tests canonicalize rotation/orientation so Andrew and Jarvis agree on vertex sets/order.
- `closest_pair_dist_sq` keeps coordinates in a small box so squared distances fit `i32`.
- `push_relabel_maxflow` preserves the Edmonds-Karp numeric value with a CLRS 26.4 C++ oracle.



#### Strings / NT / DP / trees / cuts / flow (batch7)

| Case | Encoding | Output |
|------|----------|--------|
| `sieve_prefix_prime_count` | `[n, then (n+1) mark zeros]` | primes in `[1..n]`; `n<=1` -> `0` |
| `pollard_rho_factor` | `[n]` | least prime factor; primes return `n`; `n<=1` -> `-1` (C++ Pollard Rho; Styio: trial) |
| `discrete_log_bsgs` | `[a, b, p]` | smallest `x>=0` with `a^x ≡ b (mod p)`, else `-1` (C++ BSGS; Styio: brute) |
| `aho_corasick_match_count` | `[n, k, t1..tn, m1,p.., m2,p.., ...]` | total pattern occurrences (overlaps counted) |
| `stone_merge_cost` | `[n, a1..an, then n*n dp + (n+1) prefix zeros]` | min stone-merge cost; `n<=1` -> `0` |
| `tree_diameter_length` | `[n, m, u1,v1, ..., then n*n adj + n dist zeros]` | diameter in edges; forest = max over components; `n<=0` -> `-1` |
| `tree_mis_size` | `[n, m, u1,v1, ..., then n*n adj + n take + n skip zeros]` | MIS size on a forest; `n<=0` -> `-1` |
| `stoer_wagner_mincut` | `[n, a00..a{n-1}{n-1}]` | global min-cut value; `n<=0` -> `-1`; `n==1` -> `0` (C++ Stoer-Wagner; Styio: bipartitions) |
| `mcmf_min_cost` | `[n, m, s, t, u,v,cap,cost, ..., then n*n residual + n*n cost + n dist + n parent zeros]` (`n<=5`) | min cost of max flow; malformed -> `-1` |
| `sam_distinct_substrings` | `[n, s0..s{n-1}]` | distinct substring count (C++ SAM; Styio: O(n²) prior-LCP) |
| `mos_range_distinct` | `[n, q, a0..a{n-1}, L1,R1,...,Lq,Rq]` | sum of inclusive range-distinct answers (C++ Mo's; Styio: scan) |
| `cartesian_tree_height` | `[n, a1..an, then n left + n right + n parent + n depth + n stack zeros]` | Cartesian-tree height in edges; empty -> `0` |

Notes (batch7):

- `pollard_rho_factor` / `discrete_log_bsgs` / `sieve_prefix_prime_count` extend Ch.31 number-theory coverage with clear `list[i32]` I/O.
- `aho_corasick_match_count` and `sam_distinct_substrings` continue Ch.32 string companions beyond single-pattern match indices.
- `stone_merge_cost` is the classic interval-DP exercise (merge adjacent piles).
- `tree_diameter_length` / `tree_mis_size` are tree DP companions; random tests build random trees via `parent[i] ∈ [0,i)`.
- `stoer_wagner_mincut` keeps `n<=6` so Styio bipartition enumeration stays tractable.
- `mcmf_min_cost` uses successive shortest paths (Bellman-Ford) on a residual matrix.
- `mos_range_distinct` returns the sum of answers so the stdout contract stays a single `i32`.
- `cartesian_tree_height` builds the min-heap Cartesian tree with stack nearest-smaller.

Malformed / short inputs should fail closed to the documented empty/zero/`-1`
defaults in each case's fixed-case tests (same policy as `inner_product` /
`binary_search`).

