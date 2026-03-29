# Milestone 7: Multi-Stream Synchronization & Resource Drivers

**文档作用：** M7 **验收测试与任务分解**；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md)。

**Depends on:** M6 (State & Streams)  
**Goal:** Stream zip (`&`), snapshot pull (`@[v] << @res`, `(<< @res)`), multi-threaded driver runtime, and a complete cross-source strategy compile and execute.

---

## Acceptance Tests

### T7.01 — Zip two collections

```
// File: tests/m7/t01_zip_collections.styio
// Expected stdout:
// 1 a
// 2 b
// 3 c
[1, 2, 3] >> #(n) & ["a", "b", "c"] >> #(s) => {
    >_(n + " " + s)
}
```

### T7.02 — Zip with unequal lengths (shorter wins)

```
// File: tests/m7/t02_zip_unequal.styio
// Expected stdout:
// 1 x
// 2 y
[1, 2, 3] >> #(n) & ["x", "y"] >> #(s) => {
    >_(n + " " + s)
}
```

### T7.03 — Snapshot pull from file

```
// File: tests/m7/t03_snapshot.styio
// Setup: tests/m7/data/ref.txt containing "100"
// Expected stdout:
// 101
// 102
// 103
@[ref_val] << @file{"tests/m7/data/ref.txt"}
[1, 2, 3] >> #(x) => {
    >_(x + $ref_val)
}
```

### T7.04 — Instant pull `(<< @res)`

```
// File: tests/m7/t04_instant_pull.styio
// Setup: tests/m7/data/ref.txt containing "50"
// Expected stdout: 51
x = 1
result = x + (<< @file{"tests/m7/data/ref.txt"})
>_(result)
```

### T7.05 — Zip two file streams

```
// File: tests/m7/t05_zip_files.styio
// Setup: tests/m7/data/prices_a.txt = "100\n200\n300"
//        tests/m7/data/prices_b.txt = "105\n195\n310"
// Expected stdout:
// -5
// 5
// -10
@file{"tests/m7/data/prices_a.txt"} >> #(a)
  & @file{"tests/m7/data/prices_b.txt"} >> #(b) => {
    >_(a - b)
}
```

### T7.06 — Frame lock with snapshot (consistent read)

```
// File: tests/m7/t06_snapshot_lock.styio
// Setup: tests/m7/data/ref.txt containing "42"
// Expected stdout: true
@[ref] << @file{"tests/m7/data/ref.txt"}
result = true
[1, 2, 3] >> #(x) => {
    a = $ref
    b = $ref
    result = result && (a == b)
}
>_(result)
```

### T7.07 — Driver singleton (same file opened once)

```
// File: tests/m7/t07_singleton.styio
// Setup: tests/m7/data/hello.txt = "Hello"
// Expected stdout:
// Hello
// Hello
// Only one file handle should be opened (verify via debug/runtime log)
f1 <- @file{"tests/m7/data/hello.txt"}
f2 <- @file{"tests/m7/data/hello.txt"}
f1 >> #(line) => { >_(line) }
f2 >> #(line) => { >_(line) }
```

### T7.08 — Cross-source arbitrage (comprehensive)

```
// File: tests/m7/t08_arbitrage.styio
// Setup: tests/m7/data/exchange_a.txt = prices from exchange A
//        tests/m7/data/exchange_b.txt = prices from exchange B
// Expected stdout: lines showing arbitrage gap when |a-b| > threshold
@file{"tests/m7/data/exchange_a.txt"} >> #(a)
  & @file{"tests/m7/data/exchange_b.txt"} >> #(b) => {
    gap = a - b
    (gap > 5 || gap < -5) ~> >_("Arb: " + gap) | @
}
```

### T7.09 — Snapshot with state accumulation

```
// File: tests/m7/t09_snapshot_accum.styio
// Setup: tests/m7/data/factor.txt = "2"
// Expected stdout: 12
// [1,2,3] each multiplied by factor (2), then summed: 2+4+6 = 12
@[factor] << @file{"tests/m7/data/factor.txt"}
@[total = 0](sum = $total)
[1, 2, 3] >> #(x) => {
    sum += x * $factor
}
>_(sum)
```

### T7.10 — Full pipeline: read → compute → write

```
// File: tests/m7/t10_full_pipeline.styio
// Setup: tests/m7/data/input.txt = "10\n20\n30"
// Expected: /tmp/styio_doubled.txt contains "20\n40\n60"
@file{"tests/m7/data/input.txt"} >> #(x) => {
    result = x * 2
    result << @file{"/tmp/styio_doubled.txt"}
}
```

---

## Implementation Tasks

### Task 7.1 — Parse `&` (stream zip)
**Role:** Parser Agent  
**Action:** After a `source >> #(var)` clause, if `&` follows, parse another `source >> #(var)` clause, then `=>` body. Produce `StreamZipAST(source_a, var_a, source_b, var_b, body)`.

### Task 7.2 — Parse `@[v] << @res` (snapshot declaration)
**Role:** Parser Agent  
**Action:** `@[identifier] << resource` in statement position produces `SnapshotDeclAST(var, resource)`.

### Task 7.3 — Parse `(<< @res)` (instant pull)
**Role:** Parser Agent  
**Action:** `(` `<<` resource `)` as a primary expression produces `InstantPullAST(resource)`.

### Task 7.4 — Multi-threaded runtime foundation
**Role:** New Runtime module  
**Action:** Extend `StyioRuntime` with:
- Thread pool for source drivers (1 thread per source)
- Lock-free SPSC ring buffer for inter-thread data transfer
- Atomic shadow slots for snapshot variables
- Barrier mechanism for zip synchronization

### Task 7.5 — IR lowering for stream zip
**Role:** Analyzer Agent  
**Action:** Lower `StreamZipAST` to `SGStreamZip` IR node containing two source descriptors, two parameter bindings, and the body.

### Task 7.6 — LLVM codegen for stream zip
**Role:** CodeGen Agent  
**Action:** Generate two input read calls (one per source). Wait for both to have data. Bind parameters. Execute body. Loop.

### Task 7.7 — LLVM codegen for snapshot
**Role:** CodeGen Agent  
**Action:**
- `SnapshotDeclAST`: At scope entry, start background reader thread. Store latest value in atomic shadow slot.
- `$var` reference on snapshot: Atomic load from shadow slot (frame-locked at pulse entry).

### Task 7.8 — LLVM codegen for instant pull
**Role:** CodeGen Agent  
**Action:** Synchronous call to runtime `styio_read_latest(resource)`. Returns the current value. No frame lock (this is the explicit bypass).

### Task 7.9 — Driver singleton enforcement
**Role:** Runtime  
**Action:** Maintain a global registry of `(protocol, address) → driver_instance`. On duplicate `@file{"same.txt"}`, return the existing instance.

### Task 7.10 — Test data files
**Role:** Test Agent  
**Action:** Create all data files referenced by T7.01–T7.10 in `tests/m7/data/`.

---

## Completion Criteria

**M7 is complete when:** T7.01–T7.10 produce correct output. All M1–M6 tests still pass. The cross-source arbitrage strategy (T7.08) executes correctly.

---

## Post-M7: What Comes Next

After M7, Styio has a **fully functional single-machine stream processing compiler**. Future work:

- **Performance benchmarking** against Flink, hand-written C++, Python/Polars
- **Real resource drivers** (network sockets, databases, exchange APIs)
- **Strict mode** (AOT compilation, dead code elimination)
- **Schema declarations** (explicit state ledger)
- **`styio audit`** tool (auto-rewrite scattered state to schema)
- **Paper submission** using benchmarks from M6/M7 strategies
- **WebAssembly backend** for cross-platform deployment
- **Distributed Styio** (multi-node stream processing)
