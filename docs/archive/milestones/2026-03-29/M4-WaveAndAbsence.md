# Milestone 4: Wave Operators & Algebraic Absence

**Purpose:** M4 **验收测试与任务分解**；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md)。

**Last updated:** 2026-04-08

**Depends on:** M3 (Control Flow)  
**Goal:** Conditional routing (`<~`, `~>`), the `@` (Undefined) value, `@` propagation algebra, `|` fallback, and `??` diagnostic extraction work end-to-end.

---

## Acceptance Tests

### T4.01 — Conditional merge `<~`

```
// File: tests/m4/t01_wave_merge.styio
// Expected stdout: 10
a = 10
b = 5
result = (a > b) <~ a | b
>_(result)
```

### T4.02 — Conditional merge (false branch)

```
// File: tests/m4/t02_wave_false.styio
// Expected stdout: 5
a = 3
b = 5
result = (a > b) <~ a | b
>_(result)
```

### T4.03 — Conditional dispatch `~>`

```
// File: tests/m4/t03_wave_dispatch.styio
// Expected stdout: big
x = 100
(x > 50) ~> >_("big") | >_("small")
```

### T4.04 — Dispatch to void `@`

```
// File: tests/m4/t04_dispatch_void.styio
// Expected stdout:
// (no output — signal is false, dispatch goes to @)
x = 1
(x > 50) ~> >_("big") | @
```

### T4.05 — `@` literal

```
// File: tests/m4/t05_undefined.styio
// Expected stdout: true
x = @
>_(x == @)
```

### T4.06 — `@` arithmetic propagation

```
// File: tests/m4/t06_at_propagate.styio
// Expected stdout: @
x = @
>_(x + 10)
```

### T4.07 — `@` logic propagation

```
// File: tests/m4/t07_at_logic.styio
// Expected stdout: @
>_(true && @)
```

### T4.08 — `|` fallback recovery

```
// File: tests/m4/t08_fallback.styio
// Expected stdout: 42
x = @
safe = x | 42
>_(safe)
```

### T4.09 — `|` fallback (no `@`)

```
// File: tests/m4/t09_fallback_noop.styio
// Expected stdout: 10
x = 10
safe = x | 42
>_(safe)
```

### T4.10 — Guard selector `[?, cond]`

```
// File: tests/m4/t10_guard.styio
// Expected stdout: @
x = 3
result = x[?, x > 5]
>_(result)
```

### T4.11 — Guard selector (passes)

```
// File: tests/m4/t11_guard_pass.styio
// Expected stdout: 10
x = 10
result = x[?, x > 5]
>_(result)
```

### T4.12 — Equality probe `[?=, val]`

```
// File: tests/m4/t12_eq_probe.styio
// Expected stdout: 3
x = 3
result = x[?=, 3]
>_(result)
```

### T4.13 — Chain: guard + fallback

```
// File: tests/m4/t13_chain.styio
// Expected stdout: 0
x = 3
safe = x[?, x > 5] | 0
>_(safe)
```

### T4.14 — Wave with function gate

```
// File: tests/m4/t14_func_gate.styio
// Expected stdout: ALERT
# alert := (msg: string) => >_(msg)
is_critical = true
(is_critical) ~> alert("ALERT") | @
```

### T4.15 — `@` propagation chain

```
// File: tests/m4/t15_deep_propagate.styio
// Expected stdout: safe
a = @
b = a + 1
c = b * 2
result = c | "safe"
>_(result)
```

---

## Implementation Tasks

### Task 4.1 — New tokens: `<~`, `~>`, `??`
**Role:** Lexer Agent  
**Files:** Token.hpp, Tokenizer.cpp  
**Action:** Add `TOK_WAVE_LEFT` (`<~`), `TOK_WAVE_RIGHT` (`~>`), `TOK_DBQUESTION` (`??`). Ensure maximal munch: `<~` takes priority over `<` then `~`.

### Task 4.2 — AST nodes: WaveExprAST, UndefinedAST, FallbackExprAST, GuardSelectorAST, EqProbeAST
**Role:** Parser Agent  
**Files:** Token.hpp, ASTDecl.hpp, AST.hpp  

### Task 4.3 — Parse wave expressions
**Role:** Parser Agent  
**File:** Parser.cpp  
**Action:** After parsing a parenthesized condition, if next token is `<~` or `~>`, parse true_expr `|` false_expr.

### Task 4.4 — Parse `@` as literal value
**Role:** Parser Agent  
**File:** Parser.cpp  
**Action:** When `@` appears alone (not followed by `[` or identifier), return `UndefinedAST`.

### Task 4.5 — Parse `[?, cond]` and `[?=, val]` selectors
**Role:** Parser Agent  
**File:** Parser.cpp  
**Action:** In index/selector parsing, detect `?` or `?=` after `[`.

### Task 4.6 — Parse `|` as fallback operator
**Role:** Parser Agent  
**File:** Parser.cpp  
**Action:** Lowest-precedence binary operator. Left operand is any expression, right is fallback value. Only triggers when left might be `@`.

### Task 4.7 — Implement `@` representation in LLVM
**Role:** CodeGen Agent  
**File:** CodeGenG.cpp  
**Action:** Represent `@` as a tagged value. Options: (a) Use NaN boxing for floats and a sentinel value for integers (e.g., `INT64_MIN`). (b) Use a struct `{value, is_undefined: i1}`. Recommend option (b) for correctness — `@` is never a valid number.

### Task 4.8 — Implement `@` propagation in binary ops
**Role:** CodeGen Agent  
**File:** CodeGenG.cpp  
**Action:** Before computing `a + b`, check if either operand's `is_undefined` flag is set. If so, return `@` without computing. Use `select` instruction for branchless implementation.

### Task 4.9 — Implement wave operator codegen
**Role:** CodeGen Agent  
**File:** CodeGenG.cpp  
**Action:** `<~`: `select(cond, true_val, false_val)`. `~>`: `br(cond, true_bb, false_bb)` where branches may have side effects.

### Task 4.10 — Implement fallback `|` codegen
**Role:** CodeGen Agent  
**File:** CodeGenG.cpp  
**Action:** Check left operand's `is_undefined` flag. If set, return right operand. Use `select`.

---

## Completion Criteria

**M4 is complete when:** T4.01–T4.15 produce correct output. All M1–M3 tests still pass.

---

## Critical Design Decision: `@` Representation

This milestone requires a fundamental decision about how `@` is represented at the LLVM level. This MUST be decided before implementation begins.

**Recommended:** Pair representation `{T value, i1 is_undef}` for every value that might be `@`. The compiler can optimize away the flag when static analysis proves a value is never `@` (common in non-stream code).
