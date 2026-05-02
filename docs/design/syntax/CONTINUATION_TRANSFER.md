# Continuation Transfer

**Purpose:** Define the compact syntax and one-shot lifecycle for continuation transfer.

**Last updated:** 2026-04-24

```styio
f <| a        := f(a)
f <| a <| b  := f(a)(b)
f(a)(b)      := (f(a)) <| b

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

Resource definition shorthand:

```styio
@stdin := { <|[>_] }
@stdin := { <|(>_) }
@stdin := { <| <- [>_] }
@stdin := { <| <- (>_) }  // compatibility terminal-device spelling
```

Inside symbolic standard-stream definitions, `<|` marks the value exported from the definition
block. The compact stdin form `<|[>_]` is shorthand for the expanded pull-return form
`<| <- [>_]`. The call-like form `<|(>_)` is also accepted at the design level: it reads as
`<|` returning/exporting the terminal device argument. The bracketed terminal handle `[>_]`
replaces the earlier `| >_ |` spelling to avoid a `|>` visual/tokenization ambiguity; `(>_)`
remains a compatibility spelling.

Lifecycle:

```text
suspended -> resumed
suspended -> discontinued
resumed/discontinued -> invalid
```

Captured continuations are one-shot by default. Each captured continuation must be resumed or discontinued exactly once; double resume is an error. While suspended, the continuation keeps its captured scope alive.

Runtime lowering currently supports named apply (`f <| a`) as `f(a)`; returned-continuation lowering is pending and fails closed as a one-shot continuation lowering error.
