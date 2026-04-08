# Standard-Streams-Plan — Complete stdin / stdout / stderr Syntax for Styio

**文档作用：** 标准流（stdin/stdout/stderr）特性的完整设计方案。正式定义将写入各 SSOT 文档（见下文「文件修改清单」）。

**Date:** 2026-04-08  
**Last updated:** 2026-04-08  
**Status:** Historical design plan. Frozen acceptance/spec docs live in [`../milestones/2026-04-08/`](../milestones/2026-04-08/); language-level SSOT remains [`../design/Styio-Language-Design.md`](../design/Styio-Language-Design.md), [`../design/Styio-EBNF.md`](../design/Styio-EBNF.md), and [`../design/Styio-Symbol-Reference.md`](../design/Styio-Symbol-Reference.md). This plan originally explored user-authored `@stdout := ...` wrappers; the frozen implementation instead recognizes `@stdout/@stderr/@stdin` directly as standard-stream resource atoms.

---

## 1. Design Decisions (Agreed)

| Decision | Choice |
|----------|--------|
| Architecture | **Hybrid primitive + built-in atoms** — `>_` is the terminal device primitive; `@stdout`/`@stderr`/`@stdin` are compiler-recognized standard-stream resource atoms on top of it |
| `>_` role | **First-class terminal device** — readable and writable resource handle |
| User interface | **`@`-resource layer** — users directly use `@stdout`, `@stderr`, `@stdin` without per-file wrapper definitions |
| Definitions | **Historical draft assumption:** user-written wrappers in each file. **Final frozen implementation:** compiler-recognized standard-stream resource atoms; no wrapper definitions required |
| Write syntax | **Canonical:** `expr -> @stdout` / `expr -> @stderr`; **accepted compatibility shorthand:** `expr >> @stdout` / `expr >> @stderr` when `>>` is followed by a standard-stream resource atom |
| Call syntax | **`@stdout(expr)`** freezes the operation for use in continuations |
| stdin model | **Stream-first** — `@stdin >> #(line) => {...}` iterates strings; `(<< @stdin)` currently follows the legacy scalar `cstr -> i64` instant-pull contract |
| EOF handling | **Stream naturally ends** — `>>` iteration stops at EOF |
| stderr buffering | **Unbuffered** — immediate flush |
| `!` operator | **Channel selector** — `!(x)` before `-> ( >_ )` selects stderr (fd 2) |
| Compiler optimization | **Inline + optimize early** — compiler inlines `@stdout` definition, recognizes `>_` pattern, generates direct FFI calls |

---

## 2. Core Primitives

### 2.1 `>_` — The Terminal Device

`>_` is a **first-class resource handle value** representing the user's terminal. It is the
only primitive; all standard streams are defined in terms of it.

| Operation | Syntax | Meaning | Unix fd |
|-----------|--------|---------|---------|
| Write | `x -> ( >_ )` | Write value `x` to terminal output | fd 1 (stdout) |
| Error write | `!(x) -> ( >_ )` | Write value `x` to terminal error channel | fd 2 (stderr) |
| Read (stream) | `<< ( >_ )` | Extract line stream from terminal input | fd 0 (stdin) |

### 2.2 `!()` — Channel Selector

When `!()` wraps a value targeting `>_`, it selects the **error channel** (fd 2) instead of
the default output channel (fd 1). This is a new overloaded meaning of `!` (existing meaning:
logical NOT for booleans). The compiler disambiguates by context: `!(expr) -> ( >_ )` is
a channel-selected write; `!(bool_expr)` in other contexts remains logical NOT.

### 2.3 Historical Wrapper Draft vs Frozen Implementation

Early planning assumed user-authored wrapper definitions such as:

```styio
@stdout := (o) => { o -> ( >_ ) }
@stderr := (x) => { !(x) -> ( >_ ) }
@stdin  := { << ( >_ ) }
```

- `@stdout` is a **function**: takes a value, redirects it to the terminal
- `@stderr` is a **function**: takes a value, channel-selects to error, redirects to terminal
- `@stdin` is a **stream**: extracts lines from the terminal (iterable via `>>`)

This was the original draft shape. The frozen implementation ultimately took a simpler route:
the compiler recognizes `@stdout`, `@stderr`, and `@stdin` directly as standard-stream
resource atoms, so user-authored wrapper definitions are not required.

---

## 3. Correct Usage Patterns

### 3.1 Writing to stdout

```styio
// Redirect a value to stdout (action, executes immediately)
42 -> @stdout
"Hello, Styio!" -> @stdout
3.14 -> @stdout

// Call @stdout as a function (freezes for continuation use)
@stdout(42)
@stdout("Hello, Styio!")
```

**Compatibility note:** the frozen milestone docs use `-> @stdout` / `-> @stderr` as the
canonical spelling. The current parser also accepts `"Hello" >> @stdout` and
`"warn" >> @stderr` as standard-stream resource-write shorthand, because `>>` followed by a
resource atom is parsed as `resource_write`, not string iteration.

### 3.2 Writing to stderr

```styio
"Error: file not found" -> @stderr
42 -> @stderr
```

### 3.3 Reading from stdin

```styio
// Stream iteration (line-by-line)
@stdin >> #(line) => {
  line -> @stdout
}

// Instant pull (current scalar contract)
value = (<< @stdin)
```

### 3.4 Combined example

```styio
@stdin >> #(line) => {
  line -> @stdout
  "processed: " + line -> @stderr
}
```

---

## 4. File Modification Manifest

All files that will be created or modified, grouped by execution step.

### Step 0 — Design documents (SSOT updates)

| File | Action | Changes |
|------|--------|---------|
| `docs/design/Styio-Language-Design.md` | **Modify** | §7: add §7.7 "Standard Stream Resources" — defines `>_` as terminal device, `@stdout`/`@stderr`/`@stdin` as built-in resource atoms, directionality, buffering, and current instant-pull coercion contract. §11: redefine §11.1 (`>_` as device, not just print), add §11.4 "Standard Error", add §11.5 "Standard Input", add type formatting table |
| `docs/design/Styio-Symbol-Reference.md` | **Modify** | §1: add `@stdout`, `@stderr`, `@stdin` rows. §2: update `>_` entry (now "terminal device" not just "print"). §9: add `!()` channel selector entry. Clarify that standard streams are compiler-recognized atoms, not per-file wrapper definitions |
| `docs/design/Styio-EBNF.md` | **Modify** | §9: document direct standard-stream usage patterns. Remove any implication that `@stdout` etc. require wrapper-definition grammar |

### Step 1 — Milestone documents

| File | Action | Changes |
|------|--------|---------|
| `docs/milestones/README.md` | **Modify** | Add `2026-04-08/` batch row |
| `docs/milestones/2026-04-08/00-Milestone-Index.md` | **Create** | Batch index: M9–M10 overview, dependency chain, roles |
| `docs/milestones/2026-04-08/M9-StdoutStderr.md` | **Create** | M9 spec: `>_` as device, canonical `->` write, accepted `>> @stdout/@stderr` shorthand, `!()` channel selector, `@stdout`/`@stderr` definitions |
| `docs/milestones/2026-04-08/M10-Stdin.md` | **Create** | M10 spec: `<< ( >_ )` read, `@stdin` stream definition, direction validation, and shorthand-vs-read-only boundary |
| `docs/plans/Standard-Streams-Plan.md` | **Create** | Copy of this plan (canonical location per user request) |

### Step 2 — Implementation (M9: stdout & stderr)

| File | Action | Changes |
|------|--------|---------|
| `src/StyioToken/Token.hpp` | **Modify** | Add `StyioNodeType` values: `StdStreamResource`, `TerminalDevice`. Possibly add `ChannelSelect` node type for `!()` in write context |
| `src/StyioAST/AST.hpp` | **Modify** | Add `StdStreamAST` node (or `TerminalDeviceAST` for `>_` as value). Add `ChannelSelectAST` for `!(expr)` in redirect context |
| `src/StyioAST/ASTDecl.hpp` | **Modify** | Forward declarations for new AST classes |
| `src/StyioParser/Parser.cpp` | **Modify** | Parse `>_` as value (not just print statement). Parse `!(expr) -> ( >_ )` as channel-selected write. Parse compiler-recognized `@stdout/@stderr/@stdin` atoms directly |
| `src/StyioIR/GenIR/GenIR.hpp` | **Modify** | Add `SIOStdStreamWrite` IR node with `Target` enum (Stdout, Stderr) |
| `src/StyioIR/IRDecl.hpp` | **Modify** | Forward declarations for new IR nodes |
| `src/StyioAnalyzer/TypeInfer.cpp` | **Modify** | Type inference for new nodes |
| `src/StyioAnalyzer/ToStyioIR.cpp` | **Modify** | IR lowering: `ResourceRedirectAST` targeting `>_` → `SIOStdStreamWrite(Stdout)`. With `!()` → `SIOStdStreamWrite(Stderr)`. Unify `PrintAST` to same path |
| `src/StyioToString/ToStringVisitor.hpp` | **Modify** | ToString for new AST/IR nodes |
| `src/StyioExtern/ExternLib.hpp` | **Modify** | Add `styio_stderr_write_cstr()` FFI declaration |
| `src/StyioExtern/ExternLib.cpp` | **Modify** | Implement `styio_stderr_write_cstr()`: `fprintf(stderr, "%s\n", s); fflush(stderr);` |
| `src/StyioJIT/StyioJIT_ORC.hpp` | **Modify** | Register `styio_stderr_write_cstr` in JIT symbol table |
| `src/StyioCodeGen/CodeGenIO.cpp` | **Modify** | LLVM codegen for `SIOStdStreamWrite` (stdout reuses `SIOPrint` logic; stderr calls FFI) |
| `src/StyioCodeGen/CodeGenVisitor.hpp` | **Modify** | Visitor dispatch entries for new IR nodes |
| `tests/milestones/m9/` | **Create** | Test `.styio` files (T9.01–T9.12) without wrapper boilerplate in each file |
| `tests/milestones/m9/expected/` | **Create** | Golden `.out` and `.err` files |
| `tests/CMakeLists.txt` | **Modify** | Register M9 tests |

### Step 3 — Implementation (M10: stdin & direction validation)

| File | Action | Changes |
|------|--------|---------|
| `src/StyioIR/GenIR/GenIR.hpp` | **Modify** | Add `SIOStdStreamLineIter`, `SIOStdStreamPull` IR nodes |
| `src/StyioParser/Parser.cpp` | **Modify** | Parse `<< ( >_ )` as stdin stream extraction |
| `src/StyioAnalyzer/TypeInfer.cpp` | **Modify** | Direction validation: reject write to read-only, read from write-only |
| `src/StyioAnalyzer/ToStyioIR.cpp` | **Modify** | IR lowering for stdin iteration and instant pull |
| `src/StyioExtern/ExternLib.hpp` | **Modify** | Add `styio_stdin_read_line()` FFI declaration |
| `src/StyioExtern/ExternLib.cpp` | **Modify** | Implement `styio_stdin_read_line()`: `fgets()` into thread-local buffer, strip newline, return borrowed ptr or nullptr on EOF |
| `src/StyioJIT/StyioJIT_ORC.hpp` | **Modify** | Register `styio_stdin_read_line` in JIT symbol table |
| `src/StyioCodeGen/CodeGenIO.cpp` | **Modify** | LLVM codegen for `SIOStdStreamLineIter` (loop) and `SIOStdStreamPull` |
| `tests/milestones/m10/` | **Create** | Test `.styio` files without wrapper boilerplate in each file |
| `tests/milestones/m10/expected/` | **Create** | Golden output files |
| `tests/milestones/m10/data/` | **Create** | Test input data files |
| `tests/CMakeLists.txt` | **Modify** | Register M10 tests (stdin piped, error tests) |

---

## 5. Historical M9 Acceptance Draft

The detailed examples in this section are preserved as design-history material. Frozen
acceptance for actual implementation lives in `docs/milestones/2026-04-08/`, where wrapper
boilerplate has been removed and M9 no longer depends on format-string support.

Every test file includes the required `@stdout`/`@stderr` definitions. The frozen milestone
examples use canonical `->` spelling. The implementation also accepts `>> @stdout/@stderr`
as compatibility shorthand, and that compatibility is frozen separately by parser/five-layer
tests.

### T9.01 — Basic string to stdout

```
// File: tests/m9/t01_stdout_string.styio
// Expected stdout: Hello, Styio!
@stdout := (o) => { o -> ( >_ ) }

"Hello, Styio!" -> @stdout
```

### T9.02 — Integer to stdout

```
// File: tests/m9/t02_stdout_int.styio
// Expected stdout: 42
@stdout := (o) => { o -> ( >_ ) }

42 -> @stdout
```

### T9.03 — Float to stdout

```
// File: tests/m9/t03_stdout_float.styio
// Expected stdout: 3.140000
@stdout := (o) => { o -> ( >_ ) }

3.14 -> @stdout
```

### T9.04 — Boolean to stdout

```
// File: tests/m9/t04_stdout_bool.styio
// Expected stdout:
// true
// false
@stdout := (o) => { o -> ( >_ ) }

true -> @stdout
false -> @stdout
```

### T9.05 — Variable binding then stdout

```
// File: tests/m9/t05_stdout_var.styio
// Expected stdout: 100
@stdout := (o) => { o -> ( >_ ) }

x := 50 + 50
x -> @stdout
```

### T9.06 — Concatenated formatted output to stdout

```
// File: tests/m9/t06_stdout_concat.styio
// Expected stdout: x=10
x = 10
"x=" + x -> @stdout
```

### T9.07 — Undefined (@) to stdout

```
// File: tests/m9/t07_stdout_undef.styio
// Expected stdout: @
@stdout := (o) => { o -> ( >_ ) }

x = @
x -> @stdout
```

### T9.08 — Multiple types to stdout

```
// File: tests/m9/t08_stdout_multi.styio
// Expected stdout:
// 42
// 3.140000
// Hello
@stdout := (o) => { o -> ( >_ ) }

42 -> @stdout
3.14 -> @stdout
"Hello" -> @stdout
```

### T9.09 — Basic string to stderr

```
// File: tests/m9/t09_stderr_string.styio
// Expected stderr: Error occurred
// Expected stdout: (empty)
@stderr := (x) => { !(x) -> ( >_ ) }

"Error occurred" -> @stderr
```

### T9.10 — Integer to stderr

```
// File: tests/m9/t10_stderr_int.styio
// Expected stderr: 42
// Expected stdout: (empty)
@stderr := (x) => { !(x) -> ( >_ ) }

42 -> @stderr
```

### T9.11 — Mixed stdout and stderr

```
// File: tests/m9/t11_mixed.styio
// Expected stdout: OK
// Expected stderr: Warning
@stdout := (o) => { o -> ( >_ ) }
@stderr := (x) => { !(x) -> ( >_ ) }

"OK" -> @stdout
"Warning" -> @stderr
```

### T9.12 — Legacy >_() still works

```
// File: tests/m9/t12_legacy_print.styio
// Expected stdout: Hello
>_("Hello")
```

*Note: T9.12 tests backward compatibility. No @stdout definition needed — `>_()` is the
primitive and works directly.*

---

## 6. Historical M10 Acceptance Draft

This section is also retained as design archaeology. Frozen acceptance lives in
`docs/milestones/2026-04-08/`, where `(<< @stdin)` is explicitly documented as the current
numeric instant-pull contract and no wrapper prelude is required.

Every test file includes all required definitions (`@stdout`, `@stderr`, `@stdin`).

### T10.01 — Read lines from stdin (echo)

```
// File: tests/m10/t01_stdin_echo.styio
// Stdin: tests/m10/data/hello.txt (containing "Hello\nWorld")
// Expected stdout:
// Hello
// World
@stdout := (o) => { o -> ( >_ ) }
@stdin  := { << ( >_ ) }

@stdin >> #(line) => {
  line -> @stdout
}
```

### T10.02 — Instant pull from stdin

```
// File: tests/m10/t02_stdin_pull.styio
// Stdin: tests/m10/data/pull_input.txt (containing "42")
// Expected stdout: 42
value = (<< @stdin)
value -> @stdout
```

### T10.03 — Empty stdin (EOF immediately)

```
// File: tests/m10/t03_stdin_empty.styio
// Stdin: /dev/null (empty)
// Expected stdout: (empty)
@stdout := (o) => { o -> ( >_ ) }
@stdin  := { << ( >_ ) }

@stdin >> #(line) => {
  line -> @stdout
}
```

### T10.04 — Multiple instant pulls

```
// File: tests/m10/t04_stdin_multi_pull.styio
// Stdin: tests/m10/data/multi_input.txt (containing "7\n11")
// Expected stdout:
// 7
// 11
a = (<< @stdin)
b = (<< @stdin)
a -> @stdout
b -> @stdout
```

### T10.05 — Stdin with transformation

```
// File: tests/m10/t05_stdin_transform.styio
// Stdin: tests/m10/data/numbers.txt (containing "10\n20\n30")
// Expected stdout:
// 20
// 40
// 60
@stdout := (o) => { o -> ( >_ ) }
@stdin  := { << ( >_ ) }

# double_it := (x: i64) => x * 2
@stdin >> #(line) => {
  double_it(line) -> @stdout
}
```

*Note: T10.05 depends on `cstr_to_i64` coercion (same as M5 T5.08). If unavailable, mark as **stretch goal**.*

### T10.06 — Stdin to stdout and stderr

```
// File: tests/m10/t06_stdin_to_both.styio
// Stdin: tests/m10/data/hello.txt (containing "Hello\nWorld")
// Expected stdout:
// out: Hello
// out: World
// Expected stderr:
// err: Hello
// err: World
@stdout := (o) => { o -> ( >_ ) }
@stderr := (x) => { !(x) -> ( >_ ) }
@stdin  := { << ( >_ ) }

@stdin >> #(line) => {
  $"out: {line}" -> @stdout
  $"err: {line}" -> @stderr
}
```

### T10.07 — Write to @stdin (semantic error)

```
// File: tests/m10/e01_write_to_stdin.styio
// Expected: non-zero exit code (compile error)
@stdin := { << ( >_ ) }

"hello" -> @stdin
```

### T10.08 — Read from @stdout (semantic error)

```
// File: tests/m10/e02_read_from_stdout.styio
// Expected: non-zero exit code (compile error)
@stdout := (o) => { o -> ( >_ ) }
@stderr := (x) => { !(x) -> ( >_ ) }

@stdout >> #(line) => {
  line -> @stderr
}
```

### T10.09 — Instant pull from @stderr (semantic error)

```
// File: tests/m10/e03_pull_from_stderr.styio
// Expected: non-zero exit code (compile error)
@stderr := (x) => { !(x) -> ( >_ ) }

x := (<< @stderr)
```

### T10.10 — Handle acquire on @stdin (semantic error)

```
// File: tests/m10/e04_acquire_stdin.styio
// Expected: non-zero exit code (compile error)
@stdin := { << ( >_ ) }

f <- @stdin
```

---

## 7. Implementation Approach Summary

### What changes about `>_`

Currently `>_` is only a print statement (`>_(expr)` → `PrintAST`). It must become:

1. **A value** — `>_` can appear in expression position as a terminal device handle
2. **A redirect target** — `expr -> ( >_ )` writes to stdout
3. **A channel-selectable target** — `!(expr) -> ( >_ )` writes to stderr
4. **A readable source** — `<< ( >_ )` extracts a line stream from stdin
5. **Backward-compatible** — `>_(expr)` still works as the existing print primitive

### Compiler recognition of @stdout/@stderr/@stdin

When the compiler sees `@stdout := (o) => { o -> ( >_ ) }`, it:

1. Parses the binding normally
2. Recognizes the pattern: function wrapping a `->` to `>_`
3. Tags the binding as a "standard stream alias"
4. When `expr -> @stdout` is encountered, **inlines** the definition and optimizes to a
   direct `printf`/`puts` call (same as current `>_(expr)` codegen)
5. For `@stderr`, recognizes the `!()` pattern and routes to `styio_stderr_write_cstr` FFI

---

## 8. Execution Order

1. **Step 0** — Update SSOT documents (`../design/Styio-Language-Design.md`, `../design/Styio-Symbol-Reference.md`, `../design/Styio-EBNF.md`)
2. **Step 1** — Write milestone documents (`M9-StdoutStderr.md`, `M10-Stdin.md`, index) → **user reviews**
3. **Step 2** — Implement M9 (stdout & stderr)
4. **Step 3** — Implement M10 (stdin & direction validation)

---

## 9. Open Questions

1. **`!()` disambiguation**: How does the parser distinguish `!(bool_expr)` (logical NOT) from
   `!(value) -> ( >_ )` (channel selector)? Proposal: `!()` followed by `->` targeting `>_`
   is always channel-select; in all other contexts, `!` remains logical NOT.

2. **`>_` as value vs statement**: Currently `>_` is only recognized at statement position
   (`>_(...)` as print). Making it a value requires parser changes — `>_` in expression
   position returns a terminal device handle. Need to verify this doesn't break existing
   `>_(...)` parsing.

3. **`<< ( >_ )` stream semantics**: Does `{ << ( >_ ) }` produce a lazy stream object (like
   a generator) or eagerly read all input? For `@stdin >> #(line) => {...}` to work as line
   iteration, it should be lazy — each `>>` step pulls one line.

4. **Inline optimization scope**: The compiler inlines `@stdout`/`@stderr`/`@stdin` definitions.
   Does this happen at IR lowering (AST → StyioIR) or during LLVM optimization passes?
   Proposal: at IR lowering — the analyzer recognizes the binding pattern and emits direct
   `SIOStdStreamWrite` / `SIOStdStreamLineIter` IR nodes.
