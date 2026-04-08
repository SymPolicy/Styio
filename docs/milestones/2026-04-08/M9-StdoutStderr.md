# Milestone 9: stdout & stderr — Standard Output and Error Streams

**文档作用：** M9 **验收测试与任务分解**；路线图与依赖见 [`00-Milestone-Index.md`](./00-Milestone-Index.md)。
**Status:** Frozen

**Depends on:** M5 (Resources & I/O)  
**Goal:** `>_` becomes a first-class terminal device handle. Compiler-recognized `@stdout` and `@stderr` standard-stream resources work end-to-end via canonical `expr -> @stdout` / `expr -> @stderr`. The implementation also keeps `expr >> @stdout` / `expr >> @stderr` as accepted compatibility shorthand. `>_()` is internally unified to the same codegen path. Existing M1-M8 tests remain green.

---

## Definitions

For full semantic definitions, see the authoritative design documents (per `../../specs/DOCUMENTATION-POLICY.md` §0.4):

- **`>_` as terminal device, `@stdout`/`@stderr` resource definitions:** [`../../design/Styio-Language-Design.md`](../../design/Styio-Language-Design.md) §7.7
- **Type formatting rules, `>_` as value:** [`../../design/Styio-Language-Design.md`](../../design/Styio-Language-Design.md) §11.1
- **Symbol ↔ token mapping:** [`../../design/Styio-Symbol-Reference.md`](../../design/Styio-Symbol-Reference.md) §1, §2
- **`!()` channel selector:** [`../../design/Styio-Symbol-Reference.md`](../../design/Styio-Symbol-Reference.md) §9
- **Formal grammar:** [`../../design/Styio-EBNF.md`](../../design/Styio-EBNF.md) §9.1

---

## Acceptance Tests

No prelude or user-authored wrapper definitions are required; `@stdout` / `@stderr` are
compiler-recognized standard-stream resources. Frozen acceptance examples use canonical `->`
(redirect).
Compatibility shorthand `>> @stdout/@stderr` is covered separately by parser/security and
five-layer tests, not by replacing these acceptance examples.

### T9.01 — Basic string to stdout

``` 
// File: tests/milestones/m9/t01_stdout_string.styio
// Expected stdout: Hello, Styio!
"Hello, Styio!" -> @stdout
```

### T9.02 — Integer to stdout

```
// File: tests/milestones/m9/t02_stdout_int.styio
// Expected stdout: 42
42 -> @stdout
```

### T9.03 — Float to stdout

```
// File: tests/milestones/m9/t03_stdout_float.styio
// Expected stdout: 3.140000
3.14 -> @stdout
```

### T9.04 — Boolean to stdout

```
// File: tests/milestones/m9/t04_stdout_bool.styio
// Expected stdout:
// true
// false
true -> @stdout
false -> @stdout
```

### T9.05 — Variable binding then stdout

```
// File: tests/milestones/m9/t05_stdout_var.styio
// Expected stdout: 100
x = 50 + 50
x -> @stdout
```

### T9.06 — Concatenated formatted output to stdout

```
// File: tests/milestones/m9/t06_stdout_concat.styio
// Expected stdout: x=10
x = 10
"x=" + x -> @stdout
```

*Note: M9 freezes standard-stream write behavior, not the separate format-string feature.
`$"..."` remains tracked independently and is not required for M9 acceptance.*

### T9.07 — Undefined (@) to stdout

```
// File: tests/milestones/m9/t07_stdout_undef.styio
// Expected stdout: @
x = @
x -> @stdout
```

### T9.08 — Multiple types to stdout

```
// File: tests/milestones/m9/t08_stdout_multi.styio
// Expected stdout:
// 42
// 3.140000
// Hello
42 -> @stdout
3.14 -> @stdout
"Hello" -> @stdout
```

### T9.09 — Basic string to stderr

```
// File: tests/milestones/m9/t09_stderr_string.styio
// Expected stderr: Error occurred
// Expected stdout: (empty)
"Error occurred" -> @stderr
```

### T9.10 — Integer to stderr

```
// File: tests/milestones/m9/t10_stderr_int.styio
// Expected stderr: 42
// Expected stdout: (empty)
42 -> @stderr
```

### T9.11 — Mixed stdout and stderr

```
// File: tests/milestones/m9/t11_mixed.styio
// Expected stdout: OK
// Expected stderr: Warning
"OK" -> @stdout
"Warning" -> @stderr
```

### T9.12 — Legacy >_() still works

```
// File: tests/milestones/m9/t12_legacy_print.styio
// Expected stdout:
// hello
// 42
>_("hello")
>_(42)
```

*Note: T9.12 tests backward compatibility. No `@stdout` wrapper definition is needed —
`>_()` is the primitive and works directly.*

---

## Implementation Tasks

### Task 9.1 — Add StdStreamAST node and token types
**Role:** Parser Agent  
**File:** `src/StyioToken/Token.hpp`, `src/StyioAST/AST.hpp`, `src/StyioAST/ASTDecl.hpp`  
**Action:** Add `StdStreamKind` enum (`Stdin`, `Stdout`, `Stderr`), `StdStreamAST` class with CRTP traits and `Create()` factory, new `StyioNodeType` values. Also make `>_` parseable as a value expression (`TerminalDeviceAST`). Add forward declarations in `ASTDecl.hpp`.  
**Verify:** Code compiles.

### Task 9.2 — Extend parser to recognize @stdout, @stderr, and >_ as value
**Role:** Parser Agent  
**File:** `src/StyioParser/Parser.cpp` (both legacy and nightly paths)  
**Action:** In `@` dispatch, when `NAME` is `"stdout"` or `"stderr"`, return `StdStreamAST`. Parse `>_` in expression position as a terminal device value. Parse `!(expr) -> ( >_ )` as a channel-selected write. Treat bare `@stdout` / `@stderr` as compiler-recognized standard-stream resources, not user-authored bindings.  
**Verify:** T9.01 parses correctly (use `--styio-ast`).

### Task 9.3 — Add SIOStdStreamWrite IR node
**Role:** Analyzer Agent  
**File:** `src/StyioIR/GenIR/GenIR.hpp`, `src/StyioIR/IRDecl.hpp`  
**Action:** Add `SIOStdStreamWrite` with `Target` enum (`Stdout`, `Stderr`), `data_expr`, and `promote_to_cstr` flag. Add forward declaration.  
**Verify:** Code compiles.

### Task 9.4 — Add ToString visitor for new nodes
**Role:** Analyzer Agent  
**File:** `src/StyioToString/ToStringVisitor.hpp`  
**Action:** Add `toString(StdStreamAST*)` and `toString(SIOStdStreamWrite*)`.  
**Verify:** `--styio-ast` and `--styio-ir` print new nodes correctly.

### Task 9.5 — Add type inference for StdStreamAST
**Role:** Analyzer Agent  
**File:** `src/StyioAnalyzer/TypeInfer.cpp`  
**Action:** Type inference for new nodes. `StdStreamAST` participates as a compiler-recognized standard-stream resource, without requiring wrapper definitions in source.  
**Verify:** T9.01–T9.08 pass type inference.

### Task 9.6 — Add IR lowering for stdout/stderr writes
**Role:** Analyzer Agent  
**File:** `src/StyioAnalyzer/ToStyioIR.cpp`  
**Action:** When lowering canonical `expr -> @stdout` or accepted shorthand `expr >> @stdout`, lower the compiler-recognized `@stdout` resource atom to `SIOStdStreamWrite(Stdout, data_ir)`. When `!(expr) -> ( >_ )` is present (via `@stderr`), or shorthand `expr >> @stderr` is used, produce `SIOStdStreamWrite(Stderr, data_ir)`.  
**Verify:** T9.01 produces `SIOStdStreamWrite(Stdout, ...)` in IR.

### Task 9.7 — Add stderr FFI function
**Role:** Runtime Agent  
**File:** `src/StyioExtern/ExternLib.hpp`, `src/StyioExtern/ExternLib.cpp`  
**Action:** Add `styio_stderr_write_cstr(const char* s)` — `fprintf(stderr, "%s\n", s); fflush(stderr);`  
**Verify:** FFI function works.

### Task 9.8 — Register stderr FFI in JIT
**Role:** Runtime Agent  
**File:** `src/StyioJIT/StyioJIT_ORC.hpp`  
**Action:** Add `styio_stderr_write_cstr` to the JIT symbol table.  
**Verify:** JIT can resolve symbol.

### Task 9.9 — LLVM codegen for SIOStdStreamWrite
**Role:** CodeGen Agent  
**File:** `src/StyioCodeGen/CodeGenIO.cpp`, `src/StyioCodeGen/CodeGenVisitor.hpp`  
**Action:** For `Stdout`: reuse `SIOPrint`'s `printf`/`puts` logic. For `Stderr`: convert to cstr, call `styio_stderr_write_cstr`. Add visitor dispatch.  
**Verify:** T9.01–T9.11 produce correct output.

### Task 9.10 — Unify PrintAST lowering to SIOStdStreamWrite
**Role:** Analyzer Agent  
**File:** `src/StyioAnalyzer/ToStyioIR.cpp`  
**Action:** Change `PrintAST` IR lowering from `SIOPrint` to `SIOStdStreamWrite(Stdout, ...)`. Unifies `>_()` and `-> @stdout`.  
**Verify:** T9.12 still works. All M1-M8 tests still pass.

### Task 9.11 — Test data and CTest registration
**Role:** Test Agent  
**File:** `tests/milestones/m9/*.styio`, `tests/milestones/m9/expected/*`, `tests/CMakeLists.txt`  
**Action:** Create test files T9.01–T9.12 without requiring wrapper definitions in each file. Create expected output files. Register in CMakeLists.txt. Stderr tests capture fd 2 separately.  
**Verify:** `ctest -L m9` runs all 12 tests.

---

## Completion Criteria

**M9 is complete when:** T9.01–T9.12 produce correct output (stdout and stderr as specified). All M1–M8 tests still pass. `>_()` internally uses the unified `SIOStdStreamWrite(Stdout)` path, and accepted shorthand `>> @stdout/@stderr` remains frozen by targeted parser/five-layer coverage.

---

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| stderr CTest capture fragile across platforms | FFI function guarantees `fflush(stderr)`; test via bash redirect |
| `>_()` → `SIOStdStreamWrite` unification breaks M1-M8 | Run full `ctest -L milestone` after Task 9.10 |
| `!()` disambiguation with logical NOT | Context rule: `!(expr) -> ( >_ )` is always channel-select |
| `>_` as value breaks existing `>_(expr)` parsing | Parser checks: `>_` followed by `(` is legacy print; `>_` in other expression contexts is device handle |
