# Milestone 2: Functions — Define, Call, Return

**文档作用：** M2 **验收测试与任务分解**；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md)。

**Depends on:** M1 (Foundation)  
**Goal:** Users can define named functions with typed parameters, call them, and use their return values.

---

## Current State (What's Broken)

1. **`SGFunc` argument binding is commented out** — function parameters are not wired to `named_values`/`mutable_variables`, so args can't be used in the body
2. **`FuncCallAST` → StyioIR** is stubbed (`SGConstInt(0)`) — calls never reach LLVM
3. **`SGCall` LLVM codegen** returns `i64 0` — even if the IR node existed, it wouldn't emit a real call
4. **`ReturnAST` → StyioIR** is stubbed
5. **`FunctionAST` / `SimpleFuncAST` → StyioIR** are stubbed — function bodies don't lower
6. **`main` always returns 0** — the last expression's value is discarded
7. **Recursive calls** are untested
8. **Multi-parameter functions** — parser handles them but IR/codegen path is broken

---

## Acceptance Tests

### T2.01 — Simple function definition and call

```
// File: tests/m2/t01_simple_func.styio
// Expected stdout: 7
# add := (a: i32, b: i32) => a + b
>_(add(3, 4))
```

### T2.02 — Function with explicit return type

```
// File: tests/m2/t02_typed_return.styio
// Expected stdout: 6
# mul : i32 = (a: i32, b: i32) => a * b
>_(mul(2, 3))
```

### T2.03 — Function with block body and `<|`

```
// File: tests/m2/t03_block_body.styio
// Expected stdout: 10
# compute := (x: i32) => {
    y = x * 2
    <| y
}
>_(compute(5))
```

### T2.04 — Function calling another function

```
// File: tests/m2/t04_func_chain.styio
// Expected stdout: 25
# square := (x: i32) => x * x
# add_one := (x: i32) => x + 1
>_(square(add_one(4)))
```

### T2.05 — Float function

```
// File: tests/m2/t05_float_func.styio
// Expected stdout: 6.28
# double_it := (x: f64) => x * 2.0
>_(double_it(3.14))
```

### T2.06 — Function with multiple statements

```
// File: tests/m2/t06_multi_stmt.styio
// Expected stdout: 30
# calc := (a: i32, b: i32) => {
    sum = a + b
    product = a * b
    <| sum + product
}
>_(calc(5, 5))
```

### T2.07 — Function with no parameters

```
// File: tests/m2/t07_no_params.styio
// Expected stdout: 42
# answer := () => 42
>_(answer())
```

### T2.08 — Recursive function (factorial)

```
// File: tests/m2/t08_factorial.styio
// Expected stdout: 120
# fact := (n: i32) => {
    n ?= {
        0 => { <| 1 }
        _ => { <| n * fact(n - 1) }
    }
}
>_(fact(5))
```

*Note: T2.08 depends on `?=` from M3. Mark as **stretch goal** for M2; full support in M3.*

### T2.09 — Function used in binding

```
// File: tests/m2/t09_bind_call.styio
// Expected stdout: 15
# triple := (x: i32) => x * 3
result = triple(5)
>_(result)
```

### T2.10 — Nested function definition

```
// File: tests/m2/t10_nested.styio
// Expected stdout: 8
# outer := (x: i32) => {
    # inner := (y: i32) => y + 1
    <| inner(x) + inner(x + 1)
}
>_(outer(3))
```

---

## Implementation Tasks

### Task 2.1 — Implement FunctionAST → StyioIR lowering
**Role:** Analyzer Agent  
**File:** `src/StyioAnalyzer/ToStyioIR.cpp`  
**Action:** Lower `FunctionAST` and `SimpleFuncAST` to `SGFunc` with parameter list and body properly lowered. Body must go through `toStyioIR` recursively.  
**Verify:** `--styio-ir` shows function IR nodes for T2.01.

### Task 2.2 — Fix SGFunc argument binding in LLVM codegen
**Role:** CodeGen Agent  
**File:** `src/StyioCodeGen/CodeGenG.cpp`  
**Action:** Uncomment and fix the argument binding block in `SGFunc::toLLVMIR`. For each parameter: create an `alloca`, store the LLVM function argument into it, and register in `named_values`/`mutable_variables`.  
**Verify:** T2.01 — function body can access `a` and `b`.

### Task 2.3 — Implement FuncCallAST → StyioIR lowering
**Role:** Analyzer Agent  
**File:** `src/StyioAnalyzer/ToStyioIR.cpp`  
**Action:** Lower `FuncCallAST` to `SGCall` with function name and list of lowered argument expressions.  
**Verify:** `--styio-ir` shows call IR for T2.01.

### Task 2.4 — Implement SGCall LLVM codegen
**Role:** CodeGen Agent  
**File:** `src/StyioCodeGen/CodeGenG.cpp`  
**Action:** Look up the LLVM function by name in the module, lower each argument expression, call `CreateCall`. Return the call result.  
**Verify:** T2.01 through T2.07 produce correct stdout.

### Task 2.5 — Implement `<|` (Yield/Return)
**Role:** Parser Agent + Analyzer Agent + CodeGen Agent  
**Files:** Parser.cpp (recognize `<|` as yield expression), ToStyioIR.cpp (lower to `SGReturn`), CodeGenG.cpp (`SGReturn` already partially works — verify it calls `CreateRet`).  
**Verify:** T2.03, T2.06.

### Task 2.6 — Implement implicit return (last expression)
**Role:** CodeGen Agent  
**File:** `src/StyioCodeGen/CodeGenG.cpp`  
**Action:** In `SGFunc`, if the body doesn't end with an explicit `SGReturn`, insert a `CreateRet` of the last expression's value. For `SGMainEntry`, return the last expression's value instead of always `i32 0`.  
**Verify:** T2.01, T2.02 (single-expression function bodies without explicit `<|`).

### Task 2.7 — Register functions before lowering bodies
**Role:** Analyzer Agent  
**File:** `src/StyioAnalyzer/ToStyioIR.cpp`  
**Action:** Perform a pre-pass that registers all function signatures (name + param types + return type) before lowering any bodies. This enables forward references and recursion.  
**Verify:** T2.04 (function calling another function defined later).

### Task 2.8 — Nested function scoping
**Role:** CodeGen Agent  
**File:** `src/StyioCodeGen/CodeGenG.cpp`  
**Action:** When entering a `SGFunc`, push a new scope for `named_values`. When leaving, pop it. This prevents inner functions from polluting outer scopes.  
**Verify:** T2.10.

---

## Completion Criteria

**M2 is complete when:** Tests T2.01–T2.07, T2.09, T2.10 compile and produce correct stdout. T2.08 (recursive with `?=`) is deferred to M3. All M1 tests still pass.

---

## Estimated Effort

| Task | Complexity | Dependencies |
|------|-----------|--------------|
| 2.1 Function IR lowering | Medium | M1 complete |
| 2.2 Arg binding fix | Medium | 2.1 |
| 2.3 Call IR lowering | Medium | M1 complete |
| 2.4 Call LLVM codegen | Medium | 2.3 |
| 2.5 Yield `<\|` full path | Medium | M1 complete |
| 2.6 Implicit return | Low | 2.2 |
| 2.7 Forward references | Medium | 2.1 |
| 2.8 Nested scoping | Low | 2.2 |

Tasks 2.1, 2.3, 2.5 can proceed in parallel.
