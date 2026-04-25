# Milestone 3: Control Flow — Match, Loops, Break, Continue

**Purpose:** M3 **验收测试与任务分解**；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md)。

**Last updated:** 2026-04-08

**Depends on:** M2 (Functions)  
**Goal:** Pattern matching (`?=`), infinite loops (`[...]`), conditional loops (`[...] ?(cond) >>`), break (`^^`), and continue (`>>`) work end-to-end.

---

## Acceptance Tests

### T3.01 — Simple pattern match

```
// File: tests/m3/t01_match.styio
// Expected stdout: one
x = 1
x ?= {
    1 => { >_("one") }
    2 => { >_("two") }
    _ => { >_("other") }
}
```

### T3.02 — Match as expression (with `<|`)

```
// File: tests/m3/t02_match_expr.styio
// Expected stdout: even
x = 4
label = x % 2 ?= {
    0 => { <| "even" }
    _ => { <| "odd" }
}
>_(label)
```

### T3.03 — Match with default fallthrough

```
// File: tests/m3/t03_match_default.styio
// Expected stdout: unknown
x = 99
x ?= {
    1 => { >_("one") }
    _ => { >_("unknown") }
}
```

### T3.04 — Recursive factorial (M2 stretch goal completed)

```
// File: tests/m3/t04_factorial.styio
// Expected stdout: 120
# fact := (n: i32) => {
    n ?= {
        0 => { <| 1 }
        _ => { <| n * fact(n - 1) }
    }
}
>_(fact(5))
```

### T3.05 — Collection iteration

```
// File: tests/m3/t05_for_each.styio
// Expected stdout:
// 1
// 2
// 3
[1, 2, 3] >> #(item) => {
    >_(item)
}
```

### T3.06 — Infinite loop with break

```
// File: tests/m3/t06_inf_break.styio
// Expected stdout: 5
x = 0
[...] => {
    x += 1
    x ?= {
        5 => { ^ }
        _ => { >> }
    }
}
>_(x)
```

### T3.07 — Conditional loop (while-equivalent)

```
// File: tests/m3/t07_while.styio
// Expected stdout: 10
x = 0
[...] ?(x < 10) >> {
    x += 1
}
>_(x)
```

### T3.08 — Multi-level break

```
// File: tests/m3/t08_multi_break.styio
// Expected stdout: done
[...] => {
    [...] => {
        ^^
    }
}
>_("done")
```

### T3.09 — Continue (skip iteration)

```
// File: tests/m3/t09_continue.styio
// Expected stdout:
// 1
// 3
// 5
[1, 2, 3, 4, 5] >> #(i) => {
    i % 2 ?= {
        0 => { >> }
        _ => { >_(i) }
    }
}
```

### T3.10 — FizzBuzz (comprehensive)

```
// File: tests/m3/t10_fizzbuzz.styio
// Expected stdout:
// 1
// 2
// Fizz
// 4
// Buzz
# fizzbuzz := (n: i32) => {
    n % 15 ?= {
        0 => { <| "FizzBuzz" }
        _ => {
            n % 3 ?= {
                0 => { <| "Fizz" }
                _ => {
                    n % 5 ?= {
                        0 => { <| "Buzz" }
                        _ => { <| n }
                    }
                }
            }
        }
    }
}
[1, 2, 3, 4, 5] >> #(i) => {
    >_(fizzbuzz(i))
}
```

---

## Implementation Tasks

### Task 3.1 — New tokens: `^` (break), `[...]` (infinite gen), `?(expr)`
**Role:** Lexer Agent  
**Files:** Token.hpp, Tokenizer.cpp  
**Action:** Add `BREAK_TOKEN` (count contiguous `^`), ensure `ELLIPSIS` inside `[` `]` produces `[...]` parse path, ensure `?` followed by `(` is recognizable.

### Task 3.2 — AST nodes for control flow
**Role:** Parser Agent  
**Files:** Token.hpp (StyioNodeType entries), ASTDecl.hpp, AST.hpp  
**Action:** Add: `MatchExprAST` (condition + arms + default), `MatchArmAST` (pattern + body), `InfiniteLoopAST` (body), `ConditionalLoopAST` (guard + body), `CollectionIterAST` (source + closure), `BreakAST` (depth), `ContinueAST` (depth).

### Task 3.3 — Parse `?=` match blocks
**Role:** Parser Agent  
**File:** Parser.cpp  
**Action:** When `?=` follows an expression, parse `{ pattern => block, ... _ => block }`. Return `MatchExprAST`.

### Task 3.4 — Parse `[...] => {}`, `[...] ?(cond) >> {}`, `collection >> #(i) => {}`
**Role:** Parser Agent  
**File:** Parser.cpp  
**Action:** Detect `[` `...` `]` as infinite generator. If followed by `?(`, parse guard expression. `>>` then closure.

### Task 3.5 — Parse `^` and `>>` as control statements
**Role:** Parser Agent  
**File:** Parser.cpp  
**Action:** Count contiguous `^` → `BreakAST(n)`. For `>>` as standalone (not pipe), count length → `ContinueAST(n-1)`.

### Task 3.6 — Register all new nodes in visitors
**Role:** Parser Agent  
**Files:** ToStringVisitor.hpp, ASTAnalyzer.hpp  
**Action:** Add all new AST types to visitor template lists. Implement `toString` for debugging.

### Task 3.7 — Type inference for control flow
**Role:** Analyzer Agent  
**File:** TypeInfer.cpp  
**Action:** `MatchExprAST`: infer type of each arm's `<|` expression; all must agree. Loop nodes: body has no value type. `BreakAST`/`ContinueAST`: no type.

### Task 3.8 — IR lowering for control flow
**Role:** Analyzer Agent  
**File:** ToStyioIR.cpp  
**Action:** Lower `MatchExprAST` → `SGCond` (or new `SGMatch`) with arms. Lower loops → `SGLoop` (new IR node). Lower `BreakAST` → `SGBreak(depth)`. Lower `ContinueAST` → `SGContinue(depth)`.

### Task 3.9 — LLVM codegen for match
**Role:** CodeGen Agent  
**File:** CodeGenG.cpp  
**Action:** Generate LLVM `switch` instruction for integer patterns. For `_` default, use the `default:` label. Each arm generates a basic block. If match is an expression, generate `phi` node at merge block.

### Task 3.10 — LLVM codegen for loops
**Role:** CodeGen Agent  
**File:** CodeGenG.cpp  
**Action:** Infinite loop: `entry → body_bb → br body_bb`. Conditional loop: `entry → cond_bb → br(cond, body_bb, exit_bb), body_bb → br cond_bb`. Maintain a loop stack for break/continue targets.

### Task 3.11 — LLVM codegen for break/continue
**Role:** CodeGen Agent  
**File:** CodeGenG.cpp  
**Action:** `BreakAST(n)`: look up `exit_bb` from loop stack at depth `n`, emit `br exit_bb`. `ContinueAST(n)`: look up `cond_bb` (or `body_bb` for infinite), emit `br`.

### Task 3.12 — LLVM codegen for collection iteration
**Role:** CodeGen Agent  
**File:** CodeGenG.cpp  
**Action:** Allocate counter. Loop: load element at index, bind to closure param, execute body. Increment counter. Branch if counter < length.

---

## Completion Criteria

**M3 is complete when:** T3.01–T3.10 produce correct stdout. All M1 and M2 tests still pass.
