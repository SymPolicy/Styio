# Styio — Resource Topology & `@` Semantics (Design Spec v2)

**Purpose:** `@` 资源定义、类型长度后缀、资源读取/复制/迭代、以及资源拓扑安全检查的设计级单一叙述；模块导入语法见 [`Styio-Language-Design.md`](./Styio-Language-Design.md) 与 [`Styio-EBNF.md`](./Styio-EBNF.md)。与当前编译器差异见 [`../review/Logic-Conflicts.md`](../review/Logic-Conflicts.md)。

**Last updated:** 2026-05-09

**Status:** Topology v2 source syntax plus current compiler-owned RTG validation.
**Supersedes:** M6 `@[n](name = ...)`, `$state[<<, n]`, and `$state` shadow reads. The running compiler now rejects those spellings; they remain only in archived provenance and negative migration tests.
**See also:** [`Styio-EBNF.md`](./Styio-EBNF.md) (Appendix: Topology v2), [`../review/Logic-Conflicts.md`](../review/Logic-Conflicts.md).

---

## 1. Why this document exists

Global, persistent resources must not look like local function calls. The v2 surface separates:

- **Resource identity:** `@name` is a resource object or resource entry, not a scalar latest value.
- **Value shape:** type expressions define scalar, tuple, fixed-length, recent-window, and unbounded sequence shapes.
- **Flow:** `->` writes into sinks; `<<` explicitly copies resources or snapshots; `>>` iterates.
- **Scope:** globally visible `@name` resources belong at program root unless a future scoped-resource rule says otherwise.

---

## 2. The topology-relevant roles of `@`

`@` marks things that are anchored outside the current pulse: external drivers, standard streams, or named resources.

The running compiler also reserves top-level `@import { ... }` as a module declaration. That role is real, but it is not part of the resource-topology model owned by this document.

| Role | Meaning | Typical surface form |
|------|---------|----------------------|
| **A. Honest missing** | Runtime absence produced by resources/intrinsics | no active source-level bare `@` |
| **B. Resource anchor** | External driver / file / exchange handle | `@file(...)`, `@binance(...)`, `@stdin` |
| **C. Named resource object** | Persistent resource, sequence, stream, snapshot slot, or topology output | `@price : f64|..10| := { ... }` |

**Parser rule:** `@ import { ... }` is an import declaration; `@ident ( ... )` is an explicit resource atom; `@ident { ... }` is invalid for explicit resources; `@ident : Type` is a resource declaration. Retired `@[...]` is a parse error; use top-level `@name : Type`, `expr -> @name`, and `@name[-1]` selectors.

---

## 3. Type shapes, length, and repetition

### 3.1 Core type forms

| Notation | Meaning |
|----------|---------|
| `i64`, `f64`, `bool`, `char` | Scalar types |
| `string` | Character sequence type |
| `(A, B)` | Pair / tuple type |
| `list[T]` | Unbounded sequence of `T` |
| `dict[K, V]` | Unbounded sequence of key-value pairs `(K, V)` with dictionary semantics |
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

The standard collection forms are specified through type-pattern rewrite rules:

```styio
__ : list[T] := T..
__ : string := char..
__ : dict[K, V] := (K, V)..
```

Rules:

- `__`, `___`, and any placeholder with two or more underscores mark a type rewrite rule.
- `_` remains available for value-level wildcard/ignored binding.
- These rewrites only apply in type position.
- Prefer `list[string]` when the source intent is a sequence of strings; raw `string..` is visually ambiguous because `string` is already sequence-shaped.
- The canonical fixed-length form is `T|n|`. A spelling like `list[i64]|10|` should normalize to `i64|10|` and should not be used in examples.

Examples:

```styio
list[i64] == i64..
string == char..
dict[string, string] == (string, string)..

i64|2|              // (i64, i64)
(string, string)|2| // ((string, string), (string, string))
```

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
  @file("tests/m6/data/prices.txt") >> #(p) => {
    p[avg, 5]  -> @ma5
    p[avg, 20] -> @ma20
  }
}
```

Internal resource prelude declarations may still use the function-body form:

```styio
@ stdin := #() => { <|[>_] }
@ file : ftype := #(path) => { ... }
```

This form defines built-in resource identity in Styio source. Runtime code may provide the substrate that the body lowers to, but it must not introduce a resource through an ungoverned C++ name registry.

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
| `@price >> #(v) => { ... }` | Iterate the resource object |
| `snapshot << @price[...]` | Explicitly copy the current enumerable snapshot |
| `l <- @stdin: list[i32]` | Receive and bind from a resource entry |
| `l1 << l` | Explicitly copy a resource |

`<-` is for acquiring or receiving from a resource entry. It is not the general resource-copy operator. A resource already bound to `l` must not be copied as `l1 <- l`; use `l1 << l`.

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

## 7. Intrinsics and hidden state (`p[avg, n]`)

User code may write `p[avg, 20]` or a helper such as `get_ma(p, 20)`. The compiler:

1. Fingerprints the triple `(source, avg, 20)` for deduplication.
2. Allocates implicit ledger slots for the raw samples and running state required by the intrinsic.
3. Returns a scalar per tick.

This hidden memory is not the same as a visible resource declaration. A strategy may publish only the latest two moving-average values:

```styio
@ma20 : f64|..2| := { ... }
```

while the intrinsic still keeps the 20 raw samples it needs internally.

---

## 8. Golden Cross example

```styio
@ma5 : f64|..2|, @ma20 : f64|..2| := {
  @file("tests/m6/data/prices.txt") >> #(p) => {
    p[avg, 5]  -> @ma5
    p[avg, 20] -> @ma20

    is_golden =
      @ma5[-2] <= @ma20[-2] &&
      @ma5[-1] >  @ma20[-1]

    ?(is_golden) => {
      order_logic(p)
    }
  }
}
```

The example uses `|..2|` because it needs the previous and latest published values. The intrinsic `p[avg, 20]` still owns its required raw-history storage.

---

## 9. Implementation status (this repository)

| Item | Status |
|------|--------|
| M6 `@[n](var = ...)`, pulse ledger, `$` refs | **Retired**; active tests use negative migration fixtures |
| `@name : Type|n|`, `@name : Type|..n|`, `T..` / `T...` | **Implemented for v2 resource declarations and selectors covered by milestone tests** |
| Type parameters as `list[T]` / `dict[K, V]` | **Implemented for v2 type-shape normalization covered by tests** |
| `__ : TypePattern := TypeExpr` type rewrite rules | **Implemented for type-position rewrite coverage** |
| Top-level multi-resource `@a : T, @b : U := { driver }` | **Target syntax**; current compiler only has partial internal prelude resource declarations |
| `expr -> @resource` as topology sink write | **Partially covered** by existing redirect/resource-write surfaces; strict topology semantics TBD |
| Resource object selectors `@price[-1]`, `@price[-3..]`, `@price[...]` | **Implemented for v2 resource reads; old `$state[<<, n]` is retired** |
| Compiler-owned resource topology graph (RTG) | **Implemented for current resource AST surfaces** |

**Current RTG implementation note:** RTG is an internal compiler safety layer, not a new source-level syntax contract. It validates resource AST nodes and edges before lowering, including standard streams, handle acquire, writes, redirects, iterators, zip, snapshots, instant pulls, hidden intrinsic ledgers, task resources, ownership, mutation, commit, failure-domain, and backpressure relationships.

**Next compiler-work note:** [`../plans/Resource-Topology-v2-Implementation-Plan.md`](../plans/Resource-Topology-v2-Implementation-Plan.md) tracks the parser/type migration to the new `Type|n|` / `Type|..n|` / `T..` source syntax.

---

## 10. Resource-management design references

Styio's resource model uses a small set of documented language and platform practices as references,
then maps them onto resource topology instead of copying another language's surface syntax.

| Industry practice | Source model | Styio decision |
|-------------------|--------------|----------------|
| Single owner plus invalidation after move/consume | Rust ownership and borrowing: one owner, automatic drop at scope end, and compile-time rejection of invalid references | Logical resources are move-only by default; consuming methods invalidate the receiver immediately; `<<` is the explicit copy/clone entry |
| Automatic cleanup at scope exit | C++ RAII and resource handles; C# `using`; Java try-with-resources; Python `with` | Owned close-capable resources receive compiler-owned scope-exit drop edges. User code may call `.close()`, but language safety does not depend on remembering it |
| Deterministic cleanup order | Java/C# multi-resource cleanup and Go `defer` use deterministic cleanup ordering | Styio must keep scope-exit drops deterministic. When multiple owned resources are in one scope, later acquisitions release before earlier acquisitions unless RTG establishes a stricter dependency |
| Explicit method contract for cleanup | Java `AutoCloseable`, C# `IDisposable`, Python context manager protocol | Resource methods are resolved statically. Unknown methods, property-as-method calls, wrong arity, and final-binding overrides are compile errors |
| Exception/error behavior is part of the resource contract | Java suppressed close exceptions; C# disposal through `try/finally`; Python `__exit__` receives exception state | First stage keeps drop failure as a runtime diagnostic. The language contract must document whether cleanup failures are primary, suppressed, or aggregated before stating full failure-safety |
| Concurrent mutation must be visible to the type/checking layer | Rust borrow rules and Pony reference capabilities prevent unsafe simultaneous access patterns | RTG rejects unordered exclusive resource borrows unless an explicit `=>` happens-before edge orders the accesses. Task bodies may borrow outer resources but cannot consume them |
| Dynamic convenience is acceptable only below a static safety boundary | Python/Go make cleanup easy but rely more on runtime discipline | Styio may keep convenient `@("path").close()` calls, but consuming status must be known from the method table before lowering |

Source anchors used for this reference map:

1. Rust ownership and borrowing: <https://doc.rust-lang.org/book/ch04-01-what-is-ownership.html>, <https://doc.rust-lang.org/book/ch04-02-references-and-borrowing.html>
2. C++ Core Guidelines resource management: <https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines?lang=en#Rr-raii>
3. Java try-with-resources and `AutoCloseable`: <https://docs.oracle.com/javase/tutorial/essential/exceptions/tryResourceClose.html>
4. C# `using` and `IDisposable`: <https://learn.microsoft.com/en-us/dotnet/csharp/language-reference/statements/using>
5. Python `with` statement and context managers: <https://docs.python.org/3/reference/compound_stmts.html#the-with-statement>
6. Go `defer`: <https://go.dev/blog/defer-panic-and-recover>
7. Pony reference capabilities: <https://www.ponylang.io/learn/reference-capabilities/>, <https://tutorial.ponylang.io/reference-capabilities/guarantees.html>

## 11. Public wording boundary

Current repository-stage wording:

> Styio is a modern resource-management language with compiler-visible resource topology,
> move-only resource ownership by default, static consuming-method resolution, deterministic
> scope-exit cleanup, and explicit happens-before ordering for exclusive resource access.

Statements that require more evidence before publication:

1. **Do not overstate the lifetime model.** Styio has compiler-visible ownership, consumption, and
   topology checks, while full lifetime proof and method-family proof remain separate deliverables.
2. **Do not state complete failure-safety.** Drop/close failure policy is still runtime-diagnostic
   first stage; suppressed/aggregated cleanup errors remain a later design decision.
3. **Do not state formally proven data-race freedom.** Current RTG rejects unordered exclusive
   borrows for covered AST surfaces, but a formal proof and broad async/task stress suite are
   separate deliverables.
4. **Do not state all external drivers are safe by construction.** Driver-level FFI, filesystem,
   network, and benchmark pressure behavior must be validated per driver family.

## 12. References

- Internal: [`../review/Logic-Conflicts.md`](../review/Logic-Conflicts.md) §1.3 `@` roles
- Milestones provenance: [`../archive/milestones/2026-03-29/M6-StateAndStreams.md`](../archive/milestones/2026-03-29/M6-StateAndStreams.md)
- Test coverage: [`../assets/workflow/TEST-CATALOG.md`](../assets/workflow/TEST-CATALOG.md)
- Maintainer workflow: [`../teams/CODEGEN-RUNTIME-RUNBOOK.md`](../teams/CODEGEN-RUNTIME-RUNBOOK.md)
