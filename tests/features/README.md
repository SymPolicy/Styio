# Language Feature Fixtures

**Purpose:** Document the feature-based fixture layout used by Styio language acceptance tests.

**Last updated:** 2026-07-31

Each subdirectory under `tests/features/` owns one language feature surface. Positive executable fixtures use `t*.styio`; semantic or diagnostic failures use `e*.styio`; goldens live under `expected/`; local resource inputs live under `data/` when required.

The CTest contract is feature based:

```bash
ctest --test-dir build/default -L language_feature --output-on-failure
ctest --test-dir build/default -L <feature> --output-on-failure
```

The authoritative catalog is [`workflows/TEST-CATALOG.md`](../../workflows/TEST-CATALOG.md).

Callable evolution keeps separate feature suites. In particular,
`affine_closures/` owns executable and diagnostic evidence for the explicit
`$(...)` environment contract; it must not be folded into the broader
`callable_values/` or `callable_effects/` suites.
