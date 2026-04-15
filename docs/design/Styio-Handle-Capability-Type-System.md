# Styio Handle, Capability, and Failure Type System

**Purpose:** 为 Styio 的资源值、`@stdin/@stdout`、`<<`、可迭代对象、以及默认失败处理建立统一的设计级类型系统；该文档定义目标模型，不等同于当前实现。

**Last updated:** 2026-04-12

**Status:** Target design — not fully implemented in the current compiler.  
**See also:** [`Styio-Language-Design.md`](./Styio-Language-Design.md), [`Styio-Resource-Topology.md`](./Styio-Resource-Topology.md), [`../plans/Standard-Streams-Plan.md`](../plans/Standard-Streams-Plan.md), [`../review/Logic-Conflicts.md`](../review/Logic-Conflicts.md).

---

## 1. Why this document exists

The current compiler mixes three different concerns:

1. **Representation:** what a runtime value physically is.
2. **Capability:** what operations the value supports.
3. **Protocol state:** whether a resource is open, exhausted, writable, materialized, and so on.

This leads to ad hoc special cases:

- `@stdin` and `@stdin: list[T]` currently take different AST paths.
- Iteration is dispatched partly by `NodeType`, not by a unified type protocol.
- `<<` currently behaves differently depending on parser shape instead of a single type-directed rule.

This document defines a target design that unifies these cases.

---

## 2. Design goals

1. Give every resource-like value a single conceptual type family.
2. Distinguish **iterable** from **non-iterable** statically.
3. Make `<<` mean one thing: **feed items into the left side one by one**.
4. Preserve Styio’s resource flavor: values behave like OS handles with protocol state.
5. Avoid Rust-style surface `unwrap`; failed operations should still be typed, but default handling should abort with diagnostics.
6. Support destructive update safely for unique resources and materialized collections.

---

## 3. Core model

Styio should model resource-bearing values as a typed handle family:

```text
Handle<Rep, Item, Caps, State>
```

Where:

- `Rep` is the low-level representation family.
- `Item` is the element type produced, consumed, or stored by the handle.
- `Caps` is a compile-time capability set.
- `State` is the current protocol state.

Examples:

- `@stdin : Handle<fd, string, {pull, iter}, open>`
- `@stdout : Handle<fd, string, {push}, open>`
- `list[i32] : Handle<ptr, i32, {iter, push, index, sized, collect}, materialized>`
- `range[i64] : Handle<imm, i64, {iter}, materialized>`

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

The first version should support these capabilities:

| Capability | Meaning |
|------------|---------|
| `iter` | Can produce a sequence of `Item` values |
| `pull` | Can produce at most one `Item` per explicit pull step |
| `push` | Can accept `Item` values one by one |
| `index` | Supports random access by integer index |
| `sized` | Supports `.length` / `.size` |
| `collect` | Can accumulate a stream or iterable into a materialized container |
| `clone` | Supports resource-preserving clone semantics |
| `close` | Has an explicit close / release protocol |

### 5.2 Derived concepts

These are not separate runtime kinds; they are predicates over capabilities:

- **Iterable[T]**: any type with `iter` over `T`
- **Indexable[T]**: any type with `index` over `T`
- **Writable[T]**: any type with `push` over `T`
- **Sized**: any type with `sized`
- **Cloneable**: any type with `clone`

So Styio should check:

- `>>` requires `Iterable[T]`
- `zip` requires both sides to be `Iterable`
- `[]` requires `Indexable`
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
When the state is dynamic, the runtime may check and fail through the default failure handler.

---

## 7. Standard resource families

### 7.1 `@stdin`

`@stdin` should be a raw input stream value, not a special parser escape hatch:

```text
@stdin : stream[string]
```

Conceptually:

```text
Handle<fd, string, {pull, iter}, open>
```

This means:

- it is iterable
- it is readable
- it is not writable
- it is not indexable
- it is not sized in the general case

### 7.2 `@stdout` / `@stderr`

These are write sinks:

```text
@stdout : writer[string]
@stderr : writer[string]
```

Conceptually:

```text
Handle<fd, string, {push}, open>
```

### 7.3 `list[T]`

Lists are materialized containers:

```text
list[T] : Handle<ptr, T, {iter, push, index, sized, collect, clone}, materialized>
```

### 7.4 `range[T]`

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

So in the target design:

- `a <- @stdin`
  means `a` is bound to the raw stream handle.
- `a << @stdin`
  means collect the input stream into a default container, one item at a time.

If `@stdin` yields lines, then `a << @stdin` defaults to `list[string]`.

### 8.2 Relationship with cloning

A clone is just a special case where the left side is a collector or sink over resource items and the right side is a cloneable iterable/resource source.

The checker may still lower some cases to `clone` internally, but the user-visible semantics of `<<` remains “one by one into the left side”.

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

The same rule should drive:

- plain iteration
- zip
- collect
- `<<` draining

---

## 11. Failure model

Styio should distinguish “end of sequence” from “operation failure”.

### 11.1 Internal models

Use two internal concepts:

- `Step[T] = Yield(T) | End`
- `Result[T, E]`

`Step` is for iterator progression.  
`Result` is for I/O failure, parse failure, bounds failure, closed-handle failure, and so on.

### 11.2 Default surface behavior

Styio should not require explicit `unwrap` at every use site.

Default behavior:

1. Fallible operations are typed internally as `Result`.
2. At statement or expression-use boundaries, Styio performs an implicit force.
3. If the result is `Err`, execution aborts with a structured diagnostic.

This is effectively “default unwrap with fail-fast diagnostics”.

### 11.3 Examples

- writing to `@stdout` may fail: internally `Result[unit, IOError]`
- `@stdin: list[i32]` parsing may fail: internally `Result[list[i32], ParseError]`
- `list[i][j]` bounds checks may fail: internally `Result[T, BoundsError]`

The default top-level handler reports the error immediately.

---

## 12. Typed stdin ingestion

The current special form `@stdin: list[T]` should be reinterpreted as typed ingestion from a raw stream:

```text
read_as<list[T]>(@stdin)
```

Design consequences:

1. `@stdin` itself stays a raw `stream[string]`.
2. Typed ingestion is an adapter or intrinsic layered on top.
3. The adapter is fallible and therefore internally returns `Result[list[T], ParseError]`.
4. The language default handler implicitly forces that result and aborts on parse failure.

This avoids baking a second unrelated “stdin type” into the core model.

---

## 13. Aliasing and safe mutation

Styio resources should borrow from uniqueness / capability work rather than from plain mutable variables.

### 13.1 Principle

Only references with the right capability may mutate or advance resource state.

### 13.2 Practical rule for Styio

The first implementation can use a small permission split:

- `own`
- `shared`
- `pure`

Where:

- `own` may consume, push, or change state
- `shared` may read and iterate if the resource protocol allows shared iteration
- `pure` may inspect metadata but may not change resource state

This can later refine the current final/flex binding metadata without exposing a large Rust-like borrow calculus to users.

---

## 14. Research guidance absorbed into this design

This design should borrow the following ideas:

1. **Reference capabilities on aliases, not objects**  
   From Pony and related capability systems: the capability belongs to the reference being used.

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

## 16. Explicit non-goals for v1

1. Full Rust-style borrow syntax.
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
