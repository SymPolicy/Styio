# Milestone 9: stdout & stderr

**Purpose:** M9 **验收测试与任务分解**；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md)。

**Last updated:** 2026-04-08

**Status:** Superseded draft

此文档记录了标准流早期探索阶段把 `>> @stdout/@stderr` 作为主写法的方案。冻结规格以 [`../2026-04-08/M9-StdoutStderr.md`](../2026-04-08/M9-StdoutStderr.md) 为准；当前实现仍保留 `>> @stdout/@stderr`，但只作为 accepted compatibility shorthand，canonical 写法改为 `-> @stdout/@stderr`。

**Depends on:** M8 (Topology v2)
**Goal:** Explicit standard-stream write via `expr >> @stdout` and `expr >> @stderr`, parser `@` dispatch for `stdout`/`stderr`/`stdin` keywords, `StdStreamAST` node, `SIOStdStreamWrite` IR, stderr FFI, and backward-compatible unification of `>_()` through the same codegen path.

---

## Acceptance Tests

### T9.01 — stdout string

```
// File: tests/m9/t01_stdout_string.styio
// Expected stdout:
// hello
"hello" >> @stdout
```

### T9.02 — stdout integer

```
// File: tests/m9/t02_stdout_int.styio
// Expected stdout:
// 42
42 >> @stdout
```

### T9.03 — stdout float

```
// File: tests/m9/t03_stdout_float.styio
// Expected stdout:
// 3.140000
3.14 >> @stdout
```

### T9.04 — stdout bool

```
// File: tests/m9/t04_stdout_bool.styio
// Expected stdout:
// true
true >> @stdout
```

### T9.05 — stdout variable

```
// File: tests/m9/t05_stdout_var.styio
// Expected stdout:
// 99
x = 99
x >> @stdout
```

### T9.06 — stdout format string

```
// File: tests/m9/t06_stdout_fmtstr.styio
// Expected stdout:
// value is 7
x = 7
"value is " + x >> @stdout
```

### T9.07 — stdout undefined (`@`)

```
// File: tests/m9/t07_stdout_undef.styio
// Expected stdout:
// @
@ >> @stdout
```

### T9.08 — stdout multi-expression (multiple statements)

```
// File: tests/m9/t08_stdout_multi.styio
// Expected stdout:
// first
// 2
"first" >> @stdout
2 >> @stdout
```

### T9.09 — stderr string

```
// File: tests/m9/t09_stderr_string.styio
// Expected stderr:
// error occurred
"error occurred" >> @stderr
```

### T9.10 — stderr integer

```
// File: tests/m9/t10_stderr_int.styio
// Expected stderr:
// 404
404 >> @stderr
```

### T9.11 — mixed stdout + stderr

```
// File: tests/m9/t11_mixed.styio
// Expected stdout:
// ok
// Expected stderr:
// fail
"ok" >> @stdout
"fail" >> @stderr
```

### T9.12 — legacy `>_()` backward compatibility

```
// File: tests/m9/t12_legacy_print.styio
// Expected stdout:
// hello
// 42
>_("hello")
>_(42)
```

---

## Implementation Tasks

### Task 9.1 — `StdStreamKind` enum and `StdStreamAST` node
**Role:** AST Agent
**File:** `src/StyioToken/Token.hpp`, `src/StyioAST/AST.hpp`, `src/StyioAST/ASTDecl.hpp`
**Action:** Add `StdStreamResource` to `StyioNodeType`, define `StdStreamKind { Stdin, Stdout, Stderr }`, implement `StdStreamAST` following `FileResourceAST` RAII pattern (private ctor, `Create()` factory, `getStreamKind()`, `getNodeType()`).
**Verify:** Project compiles.

### Task 9.2 — Parser `@` dispatch for `stdout` / `stderr` / `stdin`
**Role:** Parser Agent
**File:** `src/StyioParser/Parser.cpp`
**Action:** In `parse_after_at_common`, after the `"file"` name check and before the `TOK_LCURBRAC` auto-detect fallback, add checks for `"stdout"`, `"stderr"`, `"stdin"` on `context.cur_tok()->original`. Each returns `StdStreamAST::Create(kind)`. Update `parse_resource_file_atom_latest` to accept `StyioNodeType::StdStreamResource` in addition to `FileResource`.
**Verify:** Parser can tokenize `"hello" >> @stdout` without error.

### Task 9.3 — Nightly parser parallel update
**Role:** Parser Agent
**File:** `src/StyioParser/NewParserExpr.cpp`
**Action:** The nightly parser delegates to `parse_at_stmt_or_expr_latest` (line 1111) and `parse_resource_file_atom_latest` for resource atoms. Since both call `parse_after_at_common`, Task 9.2's change propagates automatically. Verify shadow gate passes.
**Verify:** Parser shadow gate for m9 tests passes.

### Task 9.4 — `SIOStdStreamWrite` IR node
**Role:** IR Agent
**File:** `src/StyioIR/GenIR/GenIR.hpp`, `src/StyioIR/IRDecl.hpp`
**Action:** Define `SIOStdStreamWrite` with `enum class Stream { Stdout, Stderr }`, `Stream stream`, `std::vector<StyioIR*> exprs`. Follow `SGResourceWriteToFile` pattern (private default ctor, public static `Create`).
**Verify:** Project compiles.

### Task 9.5 — Visitor type lists and forward declarations
**Role:** Infra Agent
**File:** `src/StyioToString/ToStringVisitor.hpp`, `src/StyioAnalyzer/ASTAnalyzer.hpp`, `src/StyioCodeGen/CodeGenVisitor.hpp`
**Action:** Add `StdStreamAST` to both AST visitor type lists. Add `SIOStdStreamWrite` to the IR codegen visitor type list. Declare `toString`, `typeInfer`, `toStyioIR`, `toLLVMType`, `toLLVMIR` methods for each new node.
**Verify:** Project compiles.

### Task 9.6 — stderr FFI
**Role:** Runtime Agent
**File:** `src/StyioExtern/ExternLib.hpp`, `src/StyioExtern/ExternLib.cpp`
**Action:** Implement `extern "C" DLLEXPORT void styio_stderr_write_cstr(const char* s)` — writes `s` to `stderr` via `std::fputs(s, stderr)`. Null-safe.
**Verify:** FFI callable from C test harness.

### Task 9.7 — JIT FFI registration
**Role:** Runtime Agent
**File:** `src/StyioJIT/StyioJIT_ORC.hpp`
**Action:** Add `{ Mangle("styio_stderr_write_cstr"), { ExecutorAddr::fromPtr(&styio_stderr_write_cstr), Callable } }` to `absoluteSymbols`.
**Verify:** JIT can resolve the new symbol.

### Task 9.8 — Analyzer IR lowering (`ResourceWriteAST` → `SIOStdStreamWrite`)
**Role:** Analyzer Agent
**File:** `src/StyioAnalyzer/ToStyioIR.cpp`
**Action:** In `toStyioIR(ResourceWriteAST*)`, before the `FileResourceAST` dynamic_cast, check for `StdStreamAST`. If target is `@stdout`, create `SIOStdStreamWrite(Stdout, {data})`. If `@stderr`, create `SIOStdStreamWrite(Stderr, {data})`. Add stub `toStyioIR(StdStreamAST*)`.
**Verify:** IR lowering produces correct `SIOStdStreamWrite` nodes.

### Task 9.9 — Codegen for `SIOStdStreamWrite(Stdout)`
**Role:** CodeGen Agent
**File:** `src/StyioCodeGen/CodeGenIO.cpp`
**Action:** Implement `toLLVMIR(SIOStdStreamWrite*)` for `Stdout` case: replicate the `SIOPrint` six-type-branch logic (bool select+puts, i32 printf %d, i64 sentinel+printf %lld, f64 printf %.6f, pointer printf %s + free, fallback cast). Each expression gets a newline.
**Verify:** T9.01–T9.08 produce correct stdout.

### Task 9.10 — Codegen for `SIOStdStreamWrite(Stderr)`
**Role:** CodeGen Agent
**File:** `src/StyioCodeGen/CodeGenIO.cpp`
**Action:** For `Stderr` case: for each expression, convert to cstr first (bool→select, i32→promotion to i64→`styio_i64_dec_cstr`, i64→sentinel check then `styio_i64_dec_cstr`, f64→`styio_f64_dec_cstr`, pointer→identity), then call `styio_stderr_write_cstr(cstr)` + newline.
**Verify:** T9.09–T9.11 produce correct stderr.

### Task 9.11 — ToString implementations
**Role:** Doc Agent
**File:** `src/StyioToString/ToString.cpp`
**Action:** Implement `toString(StdStreamAST*)` and `toString(SIOStdStreamWrite*)` with descriptive IR debug output.
**Verify:** AST/IR dump shows new nodes correctly.

### Task 9.12 — Unify `>_()` codegen path
**Role:** Analyzer Agent + CodeGen Agent
**File:** `src/StyioAnalyzer/ToStyioIR.cpp`
**Action:** Change `toStyioIR(PrintAST*)` to create `SIOStdStreamWrite(Stdout, parts)` instead of `SIOPrint(parts)`. This unifies both print paths through the same codegen. **Implement last after T9.01–T9.11 pass.**
**Verify:** All M1–M8 `>_()` tests still pass. T9.12 legacy test passes.

---

## Completion Criteria

**M9 is complete when:** T9.01–T9.12 produce correct output. All M1–M8 tests still pass. stderr output verified via `.err` golden files. Parser shadow gate for m9 passes.

---

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| T9.12 unification breaks all M1–M8 tests | Implement last; run full suite before and after. If failures, revert to dual-path. |
| stderr golden test infrastructure not in CMakeLists | Add per-test stderr capture (`2>` redirect + `cmp -s`). |
| Nightly parser shadow mismatch | Both parsers share `parse_after_at_common`; only `parse_resource_file_atom_latest` check needs update. |
