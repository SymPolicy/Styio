# Milestone 10: stdin & Direction Validation

**Purpose:** M10 **验收测试与任务分解**；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md)。

**Last updated:** 2026-04-08

**Status:** Superseded draft

此文档记录了标准流早期探索阶段的 M10 草案。冻结规格以 [`../2026-04-08/M10-Stdin.md`](../2026-04-08/M10-Stdin.md) 为准；当前实现仍保留 `>> @stdout/@stderr` compatibility shorthand，但 canonical 写法与正式 fixture 命名均以 2026-04-08 批次为准。

**Depends on:** M9 (stdout & stderr)
**Goal:** Stdin line iteration via `@stdin >> #(line) => { body }`, instant stdin pull `(<< @stdin)`, direction validation (semantic errors for write-to-stdin and read-from-stdout/stderr), and handle acquire rejection on standard streams.

---

## Acceptance Tests

### T10.01 — stdin echo lines

```
// File: tests/m10/t01_stdin_echo.styio
// Run: echo -e "hello\nworld" | styio --file tests/m10/t01_stdin_echo.styio
// Expected stdout:
// hello
// world
@stdin >> #(line) => {
    line >> @stdout
}
```

### T10.02 — stdin instant pull

```
// File: tests/m10/t02_stdin_pull.styio
// Run: echo "42" | styio --file tests/m10/t02_stdin_pull.styio
// Expected stdout:
// 42
x = (<< @stdin)
>_(x)
```

### T10.03 — stdin empty / EOF

```
// File: tests/m10/t03_stdin_eof.styio
// Run: echo -n "" | styio --file tests/m10/t03_stdin_eof.styio
// Expected stdout: (nothing)
@stdin >> #(line) => {
    line >> @stdout
}
```

### T10.04 — stdin multi-pull

```
// File: tests/m10/t04_stdin_multi_pull.styio
// Run: echo -e "10\n20" | styio --file tests/m10/t04_stdin_multi_pull.styio
// Expected stdout:
// 10
// 20
@stdin >> #(line) => {
    line >> @stdout
}
```

### T10.05 — stdin transform (string to int)

```
// File: tests/m10/t05_stdin_transform.styio
// Run: echo -e "3\n4\n5" | styio --file tests/m10/t05_stdin_transform.styio
// Expected stdout:
// 6
// 8
// 10
@stdin >> #(line) => {
    result = line * 2
    result >> @stdout
}
```

### T10.06 — stdin mixed with stdout/stderr output

```
// File: tests/m10/t06_stdin_mixed_output.styio
// Run: echo "data" | styio --file tests/m10/t06_stdin_mixed_output.styio
// Expected stdout:
// got: data
// Expected stderr:
// processing: data
@stdin >> #(line) => {
    "got: " + line >> @stdout
    "processing: " + line >> @stderr
}
```

### T10.07 (e01) — Semantic error: write to `@stdin`

```
// File: tests/m10/e01_write_to_stdin.styio
// Expected: non-zero exit (semantic error)
"hello" >> @stdin
```

### T10.08 (e02) — Semantic error: read from `@stdout`

```
// File: tests/m10/e02_read_from_stdout.styio
// Expected: non-zero exit (semantic error)
@stdout >> #(line) => {
    >_(line)
}
```

### T10.09 (e03) — Semantic error: read from `@stderr`

```
// File: tests/m10/e03_read_from_stderr.styio
// Expected: non-zero exit (semantic error)
@stderr >> #(line) => {
    >_(line)
}
```

### T10.10 (e04) — Semantic error: handle acquire on standard stream

```
// File: tests/m10/e04_handle_acquire_stdin.styio
// Expected: non-zero exit (semantic error)
f <- @stdin
```

---

## Implementation Tasks

### Task 10.1 — `styio_stdin_read_line` FFI
**Role:** Runtime Agent
**File:** `src/StyioExtern/ExternLib.hpp`, `src/StyioExtern/ExternLib.cpp`
**Action:** Implement `extern "C" DLLEXPORT const char* styio_stdin_read_line()` — reads one line from C `stdin` via `std::fgets` into a thread-local buffer, strips trailing newline/CR, returns borrowed pointer (same pattern as `styio_file_read_line` but on `stdin` instead of handle FILE*). Returns `nullptr` on EOF.
**Verify:** FFI callable from C test harness.

### Task 10.2 — JIT registration for stdin FFI
**Role:** Runtime Agent
**File:** `src/StyioJIT/StyioJIT_ORC.hpp`
**Action:** Add `{ Mangle("styio_stdin_read_line"), ... }` to `absoluteSymbols`.
**Verify:** JIT can resolve the symbol.

### Task 10.3 — `SIOStdStreamLineIter` IR node
**Role:** IR Agent
**File:** `src/StyioIR/GenIR/GenIR.hpp`, `src/StyioIR/IRDecl.hpp`
**Action:** Define `SIOStdStreamLineIter` with `std::string line_var`, `SGBlock* body`, `std::unique_ptr<SGPulsePlan> pulse_plan`, `int pulse_region_id`. Follow `SGFileLineIter` pattern. Add forward declaration.
**Verify:** Project compiles.

### Task 10.4 — `SIOStdStreamPull` IR node
**Role:** IR Agent
**File:** `src/StyioIR/GenIR/GenIR.hpp`, `src/StyioIR/IRDecl.hpp`
**Action:** Define `SIOStdStreamPull` (no fields — just calls `styio_stdin_read_line` once). Follow `SGInstantPull` pattern.
**Verify:** Project compiles.

### Task 10.5 — Visitor type lists for M10 nodes
**Role:** Infra Agent
**File:** `src/StyioCodeGen/CodeGenVisitor.hpp`, `src/StyioToString/ToStringVisitor.hpp`
**Action:** Add `SIOStdStreamLineIter` and `SIOStdStreamPull` to codegen and toString visitor type lists. Declare `toLLVMType`, `toLLVMIR`, `toString` methods.
**Verify:** Project compiles.

### Task 10.6 — Analyzer: `IteratorAST` → `SIOStdStreamLineIter`
**Role:** Analyzer Agent
**File:** `src/StyioAnalyzer/ToStyioIR.cpp`
**Action:** In `toStyioIR(IteratorAST*)`, add a check: if `collection->getNodeType() == StyioNodeType::StdStreamResource` and kind is `Stdin`, create `SIOStdStreamLineIter` instead of `SGFileLineIter`.
**Verify:** IR lowering for `@stdin >> #(line) => {}` produces `SIOStdStreamLineIter`.

### Task 10.7 — Analyzer: `InstantPullAST` → `SIOStdStreamPull`
**Role:** Analyzer Agent
**File:** `src/StyioAnalyzer/ToStyioIR.cpp`
**Action:** In `toStyioIR(InstantPullAST*)`, check if resource is `StdStreamAST(Stdin)` and create `SIOStdStreamPull` instead of `SGInstantPull`.
**Verify:** IR lowering for `(<< @stdin)` produces `SIOStdStreamPull`.

### Task 10.8 — Codegen for `SIOStdStreamLineIter`
**Role:** CodeGen Agent
**File:** `src/StyioCodeGen/CodeGenIO.cpp` (or `CodeGenStream.cpp`)
**Action:** Generate a while loop: `line = styio_stdin_read_line()`, branch on `nullptr`, bind `line_var`, execute body, repeat. Follow `SGFileLineIter` codegen pattern but without file open/close.
**Verify:** T10.01, T10.03, T10.04 pass.

### Task 10.9 — Codegen for `SIOStdStreamPull`
**Role:** CodeGen Agent
**File:** `src/StyioCodeGen/CodeGenIO.cpp`
**Action:** Emit single call to `styio_stdin_read_line()`. Convert result to appropriate type (default: cstr → i64 via `styio_cstr_to_i64`).
**Verify:** T10.02 passes.

### Task 10.10 — Direction validation (semantic errors)
**Role:** Analyzer Agent
**File:** `src/StyioAnalyzer/ToStyioIR.cpp`
**Action:** In `toStyioIR(ResourceWriteAST*)`, if target is `StdStreamAST(Stdin)`, throw `StyioTypeError("cannot write to @stdin")`. In `toStyioIR(IteratorAST*)`, if source is `StdStreamAST` with kind `Stdout` or `Stderr`, throw `StyioTypeError("cannot iterate @stdout/@stderr")`. In `toStyioIR(HandleAcquireAST*)`, if resource is `StdStreamAST`, throw `StyioTypeError("cannot acquire handle on standard stream")`.
**Verify:** T10.07–T10.10 exit non-zero.

### Task 10.11 — ToString implementations
**Role:** Doc Agent
**File:** `src/StyioToString/ToString.cpp`
**Action:** Implement `toString(SIOStdStreamLineIter*)` and `toString(SIOStdStreamPull*)`.
**Verify:** IR dump shows new nodes.

### Task 10.12 — Test data files and CTest registration
**Role:** Test Agent
**File:** `tests/milestones/m10/`, `tests/CMakeLists.txt`
**Action:** Create test `.styio` files, expected `.out` files, expected `.err` files. Register stdin tests with `bash -c "cat INPUT | styio --file TEST | cmp -s - EXPECTED"` pattern. Register error tests with non-zero exit check.
**Verify:** `ctest -L m10` passes all 10 tests.

---

## Completion Criteria

**M10 is complete when:** T10.01–T10.10 produce correct output / correct failure. All M1–M9 tests still pass. Stdin piping verified via file input. Direction validation rejects all four illegal patterns.

---

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| `stdin` conflict with `--file` argument reading | Verify `main.cpp` reads `.styio` file via filesystem, not stdin. |
| Thread-local buffer contention for `styio_stdin_read_line` | Use separate buffer pair from `styio_file_read_line` (new `g_stdin_line_bufs`). |
| Nightly parser subset doesn't support iterator on `@stdin` | The nightly parser dispatches to `parse_at_stmt_or_expr_latest` which calls `parse_expr_postfix` → `parse_iterator_tail`. May need explicit nightly support. |
| Empty stdin causes infinite loop | `styio_stdin_read_line` returns `nullptr` on EOF; codegen loop condition checks null. |
