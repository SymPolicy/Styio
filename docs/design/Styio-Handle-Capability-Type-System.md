# Styio Handle, Capability, and Failure Type System

**Purpose:** 为 Styio 的资源值、`@stdin/@stdout`、`<<`、可迭代对象、以及默认失败处理建立统一的设计级类型系统；该文档定义目标模型，不等同于当前实现。

**Last updated:** 2026-07-26

**Status:** Target design — not fully implemented in the current compiler.  
**See also:** [`Styio-Language-Design.md`](./Styio-Language-Design.md), [`Styio-Resource-Topology.md`](./Styio-Resource-Topology.md), [`../rollups/NEXT-STAGE-GAP-LEDGER.md`](../rollups/NEXT-STAGE-GAP-LEDGER.md).

Accepted `D3-RESOURCES` in [Styio Structured Resources and
Concurrency](./Styio-Structured-Resources-and-Concurrency.md) is authoritative
where this older target draft differs. In particular, capability and protocol
state do not by themselves make a value an affine owner.

---

## 1. Why this document exists

The current compiler mixes three different concerns:

1. **Representation:** what a runtime value physically is.
2. **Capability:** what operations the value supports.
3. **Protocol state:** whether a resource is open, exhausted, writable, materialized, and so on.

This leads to ad hoc special cases:

- Historically, scalar and list-shaped `@stdin : T` pulls drifted into separate implementation paths; current work keeps typed stdin ingestion on one `InstantPullAST` path before type-directed lowering.
- Iteration is dispatched partly by `NodeType`, not by a unified type protocol.
- `<<` currently behaves differently depending on parser shape instead of a single type-directed rule.

This document defines a target design that unifies these cases.

Priority note (settled decision): the type-directed unification of `<<` is a
deliberately **low-priority** workstream. The current copy/snapshot and
compatibility-pull behaviors are considered adequate for now, no active
convergence work is scheduled, and `<<` proposals should not be re-raised as a
design discussion until this priority is explicitly changed.

---

## 2. Design goals

1. Give every resource-like value a common capability description without
   collapsing its value/owner/borrow classification.
2. Distinguish **iterable** from **non-iterable** statically.
3. Make `<<` mean one thing: **feed items into the left side one by one**.
4. Preserve protocol state while distinguishing values, affine owners, and
   lexical borrows.
5. Avoid mandatory user-visible `unwrap`; failed operations should still be typed, but default handling should abort with diagnostics.
6. Support destructive update safely for unique resources and materialized collections.

---

## 3. Core model

Styio should model resource-bearing values as a typed handle family:

```text
Handle<Rep, Item, Caps, State, OwnerKind, ExitObligation>
```

Where:

- `Rep` is the low-level representation family.
- `Item` is the element type produced, consumed, or stored by the handle.
- `Caps` is a compile-time capability set.
- `State` is the current protocol state.
- `OwnerKind` is value, affine owner, or lexical borrow/view.
- `ExitObligation` records close, release, consume, join/settle, or none.

Examples:

- `@stdin : Handle<fd, string, {pull, iter, close}, open>`
- `@stdout : Handle<fd, string, {push, close}, open>`
- `list[i32]` and `matrix[f64]` are materialized values unless their semantic
  elements contain an owner; allocation does not make them affine.
- `range[i64]` is a value.
- an exclusive cursor/subscription is an affine handle with a settle/close
  obligation.

This notation is **design-level**, not fixed user syntax. The important part is the separation of concerns.

---

## 4. Runtime layout vs static type

Styio resources should be thought of as layered handles, but not all layers belong in the runtime object itself.

### 4.1 Runtime handle header

At runtime, a resource handle may carry metadata such as:

- base pointer / file descriptor / opaque driver pointer
- length
- capacity
- cursor / current offset
- codec / parse mode
- driver tag

This is close to a fat pointer or descriptor.

### 4.2 Static type metadata

These must remain compile-time facts rather than plain runtime fields:

- iterable or not
- indexable or not
- writable or not
- cloneable or not
- legal protocol transitions

For example, `iterable` must not be treated like a regular dynamic boolean property. It is a typing fact used by the checker.

---

## 5. Capability system

A Styio value is iterable if and only if its type carries an iteration capability.

### 5.1 Initial capability set

The accepted capability baseline supports exactly these public capabilities:

| Capability | Meaning |
|------------|---------|
| `iter` | Can produce a sequence of `Item` values |
| `pull` | Can produce at most one `Item` per explicit pull step |
| `push` | Can accept `Item` values one by one; the write may remain pending until a safe commit boundary |
| `index` | Supports random access by integer index |
| `slice` | Supports range or window selection |
| `sized` | Supports `.length` / `.size` |
| `collect` | Can accumulate a stream or iterable into a materialized container |
| `clone` | Supports deep clone into independent owned storage or resource state |
| `close` | Has an explicit close / release protocol |

The compiler-closed constraint relations consumed by `Q02-INF` are not members
of this public handle/resource capability set. Exact terms are owned by
[Styio Exact Numeric Literals](./Styio-Exact-Literals-and-Builtin-Add.md), and
accepted [Q05-NUMERIC-OPS](./Styio-Builtin-Numeric-Operators-and-Inference.md)
owns the finite operator/widening catalog. Internal relation names and nominal
operation completions such as `overflow` are not handle capabilities. Neither
creates structural duck typing or user-definable capability/operator
instances. Any future author-written instances or constraints require the
separate `F02` admission decision.

### 5.2 Derived concepts

These are not separate runtime kinds; they are predicates over capabilities:

- **Iterable[T]**: any type with `iter` over `T`
- **Indexable[T]**: any type with `index` over `T`
- **Sliceable[T]**: any type with `slice` over `T`
- **Writable[T]**: any type with `push` over `T`
- **Sized**: any type with `sized`
- **Cloneable**: any type with `clone`

So Styio should check:

- `>>` requires `Iterable[T]` and advances that iterable item by item into the right-side channel/consumer
- `zip` requires both sides to be `Iterable`
- single-index `[]` requires `Indexable`
- range or window selection requires `Sliceable`
- `.length` and `.size` require `Sized`
- `<<` requires a left-hand sink with `push` or `collect`

---

## 6. Typestate

Resources should also carry protocol state.

### 6.1 Initial states

The initial useful state set is:

- `open`
- `eof`
- `closed`
- `materialized`

### 6.2 Examples

| Value | Typical state |
|-------|---------------|
| `@stdin` | `open`, later possibly `eof` |
| `@stdout` / `@stderr` | `open` |
| file handle | `open`, `eof`, `closed` |
| list | `materialized` |
| range | `materialized` |

### 6.3 State transitions

Examples of desired transitions:

- `stdin.open --pull--> stdin.open | stdin.eof`
- `file.open --close--> file.closed`
- `list.materialized --index--> list.materialized`

When the state is statically known, invalid operations should be compile-time errors.
When an admitted operation can complete in several ways, Sema exposes one
success type plus its finite nominal completion-family set. `?|` may settle an
exact `family` / `family(binding)` arm or a final recoverable-only fallback;
unhandled families propagate statically. There is no statement discard, default
failure handler, ambient channel, or dynamic handler lookup. `failed` is not a
persistent resource state.

---

## 7. Standard resource families

### 7.1 `@stdin`

`@stdin` should be a raw input stream value, not a special parser escape hatch:

```text
@stdin : stream[string]
```

Conceptually:

```text
Handle<fd, string, {pull, iter, close}, open>
```

This means:

- it is iterable
- it is readable
- it is not writable
- it is not indexable
- it is not sized in the general case
- it may be explicitly released/closed by user code through its resource operation surface

### 7.2 `@stdout` / `@stderr`

These are write sinks:

```text
@stdout : writer[string]
@stderr : writer[string]
```

Conceptually:

```text
Handle<fd, string, {push, close}, open>
```

### 7.3 `list[T]`

Lists are materialized containers:

```text
list[T] : Handle<ptr, T, {iter, push, index, sized, collect, clone}, materialized>
```

### 7.4 `matrix[T]`

Matrices are materialized numeric containers backed by a flat row-major runtime handle:

```text
matrix[T] : Handle<matrix, T, {index, sized, clone, close}, materialized>
```

Typed bindings such as `m: matrix = [[...], [...]]` use nested list syntax as the source form, but
the typed context validates rectangular numeric rows and lowers to a matrix handle instead of a
list-of-lists handle. The static type carries element kind plus row/column facts when dimensions
are known, so Sema can reject incompatible `+`, `-`, `*`, and intrinsic calls before CodeGen.

### 7.5 `range[T]`

Ranges are iterable but not necessarily indexable:

```text
range[T] : Handle<imm, T, {iter}, materialized>
```

---

## 8. Formal meaning of `<<`

`<<` should have one semantic idea only:

> Feed values from the right side into the left side one item at a time.

It should not be split into unrelated “clone” and “write” meanings at the language-design level.

### 8.1 Type-directed cases

Given a left side `L` and right side `R`:

1. If `L` has `push[T]` and `R : T`, then push one item.
2. If `L` has `push[T]` and `R` is `Iterable[T]`, then drain `R` into `L`.
3. If `L` has `collect[T]` and `R` is `Iterable[T]`, then materialize the result in `L`.
4. If `L` is an unbound identifier in definition position, `L << R` means:
   create a default collector for `R`, then drain `R` into it.

So in the target design, `<<` can still model generic iterable drainage, but stdin keeps a
more explicit surface:

- `@stdin >> #(line) => { ... }`
  means iterate terminal input one line at a time.
- `value = (<- @stdin)`
  means perform a one-shot immediate pull from stdin.

Do not use `a << @stdin` or `lines << @stdin` as the current stdin design spelling. If a
program needs a materialized list of stdin lines, collect explicitly inside the iterator body or
use a future named typed-read API.

### 8.2 Relationship with cloning

A clone is just a special case where the left side is a collector or sink over resource items and the right side is a cloneable iterable/resource source.

For resources, `clone` means a deep copy:

- allocate independent storage or resource state,
- copy the reachable resource contents,
- return a fresh owner, and
- do not create a second binding that shares the same mutable backing resource.

Styio does not use reference-counted clone semantics for resource values. The checker may still
lower some cases to `clone` internally, but the user-visible semantics of `<<` remains “one by
one into the left side” and the resulting resource owner is independent.

---

## 9. Binding modes

Binding syntax should remain orthogonal to handle capabilities.

### 9.1 `<-`

`a <- expr`

- creates a **final** slot
- the name cannot be rebound
- the underlying resource may still be mutated if its capabilities allow it

### 9.2 `=`

`a = expr`

- creates a **flex** slot
- the name may be rebound
- if the old occupant owns a resource, reassignment must release it immediately

### 9.3 `<<`

`a << rhs`

- is not primarily a rebinding operator
- it is a feed / collect operator
- in definition position, it may synthesize a new collector slot from the right-hand item type

This keeps name mutability separate from data-flow direction.

---

## 10. Iterable and non-iterable values

The language should stop deciding iterability by AST shape.

### 10.1 Current issue

Today, parts of the compiler branch on `NodeType` such as:

- `Range`
- `StdinResource`
- `FileResource`
- `Id`

Instead, iteration should ask only:

```text
does this value implement Iterable[T]?
```

### 10.2 Target rule

For `expr >> #(x) => body`, typing succeeds iff:

- `expr : X`
- `X` has `iter`
- the yielded item type is `T`
- `x : T`
- the block runs on a resource snapshot created at the `>>` boundary
- block exit commits the snapshot result back to the source resource when the resource family supports commit

The operator transfers iterable items as pulses into the body. It is not a bit-shift,
single read, or bulk pipe operation.

The same rule should drive:

- plain iteration
- zip
- collect
- `<<` draining

---

## 11. Failure model

Styio should distinguish “end of sequence” from “operation failure”.

### 11.1 Internal models

Use two distinct semantic facts:

- iteration yields `T` or reaches the nominal EOF terminal family;
- every operation has one success type plus a finite set of nominal completion
  families.

Neither fact requires or exposes a source `Step[T]` or `Result[T, E]` wrapper.
EOF is not absence and a failure is not an alternate ordinary value.

### 11.2 Default surface behavior

The accepted model is defined by
[Styio Operation Completion and Settlement](./Styio-Operation-Completion-and-Settlement.md).
`?|` runs one operation once, exact family arms optionally bind payloads, a
trailing bare fallback catches recoverable failures only, normal results join
to the success type, and unhandled families propagate statically. There is no
implicit unwrap, dynamic exception search, ambient failure channel, wildcard
discard, or hidden retry.

Current `ResourceEffectAST` / `SIOResourceEffect` behavior remains
implementation evidence and must migrate to this contract rather than define
the public type system.

### 11.3 Examples

- writing to `@stdout` may have a no-payload success and an I/O completion family;
- typed stdin decoding may produce a list or a parse/decode completion family;
- checked indexing may produce an element or a bounds completion family;
- closing or dropping a resource may succeed without payload or expose cleanup failure;
- backpressure begins as an observable resource signal. Whether a resource
  upgrades it to failure belongs to that resource-family contract; settlement
  itself never implies retry, waiting, replay, or scheduling.

These examples intentionally do not spell an internal `Result` type or a
default dynamic handler because neither belongs to the language contract.

---

## 12. Typed stdin ingestion

The current special form `@stdin: list[T]` should be reinterpreted as typed ingestion from a raw stream:

```text
read_as<list[T]>(@stdin)
```

Design consequences:

1. `@stdin` itself stays a raw `stream[string]`.
2. Typed ingestion is an adapter or intrinsic layered on top.
3. The adapter has success type `list[T]` plus a statically known parse/decode
   completion family that follows the accepted static propagation algebra.
4. No implicit force, default abort, or `Result` spelling is inferred from the
   current backend.

This avoids baking a second unrelated “stdin type” into the core model.

---

## 13. Resource access and safe mutation

Styio source treats a resource as the subject of declared operations. It does not expose `borrow`,
`shared`, `own`, or `pure` as user syntax.

### 13.1 Principle

Only operations with the right capability may mutate, advance, snapshot, commit, close, or read a
resource subject.

### 13.2 Practical rule for Styio

Block-entry execution uses resource snapshots:

- `resource >> { ... }` enters a snapshot at the `>>` boundary.
- `resource >> #(x) => { ... }` binds yielded items from the snapshot stream.
- `=> { ... }`, selected `?=` arm blocks, active `||> { ... }`, and resource
  sessions `|?| { ... }` follow the same block-entry rule when they enter a
  block. Settlement-forward `|>` after a session transfers settlement; it is
  not itself a topology snapshot scope unless a stage enters a new block.
- The block body may read or write the snapshot according to the resource capability rules.
- At `}`, the compiler commits the snapshot result back to the original resource.
- A chained sequence such as `a => { 1 } => { 2 } => { 3 }` has one snapshot and one commit per block stage. Later stages read the resource state committed by earlier stages.
- Resource sharing is not currently an accepted Styio source behavior.

---

## 14. Research guidance absorbed into this design

This design uses the following ideas:

1. **Capability checks on resource operations**
   From capability-oriented systems: operation permission must be visible to the checker.

2. **Typestate for protocol resources**
   From typestate-oriented programming: file handles and streams should have explicit protocol state.

3. **Uniqueness for destructive update**
   From Clean and Cogent: unique ownership enables safe in-place updates and resource transfer.

4. **Failure as an effect, not a user chore**
   From Koka-style effect typing: operations may be statically marked as fallible while the surface language still offers a default handler.

---

## 15. Migration plan for the compiler

### 15.1 Type representation

Replace the current flat `StyioDataType` assumptions with a richer structure:

- nominal family (`list`, `stream`, `writer`, `range`, scalar)
- item type
- capability set
- typestate
- optional representation tag

### 15.2 Parser / AST

Unify raw standard streams and typed ingestion:

- keep `@stdin` as a standard stream source
- model typed reads as adapters, not as a second base resource family

### 15.3 Analyzer

Replace node-kind iteration checks with capability checks.

### 15.4 `<<`

Make `<<` type-directed:

- sink push
- iterable drain
- collector synthesis for unbound names

### 15.5 Failure

Add internal `Result` / `Step` modeling and a default fail-fast handler.

---

## 16. Explicit non-goals for the current capability model

1. Any user-visible `borrow`, `shared`, `own`, or `pure` syntax.
2. User-visible `unwrap` as a mandatory language pattern.
3. Python-style universal object dictionary semantics.
4. General structural duck typing for all user types.

Styio should remain explicit, protocol-driven, and resource-aware.

---

## 17. References

- Pony capability design: [Co-designing a Type System and a Runtime for Actor-Oriented Programming](https://www.ponylang.io/media/papers/codesigning.pdf)
- Typestate foundations: [Foundations of Typestate-Oriented Programming](https://www.cs.cmu.edu/~aldrich/papers/toplas14-typestate.pdf)
- Typestate + aliasing: [Modular Typestate Checking of Aliased Objects](https://www.cs.cmu.edu/~aldrich/papers/typestate-verification.pdf)
- Uniqueness + I/O: [The Ins and Outs of Clean I/O](https://www.cambridge.org/core/services/aop-cambridge-core/content/view/2EFAEBBE3A19EA03A8D6D75A5348E194/S0956796800001258a.pdf/the-ins-and-outs-of-clean-io.pdf)
- Uniqueness for systems code: [Cogent: Uniqueness Types and Certifying Compilation](https://www.cambridge.org/core/services/aop-cambridge-core/content/view/47AC86F02534818B95A56FA1A283A0A6/S095679682100023Xa.pdf/cogent-uniqueness-types-and-certifying-compilation.pdf)
- Failure as typed effect: [Koka: Programming with Row-Polymorphic Effect Types](https://www.microsoft.com/en-us/research/wp-content/uploads/2016/02/koka-effects-2013.pdf)
