# Styio — Resource Topology & `@` Semantics (Design Spec)

**Purpose:** `@` 资源定义、类型长度后缀、资源读取/复制/迭代、以及资源拓扑安全检查的设计级单一叙述；模块导入语法见 [`Styio-Language-Design.md`](./Styio-Language-Design.md) 与 [`Styio-EBNF.md`](./Styio-EBNF.md)。与当前编译器差异见 [`../rollups/NEXT-STAGE-GAP-LEDGER.md`](../rollups/NEXT-STAGE-GAP-LEDGER.md)。

**Last updated:** 2026-07-26

**Status:** resource topology source syntax plus current compiler-owned RTG validation.
**Supersedes:** retired state-resource state containers, history probes, and shadow reads. The running compiler rejects those families; exact old spellings are recoverable from Git history and remain covered only by negative migration tests.
**See also:** [Styio Structured Resources and
Concurrency](./Styio-Structured-Resources-and-Concurrency.md),
[`Styio-EBNF.md`](./Styio-EBNF.md) (Appendix: resource topology),
[`../rollups/NEXT-STAGE-GAP-LEDGER.md`](../rollups/NEXT-STAGE-GAP-LEDGER.md).

---

## 1. Why this document exists

Global, persistent resources must not look like local function calls. The active resource-topology surface separates:

- **Resource identity:** `@name` is a resource object or resource entry, not a scalar latest value.
- **Value shape:** type expressions define scalar, tuple, fixed-length, recent-window, and unbounded sequence shapes.
- **Flow:** `->` writes into sinks; `<<` explicitly copies resources or snapshots; every block-entry surface such as `>>`, `=>`, `?=`, active `||>`, and resource sessions `|?|` enters a snapshot-backed resource context and commits at block exit. Settlement-forward `|>` after a session transfers settlement responsibility.
- **Scope:** globally visible `@name` topology nodes belong at program root
  (§4.1). Resource *sessions* (`|?| { ... }`, §4.2) are the settled explicit
  scope for local handles and anchors; they do not authorize local topology
  declarations. Scoped subtopology remains a separate fail-closed reserve.

---

## 2. The topology-relevant roles of `@`

`@` marks things that are anchored outside the current pulse: external drivers, standard streams, or named resources.

The running compiler also reserves top-level `@import { ... }` as a module declaration. That role is real, but it is not part of the resource-topology model owned by this document.

| Role | Meaning | Typical surface form |
|------|---------|----------------------|
| **A. Resource anchor** | External driver / file / exchange handle | `@file(...)`, `@binance(...)`, `@stdin` |
| **B. Named resource object** | Persistent resource, sequence, stream, snapshot slot, or topology output | `@price : f64|..10| := { ... }` |

`@` has no missing-value role. A driver result that may be absent must expose
`? | T` in its static value shape and produces `(?)` for the empty branch.
Debugger-only provenance may use an internal display marker, but that marker is
not source syntax, resource identity, payload, or observable value state.

Host-provided resource anchors are explicit resource subjects. User code may operate them through
their declared capabilities, including explicit release/close when the resource family provides
that capability. Labels such as read-only or write-only describe data-flow direction; they do not
make the resource borrow-only.

**Parser rule:** `@ import { ... }` is an import declaration; `@ident ( ... )` is an explicit resource atom; `@ident { ... }` is invalid for explicit resources; `@ident : Type` is a resource declaration. Retired state-container prefixes are parse errors; use top-level `@name : Type`, `expr -> @name`, and `@name[-1]` selectors.

---

## 3. Type shapes, length, and repetition

### 3.1 Core type forms

| Notation | Meaning |
|----------|---------|
| `i64`, `f64`, `bool`, `char` | Scalar types |
| `string` | Character sequence type |
| `(A, B)` | Pair / tuple type |
| `list[T]` | Materialized ordered value collection of `T` |
| `dict[K, V]` | Materialized deterministic map value |
| `T|n|` | Exactly `n` values of type `T` |
| `T|..n|` | Recent-window sequence that keeps the latest `n` values of type `T` |
| `T..`, `T...` | Unbounded repetition of `T`; two or more dots are equivalent in type suffix position |

`T|n|` is length/cardinality. It does not mean "last n" unless written with the range prefix:

```styio
i64|10|     // ten i64 values
f64|..10|   // latest ten f64 values
i64..       // unbounded i64 sequence
i64...      // same as i64..
```

### 3.2 Type construction rules

Materialized collections, repetition/stream shapes, and text are distinct
canonical types. `list[T]` is not `T..`, `dict[K,V]` is not `(K,V)..`, and
`string` is not `char..`. No type-pattern rewrite may collapse these semantic
categories.

`T|n|` remains the direct exact-length form. Consequently
`list[i64]|10|` means ten list values, while `i64|10|` means ten integers.

---

## 4. Resource declaration and driver binding

Top-level resource declarations use a typed `@name`:

```styio
@name : Type := {
  StreamTopology
}
```

Multiple resources may share one driver:

```styio
@ma5 : f64|..2|, @ma20 : f64|..2| := {
  @file("tests/features/state_resources/data/prices.txt") >> #(p) => {
    avg(p, 5)  -> @ma5
    avg(p, 20) -> @ma20
  }
}
```

Internal resource prelude declarations may still use the function-body form:

```styio
@ stdin := #() => { <|[>_] }
@ file : ftype := #(path) => { ... }
```

This form defines built-in resource identity in Styio source. Runtime code may provide the substrate that the body lowers to, but it must not introduce a resource through an ungoverned C++ name registry.

### 4.1 Scope rule: topology root-only; sessions are handles-only

**Active rule for topology.** `@name` topology declarations are top-level only.
Declaring a named topology resource inside any local block — including inside
a `|?|` session — is rejected with the standing diagnostic: `The global
resource cannot be initialized in a local block.` The rule and the diagnostic
stay exactly as they are today.

**Three tiers.** Named topology nodes (`@name : Type := ...`) are root-only.
Local resource *handles* and inline *anchors* are the lifetime surface:
`f <- @file(...)`, `@file(...) >> #(line) => { ... }`. Resource sessions
(§4.2) give those handles an explicit structured-concurrency and settlement
boundary. They do **not** authorize local topology nodes.

**Reserved (separate from sessions): scoped subtopology.** Fail-closed. A
future activation may allow `@name` inside a strictly limited class of
session-shaped scopes under static multiplicity, no-escape, no-external-
reference, no-shadowing, and permanent exclusion of per-pulse closure bodies.
That direction remains blocked until its own design is pinned; §4.2 does not
activate it. Resources as first-class dynamic values stay permanently
rejected.

**Visibility rationale.** Root-only topology was chosen so readers can see
where every topology node lives. First-party tooling (IDE, highlighting,
debugger) is accepted as partial compensation for session-local *handles*;
it does not relax the topology root-only rule.

### 4.2 Resource session: `|?| { ... }`

Settled design; parser-pending and fail-closed until implementation evidence
lands. There is no `session` keyword: `|?| { ... }` *is* the session.

**One construct, four roles.** An explicit `|?|` session is simultaneously:

1. the qualifying resource scope for local handles and anchors,
2. the structured join boundary for accepted owned-child/task features,
3. a settleable resource operation, and
4. a stage that may defer settlement through `|>`.

**Placement (two surfaces, not one).**

- **Mid-transfer stage:** `|?|` sits between execution symbols, for example
  `# f => |?| { ... } |!|(cleanup) => handler` or `# f := |?| { ... } |> g`.
  `|?|` is not a statement opener by itself.
- **Statement-start settlement:** `?|` opens the statement and settles the
  session, for example `?| |?| { ... } | cleanup => handler` or
  `?| |?| { ... } |> next |> cleanup => handler`.

Examples:

```styio
# f => |?| {
    h <- @file("log.txt")
    h.write("ok")
} |!|(cleanup) => report_cleanup()

# f := |?| {
    h <- @file("log.txt")
} |> g

?| |?| {
    h <- @file("log.txt")
} |> next |> cleanup => handler

a => |?| {
    h <- @file("log.txt")
} |> b |> c |> cleanup => handler
```

**Body whitelist (conservative, settled).** Session bodies may acquire and use
**handles and anchors only** (`h <- @file(...)`, inline `@file(...)`, standard
streams, destroy sink). They may **not** declare topology nodes
(`@name : Type`). That keeps the standing local-block topology diagnostic
intact.

**Exit and settlement.**

1. **Default Close.** With no `|!|` and no deferred `|> ... |> cleanup`
   chain, owned close-capable handles release in reverse acquisition order at
   session exit (ordinary RAII Close).
2. **Special exit: `|!|(cleanup)` / `|!|(ResourceCleanupFailure)`.** Marks
   session-exit special handling for the cleanup effect family. The effect
   name aligns with the existing `cleanup` / `ResourceCleanupFailure` family;
   this is not a Java-style universal `Exception` catch-all.
3. **Deferred settlement: `|>`.** After the session leaves, settlement may be
   forwarded through one or more stages and settled at a later suitable site:
   `?| |?| { ... } |> next |> cleanup => handler`. `|>` is activated for this
   settlement / control transfer role (it is no longer a blank reserved
   symbol). `|<-` remains reserved.
4. **Effect-first obligation.** If any resource family acquired in the session
   has a fallible release, the compiler requires the session to be settled —
   either by `|!|(cleanup)`, by a statement-start `?| ... | cleanup =>`, or by
   a deferred `|> ... |> cleanup =>` chain. When every release is infallible,
   a bare session with default Close is allowed.
5. **Bounded multi-completion preservation.** The primary body completion keeps
   priority; cleanup completions occupy compiler-sized typed secondary slots and
   do not erase one another; reverse-order release continues so every declared
   cleanup completion remains available to the enclosing static settlement.
6. **Unhandled completion propagation.** If a fallible session is not settled at
   that site, its remaining nominal completion families become part of the
   enclosing operation or callable's static completion set. Only an explicit
   outermost program boundary may reject/report a still-unhandled family. There
   is no ambient program-level failure channel or dynamic propagation runtime.

**Escape discipline (Rust `thread::scope` layering, settled).**

1. Return, move-out, and store-into-outer-container of session-owned handles
   are statically rejected (affine ownership).
2. `||>` tasks spawned inside the session are joined at session exit.
3. This session contract does not activate continuation capture, resume, or
   discontinue semantics. If a continuation feature is admitted later, its
   session-exit behavior requires that feature's own owner decision.
4. v1 provides no Swift-style detached escape hatch. Work that must outlive
   the session must use root-declared resources.

**Snapshot / commit.** `|?| { ... }` is a block-entry surface: it opens a
resource snapshot at entry and commits at the matching `}`. Chained
`|>` stages after the session are settlement transfer, not additional
topology snapshot scopes, unless a stage itself enters a new block.

---

## 5. Resource reads, writes, copies, and iteration

Bare `@price` is the resource object itself, not the latest scalar value.

```styio
@price : f64|..10| := { ... }

latest = @price[-1]
prev   = @price[-2]
recent = @price[-3..]
all    = @price[...]
```

| Form | Meaning |
|------|---------|
| `expr -> @x` | Flow one produced value into resource sink `@x` |
| `x = @price[-1]` | Read a scalar value |
| `@price >> #(v) => { ... }` | Iterate the resource snapshot one produced item at a time; each item is pushed as a pulse into `v`, and the block commits at block exit |
| `a => { ... }` | Enter a block stage, operate on a resource snapshot context, and commit at block exit |
| `x ?= { arm => { ... } }` | Match block entry plus selected arm block entry; each block stage has its own snapshot/commit |
| `||> { ... }` | Task block entry; captures a resource snapshot context when constructing the task block |
| `snapshot << @price[...]` | Explicitly deep-copy the current enumerable snapshot into independent storage |
| `l <- @stdin: list[i32]` | Receive and bind from a resource entry |
| `l1 << l` | Explicitly deep-copy a cloneable resource into a fresh owner |

`<-` is for acquiring or receiving from a resource entry. It is not the general resource-copy operator. A resource already bound to `l` must not be copied as `l1 <- l`; use `l1 << l`.

Every block-entry surface enters a resource snapshot context at its operator boundary. This includes
`>>`, `=>`, `?=`, active `||>`, and resource sessions `|?| { ... }`. Settlement-forward `|>`
after a session transfers settlement responsibility and is not itself a topology snapshot scope
unless a stage enters a new block. The block operates on the snapshot rather than sharing mutable access to the original
resource context. When the block reaches `}`, the compiler commits the snapshot result back to the
source resource context for that stage. This rule covers resource state and resource effects;
ordinary lexical value scoping keeps its existing language rules.

Chained block stages commit one stage at a time. For example, `a => { 1 } => { 2 } => { 3 }`
means three independent snapshot/commit units: the first block snapshots `a` and commits at its
`}`, the second block snapshots that committed state and commits at its `}`, and the third block
does the same. The chain is therefore observable only at stage boundaries, not at every statement
inside a block.

`expr -> @name` creates a pending resource-write effect against the current resource context. Inside
a block-entered stage, that context is the block snapshot. Outside a block, it is the original resource.
The compiler should commit pending writes at the latest safe boundary rather than immediately, so
optimization and topology reasoning can keep room to fuse, reorder, or remove non-observable
intermediate writes. Barriers that force commit include same-resource reads, explicit snapshots,
iteration by another consumer, `flush`, `close`, resource-family release/commit hooks, explicit
happens-before edges, and block exit.

Accepted Q03-F sequencing distinguishes this logical effect graph from physical
instruction order. Lexical Block items order order-sensitive logical events,
while a pending write may still be fused, reordered, or removed when RTG proves
the same final state at every barrier. For `source -> endpoint`, source value and
endpoint capability are independent transfer prerequisites; the arrow's data
direction never substitutes for an RTG happens-before edge between their
preparation. See
[Functional Evaluation and Effect Ordering](./Styio-Functional-Evaluation-and-Effect-Ordering.md) §6/§8.

---

## 6. Selectors and slices

Selectors are type-checked against the left side. A scalar like `8[1..]` is a type error because `i64` is not indexable.

```styio
x[i]
x[-1]
x[a..b]
x[a..]
x[..b]
x[..]
x[...]
```

Rules:

- Negative indices count from the end; `x[-1]` is latest/last, `x[-2]` is previous.
- `x[a..b]` is the normal closed slice/range form unless a later collection-specific rule narrows it.
- `x[a..]` means from `a` to the end.
- `x[..b]` means from the start to `b`.
- `x[..]` and `x[...]` select all available values.
- Two or more dots are equivalent in selector/range separators: `a..b`, `a...b`, and `a.....b` normalize to the same separator.
- A single dot remains member access: `a.b`.

---

## 7. Intrinsics and hidden state (`avg(p, n)`)

User code writes the ordinary call `avg(p, 20)` (or a user-defined helper such
as `get_ma(p, 20)`). The removed word-mode selector spelling `p[avg, 20]` is
parser compatibility debt: selectors are a pure-symbol algebra, and named
computations live in the library namespace, recognized as compiler intrinsics
during semantic analysis. The compiler:

1. Fingerprints the triple `(source, avg, 20)` for deduplication.
2. Allocates implicit ledger slots for the raw samples and running state required by the intrinsic.
3. Returns a scalar per tick.

This hidden memory is not the same as a visible resource declaration. A strategy may publish only the latest two moving-average values:

```styio
@ma20 : f64|..2| := { ... }
```

while the intrinsic still keeps the 20 raw samples it needs internally.

Current implementation evidence is narrower: `avg` and `max` are active only for the current
i64 pulse/state intrinsic path described in
[Styio-StdLib-Intrinsics.md](./Styio-StdLib-Intrinsics.md). Other series operators and broader
numeric families remain deferred.

---

## 8. Golden Cross example

This example is illustrative topology design, not a complete current execution contract for every
shown numeric family.

```styio
@ma5 : f64|..2|, @ma20 : f64|..2| := {
  @file("tests/features/state_resources/data/prices.txt") >> #(p) => {
    avg(p, 5)  -> @ma5
    avg(p, 20) -> @ma20

    is_golden =
      @ma5[-2] <= @ma20[-2] &&
      @ma5[-1] >  @ma20[-1]

    ?(is_golden) => {
      order_logic(p)
    }
  }
}
```

The example uses `|..2|` because it needs the previous and current published values. The intrinsic `avg(p, 20)` still owns its required raw-history storage.

---

## 9. Implementation status (this repository)

| Item | Status |
|------|--------|
| Retired state-resource state family | **Retired**; active tests use negative migration fixtures |
| `@name : Type|n|`, `@name : Type|..n|`, `T..` / `T...` | **Implemented for resource declarations and selectors covered by feature tests** |
| Type arguments as `list[T]` / `dict[K, V]` | **Syntax implemented; any implementation path that normalizes them to `T..` / `(K,V)..` is D2 migration debt** |
| `__ : TypePattern := TypeExpr` type rewrite rules | **Implemented for type-position rewrite coverage** |
| Top-level multi-resource `@a : T, @b : U := { driver }` | **Target syntax**; current compiler only has partial internal prelude resource declarations |
| `expr -> @resource` as topology sink write | **Partially covered** by existing redirect/resource-write surfaces; strict topology semantics TBD |
| Resource object selectors `@price[-1]`, `@price[-3..]`, `@price[...]` | **Implemented for resource reads and bounded selector iterators across scalar/string/char/bool/f64/i64 plus list-valued, dict-valued, and matrix-valued resources; `@price[-3..]` / `@price[...]` materialize iterable snapshots, scalar latest selector iterators stay rejected, unbounded snapshots stay rejected, and retired state-history probes stay rejected** |
| Explicit selector/container copy `snapshot << @price[-n..]` / `snapshot << @price[...]` / `copy << list_or_dict_or_matrix` | **Implemented for bounded `i64`, `f64`, `bool`, `char`, `string`, `list`, `dict`, and `matrix` selector snapshots plus materialized list/dict/matrix handle deep clones; scalar latest reads, `copy <- list_or_dict_or_matrix`, and broader file/topology `<<` clone/copy remain staged or rejected** |
| Compiler-owned resource topology graph (RTG) | **Implemented for current resource AST surfaces** |

**Current RTG implementation note:** RTG is an internal compiler safety layer, not a new source-level syntax contract. It validates resource AST nodes and edges before lowering, including standard streams, handle acquire, writes, redirects, iterators, zip, snapshots, instant pulls, hidden intrinsic ledgers, task resources, ownership, mutation, commit, failure-domain, and backpressure relationships.

**Next compiler-work note:** [`../rollups/NEXT-STAGE-GAP-LEDGER.md`](../rollups/NEXT-STAGE-GAP-LEDGER.md) tracks the remaining parser/type/lowering migration work for this design.

---

## 10. Resource-management design references

Styio's resource model uses a small set of documented language and platform practices as references,
then maps them onto resource topology instead of copying another language's surface syntax.

| Industry practice | Source model | Styio decision |
|-------------------|--------------|----------------|
| Single owner plus invalidation after move/consume | Affine resource ownership: one active owner, automatic drop at scope end, and compile-time rejection of invalid references | Logical resources are move-only by default; consuming methods invalidate the receiver immediately; `<<` is the explicit deep copy/clone entry and must allocate independent storage or resource state |
| Automatic cleanup at scope exit | C++ RAII and resource handles; C# `using`; Java try-with-resources; Python `with` | Owned close-capable resources receive compiler-owned scope-exit drop edges. User code may call `.close()`, but language safety does not depend on remembering it |
| Deterministic cleanup order | Java/C# multi-resource cleanup and Go `defer` use deterministic cleanup ordering | Styio must keep scope-exit drops deterministic. When multiple owned resources are in one scope, later acquisitions release before earlier acquisitions unless RTG establishes a stricter dependency |
| Explicit method contract for cleanup | Java `AutoCloseable`, C# `IDisposable`, Python context manager protocol | Resource methods are resolved statically. Unknown methods, property-as-method calls, wrong arity, and final-binding overrides are compile errors |
| Failure behavior is part of the resource contract | Java suppressed close exceptions; C# disposal through `try/finally`; Python `__exit__` receives exception state | A cleanup operation exposes a finite nominal completion family and cannot silently lose a physical failure. Exact `family` / `family(binding)` settlement arms, recoverable-failure fallback, result join, and propagation follow the accepted operation-completion design. The resource family owns only when its physical state escalates into that family and the payload it supplies. |
| Concurrent mutation must be visible to the type/checking layer | Static alias-control and reference-capability systems prevent unsafe simultaneous access patterns | RTG rejects unordered exclusive resource accesses unless an explicit `=>` happens-before edge orders the accesses. Block-entry surfaces use snapshots instead of shared mutable access to the original resource context |
| Dynamic convenience is acceptable only below a static safety boundary | Python/Go make cleanup easy but rely more on runtime discipline | Styio may keep convenient `@("path").close()` calls, but consuming status must be known from the method table before lowering |

Source anchors used for this reference map:

1. C++ Core Guidelines resource management: <https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines?lang=en#Rr-raii>
2. Java try-with-resources and `AutoCloseable`: <https://docs.oracle.com/javase/tutorial/essential/exceptions/tryResourceClose.html>
3. C# `using` and `IDisposable`: <https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/statements/using>
4. Python `with` statement and context managers: <https://docs.python.org/3/reference/compound_stmts.html#the-with-statement>
5. Go `defer`: <https://go.dev/blog/defer-panic-and-recover>
6. Pony reference capabilities: <https://www.ponylang.io/learn/reference-capabilities/>, <https://tutorial.ponylang.io/reference-capabilities/guarantees.html>

## 11. Public wording boundary

Current repository-stage wording:

> Styio is a modern resource-management language with compiler-visible resource topology,
> move-only resource ownership by default, static consuming-method resolution, deterministic
> scope-exit cleanup, and explicit happens-before ordering for exclusive resource access.

Statements that require more evidence before publication:

1. **Do not overstate the lifetime model.** Styio has compiler-visible ownership, consumption, and
   topology checks, while full lifetime proof and method-family proof remain separate deliverables.
2. **Do not state complete failure-safety.** Drop/close failure is now typed as
   `ResourceCleanupFailure`, and the source fallback surface is fixed as
   `?| resource_operation | fallback`, but implementation coverage still needs
   resource-family tests before documenting full failure-safety.
3. **Do not state formally proven data-race freedom.** Current RTG rejects unordered exclusive
   resource accesses for covered AST surfaces, but a formal proof and broad async/task stress suite are
   separate deliverables.
4. **Do not state all external drivers are safe by construction.** Driver-level FFI, filesystem,
   network, pressure-observer behavior, and benchmark pressure behavior must be validated per
   driver family.

## 12. References

- Current implementation gaps: [`../rollups/NEXT-STAGE-GAP-LEDGER.md`](../rollups/NEXT-STAGE-GAP-LEDGER.md)
- Historical provenance: Git history for retired wording when exact old text is required
- Test coverage: [`../../workflows/TEST-CATALOG.md`](../../workflows/TEST-CATALOG.md)
- Maintainer workflow: [`../teams/CODEGEN-RUNTIME-RUNBOOK.md`](../teams/CODEGEN-RUNTIME-RUNBOOK.md)
