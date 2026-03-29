# Milestone 6: State Containers, Intrinsics & Frame Lock

**文档作用：** M6 **验收测试与任务分解**；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md)。Golden Cross **设计叙述**见 [`Styio-Resource-Topology.md`](../../Styio-Resource-Topology.md) §8。

**Depends on:** M5 (Resources & I/O)  
**Goal:** State containers (`@[n]`, `@[var=init]`), shadow references (`$var`), history probes (`[<<,n]`), pulse frame lock, anonymous ledger, and compiler intrinsics (`[avg,n]`, `[max,n]`) work end-to-end. The Golden Cross strategy compiles and runs with mock data.

**Design evolution:** A **target** syntax for global resources (`@name : [|n|] := { … }`, **`-> $name`** for writes) is documented in [`../../Styio-Resource-Topology.md`](../../Styio-Resource-Topology.md). It is **not** what the compiler parses today; acceptance tests below remain the **implemented** surface form.

---

## Acceptance Tests

### T6.01 — Scalar accumulator (scan)

```
// File: tests/m6/t01_scan.styio
// Expected stdout: 6
[1, 2, 3] >> #(x) => {
    @[total = 0](sum = $total + x)
}
>_(sum)
```

### T6.02 — Running maximum

```
// File: tests/m6/t02_running_max.styio
// Expected stdout: 5
[3, 1, 5, 2, 4] >> #(x) => {
    @[max_val = 0](high = (x > $max_val) <~ x | $max_val)
}
>_(high)
```

### T6.03 — Window buffer (moving average)

```
// File: tests/m6/t03_window_avg.styio
// Expected stdout: 4
// Moving average of [1,2,3,4,5] with window 3: last value = (3+4+5)/3 = 4
// But we only have 5 elements and window 3, so:
// tick 1: @ (cold)
// tick 2: @ (cold)
// tick 3: 2 (avg of 1,2,3)
// tick 4: 3 (avg of 2,3,4)
// tick 5: 4 (avg of 3,4,5)
// Last emitted value: 4
[1, 2, 3, 4, 5] >> #(p) => {
    @[3](ma = p[avg, 3])
}
>_(ma)
```

### T6.04 — History probe `[<<, 1]`

```
// File: tests/m6/t04_history.styio
// Expected stdout: 4
// Last iteration: p=5, $price[<<,1]=4
[1, 2, 3, 4, 5] >> #(p) => {
    @[5](price = p)
}
>_($price[<<, 1])
```

### T6.05 — Pulse frame lock (consistent reads)

```
// File: tests/m6/t05_frame_lock.styio
// Expected stdout: true
// Within a single pulse, $val read twice must be identical
result = true
[1, 2, 3] >> #(x) => {
    @[val = 0](v = $val + x)
    a = $val
    b = $val
    result = result && (a == b)
}
>_(result)
```

### T6.06 — Cold start (@ during warmup)

```
// File: tests/m6/t06_cold_start.styio
// Expected stdout: 2
// Window of 3 on [1,2,3]: first 2 ticks emit @, third emits 2
count = 0
[1, 2, 3] >> #(p) => {
    @[3](ma = p[avg, 3])
    (ma == @) ~> { count += 1 } | @
}
>_(count)
```

### T6.07 — Multiple state containers

```
// File: tests/m6/t07_multi_state.styio
// Expected stdout:
// 15
// 5
[1, 2, 3, 4, 5] >> #(x) => {
    @[sum_acc = 0](total = $sum_acc + x)
    @[max_acc = 0](high = (x > $max_acc) <~ x | $max_acc)
}
>_(total)
>_(high)
```

### T6.08 — Rolling maximum intrinsic

```
// File: tests/m6/t08_rolling_max.styio
// Expected stdout: 5
[3, 1, 5, 2, 4] >> #(p) => {
    @[5](mx = p[max, 5])
}
>_(mx)
```

### T6.09 — Anonymous ledger (scattered declarations)

```
// File: tests/m6/t09_anon_ledger.styio
// Expected stdout: 10
// Verify that scattered @[...] declarations compile correctly
# process := (data) => {
    @[a = 0](x = $a + data)
}
[1, 2, 3, 4] >> #(d) => {
    process(d)
}
>_(x)
```

### T6.10 — Golden Cross Strategy (mock data)

```
// File: tests/m6/t10_golden_cross.styio
// Setup: tests/m6/data/prices.txt containing 30 numbers (simulated prices)
// Expected stdout: "Buy at: <price>" when golden cross detected, or no output if not
@file{"tests/m6/data/prices.txt"} >> #(p) => {
    # get_ma := (src, n) => src[avg, n]

    @[5 ](ma5  = get_ma(p, 5 ))
    @[20](ma20 = get_ma(p, 20))

    is_golden = ($ma5 > $ma20) && ($ma5[<<, 1] <= $ma20[<<, 1])

    # order_logic := (price) => { >_("Buy at: " + price) }

    (is_golden) ~> order_logic(p) | @
}
```

---

## Implementation Tasks

### Task 6.1 — New tokens and AST nodes for state system
**Role:** Lexer + Parser Agent  
**Action:**
- Token: `@[` as compound token (or parse `@` then `[` with lookahead)
- Token: `$` prefix for state references
- AST: `StateDeclAST` (`@[param](assignment)`), `StateRefAST` (`$var`), `HistoryProbeAST` (`$var[<<, n]`)

### Task 6.2 — Parse `@[n](var = expr)` and `@[var = init](expr)`
**Role:** Parser Agent  
**Action:** After `@[`, determine if contents is a number (window) or `ident = expr` (accumulator). Then parse the parenthesized assignment.

### Task 6.3 — Parse `$var` and `$var[<<, n]`
**Role:** Parser Agent  
**Action:** `$` followed by identifier → `StateRefAST`. If followed by `[<<,`, parse the probe depth.

### Task 6.4 — State analysis pass (anonymous ledger)
**Role:** Analyzer Agent  
**New file:** `src/StyioAnalyzer/StateAnalyzer.hpp` / `.cpp`  
**Action:** Walk the AST, collect all `StateDeclAST` nodes. Compute memory layout:
- Window buffers: `n * sizeof(T)` + cursor + count
- Accumulators: `sizeof(T)` + initialized flag
Assign each a fixed offset in the contiguous ledger. Record in a `StateLedger` structure.

### Task 6.5 — IR lowering for state nodes
**Role:** Analyzer Agent  
**Action:** `StateDeclAST` → new `SGStateDecl` IR node carrying offset, size, and update expression. `StateRefAST` → `SGStateRef` carrying offset. `HistoryProbeAST` → `SGHistoryProbe` carrying offset and depth.

### Task 6.6 — LLVM codegen: ledger allocation
**Role:** CodeGen Agent  
**Action:** At the top of `main`, allocate a single `alloca` of `ledger_total_size` bytes. Store the pointer. All state reads/writes use `GEP` from this base pointer.

### Task 6.7 — LLVM codegen: state update within loops
**Role:** CodeGen Agent  
**Action:** At each pulse:
1. **Frame lock:** Copy current state values to frame-local snapshots
2. Execute body (all `$var` reads go to snapshots)
3. Compute new values
4. Write new values back to the ledger

### Task 6.8 — LLVM codegen: history probe
**Role:** CodeGen Agent  
**Action:** `$var[<<, n]` → compute `(cursor - n + buf_size) % buf_size` on the ring buffer, then load.

### Task 6.9 — Implement `[avg, n]` intrinsic
**Role:** CodeGen Agent  
**Action:** Inline the O(1) sliding sum algorithm from `Styio-StdLib-Intrinsics.md` §2.1. State: ring buffer + sum + cursor + count.

### Task 6.10 — Implement `[max, n]` intrinsic
**Role:** CodeGen Agent  
**Action:** Inline the monotonic deque algorithm from `Styio-StdLib-Intrinsics.md` §2.2.

### Task 6.11 — Test data for Golden Cross
**Role:** Test Agent  
**Action:** Create `tests/m6/data/prices.txt` with 30 price values that produce at least one golden cross (5-period MA crosses above 20-period MA around line 25).

---

## Completion Criteria

**M6 is complete when:** T6.01–T6.10 produce correct output. The Golden Cross strategy (T6.10) compiles and runs. All M1–M5 tests still pass.

---

## Critical Note

This milestone is the **most architecturally significant**. The state ledger, frame lock, and intrinsic inlining form the foundation for Styio's performance claims. The implementation must be:
- **Contiguous:** Single allocation for all state
- **Deterministic:** Frame lock guarantees must be provable
- **Zero-allocation:** No heap allocation during pulse processing
