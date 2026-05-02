# Resource Identifiers

**Purpose:** Define the current Styio resource identifier design surface as compact built-in symbolic definitions.

**Last updated:** 2026-04-24

These are compiler built-ins, not user-authored wrapper declarations.

```styio
@stdout := { xs >> [>_] }
@stderr := { !(xs) >> [>_] }

@stdin  := { <|[>_] }
@stdin  := { <|(>_) }
@stdin  := { <| <- [>_] }

// Scalar and compatibility terminal-device spellings.
@stdout := { x -> [>_] }
@stderr := { !(x) -> [>_] }
@stdout := { xs >> (>_) }
@stderr := { !(xs) >> (>_) }
@stdout := { x -> (>_) }
@stderr := { !(x) -> (>_) }
@stdin  := { <| <- (>_) }

@file{path} := { file(path) }
@{path}     := @file{path}

@stdin: list[T] := { read_as<list[T]>(@stdin) }
```

| Identifier | Direction |
|------------|-----------|
| `@stdin` | read |
| `@stdout` | write |
| `@stderr` | write |
| `@file{path}` | read/write |
| `@{path}` | read/write |
| `@stdin: list[T]` | read |

`[>_]` is the canonical terminal-handle spelling inside symbolic standard-stream definitions.
`<|(>_)` is the call-like compatibility shorthand that treats `<|` as the return/export form and
`(>_)` as its terminal-device argument. `@stdin` is consumed as an iterable stream through
`@stdin >> #(line) => { ... }`; older `<< (>_)` wording is not the design spelling for stdin
reads.

`value -> [>_]` and `value -> @stdout` write scalar/text output. `items >> [>_]` and
`items >> @stdout` are only for iterable values whose items can be text-serialized. A plain
string must not use `>>`; write it with `->` or split it explicitly with
`text.lines() >> [>_]` / `text.lines() >> @stdout`.

Target-only driver identifiers such as `@mysql{...}`, `@http{...}`, and `@kafka{...}` are not part of this current surface.
