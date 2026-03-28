# Styio Development Milestones — Index

**Date:** 2026-03-28  
**Methodology:** Test-Case Driven Development. Every milestone defines its acceptance tests FIRST.

**Ambiguities:** See [`docs/Logic-Conflicts.md`](../Logic-Conflicts.md) for overloaded symbols, milestone ordering tensions, and gaps between design and the current compiler.

---

## Overview

The Styio compiler has a large gap between its current implementation and the language design goals. This plan bridges that gap through 7 incremental milestones. Each milestone produces a **self-contained, testable compiler** that can execute increasingly complex programs.

```
Current State                                          Target State
────────────────                                       ──────────────
Lexer: partial                                         Full symbol lexer
Parser: basic expressions, #func, >_, @               Full EBNF grammar
TypeInfer: +,-,*,/ on int/float only                   Full type system + @
IR Lowering: primitives + binop only                   All constructs
CodeGen: constants + binop + bind + stub func          Full LLVM emission
JIT: runs main, always returns 0                       Returns computed values
                                                       State containers, streams,
                                                       wave operators, drivers
```

---

## Milestone Dependency Chain

```
M1 ──▶ M2 ──▶ M3 ──▶ M4 ──▶ M5 ──▶ M6 ──▶ M7
│       │       │       │       │       │       │
│       │       │       │       │       │       └─ Stream sync & drivers
│       │       │       │       │       └─ State containers & frame lock
│       │       │       │       └─ Resources & I/O
│       │       │       └─ Wave operators & @ algebra
│       │       └─ Control flow (match, break, continue)
│       └─ Functions (call, return, closures)
└─ Foundation (expressions, print, types end-to-end)
```

Each milestone builds on the previous one. No milestone may break tests from earlier milestones.

---

## Milestones

| ID | Name | Goal | Acceptance Criteria |
|----|------|------|-------------------|
| **M1** | Foundation | Expressions, print, bindings execute correctly end-to-end | 20+ test programs compute and print correct values |
| **M2** | Functions | Define, call, and return from functions with typed parameters | Functions with args produce correct return values |
| **M3** | Control Flow | Pattern matching, loops, break, continue | Fibonacci, FizzBuzz expressible and executable |
| **M4** | Wave & Absence | `<~`, `~>`, `@` propagation, `\|` fallback | Conditional routing and missing-value algebra work |
| **M5** | Resources & I/O | `@file`, `<-`, `>>` file reading, `<<` writing | Read a CSV, process rows, write output |
| **M6** | State & Streams | `@[n]`, `$var`, `[<<,n]`, pulse frame lock, `[avg,n]` intrinsics | Golden Cross strategy compiles (mock data) |
| **M7** | Multi-Stream | `&` zip, `<< @res` snapshot, resource drivers | Cross-source arbitrage strategy compiles |

---

## Roles

| Role | Responsibility | Tools |
|------|---------------|-------|
| **Lexer Agent** | Token definitions, tokenizer implementation | Token.hpp, Tokenizer.cpp, .clang-format |
| **Parser Agent** | Recursive descent parser, AST node definitions | Parser.cpp, AST.hpp, ASTDecl.hpp |
| **Analyzer Agent** | Type inference, semantic checks, IR lowering | TypeInfer.cpp, ToStyioIR.cpp, ASTAnalyzer.hpp |
| **CodeGen Agent** | LLVM IR generation, JIT execution | CodeGen*.cpp, GetType*.cpp, CodeGenVisitor.hpp |
| **Test Agent** | Write .styio test fixtures, GoogleTest cases, verify output | tests/, styio_test.cpp |
| **Doc Agent** | Keep design docs and AGENT-SPEC in sync with changes | docs/ |

Multiple roles may be filled by the same agent. All agents must follow `docs/AGENT-SPEC.md`.

---

## Files

| Document | Content |
|----------|---------|
| `00-Milestone-Index.md` | This file (overview) |
| `M1-Foundation.md` | Milestone 1: Expressions, print, bindings |
| `M2-Functions.md` | Milestone 2: Function definition, calls, returns |
| `M3-ControlFlow.md` | Milestone 3: Match, loops, break, continue |
| `M4-WaveAndAbsence.md` | Milestone 4: Wave operators, @ algebra |
| `M5-ResourcesIO.md` | Milestone 5: File/resource access |
| `M6-StateAndStreams.md` | Milestone 6: State containers, intrinsics |
| `M7-MultiStream.md` | Milestone 7: Stream sync, drivers |
