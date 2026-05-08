# Resource Identifiers

**Purpose:** Define the current Styio resource identifier design surface as internal Styio resource declarations.

**Last updated:** 2026-05-04

Resource identifiers are defined in Styio source, not by a C++ name registry. C++ may provide
lexer/parser support and runtime substrates such as terminal handles or file descriptors, but
new resources must enter through an internal Styio declaration of this form:

```styio
@ name : type := #(args) => { body }
```

The `#(...)` parameter list is mandatory whenever the declaration body uses local names. This is
the local-variable rule: a declaration body must not use an unbound name just because the name is
visually obvious.

Current standard-resource definitions live in [resources.styio](../../../src/StyioPrelude/resources.styio):

```styio
@ stdout := #(xs) => { xs >> [>_] }
@ stderr := #(xs) => { !(xs) >> [>_] }

@ stdin := #() => { <|[>_] }
@ stdin := #() => { <|(>_) }
@ stdin := #() => { <| <- [>_] }

// Scalar and compatibility terminal-device spellings.
@ stdout := #(x) => { x -> [>_] }
@ stderr := #(x) => { !(x) -> [>_] }
@ stdout := #(xs) => { xs >> (>_) }
@ stderr := #(xs) => { !(xs) >> (>_) }
@ stdout := #(x) => { x -> (>_) }
@ stderr := #(x) => { !(x) -> (>_) }
@ stdin := #() => { <| <- (>_) }

@ file : ftype := #(path) => { ... }
```

`@ file` deliberately does not use `file(path)`. That spelling was never an allowed Styio
primitive and must not be introduced as a hidden C++ escape hatch. The current implementation still
lowers `@file("path")` and `@{"path"}` through the existing file runtime substrate, but the
resource identity is governed by the Styio declaration above.

| Identifier | Definition form | Direction |
|------------|-----------------|-----------|
| `@ stdin` | `@ stdin := #() => { ... }` | read |
| `@ stdout` | `@ stdout := #(x-or-xs) => { ... }` | write |
| `@ stderr` | `@ stderr := #(x-or-xs) => { ... }` | write |
| `@ file` | `@ file : ftype := #(path) => { ... }` | read/write |
| `@stdin: list[T]` | typed ingestion adapter over `@ stdin` | read |

`[>_]` is the canonical terminal-handle spelling inside standard-stream definitions.
`<|(>_)` is the call-like compatibility shorthand that treats `<|` as the return/export form and
`(>_)` as its terminal-device argument. `@stdin` is consumed as an iterable stream through
`@stdin >> #(line) => { ... }`; older `<< (>_)` wording is not the design spelling for stdin
reads.

`value -> [>_]` and `value -> @stdout` write scalar/text output. `items >> [>_]` and
`items >> @stdout` are only for iterable values whose items can be text-serialized. A plain
string must not use `>>`; write it with `->` or split it explicitly with
`text.lines() >> [>_]` / `text.lines() >> @stdout`.

Target-only driver identifiers such as `@mysql(...)`, `@http(...)`, and `@kafka(...)` are not part of this current surface.
