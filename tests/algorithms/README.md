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
