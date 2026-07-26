# Styio Operation Completion and Settlement

**Purpose:** Define the accepted static completion model for operations and the
observable semantics of `?|` settlement without introducing a managed exception
runtime or an ordinary `Result` wrapper.

**Last updated:** 2026-07-26

**Status:** Accepted decisions `Q01-A`, `Q02-BC`, `Q02-SIG`, `Q02-INF`, and the
completion-facing integration of retained literal/conversion/Euclidean
subcontracts and unified `Q05-NUMERIC-OPS` through 2026-07-26.

## 1. Scope and terms

Every operation has two compile-time facts:

1. one success type `S`; and
2. a finite set of nominal completion families that the operation may produce
   instead of `S`.

This pair is a Sema fact and part of the operation/callable type identity. It is
not a source value, a hidden `Result[S, E]`, an exception object, a new keyword,
or permission to add a managed unwinder. A pure operation has an empty
completion-family set. A successful no-payload operation has success type
`unit` and produces the real value `()`.

Owner decisions `Q02-BC` and `F1-INFERRED-ABSTRACTION` require every callable
boundary to expose one finite completion-family upper bound in its canonical
contract. An eligible final, non-recursive public callable may have that
contract inferred at its definition and published in the module interface.
Recursive, native/FFI, and typed protocol ABI boundaries must still write it
in source. A definition's actual completion set must be a subset of the bound.
Every caller settles a family or propagates it; omission never authorizes a
dynamic or ambient escape path.

`Q02-SIG` fixes the source contract spelling:

```styio
# read_price : f64 ?| {io, parse} := (path: string) => { ... }
# abs : i64 := (x: i64) => { ... }
# local := (x) => { ... }
```

- `?| {io, parse}` is a non-empty finite completion-family upper bound. Its
  braces and comma separators are signature structure, not a runtime set,
  dict, Block, value union, or recovery expression.
- Family names are ordinary resolved identifiers. Duplicate names,
  non-family names, an empty `?| {}`, and a trailing comma are rejected.
- A written `: T` without a completion clause always declares an empty bound,
  regardless of scope. It never requests completion inference.
- An eligible final, non-recursive callable that omits the entire `: T`
  contract may infer the complete
  `OperationSummary(success_type, completion_set)`, including for stable
  publication at a public module boundary.
- Recursive, native/FFI, and typed protocol ABI boundaries cannot omit the
  contract. The spelling adds no
  keyword or token: `?|`, braces, and commas already exist and are selected by
  callable-signature grammar context.

The sole detailed owner of eligibility, principal constrained rank-1 schemes,
generalization, fresh instantiation, and rebinding is
[Styio Callable Principal Inference](./Styio-Callable-Principal-Inference.md).
For completion purposes, every inferred scheme still contains one stable
concrete finite completion bound; it never contains a completion-row variable.
Exact literal constraints are owned by
[Styio Exact Numeric Literals](./Styio-Exact-Literals-and-Builtin-Add.md);
concrete built-in operator rows and their completion masks are owned by
[Styio Built-in Numeric Operators and
Inference](./Styio-Builtin-Numeric-Operators-and-Inference.md). For example,
checked signed-integer addition admits `{overflow}`, floating addition admits
`{}`, and a generalized addition constraint spanning both uses the finite
conservative union `{overflow}`. Statically known integer overflow takes the
same completion edge as runtime checked addition.

Checked built-in scalar conversion is owned by
[Styio Checked Scalar Conversion](./Styio-Checked-Scalar-Conversion.md).
Every concrete `Source :> Target` row contributes only its precise finite
subset of `{out_of_range, inexact, non_finite}`; a generalized closed
conversion constraint uses the conservative union of its legal rows. A known
conversion failure takes the same named completion edge as runtime conversion.

Accepted signed-integer division and remainder are owned by
[Styio Euclidean Signed-Integer Division and
Remainder](./Styio-Euclidean-Signed-Integer-Division-and-Remainder.md).
Each concrete `/` row contributes
`{divide_by_zero, overflow}` and each `%` row contributes
`{divide_by_zero}`. A known zero divisor or unrepresentable quotient takes the
same named completion edge as runtime execution; `MIN_T % -1` succeeds with
zero and never acquires an overflow completion.

## 2. Completion-family names are identifiers

A completion family is nominally declared by the prelude or the owning typed
protocol. Its source name is parsed as an identifier and resolved semantically;
it is not placed in the tokenizer keyword table. This design does not reserve
`io`, `err`, `parse`, `bounds`, or any other example name.

Within a named settlement arm:

```styio
family => recovery
family(binding) => recovery
```

- `family` is a completion-family identifier.
- `binding` is an ordinary author-chosen local identifier whose scope is only
  the recovery expression or Block.
- The parenthesized form binds the family's typed payload.
- The bare form performs the same exact family match without binding a payload.
- Binding a family that has no payload is a compile-time error.

The prelude's `overflow` family is the accepted payload-free arithmetic
completion identity. It is still an ordinary resolved identifier rather than a
keyword: `overflow => recovery` is valid, while `overflow(binding)` is invalid.

The prelude's `divide_by_zero` family is likewise a payload-free ordinary
identifier. Its accepted use by signed-integer `/` and `%`, and the operations'
different completion masks, belong to
[Styio Euclidean Signed-Integer Division and
Remainder](./Styio-Euclidean-Signed-Integer-Division-and-Remainder.md).
`divide_by_zero(binding)` is invalid.

The prelude's `out_of_range`, `inexact`, and `non_finite` checked-conversion
families are likewise payload-free ordinary identifiers. Their exact
classification order and source/target matrix belong to
[Styio Checked Scalar Conversion](./Styio-Checked-Scalar-Conversion.md);
settlement cannot bind a payload to any of them.

For example, if a protocol declares a family named `io`, then these names are
ordinary identifiers rather than fixed language words:

```styio
answer = ?| read_packet()
         | io(problem) => recover(problem)
         | fallback_value
```

Renaming the local `problem` to `err` does not change the program. Lookup,
visibility, and qualification of family identifiers follow the later module
decision; lexical recognition does not special-case their spelling.

## 3. Directional transfer completion

`source -> destination` keeps its separately frozen graphical meaning: data
produced on the left flows to the endpoint on the right.

Its completion contract is now fixed:

- successful transfer has result `() : unit`;
- it never implicitly returns the source, destination, or an acknowledgement;
- a business receipt requires an explicit value-producing operation;
- `->` never declares a name;
- an identifier destination must already exist and have the required write
  capability;
- a resource, terminal, or constructed destination expression must independently
  resolve to a legal endpoint.

Endpoint protocols may contribute completion families, capability checks, and
lowering rules without changing the arrow's meaning or success type. Copy,
consume, borrow, capture, and ownership post-states are owned by accepted
[Q04-Core](./Styio-Ownership-Capture-and-Capability.md). Q03-F ordering is owned by
[Functional Evaluation and Effect Ordering](./Styio-Functional-Evaluation-and-Effect-Ordering.md):
source value and endpoint capability are independent transfer prerequisites,
and arrow direction does not order their preparation.

## 4. Settlement forms

```styio
value = ?| operation

value = ?| operation
        | fallback_value

value = ?| operation
        | family => recovery
        | family(payload) => recover(payload)
        | fallback_value
```

`?|` settles exactly one complete operation. Success bypasses every recovery
arm and produces the operation's `S`. The operation is evaluated once. Only the
selected recovery arm is evaluated, exactly once and lazily; unselected arms
are not evaluated. Settlement never retries the operation implicitly.

Named arms match nominal family identity exactly. There is no exception-class
inheritance, superclass catch, or dynamic handler search. The same family may
not appear twice. A catch-all fallback, when present, must be last.

The bare `| fallback_value` matches only remaining recoverable failure families.
It deliberately does not match EOF, cancellation, shutdown, or fatal/trap.
These non-failure or control completions require an exact named arm when the
owning protocol makes them handleable; otherwise they propagate. Fatal/trap is
outside `?|` entirely.

## 5. Result join and propagation

The success result and every recovery arm that can complete normally must have
one compatible canonical type `S`:

```text
join(S, recovery_1, ..., recovery_n) = S
join(S, never) = S
```

Reachable `S`/`unit` disagreement is an error. The compiler never repairs a
join with a default, zero, Unit discard, Optional injection, or a synthesized
union. A settled directional transfer therefore requires every normally
completing recovery arm to produce `unit`.

Static propagation is set subtraction plus union:

```text
remaining completions
  = operation completions not handled by an admitted arm
  + completions produced by recovery expressions
```

An exact named arm removes only its family. A catch-all removes only remaining
recoverable failure families. Every other completion propagates as a static
fact of the enclosing operation/callable. No dynamic stack search or ambient
error channel exists.

## 6. Completion categories

| Category | Static meaning | Settlement behavior |
|---|---|---|
| Value absence | Ordinary value of type `? | T` | Never caught by `?|`; use explicit Optional control flow |
| Success | Produces exactly one `S` | Bypasses recovery arms |
| EOF | Normal data-source terminal family, distinct from absence | Exact named arm or propagation; never bare fallback |
| Recoverable failure | Nominal typed failure family | Exact named arm or trailing catch-all fallback |
| Checked integer overflow | Prelude's payload-free nominal `overflow` family | Exact `overflow` arm or propagation; payload binding is invalid |
| Signed-integer division/remainder | Exact row subset of payload-free `divide_by_zero` and `overflow`; remainder excludes `overflow` | Exact named arm or propagation; payload binding is invalid |
| Checked scalar conversion | Precise payload-free subset of `out_of_range`, `inexact`, and `non_finite` | Exact named arm or propagation; payload binding is invalid |
| Cancellation | Control-terminal family | Exact named arm only when protocol admits it, otherwise propagation |
| Shutdown | Control-terminal family | Exact named arm only when protocol admits it, otherwise propagation |
| Fatal/trap | Non-recoverable termination | Outside `?|` |
| Pressure | Observable resource signal, not automatically a completion family | Invisible to `?|` until a resource protocol explicitly escalates it |

This distinction prevents Optional flattening from collapsing EOF with a real
absent element and prevents a catch-all from swallowing structured cancellation
or shutdown.

## 7. No wildcard discard

`?| operation | ...` is not canonical syntax in any context. Existing parser,
AST, Sema, IR, fixture, or documentation routes for it are deletion debt.

To absorb one known completion family in statement position, the author names
it explicitly and produces Unit:

```styio
?| close(resource)
 | cleanup(problem) => {
     report(problem)
 }
```

The reachable Block fallthrough above is `() : unit`. Other families continue
to propagate. Ignoring a family's payload with a bare exact arm is distinct
from silently discarding an unknown family.

## 8. No hidden retry or scheduling contract

Settlement does not define arrow chaining, fan-out, parallel launch, buffering,
backpressure scheduling, retry, continuation capture, ownership transfer, or
rollback. A recovery expression may explicitly call another operation, but
that is a new author-visible evaluation with its own completion facts rather
than an implicit replay of the failed operation.

Q03-F now fixes the surrounding graph: ordinary safe-pure siblings have only
dependency order; a settlement node remains one order-sensitive computation;
Block sequence and accepted control/resource edges order it against other
observable work. The local invariant remains one operation evaluation, one
selected lazy arm at most, and no evaluation of unselected arms. See
[Functional Evaluation and Effect Ordering](./Styio-Functional-Evaluation-and-Effect-Ordering.md) §7.3.

## 9. Runtime-free implementation boundary

The compiler represents finite completion sets and bounded payload shapes in
typed Sema/IR facts. Unit success consumes no physical payload while retaining
its logical success state. Block-result candidates remain unpublished until
required exit obligations settle, as specified by
[Lexical Block Completion](./syntax/BLOCK_COMPLETION.md).

Implementation may use fixed compiler-sized state and direct control-flow
edges. It may not require heap-allocated exception objects, an open-ended
failure list, dynamic handler lookup, stack unwinding, truncation, or a hidden
fallback value.

## 10. Authority and follow-up boundary

- Formal grammar: [Styio-EBNF.md](./Styio-EBNF.md).
- Glyph summary: [Styio-Symbol-Reference.md](./Styio-Symbol-Reference.md).
- Compact authoring map: [syntax/ACTIVE-SYNTAX.md](./syntax/ACTIVE-SYNTAX.md).
- Directional flow and task/resource context:
  [Styio-Language-Design.md](./Styio-Language-Design.md), §6.9 and §8.6.
- Frozen decision index:
  [Styio-Language-Decision-Ledger.md](./Styio-Language-Decision-Ledger.md).
- Single implementation/migration owner:
  [Directional Flow and Operation Settlement](../plan/styio-directional-flow-and-settlement/Requirements.md).

Accepted decision `Q02-INF`, owned by
[Styio Callable Principal Inference](./Styio-Callable-Principal-Inference.md),
now fixes local/private callable inference, stable rebinding, fresh
instantiation, and failure-closed behavior. Accepted Q03-F evaluation and
effect-order semantics are owned by
[Functional Evaluation and Effect Ordering](./Styio-Functional-Evaluation-and-Effect-Ordering.md);
accepted [Q04-Core](./Styio-Ownership-Capture-and-Capability.md) owns
copy/consume/borrow/capture and ownership post-states; [Styio Exact Numeric
Literals](./Styio-Exact-Literals-and-Builtin-Add.md) owns exact terms,
[Styio Checked Scalar Conversion](./Styio-Checked-Scalar-Conversion.md) owns
`:>`, and
[Styio Euclidean Signed-Integer Division and
Remainder](./Styio-Euclidean-Signed-Integer-Division-and-Remainder.md) owns the
signed Euclidean invariant. Accepted [Styio Built-in Numeric Operators and
Inference](./Styio-Builtin-Numeric-Operators-and-Inference.md) owns every
numeric result/completion row, lossless widening, mixed comparison, and
compound assignment. Intentionally lossy named conversions, unsigned/platform
types, and remaining NaN bit-level policy stay deferred; `Q09` owns resource-family
escalation, buffering, and pressure policy; and `F02` owns author-written
generics and completion rows. None may redefine this completion algebra
implicitly.
