# Milestone 5: Resources & I/O

**Depends on:** M4 (Wave & Absence)  
**Goal:** File resources (`@file{}`), handle acquisition (`<-`), reading (`collection >> #(line) => ...`), writing (`expr >> @file{...}`), and scope-based RAII cleanup work end-to-end.

---

## Acceptance Tests

### T5.01 — Read file line by line

```
// File: tests/m5/t01_read_file.styio
// Setup: create tests/m5/data/hello.txt containing "Hello\nWorld"
// Expected stdout:
// Hello
// World
f <- @file{"tests/m5/data/hello.txt"}
f >> #(line) => {
    >_(line)
}
```

### T5.02 — Write to file

```
// File: tests/m5/t02_write_file.styio
// Expected: creates /tmp/styio_out.txt containing "Hello from Styio"
"Hello from Styio" >> @file{"/tmp/styio_out.txt"}
```

### T5.03 — Read and process (sum numbers from file)

```
// File: tests/m5/t03_process.styio
// Setup: create tests/m5/data/numbers.txt containing "10\n20\n30"
// Expected stdout: 60
@[total = 0](sum = $total)
f <- @file{"tests/m5/data/numbers.txt"}
f >> #(line) => {
    sum += line
}
>_(sum)
```

*Note: T5.03 depends on M6 state containers. Mark as **stretch goal**.*

### T5.04 — Resource auto-cleanup (RAII)

```
// File: tests/m5/t04_raii.styio
// Expected: file handle closed after scope exits, no resource leak
// Verify via debug output or OS handle count
{
    f <- @file{"tests/m5/data/hello.txt"}
    f >> #(line) => { >_(line) }
}
// f is no longer valid here
```

### T5.05 — Resource with auto-detect protocol

```
// File: tests/m5/t05_auto_detect.styio
// Expected stdout: contents of the file
f <- @{"tests/m5/data/hello.txt"}
f >> #(line) => { >_(line) }
```

### T5.06 — Non-existent file (fail-fast)

```
// File: tests/m5/t06_fail_fast.styio
// Expected: program terminates with error message, no crash/segfault
f <- @file{"nonexistent.txt"}
```

### T5.07 — Redirect to resource (`->`)

```
// File: tests/m5/t07_redirect.styio
// Expected: writes "42" to /tmp/styio_val.txt
x = 42
x -> @file{"/tmp/styio_val.txt"}
```

### T5.08 — Pipe from resource through function

```
// File: tests/m5/t08_pipe_func.styio
// Setup: tests/m5/data/numbers.txt containing "10\n20\n30"
// Expected stdout:
// 20
// 40
// 60
# double_it := (x: i32) => x * 2
@file{"tests/m5/data/numbers.txt"} >> #(line) => {
    >_(double_it(line))
}
```

---

## Implementation Tasks

### Task 5.1 — Resource driver runtime interface
**Role:** CodeGen Agent + new Runtime module  
**Action:** Implement a minimal C++ runtime library (`StyioRuntime`) with:
- `styio_file_open(path) → handle`
- `styio_file_read_line(handle) → string | @`
- `styio_file_write(handle, data)`
- `styio_file_close(handle)`
Register these as external symbols in `ExternLib.cpp` for JIT resolution.

### Task 5.2 — Parse `<-` (handle acquisition)
**Role:** Parser Agent  
**Action:** `identifier <- resource` produces `HandleAcquireAST(name, resource)`.

### Task 5.3 — Parse `>>` as write-to-resource
**Role:** Parser Agent  
**Action:** `expression >> @file{...}` (or `@{...}`) produces `ResourceWriteAST(expr, resource)` when `>>` is followed by a file resource; otherwise `>>` continues to mean read/iterate as in Task 5.5.

### Task 5.4 — Parse `->` as redirect
**Role:** Parser Agent  
**Action:** `expression -> resource` produces `ResourceRedirectAST(expr, resource)`.

### Task 5.5 — Parse `resource >> #(var) => { body }`
**Role:** Parser Agent  
**Action:** When a resource (or handle identifier) is followed by `>>`, parse as stream iteration where the resource is the source. Differentiate from collection iteration by the source type.

### Task 5.6 — IR lowering for resource nodes
**Role:** Analyzer Agent  
**Action:** Lower resource AST nodes to `SIO*` IR nodes. `HandleAcquireAST` → `SIOOpen`. Resource iteration → `SIORead` in a loop. Write → `SIOWrite`. Redirect → `SIOWrite` + close.

### Task 5.7 — LLVM codegen for resource operations
**Role:** CodeGen Agent  
**Action:** Generate calls to the runtime functions registered in Task 5.1. For resource iteration, generate a loop that calls `read_line` until it returns `@`, then calls `close`.

### Task 5.8 — RAII cleanup insertion
**Role:** CodeGen Agent  
**Action:** Track all open handles in the current scope. At every scope exit point (normal exit, `^` break, `<|` return), insert `styio_file_close` calls.

### Task 5.9 — Fail-fast on missing resources
**Role:** Runtime  
**Action:** `styio_file_open` checks if file exists. If not, print error message with file path and line number, then call `exit(1)`.

### Task 5.10 — Test data files
**Role:** Test Agent  
**Action:** Create `tests/m5/data/hello.txt` and `tests/m5/data/numbers.txt` with the contents specified in the test descriptions.

---

## Completion Criteria

**M5 is complete when:** T5.01, T5.02, T5.04–T5.08 produce correct behavior. T5.03 deferred to M6. All M1–M4 tests still pass.
