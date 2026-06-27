# Examples

**Purpose:** List runnable repository-local examples and their checked outputs.

**Last updated:** 2026-06-25

The active examples in this directory are runnable with
`build/default/bin/styio` and are covered by CTest.

These examples are smoke coverage and user-facing samples. They do not define
standard-library APIs or compiler-intrinsic behavior; promotion requires the
manifest, IM-D8, and test-catalog evidence path.

## Hello World

```bash
build/default/bin/styio --file example/hello_world.styio
```

Expected output:

```text
Hello, World!
```

## Parallel Job Signal

```bash
printf '101\n94\n' | build/default/bin/styio --file example/job_parallel_signal.styio
```

Expected output:

```text
parallel signal: spread=7.000000, midpoint=97.500000
```

## Bubble Sort

```bash
printf '[3, 1, 2]\n' | build/default/bin/styio --file example/algorithms/bubble_sort.styio
```

Expected output:

```text
[1,2,3]
```

## Eight Queens

```bash
build/default/bin/styio --file example/algorithms/eight_queens.styio
```

Expected output:

```text
92
```

## CLI Calculator

```bash
STYIO_BIN=build/default/bin/styio ./example/cli_calculator.sh "1 + 2 * (3 + 4)"
```

Expected output:

```text
15
```
