# Styio Development Milestones — Index (Standard Streams)

**文档作用：** 本批（`2026-04-08/`）里程碑的 **总索引**：M9–M10 目标、依赖链、各 `M*.md` 路径及与设计文档的链接；**不**替代各 `M*-*.md` 中的验收细则。

**Date:** 2026-04-08  
**Last updated:** 2026-04-08  
**Status:** Frozen
**Methodology:** Test-Case Driven Development. Every milestone defines its acceptance tests FIRST.

**Documentation policy:** [`../../specs/DOCUMENTATION-POLICY.md`](../../specs/DOCUMENTATION-POLICY.md)

**Ambiguities:** See [`../../review/Logic-Conflicts.md`](../../review/Logic-Conflicts.md)

**Design plan:** See [`../../plans/Standard-Streams-Plan.md`](../../plans/Standard-Streams-Plan.md) for the original design rationale. It is a historical planning artifact, not the language SSOT.

**Definitions (SSOT):** See [`../../design/Styio-Language-Design.md`](../../design/Styio-Language-Design.md) §7.7 and §11 for the authoritative definitions of `>_`, `@stdout`, `@stderr`, `@stdin`.

---

## Overview

The Styio language has `>_()` as its only historical output primitive and no input or error
output primitives. This batch introduces a complete standard-stream model built on the `>_`
terminal device: `@stdout`, `@stderr`, `@stdin` as compiler-recognized standard-stream
resource atoms.

Frozen milestone examples use `expr -> @stdout` / `expr -> @stderr` as the canonical write
surface. The implementation also keeps `expr >> @stdout` / `expr >> @stderr` as accepted
compatibility shorthand, and that shorthand is frozen by security/five-layer tests.

```
Current State                                  Target State
────────────────                               ──────────────
Output: >_() only (implicit stdout)            >_ as terminal device + @stdout/@stderr atoms
Input:  @file{} only (no stdin)                @stdin as built-in read-only stream atom
Error:  runtime-internal only (no stderr)      stderr exposed as @stderr resource atom
>_ role: print statement only                  >_ role: first-class resource handle
```

---

## Milestone Dependency Chain

```
M1–M8 (existing)
   │
   ├── M5 (Resources & I/O) ──▶ M9 ──▶ M10
   │                             │       │
   │                             │       └─ stdin reading, direction validation
   │                             └─ >_ as device, canonical -> write, accepted >> shorthand, !() channel, @stdout/@stderr
   │
   └── M4 (Wave & Absence) ──── used in M9/M10 test compositions
```

M9 and M10 depend on M5 (resource model) and build on M1-M8 foundations.
No milestone may break tests from earlier milestones (M1-M8).

---

## Milestones

| ID | Name | Goal | Acceptance Criteria |
|----|------|------|---------------------|
| **M9** | stdout & stderr | `>_` as device, canonical `expr -> @stdout`, accepted `expr >> @stdout` shorthand, `!(expr) -> ( >_ )` for stderr, no user-authored wrappers required | T9.01–T9.12 produce correct output; M1-M8 tests still pass |
| **M10** | stdin & Direction Validation | `@stdin >> #(line) => {...}` and `(<< @stdin)` work; `@stdout/@stderr` shorthand remains write-only; direction constraints enforced; instant pull keeps current scalar `cstr -> i64` contract | T10.01–T10.10 produce correct output/errors; M1-M9 tests still pass |

---

## Roles

| Role | Responsibility | Tools |
|------|---------------|-------|
| **Parser Agent** | `>_` as value, `!()` channel selector, `<< ( >_ )` | Parser.cpp, AST.hpp, ASTDecl.hpp |
| **Analyzer Agent** | Type inference, semantic checks, inline optimization, IR lowering | TypeInfer.cpp, ToStyioIR.cpp, ASTAnalyzer.hpp |
| **CodeGen Agent** | LLVM IR generation for stdout/stderr/stdin | CodeGen*.cpp, GetType*.cpp, CodeGenVisitor.hpp |
| **Runtime Agent** | FFI functions for stderr write, stdin read | ExternLib.hpp/.cpp, StyioJIT_ORC.hpp |
| **Test Agent** | Write .styio test fixtures, golden outputs, CTest registration | tests/milestones/m9/, m10/, tests/CMakeLists.txt |
| **Doc Agent** | Keep SSOTs in sync | docs/ |

Multiple roles may be filled by the same agent. All agents must follow [`../../specs/AGENT-SPEC.md`](../../specs/AGENT-SPEC.md).

---

## Files

| Document | Content |
|----------|---------|
| `00-Milestone-Index.md` | This file (overview) |
| `M9-StdoutStderr.md` | Milestone 9: `>_` as device, @stdout, @stderr |
| `M10-Stdin.md` | Milestone 10: @stdin, direction validation |
