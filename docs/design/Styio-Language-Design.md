# Styio Language Design Specification

**Purpose:** Styio 语言的 **权威语义与特性说明**（正文规格）；形式文法见 [`Styio-EBNF.md`](./Styio-EBNF.md)，符号与 token 名见 [`Styio-Symbol-Reference.md`](./Styio-Symbol-Reference.md)，`@` **目标**拓扑见 [`Styio-Resource-Topology.md`](./Styio-Resource-Topology.md)，当前实现缺口见 [`../rollups/NEXT-STAGE-GAP-LEDGER.md`](../rollups/NEXT-STAGE-GAP-LEDGER.md)。

**Last updated:** 2026-07-20

**Version:** 1.0-draft  
**Date:** 2026-03-28  
**Status:** Active Design — Pre-stabilization

---

## 1. Introduction

Styio is an **intent-aware, symbol-driven stream processing language** designed for high-performance resource dispatching, with an initial target domain of **financial quantitative analysis**. It compiles through LLVM to native code, achieving C++-level performance with a fraction of the syntactic overhead.

The name encodes the language's identity:
- **St** — Stream Computing
- **y** — Style / Syntax
- **io** — Native I/O primitives

### 1.1 Design Pillars

| Pillar | Description |
|--------|-------------|
| **Pure Symbolism** | Replace natural-language keywords (`if`, `while`, `for`, `def`) with unambiguous symbolic operators (`?=`, `>>`, `#`, `@`). |
| **Intent Awareness** | The compiler statically analyzes field access patterns and pushes intent down to resource drivers (e.g., only fetch needed database columns). |
| **Honest Missing** | Absence exists only in the static type `? \| T`; `(?)` is its empty source value and an ordinary `T` never contains it. Source `@` belongs to resource anchors, not the value algebra. Debuggers may render empty provenance with an internal marker, but that display is not syntax or a value. |
| **Thick Library, Thin Artifact** | Development uses a rich standard library with protocol detection and AI-assisted probing. Production builds perform dead-code elimination to produce minimal binaries. |

### 1.2 Compiler Toolchain

- **Language:** C++20
- **Backend:** LLVM 18+ (IRBuilder + ORC JIT)
- **Parser Strategy:** Hand-written recursive descent, LL(n) with lookahead
- **Dependencies:** LLVM, ICU，及测试用 GoogleTest、cxxopts 等 — **完整清单与许可** 见 [`../specs/THIRD-PARTY.md`](../specs/THIRD-PARTY.md)。

---

## 2. Core Philosophy

### 2.1 Everything Is a Flow

Styio has no explicit loop constructs (`for`, `while`). Instead, data sources emit **pulses** into **closures** via the pipe operator `>>`. In `left >> right`, the left side is an iterable or pulse source; `>>` advances it one item at a time and pushes each item as a pulse into the right-side channel, closure, resource sink, or pipeline stage. The closure executes once per pulse. Loops emerge naturally from infinite or finite data generators.

### 2.2 Progressive Performance

The language follows a "write less, get convenience; write more, get speed" model:
- Omit type annotations where the local-inference contract permits it → the
  compiler infers from surrounding constraints. Exact numeric literals remain
  unmaterialized inside a generalizable callable scheme; at a concrete
  unconstrained value boundary they default once to `i64` or `f64` under
  [Q05-LIT-ADD](./Styio-Exact-Literals-and-Builtin-Add.md).
- Add explicit types → compiler generates optimized, specialized instructions
- Omit resource protocol → runtime probes automatically
- Specify protocol (e.g., `@file`, `@mysql`) → static dispatch without runtime probing

### 2.3 Expression-Oriented

All control flow constructs (match, conditional wave, loops) are **expressions** that produce values. There are no void statements — everything flows.

### 2.4 Functional Evaluation and Explicit Effect Order

Styio uses the accepted Q03-F model: **strict values + dependency graphs +
explicit effect sequences**. Ordinary calls and operators require their strict
input values but do not give independent safe-pure siblings an author-visible
left-to-right timeline. Top-level items of a lexical Block, data/control edges,
operation settlement, and resource-topology happens-before edges order
effectful, completing, or otherwise non-normal computations. Two unordered
order-sensitive sibling computations in one ordinary expression are rejected;
the author first settles/binds them in consecutive Block items or uses an
explicit task construct when concurrency is intended.

`source -> endpoint` orders neither source preparation nor endpoint
preparation: both are prerequisites of the transfer, and the successful
transfer alone produces Unit. Optimizers consume separate reorder, speculate,
duplicate, and elide rights; `pure` by itself never authorizes all four.

The complete normative contract, construct matrix, runtime-free lowering
boundary, diagnostics, and rejected alternatives are owned by
[Functional Evaluation and Effect Ordering](./Styio-Functional-Evaluation-and-Effect-Ordering.md).

---

## 3. Type System

### 3.1 Primitive Types

| Type | Bits | Description |
|------|------|-------------|
| `bool` | 1 | Boolean |
| `i8`, `i16`, `i32`, `i64`, `i128` | 8–128 | Signed integers |
| `f32`, `f64` | 32, 64 | IEEE 754 floating point |
| `char` | variable | Unicode character |
| `string` / `str` | variable | UTF-8 string |
| `byte` | 8 | Raw byte |

### 3.2 Exact Numeric Literals, Built-in `Add`, and Late Defaults

Accepted decision `Q05-LIT-ADD` is defined in
[Styio Exact Literals and Built-in Add](./Styio-Exact-Literals-and-Builtin-Add.md),
the sole detailed semantic owner. Integer and decimal literals preserve their
exact mathematical value before materialization. Context selects a concrete
type first; materialization then fails closed on an out-of-range integer,
inexact integer-to-float conversion, forbidden decimal-to-integer conversion,
or a finite decimal that would become infinity.

The compiler-owned scalar `Add` table is closed over `i8` through `i128`, `f32`,
and `f64`. Two already concrete operands must have the same type; an exact
literal may materialize symmetrically to the other operand's concrete type, and
the selected row uniquely returns that type. There is no C-style common-type
promotion. At an otherwise unconstrained concrete value boundary only, an
integer expression defaults once to `i64` and a decimal expression to `f64`.
No default occurs inside a generalizable callable scheme.

Signed-integer `Add` is checked and admits the payload-free prelude completion
family `overflow`; floating `Add` has an empty completion bound and strict IEEE
behavior. Constant and runtime evaluation use the same relation. A generalized
constraint spanning integer and floating rows has the conservative finite upper
bound `{overflow}`, and statically known overflow takes that same completion
edge rather than becoming a new syntax-error class. This decision adds no token
or grammar production; matrix/text operations, conversions, other arithmetic,
and remaining NaN policy stay with their deferred owners.

### 3.3 Type Annotations

Types are annotated with `:` on both parameters and return values:

```
# add : f32 = (a: f32, b: f32) => a + b
```

- `add : f32` — return type is `f32`
- `a: f32` — parameter type
- `:` always binds a **type** to its left-hand identifier
- `m: matrix = [[1,0],[0,1]]` — explicit matrix context accepts a nested list source form and checks that rows are non-empty, rectangular, and numeric; its element-kind/coercion policy remains separately owned, and untyped nested lists remain ordinary lists

#### 3.3.1 Ordinary Binding Initialization

An ordinary binding is created only when its right-hand side successfully
produces the first value. Both canonical binding operators therefore require an
RHS:

```styio
count: i64 = 0
limit: i64 := 100
missing: ? | i64 = (?)
done: unit = ()
```

`name: T` by itself is not a delayed declaration and is rejected for every
ordinary `T`. The compiler never interprets the missing syntax as zero,
`false`, an empty string, `()`, `(?)`, an uninitialized slot, or a type-provided
default. Styio initially exposes no `Default` capability; if a default-producing
feature is added later, it must be requested by an explicit expression and does
not change this grammar rule.

This rule applies to executable ordinary storage bindings, not every occurrence
of `identifier: Type`. Parameters, pattern binders, and iteration binders receive
their first value atomically from their enclosing operation. Record field
schemas and resource topology slots describe shapes or protocols and retain
their separately owned construction rules; they gain no implicit default from
this distinction. Settlement creates no typed-target exception: an ordinary
result uses an ordinary binding with an explicit RHS, such as
`answer: i64 = ?| operation | fallback`.

Cold, pending, moved, partially initialized, and uninitialized storage are
compiler, runtime, or resource-protocol states, never values of an ordinary
`T`. FFI out-pointers and bulk partial construction require a separately typed,
restricted storage facility if they are introduced; ordinary bindings are not
weakened to model them.

#### 3.3.2 Unit, optional Unit, and zero-payload boundaries

`unit` is a first-class built-in type with exactly one value, `()`. It is not
absence, failure, uninitialized state, EOF, `never`, C `void`, or integer zero.
`never` is the distinct uninhabited built-in type for paths proven not to
complete. Both names are recognized contextually in type position from ordinary
`NAME` tokens; neither adds a lexer keyword.

`unit` is accepted wherever the corresponding built-in generic constructor
accepts an ordinary type argument:

```styio
present: ? | unit = ()
missing: ? | unit = (?)
ticks: list[unit] = [(), (), ()]
membership: dict[string, unit] = dict{"ready": ()}
job: task[unit] = ||> { <| () }
```

The semantic and physical models are deliberately separate:

| Type/boundary | Required semantic fact | Permitted physical optimization |
|---|---|---|
| `unit` | one value `()` | zero payload bytes |
| `? | unit` | absent versus present `()` | presence tag with no Unit payload |
| `list[unit]` | logical length and exactly that many iteration events | no per-element payload storage |
| `dict[K,unit]` | key order, cardinality, membership, and optional lookup | keys/index only; no mapped Unit payload |
| `task[unit]` | lifecycle, success/failure, consumption, and successful `()` settlement | no result payload slot |
| returning foreign `void` | a call that returned successfully produces `()` | target ABI no-result convention |

Logical count or state is always authoritative and independent of physical
payload. It may never be derived from payload byte size, allocation, pointer
identity/difference, or an integer placeholder. Unit-list iteration therefore
uses explicit logical indices; Unit dictionaries use their key/member structure;
Unit tasks use their control state. Zero-sized storage must not introduce
division by zero, fake allocation, non-advancing pointer iteration, or unchecked
logical-count overflow.

A fallible no-business-payload operation has success type `unit` plus a finite
nominal completion-family set. It never uses bare Unit, absence, or `i64` to
encode both outcomes. This rule does not introduce a `Result` keyword, an
ordinary value fallback operator, or an ambient failure channel. The frozen
`?|`/`->` composition is described in §6.9 and §8.6; the accepted completion
algebra is owned by [Operation Completion and Settlement](./Styio-Operation-Completion-and-Settlement.md).

At an explicit foreign adapter, a returning C `void` result maps to `unit`, a
declared nullable result maps to `? | T`, and a proven no-return call maps to
`never`. Missing/unspecified pointer nullability fails closed until the adapter
states nullable or non-null behavior. Source Unit is not exported as a C object
or parameter, and optional unions acquire no implicit C layout.

**Implementation status:** design accepted; delivery and deletion gates are in
[`unit-zero-payload-boundaries/Plan.md`](../plan/styio-block-completion-and-bottom-type/unit-zero-payload-boundaries/Plan.md).

### 3.4 Matrix Values

`matrix` is a typed numeric collection, not a universal nested-list mode. The parser keeps
`[[...], [...]]` as an ordinary list literal unless the surrounding type context explicitly says
`matrix`; this avoids paying rectangular-shape checks for every nested list expression.

```styio
m: matrix = [[1,0],[0,1]]
```

Matrix binding rules:

- rows must be non-empty and rectangular
- elements must be numeric; mixed concrete element kinds do not gain an implicit
  promotion from the scalar `Q05-LIT-ADD` decision and fail closed until a
  separate matrix conversion policy is accepted
- statically known dimensions are preserved in the inferred type
- shape mismatches are semantic errors before lowering
- `m[row][col]` reads one element, while `m[row]` materializes a list row

The scalar `Add` table does not admit matrix operands. Matrix `+`, `-`, `*`,
mixed-kind coercion, and scalar/matrix arithmetic remain deferred to a separate
`Q05`/`Q08` relation. Existing named helpers such as `mat_add`, `matmul`,
`transpose`, and `mat_set` remain tracked compiler/standard-library surfaces;
their current runtime paths do not freeze a general matrix operator algebra or
authorize implicit numeric promotion.

The current runtime representation is a flat row-major matrix handle with element-kind-specific
helpers for `i64` and `f64`. Small statically shaped same-type operations may lower directly to
LLVM loads/stores over the flat backing store; dynamic, mixed-kind, or larger operations route
through the runtime helper surface registered in ORC.

### 3.5 Optional Absence: `? | T` / empty atoms

Absence is a state of the explicit Optional union `? | T`, not a hidden member
of every runtime value. `(?)`, `[?]`, and `{?}` are delimiter variants of the
same empty Optional value; ordinary prose and examples use `(?)`. In an expected
`? | T`, an ordinary `T` expression selects the present branch. These forms are
distinct from `null`, zero, `false`, NaN, Unit, and failure. An ordinary `T` can
never carry or receive the empty state.

The type layer normalizes repeated optionality as a set-like union:

```text
? | (? | T) == ? | T
```

This is canonical type identity, not an implicit unwrap and not permission to
encode several different empty meanings in repeated layers. Several distinct
empty states require an explicitly tagged type. An empty atom without an
expected Optional payload type or a compatible control-flow join is rejected;
the compiler does not guess `T`.

Every resource field, parameter, return, binding, or intermediate expression
that may be missing must therefore expose `? | T` statically. A
resource/intrinsic adapter may produce `(?)` only through such a typed boundary.
Operations that preserve absence must declare an Optional result; no hidden
sentinel or tainted ordinary value may propagate through arithmetic, logic,
equality, or control flow.

Source `@` has no absence-value role. It remains the resource-anchor prefix;
historical bare-`@` value fixtures are parse errors. A debugger or diagnostic
formatter may display an internal empty-state marker and retain a reason code
or source location, but that provenance is semantically inert: it is not
source syntax, payload, type identity, equality state, or a branch condition,
and it has no extraction operator. In particular, `??` is not a
diagnostic-extraction spelling.

---

## 4. Module Imports

Styio uses explicit top-level imports to declare module dependencies across `.styio` files.

### 4.1 Import Declaration

```text
@import { styio/mod, styio.mod; core }
```

Rules:

- `@import` is only valid at file top level.
- `/` is the native package and module path separator.
- `.` is accepted as a compatibility spelling and is normalized to slash form internally.
- A single import item must not mix `.` and `/`.
- `,` and `;` are equivalent separators between import items.
- Empty import lists, trailing separators, and the legacy leading string-list form such as `["pkg"]` are syntax errors.

### 4.2 Resolution Semantics

Each import item creates one explicit import fact for the current file. The IDE and HIR layers expose these facts in canonical slash form, so `styio.mod` and `styio/mod` both resolve as `styio/mod` internally.

Import resolution remains explicit:

- bare package paths are resolved through the project-aware import lookup rules
- `.styio` is tried when the import candidate does not already name a Styio file
- unresolved imports stay unresolved instead of binding to unrelated same-text symbols elsewhere in the workspace

---

## 5. Callable / Operation-Channel Bindings

### 5.1 Unified Binding Syntax

Styio binds names to targets. A target may be an ordinary value, a callable, a
resource, a channel endpoint, a processor, or a continuation. The binding
operator carries mutability:

```
value = 1                            // mutable ordinary value binding
value := 1                           // final ordinary value binding
# add : i32 ?| {overflow} = (a: i32, b: i32) => a + b // checked integer Add
# add = (a, b) => { <| a + b }        // rebind against that established scheme
# identity := (x) => x                // eligible final principal inference
# add_five := (x) => x + 5            // eligible final constrained inference
# read : f64 ?| {io, parse} := (path: string) => { ... } // completion upper bound
# transform : i64 = #(x: i64) => x * 2 // explicit callable-body marker
```

`#` is the callable/operation-channel binding prefix. It tells readers and the
compiler that the binding target must be callable or operable. It is not a plain
function-declaration keyword: `# f = ...` is rebindable because `=` is mutable,
and `# f := ...` is final because `:=` is final.

`f = expr` does not promise that `expr` is callable. `# f = (...) => expr`
does make that promise and enters the callable/operation-channel binding path.
A bare `# f = ...` is only a replacement against an already established
callable scheme; it never creates or changes one through inference. An initial
mutable callable needs a complete explicit contract. A final callable binding
cannot be redefined by `=` or `:=`.

#### 5.1.1 Completion boundary contract

Public, recursive, native/FFI, and typed protocol-boundary callables must expose
a finite upper bound of nominal completion families in their source contract.
The implementation body may produce a strict subset of that bound, but any
family outside it is a definition-site error. A caller either settles each
relevant family with the operation-settlement construct or propagates the
unhandled family into its own admitted boundary contract.

This is a static callable-type fact, not a returned union value, hidden
`Result`, exception object, or ambient program-level channel. Its accepted
spelling is:

```styio
# read_price : f64 ?| {io, parse} := (path: string) => { ... }
# abs : i64 := (x: i64) => { ... }
# local := (x) => { ... }
```

The `?| {...}` clause is a finite non-empty upper bound of ordinary nominal
family identifiers. Its braces are signature grammar, not a runtime set/dict or
Block. Duplicate families, non-family names, `?| {}`, and a trailing comma are
rejected. Whenever an author writes `: T`, omission of the completion clause
means the empty upper bound in every scope; it never means “infer completions”.
Only an eligible non-boundary lexical-local or module-private callable that
omits the entire `: T` contract may infer the complete success type and
completion set. Required boundary callables cannot omit that contract.

Resources keep their visible `@` identity. A direct resource atom is not a
valid right side for a `#` binding:

```
# sink = @stdout     // invalid: @stdout is a resource, not a callable binding body
```

Use `expr -> @stdout` or `items >> @stdout` for resource writes. Resource-family
definitions use the `@family::member` forms described in the resource section.

#### 5.1.2 Principal inference for eligible callable bindings

Accepted decision `Q02-INF` is defined in
[Styio Callable Principal Inference](./Styio-Callable-Principal-Inference.md),
the sole detailed semantic owner. In summary, only a capture-safe, final `:=`,
non-recursive, non-boundary lexical-local or module-private callable value can
receive automatic principal constrained rank-1 generalization. The definition
site generalizes only variables not free in its lexical environment; every use
gets a fresh instance, and neither first use, future calls, defaults, `any`, nor
backend choices may determine the scheme.

`# f = ...` only rebinds an existing stable scheme, while all required
boundaries remain explicit. Internal scheme and closed-constraint notation is
metalanguage rather than Styio source. The accepted closed literal/`Add`
contract is owned by
[Styio Exact Literals and Built-in Add](./Styio-Exact-Literals-and-Builtin-Add.md),
while `F02` owns any future author-written generics, constraints, user
instances, higher-rank polymorphism, or completion rows. Neither decision adds
a token or EBNF production.

### 5.2 Pulse Closures

Used within stream pipes:

```
prices >> #(p) => { <| p * 2 }
```

`#(p)` binds the current pulse to the local name `p`.

### 5.3 Derived Bindings: `name := $(deps) => expr`

```
trade := $(bal, is_open) => decide(bal, is_open)   // derived: ambient state flows in at frame commit
```

A derived binding declares a **frame-committed derived slot**: the `$(...)`
head lists the binding's complete dependency set, and `=>` maps those
dependencies into a pure expression body. The old head spelling
`trade $(bal, is_open) := ...` is removed — a capture head is an
expression-side construct and sits to the right of `:=`, keeping the name
side identical to every other binding.

The three input-feeding forms are symmetric:

```
# add := (a, b) => a + b                            // callable: parameters flow in at call time
trade := $(bal, is_open) => decide(bal, is_open)    // derived: ambient state flows in at frame commit
@src >> #(p) => { ... }                             // pulse closure: events flow in per pulse
```

#### 5.3.1 Frame-Commit Semantics

Derived bindings follow synchronous-dataflow discipline — the pulse frame is
the logical instant. They are not eager FRP:

1. **Frame-edge recomputation.** A write to a captured variable never
   executes the body; it only marks the slot dirty. Dirty slots recompute at
   the pulse-frame commit boundary, in topological dependency order, at most
   once per frame. Diamond dependency graphs are glitch-free by construction:
   no reader can observe a mixed old/new dependency state.
2. **Frame constancy.** Within a frame, a derived name is constant — it is
   part of the frame snapshot, and the pulse frame lock invariant (repeated
   reads in one closure observe one value) extends to derived names. The
   recomputed value becomes visible from the next frame's snapshot and to
   reads after the commit.
3. **Static graph.** The dependency set is exactly the `$(...)` list. The
   compiler builds the dependency graph at compile time, rejects cycles
   (including self-capture) statically, and inlines recomputation at frame
   commit points. There is no runtime dependency discovery, and writes to
   non-captured variables cost nothing.
4. **Value decay.** A derived name decays to a plain committed value at every
   use. No `signal[T]` type exists; passing a derived name to a function
   passes the current committed value; reactivity never crosses a function
   boundary.

Cold start never injects a hidden sentinel into a derived value. A derived slot
that can be empty must have static type `? | T` and may initialize to `(?)`; a
non-optional `T` slot instead requires a proven present initializer/dependency
state or is rejected. A body failure during recomputation is a separate typed
frame-commit failure, and the graph never partially updates.

#### 5.3.2 Usage Restrictions (Fail-Closed Whitelist)

Automatic recomputation is the most dangerous semantics in the language, so
the accepted surface is deliberately narrow. Everything outside this
whitelist is a compile error.

Declaration site:

1. Module/topology scope only. A derived binding may not be declared inside a
   pulse closure, function body, match arm, guard branch, task block, or any
   other nested block — graph nodes never appear or disappear dynamically.
2. `:=` only — derived slots are final and never rebind. The body is a single
   expression: no `{ ... }` block, no statements, no local bindings.

Capture list:

3. Captured names must be module-scope value state: mutable `=` bindings or
   other derived bindings. Resources (`@` family), closure locals, function
   parameters, and callables are rejected in the capture list.
4. `$()` (empty), duplicate names, captured-but-unused names, and the
   identity alias form (`x := $(y) => y`) are rejected as unnecessary
   surface.
5. Cycles, including self-capture, are rejected statically.

Body:

6. The derived-binding body checker must prove the body effect-free on its own:
   no resource atoms or selectors, no `->` / `>>` / `<-` / `?|` / `||>`, no
   continuation operations, and no call whose effect status is unknown.
   Unknown means rejected. This whitelist does not classify or reinterpret
   operator syntax.
7. The body may reference only captured names, literals, and provably pure
   callables/intrinsics. Any other free name is an undeclared dependency and
   is rejected.

Interaction:

8. After its initializing declaration, a captured variable may be rewritten
   only inside pulse-frame contexts (`>>` pipelines and zip closures). A
   write site outside any frame has no commit boundary — no defined
   recomputation time — and is rejected.
9. Derived names may not be read or captured inside task (`||>`) blocks until
   the task memory model defines cross-frame visibility.

Status: design-accepted surface. The parser does not implement derived
bindings; every occurrence fails closed until parser, Sema (graph and effect
checks), lowering, and test evidence land.

---

## 6. Control Flow

Styio's control flow is entirely **symbol-driven** and **expression-oriented**.

### 6.1 Pattern Matching: `?=`

```
x ?= {
    1  => { <| "one" }
    2  => { <| "two" }
    _  => { <| "other" }
}
```

- `?=` — match operator (condition probe)
- `=>` — pattern-to-result mapping
- `_` or an all-underscore identifier such as `_______` — wildcard / default branch
- `<|` — yield (explicit return from block)

The binding form matches a scrutinee while exposing it to every arm:

```
#(n = values.length) ?= {
    0 => { /* empty */ }
    1 => { answer = values[0] }
    _ => { /* n is available here */ }
}
```

For integer match lowering, literal arms (`1 => ...`) and guarded equality arms
that compare the scrutinee to an integer (`(n == 1) => ...`) are semantically
the same arm. AST lowering emits ordinary StyioIR, then the StyioIR optimizer
canonicalizes equivalent match shapes before LLVM codegen so accepted source
spellings can produce identical switch-shaped LLVM IR.

Current executable match expression and function match sugar result families are
`i64`, `f64`, `bool`, `char`, and `string`; tuple/container result families fail
closed until their value IR and merge semantics are defined.

### 6.2 Infinite Loop

```
[...] => { /* body */ }
```

`[...]` is an infinite pulse generator. The closure executes indefinitely.

### 6.3 Conditional Loop (While-equivalent)

```
[...] >> ?(expr) => { /* body */ }
```

- `[...]` — infinite generator
- `>>` — pulse the generator one item at a time into the workflow
- `?(expr) =>` — guard / valve: only passes pulses into the body when `expr` is truthy

### 6.4 Collection Iteration (For-each-equivalent)

```
[1, 2, 3] >> #(item) => { /* body */ }
[start..end] >> #(i) => { /* body */ }
```

The collection becomes a finite pulse source. `>>` advances the collection one element at a time and pushes each element as a pulse into the closure; each pulse binds one element to `item`.

`start..end` is the naked expression-level range form. It is not a list literal.
`[start..end]` is the materialized range form: the brackets materialize the
range as an iterable `list[i64]` source. Therefore `[0..n] >> #(i) => { ... }`
pushes each materialized integer into the consumer one by one; `[0..n]` is not
parsed as a normal list containing one range expression. Step range spellings
such as `[start..end..step]` or `[0..n..2]` are removed from the design and
rejected by the parser; `[start..end]` is the only materialized range form.

### 6.5 Break: `^...` (Immediate Loop)

```
^       // break out of the nearest enclosing loop
^^      // same as ^
^^^^    // same as ^
```

Rules:
- `^` characters must be **contiguous** (no spaces)
- any contiguous run of `^` is one break statement
- the count of `^` characters has no semantic depth and is normalized to 1
- `^^ ^^` is **illegal** — it is two adjacent break statements, not a deeper break
- a break outside an enclosing loop is rejected by code generation

Single-level exit is a settled language decision. Break always exits exactly
one level — the nearest enclosing loop — and multi-level break (encoding jump
depth in caret count or any other spelling) is permanently rejected. The
rationale is goto-hell prevention: control transfer stays local and visible.
This is closed by decision and is not an open design question.

### 6.6 Continue: `>>...` (Standalone, Variable Length, >=2)

```
>>       // skip the rest of the current block for this pulse/session
>>>      // same as >>
>>>>     // same as >>
>>>>>>   // same as >>
```

The base continue spelling is 2 characters (`>>`), and any longer contiguous run of `>` characters has the same meaning when it is a standalone statement. The count of `>` characters has no semantic depth and is normalized to one continue operation.

Context distinguishes continue from pipe: the pipe form connects a left iterable or pulse source to a right channel/consumer (`left >> right`). The continue form is a standalone statement: aside from horizontal whitespace, the `>>...` token is the whole line/statement and is followed by a newline, statement separator, block end, or EOF. In a pulse/session domain, it skips the remaining statements in the current block and resumes at the next pulse/session of the nearest continue-capable domain. Outside such a domain, current code generation rejects it as `continue outside enclosing loop`.

Two settled decisions apply here and are not open design questions:

1. **Continue is single-level only.** `>>...` always targets the nearest
   continue-capable domain, and token length never encodes nesting depth.
   Multi-level continue is permanently rejected for the same goto-hell
   prevention rationale as break.
2. **`>>` serves multiple syntax roles by explicit design requirement.** Pipe
   / iterate, resource-write shorthand, and standalone continue deliberately
   share the `>>` spelling. Disambiguation is compiler-owned context logic
   (EBNF Appendix Rule 1); proposals to split these roles across different
   symbols are out of scope.

### 6.7 Lexical Block Completion: `<|`

```styio
# square := (x) => { <| x * x }
```

`<| expr` completes exactly the immediately enclosing lexical Block and makes
`expr` that Block's result. It never searches for a function and never crosses
an intervening Block, closure, task body, match arm, guard branch, or resource
session. Only after the outermost function-body Block has been typed does its
Block result become the function result.

There are two authored value-body forms:

1. A direct single expression: `=> expr`.
2. A Block result: `=> { <| expr }`. When a Block contains exactly one ordinary
   expression and no other item, `=> { expr }` is sugar for the same result.

Therefore `=> expr`, `=> { expr }`, and `=> { <| expr }` are equivalent for a
direct single-expression body. This is deliberately not Rust-style general tail
inference: a multi-item Block does not infer its final ordinary expression as a
result and must use `<|` when it produces a non-`unit` value. Separators and
formatting never toggle this rule.

For compressed one-line Blocks, `|<| expr |;` is the exact inline spelling. It
creates the same lexical Block-yield node and target as `<| expr`; the closing
`|;` is mandatory and has no independent semantic effect. `<| ()` is legal and
explicitly completes the current Block with Unit. `|<-` remains reserved, and
`|>` belongs only to resource-session settlement (§6.10).

Every normally reachable `}` completes with `() : unit`. All reachable normal
exits of a value Block must have compatible canonical result types. A `T` exit
and reachable Unit fallthrough are a compile-time error; the compiler never
repairs the join with a default, integer zero, absence, or implicit `? | T`.
A proven non-completing edge has type `never` and contributes no normal value,
so `join(T, never) = T`; `never` is never an inference fallback.

A Block consumer that accepts only Unit rejects `<| expr` when `expr` is not
Unit; it does not evaluate and silently discard the value. A sibling or region
whose every structural incoming path has already completed is a compile-time
unreachable-code error independent of optimization or backend terminator
omission.

`<|` is not an infix application or continuation operator. The retired
`f <| a <| b` spelling has no grammar role; ordinary application uses
`f(a)(b)`. Continuation admission, capture, resume, discontinue, and lifetime
semantics remain a separate owner decision and are not implied by lexical Block
completion.

#### 6.7.1 Block-result publication barrier

A Block result becoming ready and that result becoming observable are different
events. The result expression is evaluated exactly once into an immutable
epilogue-owned candidate. Ordinary `T` is published only after the Block's
required logical commit and every non-transferable lexical exit obligation has
reached a terminal outcome. A failure before publication invalidates the
candidate; recovery must produce an explicit replacement value and never revives
the failed candidate.

All exit reasons—natural `}`, `<|`, outer function completion, loop control,
typed failure, and cancellation where that feature exists—enter one verified
exit-action dependency graph. Real dependencies such as last borrow before drop,
owned child join before releasing reachable resources, pending state before
logical commit, and commit before the resource family's required flush/close
override default reverse-registration ordering. Independent actions use stable
lexical/source ordinals, never pointer identity, hash iteration, wall-clock
completion order, or scheduler races. Any future lexical feature contributes an
exit action only after that feature is separately accepted; this rule does not
activate continuations or another control surface.

Successful control exits commit the current unpublished logical frame. Failure
or cancellation may abort only still-unpublished pending state; neither implies
rollback of earlier publication barriers or irreversible external effects.
Late work is visible only through an explicit task, effect, settlement, or
resource-completion capability. A cancellation request is not completion, and a
child that can reach lexical resources must terminate or join before those
resources are released.

Exit actions are classified internally as proven total, typed fallible, or
fatal. Declaring an operation infallible, ignoring an OS result, logging out of
band, or terminating does not remove a physical failure mode. Statically known
or explicitly bounded typed failures use compiler-sized fixed status/payload
slots with deterministic semantic ordinals; no managed exception runtime,
growable failure list, hidden truncation, or heap fallback is introduced.
Existing failure remains primary and later failures remain observable as
secondary evidence. Unbounded independently fallible exit work must expose an
explicit bounded or settleable language capability instead of hiding behind an
ordinary `T`.

This publication protocol adds no source token or authored classification. Its
compiler architecture and delivery evidence are specified by the Block Exit
Publication and Settlement Better Plan; the language-level observable contract
is the barrier and failure preservation defined above.

### 6.8 Block-Entry Snapshot / Commit

Every language form that enters a `{ ... }` block creates a resource snapshot context at the
block-entry operator and commits resource effects at the matching `}`. This is a general block
semantics rule, not a special case for resource loops.

Covered block-entry surfaces include `>>`, `=>`, `?=`, active `||>`, and
resource sessions `|?| { ... }`. The rule applies to resource state and resource
effects; ordinary lexical values keep their normal scoping rules.

Chained stages commit once per block. For example:

```styio
a => { 1 } => { 2 } => { 3 }
```

creates three snapshot/commit units. The second block starts from the state committed by the first
block, and the third starts from the state committed by the second.

If a block-entry operation supports recovery, recovery must wrap the
block-entry operation with `?| block_entry_operation | fallback`. A trailing
`} | fallback` is not resource fallback.

### 6.9 Tasks and Effect Settlement: `||>` / `?|`

`||> { ... }` constructs one scheduled task. `||> [ name := { ... } ... ]`
launches a group of independent task blocks and binds each name to its task handle.

```styio
||> [
    price := { <| fetch_price() }
    risk  := { <| calc_risk() }
]

p: f64 = ?| price
r: f64 = ?| risk | 0.0
?| @("log.txt").close()
?| @("archive.log").close() | cleanup(problem) => report_cleanup(problem)
```

`?|` has no task-only await/binder variant. It settles exactly one operation.
The operation's ordinary result is bound by the surrounding `=` / `:=`, as in
`answer: T = ?| operation | fallback`. The unauthorized forms
`?| task -> value: T` and `?| -> value: T` are rejected; `:` after the arrow
does not declare a settlement target.

This does not remove `job -> answer`. That is an instance of the language-wide
directional transfer axiom: the value produced on the left flows into the
compatible destination/receiver endpoint on the right. If a complete operation
contains that transfer, `?| job -> answer | fallback` parses only as
`?| (job -> answer) | fallback`. `?|` settles the complete transfer operation;
it does not reinterpret `->` as task binding.

The accepted operation-completion algebra is specified normatively in
[Styio Operation Completion and Settlement](./Styio-Operation-Completion-and-Settlement.md).
Its source boundary is:

- leading `?|` opens settlement of one complete operation;
- a following `| fallback`, `| family => recovery`, or
  `| family(binding) => recovery` belongs only to that settlement production;
- a bare `operation | fallback` is never a settlement form;
- `?=` continues to match ordinary materialized values and does not implicitly
  catch an unsettled effect;
- `??`, general binary value `|`, and `?| operation | ...` have no
  source-language role.

Every operation has one success type and a finite set of nominal completion
families as static Sema/callable facts. This is not a source `Result`, an
exception object, or a new keyword. `family` and its optional payload `binding`
are ordinary identifiers: `io(err)` means “match the family currently resolved
as `io` and bind its payload locally as `err`”; neither spelling is reserved.

The operation runs once. Success bypasses recovery. Only the selected arm runs,
lazily and once, with no implicit retry. Named arms match exact nominal family
identity; duplicates are errors and catch-all is last. Bare fallback catches
only remaining recoverable failures. EOF, cancellation, and shutdown require an
exact admitted named arm or propagate; fatal/trap is outside settlement.
Success and every normally completing recovery arm must join to the same type;
`never` contributes no normal value. Unhandled families plus failures produced
by recovery expressions remain static facts of the enclosing operation.

Backpressure remains a resource pressure signal, not an automatic completion
family. The owning resource protocol decides whether and when it escalates that
signal into a nominal recoverable failure. Settlement may then match that
family, but never infers retry, waiting, replay, or scheduling from the arm.

Resource families may expose pressure as an ordinary observable effect stream:

```styio
?| res -> msg_queue | backpressure => do_something()

channel.pressure >> #(p) => {
    ?(p.pending > 10000) => {
        ||> {
            ?| channel.inspect()
              | inspection_failed => report_pressure_probe(p)
        }
    }
}
```

This form is side-effecting resource code, not implicit error handling. The
observer can count, log, spawn a task, or invoke recovery operations, and those
operations still obey normal resource capabilities and fallback rules.
`pressure` is a Sema-recognized member attribute in the member namespace, not
a grammar word.

The observer contract is pinned as follows.

**Delivery: single-slot level sensor (conflated, latest-wins).** A pressure
stream is not an event queue. The runtime keeps exactly one slot per observed
resource and atomically overwrites it on every pressure-state change; the
scheduler delivers the latest reading to the observer body only when the
observer is free. Intermediate readings are overwritten, never queued. There
is no reading backlog, so observation staleness is bounded by one observer
execution and a pressure stream can never itself develop pressure —
`.pressure.pressure` is impossible by construction, not by rule. Programs that
need a per-event audit trail must build an explicit data channel and accept
that it is ordinary data with ordinary pressure.

**Payload: a compiler-declared read-only struct.** The pulse payload is an
ordinary struct instance constructed by the runtime and declared in the
prelude, with the initial closed field set `pending: i64` (current backlog),
`limit: i64` (declared capacity), and `peak: i64` (highest watermark since the
previous delivery, reset at delivery, so spike magnitude survives conflation).
This depends on the general struct story — a separate design item — landing
first; the observer surface stays fail-closed until then.

**Trigger: hysteresis state transitions only.** A pulse fires only when the
resource's pressure state transitions: entering the pressured state at the
family's high watermark, leaving it at the low watermark, or escalating to
failure. Oscillation between the watermarks produces no pulses. Watermark and
escalation parameters are resource-family policy documented at activation;
there is no user-level threshold syntax.

**Execution: never inline in the writer path.** Observer bodies are scheduled
as ordinary pulse frames. A pressured writer never executes and never blocks
on an observer; the data path and the observation path stay decoupled by
contract.

**Static safety rules (fail-closed).**

1. An observer body must not write to the resource it observes, and
   cross-resource observer-write cycles (the observer of `@a` writes `@b`
   while the observer of `@b` writes `@a`) are rejected: the resource topology
   graph performs cycle detection over observer-to-resource write edges.
2. At most one pressure observer per resource per program, declared at
   module/topology scope only; duplicate or nested observers are compile
   errors.
3. Pressure streams are not zip sources: a conflated level stream contradicts
   the event-arrival barrier semantics of `&`.
4. A family qualifies for activation only when it has an honestly countable
   pending metric (a Styio-owned bounded structure such as a task or zip input
   queue). OS-opaque sinks whose buffers cannot be counted do not qualify.

The current compiler recognizes the observer syntax and rejects every current
resource family in Sema with `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED`
until a family declares a pressure payload and runtime stream that satisfy
this contract.

This pressure model is a core Styio design choice. The compiler may do additional
effect inference when that preserves a valuable language feature: useful resource
effects should not be collapsed into failures merely because they require more
static reasoning. Backpressure is useful precisely because it can remain
observable and mostly harmless until a resource-family policy escalates it.

### 6.10 Resource Session: `|?| { ... }` / `|!|` / `|>`

The design contract is settled; implementation availability belongs to the
syntax-convergence matrix and delivery plans. Owner detail: Resource Topology
§4.2. There is no `session` keyword.

`|?| { ... }` is an explicit resource session: the qualifying scope for local
handles and anchors, the owned-child join and lexical-obligation boundary, a
settleable resource operation, and a stage that may defer settlement through
`|>`.

**Placement.**

- Mid-transfer only for bare `|?|`: execution symbols must stand before and
  after, e.g. `# f => |?| { ... } |!|(cleanup) => handler` or
  `# f := |?| { ... } |> g`.
- Statement-start settlement uses `?|`: e.g.
  `?| |?| { ... } | cleanup => handler` or
  `?| |?| { ... } |> next |> cleanup => handler`.

**Body.** Handles and anchors only (`h <- @file(...)`, inline anchors). Topology
declarations `@name : Type` remain rejected inside sessions.

**Exit.** No `|!|` and no deferred cleanup chain → default Close (reverse-order
RAII). `|!|(cleanup)` / `|!|(ResourceCleanupFailure)` marks special exit
handling for the cleanup effect family (not a universal exception catch-all).
`|>` forwards settlement to a later stage. Fallible releases contribute their
nominal completion families to the session operation. An explicit settlement
site may handle them; otherwise they remain in the enclosing operation or
callable's static completion set. Infallible sessions may omit settlement.
Multi-failure merge keeps the primary body completion first and preserves
cleanup completions as bounded secondary facts. There is no ambient
program-level failure channel or dynamic stack search.

**Escape.** Return/move-out/store-out are statically rejected; in-session `||>`
tasks join at exit and no detached escape hatch exists in v1. If a future
continuation feature is admitted, its session escape and settlement behavior
must be decided by that feature; resource-session syntax does not activate or
predefine it.

---

## 7. Guard Conditionals

Guard conditionals replace ternary expressions and if/else chains with a single
condition-first spelling. The old wave spellings are tokenized but reserved:
`<~` and `~>` are **reserved symbols only**. They participate in no syntax
feature, and no grammar production may consume them, until the language design
explicitly declares an active semantics for them.

### 7.1 Inline Guard Value: `?(cond) => A | B`

```
val = ?(a > b) => a | b
```

Read as: "If condition holds, evaluate to `a`; otherwise evaluate to `b`." This is the canonical inline value-selection form.

### 7.2 Block Guard: `?(cond) =>`

```
?(signal) => {
    order_logic(p)
} | {
    else_logic(p)
}
```

Read as: "If signal is truthy, execute the block; otherwise execute the else
block." A statement-position block guard may omit the else block; its false
path executes no statements and completes with Unit `()`. A guard used as a
value expression must provide an explicit else branch whose result is
join-compatible with the then branch. The compiler never synthesizes absence
or a default value for an omitted branch.

### 7.3 Visual Semantics

| Form | Meaning |
|------|---------|
| `?(cond) => A \| B` | Inline value selection |
| `?(cond) => { A } \| { B }` | Block-level if/else |
| `\|` | Else separator inside the enclosing guard production |

### 7.4 `|` Is Grammar-Anchored, Never a General Value Operator

The parser determines every accepted single `|` from syntax that is already
open; Sema never classifies a generic binary pipe from operand types, inferred
truthiness, effects, or purity:

1. In type position after `?`, `? | T` is the Optional union grammar.
2. After the then branch of `?(cond) => ...`, `|` is that guard's else
   separator. The guard's semantic checks may still reject an invalid condition,
   but they cannot reinterpret the separator as another operator.
3. After a leading `?|`, `|` separates the complete operation's settlement
   fallback or a registered exact named arm. This role exists only because the
   settlement production was opened by `?|`. The retired `| ...` candidate is
   rejected rather than treated as another branch.
4. In every other value-expression position, `lhs | rhs` is a syntax error
   before type inference. Consequently `true | false`, `0 | 1`, `a | b`, and
   `a | b | 42` have no inferred type and no fallback interpretation.

There is no purity-based escape hatch and no "accept first, diagnose after
inference" path. D02 is closed with no ordinary Optional/value fallback
operator: `??` is rejected just like a bare binary `|`. Optional values must be
handled by explicit control flow or pattern facilities whose own grammar and
typing contracts are defined independently.

---

## 8. Resource System

### 8.1 Resource Identifiers: `@`

Resources are accessed via the `@` prefix:

```
@("localhost:8080")          // auto-detect protocol
@file("readme.txt")          // explicit file protocol
@mysql("localhost:3306")     // explicit MySQL protocol
@binance("BTCUSDT")         // exchange data feed
```

**Protocol resolution:**
- `@{...}` or `@(...)` without prefix → runtime probes via plugin dictionary
- `@protocol(...)` with prefix → compile-time static dispatch (zero overhead)

### 8.2 Handle Acquisition: `<-`

```
f <- @file("readme.txt")
```

`<-` extracts a live handle (file descriptor, socket, cursor) from a resource.

### 8.3 Reading: `>>`

```
f >> #(chunk: [byte; 4096]) => { buf += chunk }
```

For resource reads, `>>` treats the left resource handle as an iterable source and pushes each produced chunk or line as a pulse into the right-side closure.

### 8.4 Writing: `<<`

```
"Hello Styio" << f
```

### 8.5 Lifecycle: Scope-based RAII

Resources are automatically released when their enclosing scope ends. The
compiler inserts cleanup code at every exit path, including normal scope exit,
loop `^` / `>>` control-flow exits, and `<|` returns. Current implementation
evidence covers tracked file handles for those paths. It also covers the
default file flex-rebind cleanup boundary: before `name = @file(...)`
overwrites an existing tracked file handle, codegen closes the old handle,
checks the cleanup error channel, and stops before the new acquire or later
statements if cleanup failed. Statement-shaped file rebind can now install an
explicit settlement site with `?| name = @file(...) | fallback` or named
handlers; under that wrapper, the old-handle cleanup error remains on the
resource-effect channel instead of continuing into the replacement open.
Broader resource-family cleanup and source-level fallback recovery for implicit
cleanup remain staged implementation work.

### 8.6 Directional Transfer Axiom: `->`

```styio
operation -> result
job -> answer
ma5 -> @database("redis://localhost/ma5_cache")
```

`left -> right` is a graphical statement about real data direction: the value
produced at the location/action on the left flows into the
destination/location/receiver endpoint drawn on the right. This is the arrow's
only language-level meaning. The language must not first classify it as
assignment, export, redirection, resource write, or task binding. Ordinary
variables, task-result receivers, resources, channels, and terminal devices are
different endpoint types, not different arrow meanings. Each endpoint's
separately decided protocol determines compatibility, completion families, and
lowering. The right side never declares a name: an identifier destination must
already exist and have the required write capability, while another destination
expression must independently resolve to a legal endpoint. A successful
directional transfer always produces `() : unit`; it never implicitly returns
the source, destination, or a receipt. Ownership, backpressure scheduling,
multi-edge chaining, and arrow associativity remain with their focused owners.
Evaluation ordering is fixed by Q03-F: source value and endpoint capability are
independent prerequisites of transfer, so arrow direction does not imply
source-before-endpoint preparation. If both preparations are order-sensitive,
the author must settle/bind them in consecutive Block items before transfer.
See [Functional Evaluation and Effect Ordering](./Styio-Functional-Evaluation-and-Effect-Ordering.md) §6.

This direction axiom is orthogonal to effect settlement. `?|` consumes one
complete operation, so `?| operation -> result | fallback` is parsed as
`?| (operation -> result) | fallback`. It is never a task-specific await/binder
form, and the arrow does not declare a typed target. To bind the successful
value returned by settlement itself, use the ordinary binding expression
`answer = ?| operation | fallback`.

### 8.7 Standard Stream Resources

Styio models the three Unix standard streams as **compiler-recognized resource atoms** over a
single built-in terminal handle, canonically written `[>_]`. The parenthesized terminal device
`(>_)` remains a compatibility spelling for parser/runtime surfaces that already use it.

The current frozen grammar accepts:

```
@stdout
@stderr
@stdin
```

directly in source code. Users do not need to repeat the internal prelude declarations before
using these standard streams. The declarations still exist as Styio source in the resource prelude
rather than as a C++ resource-name registry.

**`>_` — The Terminal Device**

`>_` is the first-class terminal device value. In symbolic standard-stream definitions, the
bracketed terminal-handle spelling `[>_]` is canonical:

| Operation | Canonical symbolic form | Compatibility form | Unix fd | Semantics |
|-----------|--------------------------|--------------------|---------|-----------|
| Scalar write | `x -> [>_]` | `x -> (>_)` | fd 1 | Write one scalar/text value to stdout |
| Iterable write | `xs >> [>_]` | `xs >> (>_)` | fd 1 | Advance `xs` item by item; write each item as a pulse to stdout |
| Scalar error write | `!(x) -> [>_]` | `!(x) -> (>_)` | fd 2 | Write one scalar/text value to stderr (unbuffered) |
| Iterable error write | `!(xs) >> [>_]` | `!(xs) >> (>_)` | fd 2 | Advance `xs` item by item; write each item as an unbuffered pulse to stderr |
| Read stream shorthand | `<\|[>_]` | `<\|(>_)` | fd 0 | Return the terminal input stream |
| Read stream expanded | `<\| <- [>_]` | `<\| <- (>_)` | fd 0 | Pull the terminal input stream, then return it |

`!()` acts as a **channel selector**: without `!`, data goes to fd 1 (stdout); with `!`,
data goes to fd 2 (stderr). The compiler disambiguates from logical NOT by context:
`!(expr) -> ( >_ )` is always channel-select.

`expr -> [>_]` and `expr -> @stdout` are scalar/text redirects to stdout. `items >> [>_]`
and `items >> @stdout` are narrower: the left side
must be an iterable value whose items can be serialized to text, such as `list[T]`, `dict[string,T]`,
or an explicitly produced line list. `>>` serializes and writes each item as a separate pulse into the stream sink. Plain `string >> [>_]` and `string >> @stdout` are rejected so the compiler never has
to guess between character iteration and newline splitting. Use `string -> [>_]` for scalar text,
or `string.lines() >> [>_]` / `string.lines() >> @stdout` when newline splitting is intended.

**@stdout** — write-only, system-default buffering (line-buffered for TTY, block-buffered for pipes).

**@stderr** — write-only, **unbuffered** (immediate `fflush(stderr)` after each write).

**@stdin** — read-only, iterable stream. The canonical internal declarations are:

```styio
@ stdin := #() => { <|[>_] }
@ stdin := #() => { <|(>_) }
@ stdin := #() => { <| <- [>_] }
@ stdin := #() => { <| <- (>_) }  // compatibility terminal-device spelling
```

`<|(>_)` is a call-like shorthand: `<|` supplies the exported value and `(>_)` is the
terminal-device argument. `[>_]` replaces the earlier `| >_ |` spelling to avoid a `|>`
visual/tokenization ambiguity. `@stdin >> #(line) => {...}` iterates lines.
EOF terminates iteration naturally. New design text should not use `<<` for stdin reads or
`lines << @stdin` for implicit collection; collect explicitly inside the iterator body or through
a named typed-read API. Older frozen docs and implementations accepted `(<< @stdin)` as instant
pull; treat that as a compatibility artifact, not the canonical read/pull spelling.

**Write syntax:**

```
42 -> @stdout              // redirect value to stdout (action)
"Hello" -> @stdout         // redirect string to stdout
@stdout("Hello")           // resource call form; not the canonical write spelling
```

Feature test catalog use `-> @stdout` / `-> @stderr` as the **canonical spelling**.
The current compiler also accepts iterable sink writes:

```
values >> @stdout
text.lines() >> @stdout
warnings >> @stderr
records >> @file("out.txt")
```

When `>>` is followed by a writable resource atom (`@stdout`, `@stderr`, or `@file(...)`), the
parser builds a `resource_write` node. The semantic rule matches terminal-handle `>> [>_]`:
the left side must be iterable and text-serializable. Use `->` for scalar values and `>>` only
where sink-pulse style is intentional.

**Direction constraints:**

- `@stdin` is read-only: `expr -> @stdin` and `expr >> @stdin` are semantic errors
- `@stdout` / `@stderr` are write-only: `@stdout >> #(x) => {...}` is a semantic error
- Standard streams need no repeated user-authored declarations; `f <- @stdout` is a semantic error

**Compiler recognition:** The compiler recognizes `@stdout`, `@stderr`, `@stdin` directly at
parse/lowering time and emits FFI-backed standard-stream runtime-helper IR (`styio_stdout_write_cstr`
for stdout writes, `styio_stderr_write_cstr` for stderr writes, and `styio_stdin_read_line` for
stdin line reads). Scalar `expr -> @stdout` lowers directly through the standard-stream write
IR family. Iterable `items >> @stdout` and `items >> @file(...)` first expand into a per-item
pulse loop, and the loop body writes each item through the matching sink-write IR family. The
`>>` route requires text-serializable iterable input before lowering.

---

## 9. State Management

### 9.1 The Problem

Stream processing requires **memory across pulses**. A simple local variable resets every frame. Styio solves this with explicit state containers.

### 9.2 Resource Object State

resource topology treats named state as a resource object:

```styio
@price : f64|..10| := { ... }
```

Bare `@price` is the resource object itself. It is not the latest scalar value.

```styio
latest = @price[-1]
prev   = @price[-2]
recent = @price[-3..]
all    = @price[...]
```

`T|n|` means exact length; `T|..n|` means a recent-window resource that keeps the latest `n` values. Unbounded repetition is written with a type suffix:

```styio
i64|10|     // ten i64 values
f64|..10|   // latest ten f64 values
i64..       // unbounded i64 sequence
i64...      // same as i64..
```

### 9.3 Type-Level Collection Sugar

Collection types are ordinary type-position forms:

```styio
__ : list[T] := T..
__ : string := char..
__ : dict[K, V] := (K, V)..
```

Examples:

```styio
@input : i64|10| := ...
@meta  : dict[string, string] := ...
@pairs : (string, string)|2| := ...
@price : f64|..10| := ...
@log   : string := ...
@logs  : list[string] := ...
```

`list[i64]|10|` is not the canonical spelling for ten integers; it normalizes to `i64|10|`. Prefer the direct length form.

### 9.4 Resource Flow and Copy

```styio
expr -> @price
@price >> #(x) => { ... }
snapshot << @price[...]
```

Here the destination endpoint happens to be a resource, so the generic `->`
directional edge lowers as a resource write; this is not a second arrow role.
`>>` enters a snapshot-backed block or iteration and commits resource effects at
block exit. `<<` makes an explicit copy or snapshot.

Resource acquisition uses `<-` only at resource-entry boundaries:

```styio
l <- @stdin: list[i32]
l1 << l
```

Copying an already-bound resource as `l1 <- l` is rejected; use `l1 << l`.

### 9.5 Retired state-resource State Families

The old state-resource state-declaration and shadow-read families are retired parser errors. Active code must
use resource objects:

```styio
@ma5 : f64|..5|
get_ma(prices, 5) -> @ma5

@total_vol : f64|..1|
next_total = @total_vol[-1] + volumes
next_total -> @total_vol
```

The compiler rejects retired state-family spellings with migration diagnostics. Exact old wording
is recoverable from Git history when needed.

### 9.6 Retired History Probe Family

The old state-resource history-probe selector family is not active syntax.

---

## 10. Stream Synchronization

### 10.1 Aligned Sync (Zip): `&`

```
@binance >> #(p) & @okx >> #(p_okx) => {
    arbitrage_gap = p - p_okx
}
```

`&` is an **event-arrival barrier**. Each side delivers events independently;
the first-arriving value stops at the barrier and waits — it does nothing else —
until the other side's event arrives. Only when both values are ready does the
closure fire once with the matched pair. The trigger frequency is
`min(freq_A, freq_B)`.

Blocking is the contract, not an accident: the wait deliberately trades
progress for the guarantee that both channel states are fresh at the frame
boundary. The operator defines **no staleness, expiry, timeout, or tolerance
policy** for the earlier value — a developer who writes `&` is explicitly
choosing to wait on the mutual dependence and already knows the earlier value
will sit through some delay. When the two streams do not actually depend on
each other, write separate per-stream pipelines instead of zipping them.

Finite zip also applies to the standard input line stream when the other side is a materialized
list or an `@file` stream:

```styio
@stdin >> #(label) & prices >> #(price) => {
    >_(label + " " + price)
}
```

This consumes at most one stdin line per matched frame and terminates at stdin EOF or the
shorter finite peer. Zipping `@stdin` with itself is not active syntax until duplicate
consumption of the same external stream has a driver-level decision.

The old tolerance-window sketch (`&[5ms]`) is removed: it presupposed a
staleness policy that contradicts the barrier semantics, and no duration
literal exists in the lexical grammar. `&` takes no bracketed argument.

### 10.2 Snapshot Pull: `<< @resource`

```
@binance >> #(p) => {
    p_okx << @okx("BTC")
    gap = p - p_okx[-1]
}
```

The explicit `<<` copy makes the snapshot boundary visible. New topology text should prefer
resource-object or snapshot-object reads such as `p_okx[-1]`.

Inline immediate pull (no state declaration, live read):

```
gap = p - (<- @okx("BTC"))
```

### 10.3 Synchronization Summary

| Mode | Syntax | Trigger | Use Case |
|------|--------|---------|----------|
| Zip | `A >> #(a) & B >> #(b)` | Both arrive | Atomic cross-exchange arbitrage |
| Snapshot | `v << @res` + `v[-1]` | Main flow only | Cross-frequency reference |
| Immediate Pull | `(<- @res)` | On-demand | One-shot live sampling |

---

## 11. Selector Operators: `[mode, arg]`

Square brackets form a **pure-symbol selection algebra**: they decide *which
elements* of the receiver are selected, never *what computation* runs on them.
The rule is **brackets select, functions compute** — no word (identifier,
parameter name, or operation name) participates in selector syntax. Named
computations are ordinary function calls in the library namespace.

### 11.1 Static Indexing and Slicing

```
a[0]        // element at index 0
a[-1]       // last element / latest committed value
a[0..5]     // slice from 0 to 5
a[2..]      // slice from index 2 to end
a[..5]      // slice from start to index 5
a[..]       // all values
a[...]      // all values
```

Two or more dots are equivalent in selectors: `a[0..5]`, `a[0...5]`,
and `a[0.....5]` use the same range separator. A single dot remains
member access, as in `a.length`.

### 11.2 Retired Guard Selector: `[?, cond]`

The postfix guard selector was an early draft and is no longer active syntax.
Use `?(cond) => then_value | else_value` for value selection, or normal `?(cond) => { ... }`
blocks for statement-level control.

### 11.3 Retired Equality Probe: `[?=, val]`

The postfix equality probe was retired with the guard selector. Use `?=` match
blocks for equality-style branching.

### 11.4 Removed Word-Mode Selectors: `[op, n]` → Ordinary Calls

The word-mode selector spellings are removed from the design:

```
prices[avg, 20]    // removed spelling — parser acceptance is compatibility debt
avg(prices, 20)    // accepted surface: ordinary call syntax
max(prices, 14)    // accepted surface: ordinary call syntax
```

Words such as `avg` and `max` belong to the library namespace, not to the
grammar. Series intrinsics follow the same model as the matrix helpers
(`matmul(a, b)`, `transpose(m)`): ordinary identifier calls at the grammar
level, recognized as compiler intrinsics during semantic analysis. The
compiler machinery is unchanged behind the name — fingerprinting the triple
`(source, avg, n)` for deduplication, implicit hidden-ledger slots, and a
scalar result per tick, with implementation and limits recorded in
[Styio-StdLib-Intrinsics.md](./Styio-StdLib-Intrinsics.md).

The parser path that still recognizes `avg` / `max` inside brackets is
compatibility debt and must converge on rejecting the word-mode spelling with
a migration diagnostic. Deferred operators such as `min`, `std`, `ema`, and
`rsi` will land as ordinary calls only (`min(prices, n)`, `rsi(prices, n)`);
no word-mode selector spelling will be added for them.

### 11.5 Stride Selector: `[%n]`

```
[0..100][%5]           // 0, 5, 10, ..., 95
@prices[-100..][%2]    // every 2nd pulse of the last 100
```

`x[%n]` keeps every element of the receiver whose position satisfies
`index % n == 0`, counting from the first selected element. The symbol is the
semantics: "every n-th" is exactly the modulo filter, so `%` — the existing
modulo token — is reused rather than inventing a word or a new glyph. There
is no left operand inside the bracket, so `[%` can never collide with the
binary modulo operator.

Rules: `n` is a positive integer; `[%1]` is the identity selection; `[%0]` is
rejected. The stride selector composes with the other symbolic selectors —
`xs[a..b][%n]` strides the slice, and on a materialized range the compiler may
fuse `[a..b][%n]` into an arithmetic progression without materializing.

Status: design-accepted surface. The parser does not implement it yet and
fails closed until parser, Sema, lowering, and test evidence land.

### 11.6 Retired History Probe Family

The old state-resource postfix history selector family is not active syntax. Future history access
must use resource selectors or a revised state-topology fixture.

Historical examples remain provenance only in Git history.

---

## 12. I/O and Side Effects

### 12.1 Terminal Device: `>_`

`>_` is the **terminal device primitive** — a first-class resource handle representing the
user's terminal. Symbolic standard-stream definitions write it canonically as `[>_]`, with
`(>_)` retained as a compatibility spelling. All standard streams (`@stdout`, `@stderr`,
`@stdin`) are compiler-recognized resource atoms over this terminal device. See §8.7 for the
complete resource definitions and usage patterns.

**As a print statement (legacy, backward-compatible):**

```
>_("Hello, Styio!")
>_(variable)
```

`>_(expr)` remains functional for backward compatibility and is internally equivalent to
`expr -> ( >_ )`.

**As a value in expressions:**

```
42 -> @stdout                        // >_ used as redirect target under the hood
@stdin >> #(line) => { >_(line) }     // >_ used as stream source under the hood
```

**Type formatting rules** (applies to `>_()`, scalar `-> @stdout`, and each item emitted by iterable `>> @stdout` / `>> @file(...)`):

| Type | Output format |
|------|---------------|
| Integer | `%lld\n` |
| Float | `%.6f\n` |
| Bool | `true\n` / `false\n` |
| String | `%s\n` |
| Char | `%c\n` |
| Empty `? \| T` | `(?)\n` |

### 12.2 I/O Buffer: `>_` (stream context)

In stream contexts, `>_` writes to the system's buffered output channel.

### 12.3 Format Strings: `$`

```
$"Price is {p}, Volume is {v}" -> @stdout
```

### 12.4 Standard Error: `@stderr`

`@stderr` writes to Unix fd 2 with immediate flush. See §7.7 for definition.

```
"Error: file not found" -> @stderr
```

### 12.5 Standard Input: `@stdin`

`@stdin` is a line stream from Unix fd 0. See §7.7 for definition.

```
@stdin >> #(line) => {
  line -> @stdout
}
```

---

## 13. Error Handling Philosophy

### 13.1 Fail-Fast for Structural Errors

If a resource schema mismatch is detected (e.g., accessing a non-existent database column), the program terminates immediately at connection time — **before** the first data pulse. No silent degradation.

### 13.2 Typed Propagation for Missing Data

Missing stream data is represented only as `(?)` inside a static `? | T`.
Downstream operations may preserve that Optional state only when their result
type also exposes it; an ordinary `T` result must prove presence or be rejected.
Terminal value formatting uses `(?)`. Debuggers may use a distinct internal
display marker for provenance, but it is not source-observable value state.

### 13.3 Diagnostic Provenance Has No Source Operator

Debuggers and runtime diagnostics may display internal absence provenance such
as a reason code and source location. That provenance is semantically inert:
source code cannot bind it, branch on it, compare it, or change release behavior
through it. The discarded proposal that assigned diagnostic extraction to
`??` is not active or approved syntax.

`??` has no source-language role. The orphan token and the old
diagnostic-extraction route are implementation debt to delete, not syntax to
reserve for a second interpretation.

### 13.4 No Ordinary Optional Fallback Operator

Styio has no general value-level fallback or coalescing operator. In particular,
`optional | default`, `optional ?? default`, `true | false`, `0 | 1`, and
unconstrained `a | b` are rejected by the expression grammar before type
inference (§7.4). This is a settled syntax boundary, not a late type or purity
diagnostic.

D02 is closed by excluding that feature, so there is no operator precedence,
chaining, laziness, result-type algebra, or overload contract to infer for value
fallback. Effect failure recovery remains the distinct, grammar-anchored
settlement form `?| operation | fallback`: the leading `?|` opens settlement of
one complete operation, and its following `| fallback` branch is not an ordinary
value operator and does not authorize one. If the operation contains `->`, the
arrow keeps its single directional-transfer meaning.

---

## 14. Compilation Modes

### 14.1 Development Mode (JIT)

- Full standard library loaded
- AI-assisted protocol probing enabled
- LLVM ORC JIT for instant execution
- Diagnostic provenance collection may be active for debugger/runtime output;
  it remains semantically inert and has no source extraction operator

### 14.2 Strict Mode (AOT, `styio build --strict`)

- All types must be explicitly annotated
- All resource protocols must be specified
- Intent-aware dead code elimination
- Output: minimal native binary or WebAssembly module

### 14.3 Audit Mode (`styio audit --fix`)

- Scans retired state syntax as migration diagnostics and target `@name : Type` resources
- Generates explicit `schema` block at file header
- Reformats source according to Styio style guide

---

## Appendix A: Authority And Decision Boundary

This file specifies accepted language semantics. It does not use parser
recognition, a token enum, an executable fixture, or a proposed lowering path as
evidence that an undecided feature exists in the language.

- Exact grammar is owned by [`Styio-EBNF.md`](./Styio-EBNF.md).
- Token/glyph lookup is owned by
  [`Styio-Symbol-Reference.md`](./Styio-Symbol-Reference.md).
- The compact accepted-surface index is
  [`syntax/ACTIVE-SYNTAX.md`](./syntax/ACTIVE-SYNTAX.md).
- Frozen owner decisions and their unique design landing points are indexed by
  [`Styio-Language-Decision-Ledger.md`](./Styio-Language-Decision-Ledger.md).
- Questions requiring an owner choice, historical failure evidence, and their
  dependency order live in
  [`../review/STYIO-SYNTAX-DECISION-REVIEW-Draft.md`](../review/STYIO-SYNTAX-DECISION-REVIEW-Draft.md).
- Parser/Sema/backend availability, compatibility deletion, and migration work
  belong to the convergence matrix, rollups, and Better Plans.

Reserved tokens and implementation experiments remain non-language until an
owner decision is landed in these authorities. In particular, current-Block
completion does not activate a continuation system. The `->` direction and
operation-completion decisions now fix Unit success, pre-existing destinations,
static finite completion facts, settlement matching, joins, propagation, and
discard rejection; they do not decide transfer ownership, public signature
spelling, chaining, scheduling, or resource-family pressure policy.
