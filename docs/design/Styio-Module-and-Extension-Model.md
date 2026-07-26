# Styio Module and Extension Model

**Purpose:** Define the accepted `D4-MODULES` package/module identity,
visibility, import/export, initialization, public-contract, and coherence
model.

**Last updated:** 2026-07-26

**Status:** Accepted owner decision `D4-MODULES`.

## 1. Canonical identity

A resolved package identity contains its canonical source, package name, and
resolved version. A module identity is the package identity plus its canonical
slash-separated module path. A declaration identity additionally contains its
declared name and namespace.

Two resolved versions of the same named package have distinct nominal type and
protocol identities. Filesystem spelling, import alias, search order, and
re-export path do not replace canonical identity.

`/` is the only source module-path separator. The former dot-path
compatibility spelling is removed rather than normalized indefinitely.

## 2. Import and export surface

Imports are top-level and explicit. Importing a module binds a module
namespace; it does not inject all members into the current scope. Selective
imports and aliases are explicit. Glob imports are not admitted.

The canonical surface is:

```styio
@import {
    std/text as text,
    app/model::{User, Role},
}

@export {
    User,
    create_user,
}
```

Import/export lists use commas and permit a trailing comma. Semicolon as an
equivalent list separator and dot-path compatibility are removed.
Re-exporting a foreign declaration must name its canonical imported
declaration explicitly; visibility never propagates accidentally.

All declarations are module-private by default. Only declarations named by an
explicit export declaration form the public module API.

## 3. Namespaces and resolution

Module, type, value, and `@resource` identities occupy statically distinguishable
namespaces. Completion families are nominal type-level identities. The same
text may appear in different namespaces when syntax selects one uniquely; an
ambiguous context is a compile error.

Resolution never uses first-found, last-imported, or backend declaration order.
Unresolved, multiply resolved, or version-confused names fail closed.

## 4. Initialization and dependencies

Version 1 rejects cyclic module dependencies. It also rejects implicit
effectful top-level initialization. Compile-time constants and proven pure
module values may be initialized statically; resource acquisition, I/O, task
creation, and other effects require an explicit callable/session entry.

This conservative rule may later be relaxed only when the compiler can prove
a deterministic, cycle-safe initialization result.

## 5. Public contracts and coherence

Public, recursive, native/FFI, and typed protocol boundaries publish stable
canonical contracts. A uniquely inferred principal callable contract may be
published according to
[Styio Callable Principal Inference](./Styio-Callable-Principal-Inference.md);
first use, import order, backend state, and future calls cannot determine it.

A protocol implementation is globally coherent:

- the implementing package owns the protocol or the target type;
- one concrete protocol/type relation has one implementation in the resolved
  graph;
- overlapping implementations and specialization are not admitted;
- an extension cannot change an existing type's inherent operation,
  conversion, or member-resolution result.

Protocol requirements may be inferred from a callable body, but conformance of
a concrete user type is explicit. Method-name coincidence is not structural
duck typing.

## 6. Prelude boundary

The prelude contains only foundational language identities and closed core
relations. Codecs, decoding, normalization, JSON, CSV, file/network/database
APIs, drivers, and domain adapters require explicit standard-library imports.

Prelude short names remain ordinary and may be shadowed. Shadowing changes
name resolution, not the canonical identity or compiler behavior of the
original declaration.
