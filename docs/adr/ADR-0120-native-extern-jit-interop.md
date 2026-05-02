# ADR-0120: Native Extern JIT Interop

**Purpose:** Record the decision, context, alternatives, and consequences for compiling C/C++ `@extern` blocks and calling their symbols through the Styio JIT.

**Last updated:** 2026-04-29

## Status

Accepted

## Context

Styio needs package-level import/export semantics and must interoperate with existing C/C++ code because the compiler and runtime are already native C++ components. The first usable checkpoint must make syntax-only `@export` and `@extern(c) => { ... }` become executable behavior without inventing a separate foreign runtime.

The current compiler already lowers StyioIR to LLVM IR and resolves runtime helpers through ORC `absoluteSymbols`. That makes native interop safest when it follows the same explicit symbol-registration path instead of depending on process-global dynamic symbol visibility.

## Decision

1. Treat top-level `@extern(c) => { ... }` and `@extern(c++) => { ... }` as inline native compilation units. The C++ ABI spelling is case-insensitive for the `C` head, so `@extern(C++)` is equivalent to `@extern(c++)`.
2. Capture each native block body as a raw source slice after the `@extern(...) => {` boundary. Styio does not tokenize, rewrite, translate, or lower C/C++ body contents; the captured source is handed to the native compiler driver unchanged except for the small standard preamble.
3. Parse a conservative top-level C ABI function-signature subset from each native block and expose those names to type inference, StyioIR call lowering, and LLVM call emission. Signature discovery must not scan inside native function bodies.
4. Compile native blocks through the configured native toolchain resolver and key the resulting shared object by ABI, compiler command, compile flags, preamble, and raw source. Release installs package clang+LLVM into `bin/native-toolchain` by default. `STYIO_NATIVE_CC` / `STYIO_NATIVE_CXX` remain the highest-priority explicit overrides. Otherwise `STYIO_NATIVE_TOOLCHAIN_MODE=auto|bundled|system` controls whether Styio searches the packaged bundle before falling back to `cc` / `c++`.
5. Load compiled native objects with `dlopen`, resolve exported functions with `dlsym`, and register addresses into `StyioJIT_ORC` with `absoluteSymbols`.
6. Cache native shared objects under the user cache directory (`STYIO_NATIVE_CACHE_DIR`, `XDG_CACHE_HOME`, or `$HOME/.cache`) and keep loaded modules in a process-local module cache. `STYIO_NATIVE_CACHE=0` disables disk caching, but the process-local cache may still keep already loaded modules alive for the process lifetime.
7. Use `@export { ... }` as the current checkpoint's explicit native symbol filter. When present, only matching native functions become callable from Styio.
8. Require C++ blocks to expose callable Styio-facing symbols with `extern "C"` so the ABI stays stable and does not depend on C++ name mangling.

## Alternatives

1. Link by relying on `DynamicLibrarySearchGenerator` alone.
   - Rejected because test binaries and downstream hosts may not expose every native symbol in the process dynamic symbol table.
2. Embed libclang or a full C++ semantic frontend in-process.
   - Rejected for this checkpoint because invoking a packaged clang driver is enough to produce a usable mixed-compilation path without adding a libclang API surface.
3. Support arbitrary C++ ABI names directly.
   - Rejected because overloaded and mangled C++ names need a richer type and name-resolution model. The stable boundary is C ABI exports from C++ code.

## Consequences

Positive:

1. Styio source can now define native C/C++ helpers inline and call them through JIT execution.
2. `@export` is no longer syntax-only for native code; it gates callable exported native symbols.
3. The feature reuses existing LLVM ORC symbol-registration policy and keeps native calls visible in LLVM IR.
4. Release packages provide native C++ support by placing clang+LLVM under the configured native toolchain root, so users do not need a preinstalled system C++ compiler for `@extern(c++)` blocks.
5. C/C++ source layout, macro continuations, strings, comments, and raw string literals are preserved for the compiler frontend instead of being reconstructed from Styio tokens.
6. Repeated execution of identical native blocks avoids repeated compiler-driver startup after the first compile, and repeated in-process loading reuses the module handle.

Negative:

1. The first implementation supports a conservative scalar/pointer ABI subset, not structs, callbacks, variadics, or direct C++ overload resolution.
2. Native block compilation depends on a working clang+LLVM bundle or a working local C/C++ compiler fallback.
3. The first execution of a new native block still pays the native compiler and linker cost.
4. Package-manager graph resolution and external library package fetching remain separate follow-up work.
