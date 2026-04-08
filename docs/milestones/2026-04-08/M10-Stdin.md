# Milestone 10: stdin & Direction Validation — Standard Input and Semantic Constraints

**文档作用：** M10 **验收测试与任务分解**；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md)。
**Status:** Frozen

**Depends on:** M9 (stdout & stderr)  
**Goal:** `@stdin >> #(line) => { ... }` reads lines from stdin; `(<< @stdin)` performs instant pull; direction constraints prevent writing to `@stdin` or reading from `@stdout`/`@stderr`. Canonical examples use `-> @stdout/@stderr`, while accepted `>> @stdout/@stderr` shorthand remains write-only. Standard streams are compiler-recognized resource atoms, not user-authored wrapper definitions. EOF terminates stdin iteration naturally. The current frozen instant-pull contract keeps legacy scalar coercion: `(<< @stdin)` lowers through `styio_cstr_to_i64()` unless a future typed string path is introduced.

---

## Definitions

For full semantic definitions, see the authoritative design documents (per `../../specs/DOCUMENTATION-POLICY.md` §0.4):

- **`@stdin` resource definition, `<< ( >_ )` semantics:** [`../../design/Styio-Language-Design.md`](../../design/Styio-Language-Design.md) §7.7
- **Direction constraints:** [`../../design/Styio-Language-Design.md`](../../design/Styio-Language-Design.md) §7.7 (Direction constraints)
- **Formal grammar:** [`../../design/Styio-EBNF.md`](../../design/Styio-EBNF.md) §9.1

---

## Acceptance Tests

No prelude or user-authored wrapper definitions are required; `@stdout`, `@stderr`, and
`@stdin` are compiler-recognized standard-stream resources. Frozen acceptance examples use
canonical `->` (redirect) for stdout/stderr writes.
The implementation also accepts `>> @stdout/@stderr` as compatibility shorthand; both
`expr -> @stdin` and `expr >> @stdin` remain semantic errors.

### T10.01 — Read lines from stdin (echo)

```
// File: tests/milestones/m10/t01_stdin_echo.styio
// Stdin: tests/milestones/m10/data/echo_input.txt (containing "Hello\nWorld")
// Expected stdout:
// Hello
// World
@stdin >> #(line) => {
  line -> @stdout
}
```

### T10.02 — Instant pull from stdin (current scalar contract)

```
// File: tests/milestones/m10/t02_stdin_pull.styio
// Stdin: tests/milestones/m10/data/pull_input.txt (containing "42")
// Expected stdout: 42
value = (<< @stdin)
value -> @stdout
```

### T10.03 — Empty stdin (EOF immediately)

```
// File: tests/milestones/m10/t03_stdin_eof.styio
// Stdin: /dev/null (empty)
// Expected stdout: (empty)
@stdin >> #(line) => {
  line -> @stdout
}
```

### T10.04 — Multiple instant pulls (current scalar contract)

```
// File: tests/milestones/m10/t04_stdin_multi_pull.styio
// Stdin: tests/milestones/m10/data/multi_input.txt (containing "7\n11")
// Expected stdout:
// 7
// 11
a = (<< @stdin)
b = (<< @stdin)
a -> @stdout
b -> @stdout
```

*Note: line-iterator keeps string lines; instant pull currently keeps the older scalar
convention and therefore uses numeric input in frozen acceptance fixtures.*

### T10.05 — Stdin with transformation

```
// File: tests/milestones/m10/t05_stdin_transform.styio
// Stdin: tests/milestones/m10/data/transform_input.txt (containing "10\n20\n30")
// Expected stdout:
// 20
// 40
// 60
# double_it := (x: i64) => x * 2
@stdin >> #(line) => {
  double_it(line) -> @stdout
}
```

*Note: T10.05 depends on `cstr_to_i64` coercion (same as M5 T5.08). If unavailable, mark as **stretch goal**.*

### T10.06 — Stdin to stdout and stderr

```
// File: tests/milestones/m10/t06_stdin_mixed_output.styio
// Stdin: tests/milestones/m10/data/mixed_input.txt (containing "Hello\nWorld")
// Expected stdout:
// out: Hello
// out: World
// Expected stderr:
// err: Hello
// err: World
@stdin >> #(line) => {
  "out: " + line >> @stdout
  "err: " + line >> @stderr
}
```

*Note: this mixed-output fixture intentionally uses the accepted `>> @stdout/@stderr`
shorthand inside the iterator block, because that is the currently frozen implementation path
already exercised by five-layer case `p15_stdin_mixed_output`.*

### T10.07 — Write to @stdin (semantic error)

```
// File: tests/milestones/m10/e01_write_to_stdin.styio
// Expected: non-zero exit code (compile error)
"hello" -> @stdin
```

### T10.08 — Read from @stdout (semantic error)

```
// File: tests/milestones/m10/e02_read_from_stdout.styio
// Expected: non-zero exit code (compile error)
@stdout >> #(line) => {
  line -> @stderr
}
```

### T10.09 — Instant pull from @stderr (semantic error)

```
// File: tests/milestones/m10/e03_read_from_stderr.styio
// Expected: non-zero exit code (compile error)
x = (<< @stderr)
```

### T10.10 — Handle acquire on @stdin (semantic error)

```
// File: tests/milestones/m10/e04_handle_acquire_stdin.styio
// Expected: non-zero exit code (compile error)
f <- @stdin
```

---

## Implementation Tasks

### Task 10.1 — Add SIOStdStreamLineIter and SIOStdStreamPull IR nodes
**Role:** Analyzer Agent  
**File:** `src/StyioIR/GenIR/GenIR.hpp`, `src/StyioIR/IRDecl.hpp`  
**Action:** Add `SIOStdStreamLineIter` (fields: `line_var`, `body`, `pulse_plan`, `pulse_region_id`) and `SIOStdStreamPull` IR nodes. Add forward declarations.  
**Verify:** Code compiles.

### Task 10.2 — Extend parser to recognize @stdin
**Role:** Parser Agent  
**File:** `src/StyioParser/Parser.cpp` (both engines)  
**Action:** In `@` dispatch, when `NAME` is `"stdin"`, return `StdStreamAST(Stdin)`. Parse `<< ( >_ )` as stdin stream extraction. Existing postfix rules handle `@stdin >> #(x) => {...}` and `(<< @stdin)`. No wrapper definition syntax is required.  
**Verify:** T10.01 parses correctly.

### Task 10.3 — Add direction validation (semantic checks)
**Role:** Analyzer Agent  
**File:** `src/StyioAnalyzer/TypeInfer.cpp`  
**Action:** Validate direction constraints:
- `expr -> @stdin` → error: `"@stdin is a read-only stream; cannot write to it"`
- `expr >> @stdin` → error: `"@stdin is a read-only stream; cannot write to it"`
- `@stdout >> #(x) => {...}` → error: `"@stdout is a write-only stream; cannot iterate over it"`
- `(<< @stderr)` → error: `"@stderr is a write-only stream; cannot read from it"`
- `f <- @stdin` → error: `"standard streams do not require handle acquisition"`
**Verify:** T10.07–T10.10 produce non-zero exit code.

### Task 10.4 — Add stdin FFI functions
**Role:** Runtime Agent  
**File:** `src/StyioExtern/ExternLib.hpp`, `src/StyioExtern/ExternLib.cpp`  
**Action:** Add `styio_stdin_read_line()` — `fgets()` into a thread-local buffer, strip newline, return borrowed ptr or `nullptr` on EOF.  
**Verify:** FFI function works.

### Task 10.5 — Register stdin FFI in JIT
**Role:** Runtime Agent  
**File:** `src/StyioJIT/StyioJIT_ORC.hpp`  
**Action:** Add `styio_stdin_read_line` to JIT symbol table.  
**Verify:** JIT can resolve symbol.

### Task 10.6 — IR lowering for stdin patterns
**Role:** Analyzer Agent  
**File:** `src/StyioAnalyzer/ToStyioIR.cpp`  
**Action:** Lower the compiler-recognized `@stdin` resource atom directly. `@stdin >> #(line) => {body}` → `SIOStdStreamLineIter(line, body_ir)`. `(<< @stdin)` → `SIOStdStreamPull()`.  
**Verify:** T10.01 produces correct IR.

### Task 10.7 — Add ToString visitor for stdin IR nodes
**Role:** Analyzer Agent  
**File:** `src/StyioToString/ToStringVisitor.hpp`  
**Action:** Add `toString(SIOStdStreamLineIter*)` and `toString(SIOStdStreamPull*)`.  
**Verify:** `--styio-ir` prints correctly.

### Task 10.8 — LLVM codegen for SIOStdStreamLineIter
**Role:** CodeGen Agent  
**File:** `src/StyioCodeGen/CodeGenIO.cpp`, `CodeGenVisitor.hpp`  
**Action:** Generate loop: call `styio_stdin_read_line()`, check null (EOF), bind line var, emit body, loop back. Do not free the borrowed thread-local buffer.  
**Verify:** T10.01, T10.03, T10.05 produce correct output.

### Task 10.9 — LLVM codegen for SIOStdStreamPull
**Role:** CodeGen Agent  
**File:** `src/StyioCodeGen/CodeGenIO.cpp`, `CodeGenVisitor.hpp`  
**Action:** Call `styio_stdin_read_line()`. If null, return sentinel. Otherwise follow the current `InstantPullAST` scalar contract and convert via `styio_cstr_to_i64()`.  
**Verify:** T10.02, T10.04 produce correct output.

### Task 10.10 — Test data files and CTest registration
**Role:** Test Agent  
**File:** `tests/milestones/m10/data/*`, `tests/milestones/m10/*.styio`, `tests/milestones/m10/expected/*`, `tests/CMakeLists.txt`  
**Action:** Create data files, test files, expected outputs, error tests. Register in CMakeLists.txt with stdin piping and error exit code verification.  
**Verify:** `ctest -L m10` runs all 10 tests.

### Task 10.11 — Extend fuzz corpus
**Role:** Test Agent  
**File:** `tests/fuzz/corpus/`  
**Action:** Add fuzz inputs with `@stdin`, `@stdout`, `@stderr` patterns.  
**Verify:** Fuzz targets don't crash.

### Task 10.12 — Update docs
**Role:** Doc Agent  
**File:** SSOTs already updated in Step 0. Verify consistency.  
**Verify:** Docs match implementation.

---

## Completion Criteria

**M10 is complete when:** T10.01–T10.06 produce correct stdout/stderr with piped stdin. T10.07–T10.10 produce non-zero exit code. All M1–M9 tests still pass, and accepted `>> @stdout/@stderr` shorthand remains covered without weakening read-only/write-only constraints.

---

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| Stdin piping in CTest platform-dependent | Use `bash -c "cat FILE \| styio ..."` on Linux |
| `styio_stdin_read_line()` blocks without input | Tests always pipe from files |
| EOF detection varies | `fgets()` returns NULL on EOF regardless of source |
| String→int coercion for T10.05 | Depends on `styio_cstr_to_i64` from M5; mark stretch goal if unavailable |
