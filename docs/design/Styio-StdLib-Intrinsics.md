# Styio Standard Library — Compiler Intrinsics Specification

**文档作用：** 编译器内建 **`[op, n]`** 等算法的规范（行为、复杂度、`@` 处理、代码生成提示）；**不**重复语言总语义（见 `Styio-Language-Design.md`）。

**Version:** 1.0-draft  
**Date:** 2026-03-28  
**Target Audience:** Compiler developers, LLVM CodeGen implementors, quantitative strategy researchers

---

## 1. Design Principles

All intrinsics in this document are **compiler-inlined**. They are not function calls — the compiler recognizes the `[op, n]` selector syntax and emits optimized LLVM IR directly.

### 1.1 Zero Heap Allocation

Every intrinsic operates on **pre-allocated** memory. Ring buffers, accumulators, and queues are sized at compile time and placed on the stack or in the global state ledger. No `malloc`/`new` at runtime.

### 1.2 Honest Missing (`@`) Handling

Default behavior when input is `@`:
- **Aggregators:** Skip the pulse. Do not update internal state. Do not emit downstream.
- **Signal generators:** Emit `false` (not `@`) — signals should never be uncertain.
- **Sanitizers:** Explicitly handle `@` as their primary purpose.

### 1.3 Inlining Requirement

All intrinsics must be marked `alwaysinline` in LLVM IR. The goal is zero function-call overhead.

---

## 2. Time-Series Aggregators

These operate on streaming data within a **sliding window** of size `n`.

### 2.1 Simple Moving Average: `[avg, n]`

**Syntax:**

```
ma20 = price[avg, 20]
```

**Algorithm:** O(1) Incremental Sliding Sum

```
State: ring_buffer[n], sum, cursor, count

On new value x:
    if x == @: skip (do not advance cursor)
    old = ring_buffer[cursor]
    ring_buffer[cursor] = x
    sum = sum - old + x
    cursor = (cursor + 1) % n
    count = min(count + 1, n)
    if count < n: emit @    // cold start
    else: emit sum / n
```

**Memory:** `n * sizeof(T) + 3 scalars`

**LLVM hint:** The division by `n` can be replaced with multiplication by `1.0/n` (precomputed constant) for floating-point types.

---

### 2.2 Rolling Maximum: `[max, n]`

**Syntax:**

```
high14 = price[max, 14]
```

**Algorithm:** O(1) amortized Monotonic Deque

```
State: ring_buffer[n], deque_indices[n], head, tail, cursor

On new value x:
    if x == @: skip
    // Remove expired indices
    while head != tail && deque_indices[head] <= cursor - n:
        head++
    // Remove smaller elements from back
    while head != tail && ring_buffer[deque_indices[tail-1]] <= x:
        tail--
    deque_indices[tail++] = cursor
    ring_buffer[cursor % n] = x
    cursor++
    emit ring_buffer[deque_indices[head]]
```

**Memory:** `2 * n * sizeof(T) + 4 scalars`

---

### 2.3 Rolling Minimum: `[min, n]`

Identical to `[max, n]` with reversed comparison (`>=` instead of `<=` in the deque pruning step).

---

### 2.4 Rolling Standard Deviation: `[std, n]`

**Syntax:**

```
vol20 = price[std, 20]
```

**Algorithm:** Sliding Welford's (windowed online variance)

For a fixed-size sliding window, we maintain:

```
State: ring_buffer[n], mean, m2, cursor, count

On new value x:
    if x == @: skip
    old = ring_buffer[cursor % n]
    ring_buffer[cursor % n] = x
    
    if count < n:
        // Growing phase: standard Welford update
        count++
        delta = x - mean
        mean += delta / count
        delta2 = x - mean
        m2 += delta * delta2
    else:
        // Sliding phase: remove old, add new
        old_mean = mean
        mean += (x - old) / n
        m2 += (x - old) * ((x - mean) + (old - old_mean))
    
    cursor++
    if count < n: emit @
    else: emit sqrt(m2 / (n - 1))    // sample std dev
```

**Memory:** `n * sizeof(T) + 4 scalars`

**Precision note:** For `f32` with large `n` (>1000), consider promoting internal accumulators to `f64` to avoid catastrophic cancellation.

---

### 2.5 Exponential Moving Average: `[ema, n]`

**Syntax:**

```
ema20 = price[ema, 20]
```

**Algorithm:** Wilder's Smoothing (no ring buffer needed)

```
State: ema_val, initialized (bool)
Constant: alpha = 2.0 / (n + 1)

On new value x:
    if x == @: skip
    if !initialized:
        ema_val = x
        initialized = true
    else:
        ema_val = alpha * x + (1 - alpha) * ema_val
    emit ema_val
```

**Memory:** `2 scalars` (extremely lightweight)

---

## 3. Signal Generators

These produce **boolean** streams for trading logic.

### 3.1 Crossover Detection: `cross_over(a, b)` / `cross_under(a, b)`

**Syntax:**

```
buy_signal = cross_over($ma5, $ma20)
sell_signal = cross_under($ma5, $ma20)
```

**Expansion:** The compiler desugars these into native Styio:

```
// cross_over(a, b) expands to:
(a > b) && (a[<<, 1] <= b[<<, 1])

// cross_under(a, b) expands to:
(a < b) && (a[<<, 1] >= b[<<, 1])
```

**Requirement:** Both `a` and `b` must be `$`-prefixed state references (they need history). If either operand lacks `[<<, 1]` capability, the compiler emits an error.

**Output:** Strict `bool` — never `@`.

---

### 3.2 Relative Strength Index: `[rsi, n]`

**Syntax:**

```
rsi14 = price[rsi, 14]
```

**Algorithm:** Wilder's Smoothed RSI

```
State: avg_gain, avg_loss, prev_price, initialized, warm_count
Constant: period = n

On new value x:
    if x == @: skip
    if prev_price == @:
        prev_price = x
        emit @
        return
    
    change = x - prev_price
    prev_price = x
    gain = (change > 0) ? change : 0
    loss = (change < 0) ? -change : 0
    
    if warm_count < period:
        avg_gain += gain
        avg_loss += loss
        warm_count++
        if warm_count == period:
            avg_gain /= period
            avg_loss /= period
        else:
            emit @
            return
    else:
        avg_gain = (avg_gain * (period - 1) + gain) / period
        avg_loss = (avg_loss * (period - 1) + loss) / period
    
    if avg_loss == 0: emit 100.0
    else:
        rs = avg_gain / avg_loss
        emit 100.0 - (100.0 / (1.0 + rs))
```

**Memory:** `5 scalars`

---

## 4. Sanitization & Recovery Operators

### 4.1 Validity Check: `is_valid(x)`

**Syntax:**

```
(is_valid(p)) ~> process(p) | @
```

**LLVM IR:** Single `icmp` or metadata flag test. Emits `i1` (boolean).

---

### 4.2 Fallback: `x | default`

**Syntax:**

```
safe = price | $last_valid
```

**LLVM IR:** Compiles to a single `select` instruction (no branch, no phi node). If left operand's undefined flag is set, return right operand.

---

### 4.3 Diagnostic Extract: `x ?? reason`

**Syntax:**

```
msg = last_result ?? reason
```

**Runtime:** In debug mode, extracts the `ReasonCode` and `SourceLocation` from the tainted `@` metadata. In release mode, this is a no-op that returns an empty string.

---

## 5. Accumulator Patterns (Scan)

These are not intrinsics per se but common patterns enabled by `@[var = init]`.

### 5.1 Running Sum

```
@[total = 0.0](total_vol = $total + volume)
```

### 5.2 Running Maximum

```
@[max_p = 0.0](high_price = (p > $max_p) <~ p | $max_p)
```

### 5.3 Running Count

```
@[count = 0](break_times = (signal) <~ $count + 1 | $count)
```

### 5.4 VWAP (Volume-Weighted Average Price)

```
@[sum_pv = 0.0](total_pv = $sum_pv + (price * volume))
@[sum_vol = 0.0](total_vol = $sum_vol + volume)
vwap = total_pv / total_vol
```

---

## 6. Execution Primitives

These interact with the resource driver layer.

### 6.1 Order Dispatch

```
(signal) ~> @order{"Limit", price, qty}
```

Compiles to an **async non-blocking** call to the trade execution driver. Must not block the pulse clock.

### 6.2 State Snapshot

```
(time == 15:00) ~> GlobalState.snapshot() | @
```

Triggers `mmap`-based or copy-on-write serialization of the entire state ledger to disk.

---

## 7. LLVM CodeGen Constraints

### 7.1 Auto-Vectorization

For window operations over `@[n]` buffers where `n > 8`, the generated LLVM loop must:
- Avoid loop-carried dependencies where possible
- Use `<n x float>` vector types when the operation is element-wise
- Emit `llvm.vector.reduce.*` intrinsics for reductions

### 7.2 Branch Prediction

For wave operators (`<~`, `~>`):
- Prefer LLVM `select` instruction over `br` + `phi` when both branches are pure expressions
- Use `br` only when branches have side effects (e.g., `~>` dispatching to different drivers)

### 7.3 Memory Layout

State containers in the anonymous ledger must be allocated in a **single contiguous block**:

```
[Ring_MA5 (5*f64)] [Ring_MA20 (20*f64)] [Scalar_total (f64)] [Scalar_count (i64)] ...
```

Base address is stored in a single pointer. All `$var` accesses compile to `base_ptr + constant_offset`, enabling the CPU prefetcher to work effectively.

---

## Appendix: Consultant's Additional Notes

### On the `[op, n]` Extensibility Model

The current design hardcodes intrinsic names (`avg`, `max`, `std`, `rsi`) in the parser's selector mode. This is performant but inflexible. Consider a two-tier approach:

1. **Tier 1 (Compiler Intrinsics):** The operators listed in this document. Recognized by the parser, inlined by codegen. Fixed set.
2. **Tier 2 (User-Defined Selectors):** Allow users to register custom selector functions that the compiler can optionally inline via LTO. Syntax could be:

```
# [bollinger, n, k] := (src) => {
    mu = src[avg, n]
    sigma = src[std, n]
    <| (mu + k * sigma, mu - k * sigma)
}
```

This would let the quant community build a shared library of indicators without forking the compiler.

### On Cold Start Behavior

All windowed intrinsics emit `@` during their warm-up period (first `n-1` pulses). This is consistent with the "honest missing" philosophy but means the first ~20 seconds of a 20-period MA on 1-second data will produce no output. For live trading, consider:

- A `warm_from` directive that pre-loads historical data into the buffer before the main loop starts
- Integration with the resource driver's "replay" capability to fast-forward through historical ticks

### On Numerical Stability

The sliding Welford algorithm for `[std, n]` can accumulate floating-point drift over millions of ticks. For production systems running 24/7, consider periodic "ground-truth recalculation" — every `N` ticks, recompute from the full ring buffer. This adds O(n) cost once every N ticks but bounds the maximum drift.
