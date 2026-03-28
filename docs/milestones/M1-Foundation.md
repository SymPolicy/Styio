# Milestone 1: Foundation — Expressions, Print, Bindings

**Depends on:** Nothing (first milestone)  
**Goal:** A `.styio` program with integer/float arithmetic, variable bindings, and print statements executes end-to-end and produces correct output.

---

## Current State (What's Broken)

1. **Top-level integer/float expressions** don't parse as full binary expressions (parser's `INTEGER`/`DECIMAL` branch only consumes one literal)
2. **`PrintAST` → StyioIR** is stubbed (`SGConstInt(0)`) — print never reaches LLVM
3. **`BlockAST` → StyioIR** is stubbed — function bodies and grouped statements don't lower
4. **`StringAST` → StyioIR** is stubbed — can't print strings
5. **`ReturnAST` → StyioIR** is stubbed
6. **`main` always returns 0** regardless of program content
7. **`SGFunc` argument binding is commented out** — params not available in function body
8. **`Binary_Mod` and `Binary_Pow`** have no LLVM codegen
9. **Type inference for `%` and `**`** is missing

---

## Acceptance Tests

Each test is a `.styio` file. The test passes when the compiler produces the **exact expected output**.

### T1.01 — Integer arithmetic

```
// File: tests/m1/t01_int_arith.styio
// Expected stdout: 10
>_(3 + 7)
```

### T1.02 — Chained arithmetic with precedence

```
// File: tests/m1/t02_precedence.styio
// Expected stdout: 14
>_(2 + 3 * 4)
```

### T1.03 — Parenthesized expressions

```
// File: tests/m1/t03_parens.styio
// Expected stdout: 20
>_((2 + 3) * 4)
```

### T1.04 — Float arithmetic

```
// File: tests/m1/t04_float.styio
// Expected stdout: 3.14
>_(1.57 + 1.57)
```

### T1.05 — Mixed int/float (type promotion)

```
// File: tests/m1/t05_mixed.styio
// Expected stdout: 5.5
>_(3 + 2.5)
```

### T1.06 — Variable binding and use

```
// File: tests/m1/t06_binding.styio
// Expected stdout: 42
x = 40
y = 2
>_(x + y)
```

### T1.07 — Final binding with type annotation

```
// File: tests/m1/t07_typed_bind.styio
// Expected stdout: 100
x : i32 := 100
>_(x)
```

### T1.08 — String printing

```
// File: tests/m1/t08_string.styio
// Expected stdout: Hello, Styio!
>_("Hello, Styio!")
```

### T1.09 — Multiple print statements

```
// File: tests/m1/t09_multi_print.styio
// Expected stdout:
// 10
// 20
// 30
>_(10)
>_(20)
>_(30)
```

### T1.10 — Modulo operator

```
// File: tests/m1/t10_modulo.styio
// Expected stdout: 1
>_(10 % 3)
```

### T1.11 — Comparison operators

```
// File: tests/m1/t11_compare.styio
// Expected stdout: true
>_(5 > 3)
```

### T1.12 — Logical operators

```
// File: tests/m1/t12_logic.styio
// Expected stdout: true
>_(true && true)
```

### T1.13 — Negative numbers

```
// File: tests/m1/t13_negative.styio
// Expected stdout: -5
>_(3 - 8)
```

### T1.14 — Compound assignment

```
// File: tests/m1/t14_compound.styio
// Expected stdout: 15
x = 10
x += 5
>_(x)
```

### T1.15 — Division (integer)

```
// File: tests/m1/t15_int_div.styio
// Expected stdout: 3
>_(10 / 3)
```

### T1.16 — Division (float)

```
// File: tests/m1/t16_float_div.styio
// Expected stdout: 3.333333
>_(10.0 / 3.0)
```

### T1.17 — Power operator

```
// File: tests/m1/t17_power.styio
// Expected stdout: 8
>_(2 ** 3)
```

### T1.18 — Boolean literal

```
// File: tests/m1/t18_bool.styio
// Expected stdout: false
>_(false)
```

### T1.19 — Chained bindings

```
// File: tests/m1/t19_chain_bind.styio
// Expected stdout: 6
a = 1
b = a + 2
c = b + 3
>_(c)
```

### T1.20 — All together

```
// File: tests/m1/t20_combined.styio
// Expected stdout:
// 42
// 3.14
// Hello
x = 40 + 2
y = 1.57 * 2.0
>_(x)
>_(y)
>_("Hello")
```

---

## Implementation Tasks

### Task 1.1 — Fix top-level expression parsing
**Role:** Parser Agent  
**File:** `src/StyioParser/Parser.cpp`  
**Action:** In `parse_stmt_or_expr`, for `INTEGER` and `DECIMAL` cases, call `parse_expr` (or the binary-expression parser) instead of returning a bare literal node. This allows `1 + 2 * 3` at file top level.  
**Verify:** T1.01, T1.02, T1.03 parse into correct AST (use `--styio-ast`).

### Task 1.2 — Implement PrintAST → StyioIR lowering
**Role:** Analyzer Agent  
**File:** `src/StyioAnalyzer/ToStyioIR.cpp`  
**Action:** Lower `PrintAST` to `SIOPrint` (or a new IR node) instead of `SGConstInt(0)`. The IR node should carry the list of expressions to print.  
**Verify:** T1.08, T1.09 produce non-zero IR (use `--styio-ir`).

### Task 1.3 — Implement SIOPrint → LLVM codegen
**Role:** CodeGen Agent  
**File:** `src/StyioCodeGen/CodeGenIO.cpp`  
**Action:** Generate LLVM calls to `printf` (or a Styio runtime print function registered via `ExternLib`). Handle `i64` → `%lld`, `double` → `%f`, `string` → `%s`, `bool` → `true`/`false`.  
**Verify:** T1.01 through T1.09 produce correct stdout.

### Task 1.4 — Implement StringAST → StyioIR lowering
**Role:** Analyzer Agent  
**File:** `src/StyioAnalyzer/ToStyioIR.cpp`  
**Action:** Lower `StringAST` to `SGConstString` instead of `SGConstInt(0)`.  
**Verify:** T1.08 works.

### Task 1.5 — Implement BlockAST → StyioIR lowering
**Role:** Analyzer Agent  
**File:** `src/StyioAnalyzer/ToStyioIR.cpp`  
**Action:** Lower `BlockAST` to `SGBlock` with all child statements lowered.  
**Verify:** Multi-statement programs (T1.09, T1.19, T1.20) work.

### Task 1.6 — Implement modulo and power codegen
**Role:** CodeGen Agent  
**File:** `src/StyioCodeGen/CodeGenG.cpp`  
**Action:** Add `Binary_Mod` → `CreateSRem`/`CreateFRem` and `Binary_Pow` → call to `llvm.pow.*` intrinsic.  
**Verify:** T1.10, T1.17.

### Task 1.7 — Implement comparison and logic codegen
**Role:** CodeGen Agent  
**File:** `src/StyioCodeGen/CodeGenG.cpp`  
**Action:** Add `Greater_Than`, `Less_Than`, `Equal`, `Not_Equal`, `Logic_AND`, `Logic_OR`, `Logic_NOT` → appropriate `CreateICmp`/`CreateFCmp`/`CreateAnd`/`CreateOr` instructions.  
**Verify:** T1.11, T1.12, T1.18.

### Task 1.8 — Implement compound assignment codegen
**Role:** CodeGen Agent  
**File:** `src/StyioCodeGen/CodeGenG.cpp`  
**Action:** `Self_Add_Assign` etc. → load, compute, store back to alloca.  
**Verify:** T1.14.

### Task 1.9 — Extend type inference for mod/pow/comparison/logic
**Role:** Analyzer Agent  
**File:** `src/StyioAnalyzer/TypeInfer.cpp`  
**Action:** Add handling for `Binary_Mod`, `Binary_Pow` (same numeric rules as `*`), comparison ops (result is `bool`), logic ops (operands must be `bool`, result is `bool`).  
**Verify:** All tests type-check without errors.

### Task 1.10 — Fix Tokenizer for `**` (power)
**Role:** Lexer Agent  
**File:** `src/StyioParser/Tokenizer.cpp`  
**Action:** When `*` is seen, lookahead for another `*`. If found, emit `BINOP_POW` token. Otherwise emit `TOK_STAR`.  
**Verify:** T1.17 tokenizes `**` as a single token (use `--debug`).

---

## Completion Criteria

**M1 is complete when:** All 20 test programs (`t01` through `t20`) compile and produce the exact expected stdout output via JIT execution. No existing tests in `tests/parsing/` are broken.

---

## Estimated Effort

| Task | Complexity | Dependencies |
|------|-----------|--------------|
| 1.1 Parser fix | Low | None |
| 1.2 Print IR lowering | Medium | None |
| 1.3 Print LLVM codegen | Medium | 1.2 |
| 1.4 String IR lowering | Low | None |
| 1.5 Block IR lowering | Low | None |
| 1.6 Mod/Pow codegen | Low | 1.10 |
| 1.7 Compare/Logic codegen | Medium | None |
| 1.8 Compound assign codegen | Low | None |
| 1.9 Type inference extension | Medium | None |
| 1.10 Power tokenization | Low | None |

Tasks 1.1, 1.2, 1.4, 1.5, 1.9, 1.10 can proceed in parallel.  
Tasks 1.3 depends on 1.2. Task 1.6 depends on 1.10.
