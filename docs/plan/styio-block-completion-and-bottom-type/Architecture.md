# Styio Block Completion and Bottom Type — Architecture

**Purpose:** Define the module boundaries, completion-summary model, and AST-to-backend interfaces for the frozen Block completion algebra.

**Plan:** `styio-block-completion-and-bottom-type`

**Last updated:** 2026-07-16

## 1. Module responsibilities

| Module | Responsibility in this plan |
|---|---|
| `StyioToken` | Canonical built-in `unit`/`never` type identities and display; no keyword token. |
| `StyioParser` | Parse existing function/Block spellings and contextual type names; normalize `<| expr` and `|<| expr |;` to one lexical Block-yield AST while requiring inline `|;`. |
| `StyioAST` | Own syntax structure only: Block, direct body, and `BlockYieldAST`; no function-return semantics inferred here. |
| `StyioSema` | Compute and cache Block completion summaries, canonicalize result types, join exits, enforce value/Unit-only consumer contracts, and emit structural-flow diagnostics. |
| `StyioIR` | Represent a lexical Block result separately from a function return and verify valid terminator/result structure. |
| `StyioLowering` | Convert Sema-proven Block results to IR; adapt only the outer function-body result to `SGReturn`. |
| `StyioCodeGen` | Emit zero-payload `unit`, no value for `never`, PHI/control-flow joins for compatible values, and `unreachable` only for proven divergence. |
| Tests/tooling/docs | Mirror the single accepted syntax and prove positive, negative, nested, and diagnostic behavior. |

Dependency direction remains `Token -> Parser/AST -> Sema -> Lowering/IR -> CodeGen`. Tooling consumes the public syntax contract and must not become a second acceptance authority.

## 2. Completion summary data model

Sema owns one typed control-flow summary per lexical Block. The conceptual model is:

```text
Completion =
  Normal(TypeId, OriginSet)
  | Diverges

BlockSummary = {
  normal_exits: small set of canonical Normal results,
  has_reachable_fallthrough: bool,
  terminated_regions: source spans
}

BlockConsumer =
  ValueExpected(optional canonical TypeId)
  | UnitOnly
```

Reachable fallthrough contributes `Normal(unit)`. `<| expr` contributes `Normal(type(expr))` to its current Block only. A proven non-completing expression/edge contributes `Diverges` and no runtime value.

Join rules are total and deliberately small:

```text
join(Diverges, X) = X
join(X, Diverges) = X
join(Normal(A), Normal(B)) = Normal(canonical(A)) when A and B are compatible
join(Normal(A), Normal(B)) = diagnostic otherwise
```

There is no fallback edge that returns `undefined`, `i64`, a default value, or `? | T`. Origins are retained so an incompatible-exit diagnostic can point to both the value exit and reachable fallthrough/other exit.

The implementation should use a compact vector/small set of exits accumulated
in one traversal, then memoize the summary on the Sema side table keyed by
stable AST identity. A `UnitOnly` consumer accepts only `Normal(unit)` and never
converts `Normal(T)` into `unit` by discard. Structural reachability uses the
same summary: a sibling is rejected only when every incoming structural edge
has completed. This keeps analysis O(nodes + edges), preserves diagnostic
origins, and avoids repeated recursive tail scans.

## 3. AST and IR boundary

1. Rename the source-level completion node from `ReturnAST` to `BlockYieldAST` in one complete migration. Do not keep an alias, duplicate visitor, or compatibility class.
2. `<| expr` and `|<| expr |;` construct that same node. Inline `|;` is required parser punctuation, creates no semantic node, and cannot select another target. `<| ()` is an ordinary Unit-valued instance.
3. A `BlockYieldAST` is lexically owned by the immediately enclosing `BlockAST`. Parser construction does not search for a function.
4. Sema validates the yield expression against the owning `BlockConsumer` and records the Block summary. Unit-only consumers reject non-Unit values. Statements after unconditional completion, including regions whose every structural predecessor has completed, are rejected before lowering and independently of optimization.
5. StyioIR gains one Block-result form distinct from `SGReturn`. Its verifier requires all value exits to match the Sema-proven canonical result type and contains no inline-spelling discriminator.
6. Lowering the outermost function-body Block consumes its result and emits `SGReturn` (or `ret void` for logical `unit`). Nested Blocks deliver values to their lexical consumer, not directly to the function epilogue.
7. The Block-result form exposes a typed candidate/result boundary consumed by the child `block-exit-publication-and-settlement` plan. This parent proves what value a Block produces; the child owns when that candidate publishes and how exit obligations/failures are ordered.

## 4. Type representation

1. Add canonical `Unit` and `Never` alternatives to the compiler type model rather than encoding either as `Defined`, `Undefined`, or `Integer`.
2. Register `unit` and `never` as contextual built-in type names. The tokenizer continues to produce `NAME`.
3. `()` is the sole `unit` value. `never` has no value constructor, literal, default, or storage-producing expression.
4. `never` participates in flow joins through `Diverges`; it is not a conventional runtime value and is never materialized as an LLVM constant.
5. Generic storage and public ABI representation are deliberately deferred. No representation choice in this plan may pre-empt those decisions.

## 5. Deliberate patterns

| Pattern | Use | Why it earns its complexity |
|---|---|---|
| Algebraic completion summary | Sema Block analysis | Makes every exit kind explicit and gives one total join function instead of scattered tail special cases. |
| Side-table memoization by AST identity | Sema | Prevents repeated subtree scans while keeping AST syntax-only. |
| Adapter at function boundary | Lowering | Preserves lexical Block semantics internally and concentrates function ABI return emission in one place. |
| Explicit Block consumer contract | Sema | Makes value loss impossible at Unit-only boundaries and keeps consumer typing out of codegen. |
| IR verifier contract | StyioIR | Prevents codegen from receiving incompatible Block results or fabricated fallthrough values. |
| Pattern-free parsing | Parser | Existing token/context parsing is sufficient; no framework or alternate parser is justified. |

## 6. File ownership and sequencing

| Checkpoint | Primary write scope |
|---|---|
| Type/frontend | `src/StyioToken/Token.hpp`, `src/StyioAST/AST*.hpp`, `src/StyioParser/*`, parser tests |
| Sema flow | `src/StyioSema/*`, semantic negative/positive tests |
| IR/backend | `src/StyioIR/*`, `src/StyioLowering/*`, `src/StyioCodeGen/*`, pipeline/security tests |
| Docs/tooling | active design docs, syntax matrix, tree-sitter/editor/formatter mirrors, runbooks, fixture migrations |
| Child interface | expose the verified Block-result candidate contract; `block-exit-publication-and-settlement` owns epilogue scheduling and fixed failure state |

These checkpoints are serial where their interface contracts meet. Read-only evidence and test discovery may run in parallel; source-writing nodes must follow the declared prerequisites in `Checkpoints.json`.
