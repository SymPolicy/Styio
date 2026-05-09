# Continuation Transfer

**Purpose:** Define the compact syntax and one-shot lifecycle for continuation transfer.

**Last updated:** 2026-05-04

```styio
f <| a        := f(a)

k : cont[A -> B, oneshot]
k <| a       := resume k with a

<| value     := return value
|<| value |; := return value

;  := statement_sep
|; := statement_sep
```

| Form | Rule |
|------|------|
| `<|` at statement start | return from current block |
| `<|` between expressions | left-associative one-shot resume/apply |
| `|<| ... |;` | inline return for one-line blocks |
| `|>` | reserved |
| `|<-` | reserved |

Standard-stream declaration forms:

```styio
@ stdin := #() => { <|[>_] }
@ stdin := #() => { <| <- [>_] }
```

These are internal Styio resource declarations. `<|` marks the value exported from the declaration
body. The compact stdin form `<|[>_]` is shorthand for the expanded pull-return form
`<| <- [>_]`. The bracketed terminal handle `[>_]` avoids a `|>` visual/tokenization ambiguity.
Compatibility terminal-handle aliases may remain in the prelude, but new syntax docs should use
the bracketed form.

Lifecycle:

```text
suspended -> resumed
suspended -> discontinued
resumed/discontinued -> invalid
```

Captured continuations are one-shot by default. Each captured continuation must be resumed or discontinued exactly once; double resume is an error. While suspended, the continuation keeps its captured scope alive.

Runtime lowering currently supports named apply (`f <| a`) as `f(a)`; returned-continuation lowering is pending and fails closed as a one-shot continuation lowering error.
