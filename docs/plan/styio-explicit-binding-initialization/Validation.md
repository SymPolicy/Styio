# Explicit Binding Initialization Validation

**Purpose:** Map each explicit-binding requirement to positive, negative, structural, migration, and documentation evidence.

**Last updated:** 2026-07-14

## Validation principles

1. A positive explicit binding and its adjacent missing-RHS rejection are tested together.
2. Parser-only rejection is insufficient; AST/Sema/IR static checks prove that no later default repair exists.
3. Supplied binders and schema/protocol declarations receive positive regression coverage so the grammar boundary is not implemented as a blanket ban on `identifier : Type`.
4. Rejected old behavior survives only in negative diagnostic fixtures.

## Requirement matrix

| Requirement | Positive evidence | Negative/static evidence |
|-------------|-------------------|--------------------------|
| `REQ-BI-001` | Mutable/final bindings with inferred and explicit scalar, optional, Unit, collection, record, callable-value, and handle types. | Bare `name : T` for each representative family emits the stable parser diagnostic. |
| `REQ-BI-002` | `(?)`, `()`, zero, `false`, and `""` work only when explicitly authored and type-correct. | No missing-RHS case yields an AST, output, or inferred value; static search finds no synthesized-default helper. |
| `REQ-BI-003` | Parameters and match/iteration binders use their supplying routes; `answer : T = ?| operation | fallback` uses an ordinary RHS; generic `?| (operation -> destination) | fallback` preserves a directional endpoint. | Supplying context cannot be omitted; ordinary missing RHS cannot masquerade as a binder, settlement cannot declare a target, and a generic arrow cannot be reclassified as a task-specific binder. |
| `REQ-BI-004` | Schema and topology declarations still parse under their owning rules; resource reads require valid protocol state. | No schema/topology declaration gains an implicit construction default; resource audit flags read-before-valid-initialization. |
| `REQ-BI-005` | AST/Sema/IR tests construct and lower bindings with real RHS operands. | Constructors/verifiers reject null/missing operands; code search finds no compatibility AST, initialized bit, or default repair. |
| `REQ-BI-006` | EBNF, Language Design, Symbol Reference, ACTIVE-SYNTAX, runbooks, editor grammar, and formatter agree. | Docs/syntax/lifecycle/local-information gates pass; old positive fixtures are absent. |

## Required source cases

### Accepted

```styio
x = 1
y: i64 := 2
missing: ? | i64 = (?)
done: unit = ()
# f = (arg: i64) => arg
items >> #(item: i64) => { ... }
answer: i64 = ?| job | fallback
?| (operation -> destination) | fallback
```

Schema fields and resource topology slots use the canonical syntax defined by their owning SSOT and are included as separate regression inputs.

### Rejected

```styio
x: i64
flag: bool
text: string
missing: ? | i64
done: unit
items: list[i64]
record_value: RecordType
handle: FileHandle
```

Each case must fail before Sema/default construction and include the stable diagnostic fragment.

## Structural inspections

1. `rg -n "make_default_value_for_decl_latest" src tests docs` returns no implementation or positive-test match after migration; historical explanation may remain only in the decision/evidence docs.
2. Binding AST constructors/factories require a value expression.
3. Sema binding metadata has no ordinary maybe-initialized state added by this work.
4. StyioIR binding nodes and verifiers require one value operand.
5. Lowering/codegen have no missing-binding-value fallback.
6. Resource zero-fill sites are classified as protocol-internal and cannot be read as ordinary values before validity.

## Targeted commands

Exact test names are finalized in the test-discovery checkpoint. The final suite must include the repository's parser/unit target, affected language-feature cases, task/resource settlement cases, pipeline goldens, and security diagnostics, followed by:

```text
python scripts/syntax-convergence-gate.py
python scripts/docs-index.py --check
python scripts/docs-audit.py
python scripts/docs-lifecycle.py validate
python scripts/local-info-leak-gate.py --mode worktree
python <better-plan>/scripts/manifest_tool.py validate docs/plan
```

## Final evidence record

The final-validation checkpoint records the head commit, command result, requirement IDs covered, migrated/deleted fixture list, stable diagnostic code, and any separately routed resource-topology finding. Acceptance cannot be weakened to retain the old spelling or a default fallback.
