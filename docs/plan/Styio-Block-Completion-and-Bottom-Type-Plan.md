# Styio Block Completion and Bottom Type Plan

**Purpose:** Deliver the frozen Styio lexical-Block completion algebra and public `never` type as one parser, type-system, IR, code-generation, documentation, and validation convergence.

**Last updated:** 2026-07-16

**Status:** Pending implementation. The owner contracts `O01-Q04..Q05`,
`O05-Q01`, and `P01.11-A` / `O05-Q03..Q05, O05-Q07` are frozen; adjacent
frozen Unit and Block-exit contracts are delivered by their named child plans
rather than this parent.

## 前置条件

1. **并行：** Requirements, repository evidence, external-language evidence, and test discovery may be reviewed in parallel. Source changes that share `Token.hpp`, AST result nodes, or the Block-flow contract remain serial behind the architecture checkpoint.
2. **子智能体：** Sub-agents may perform read-only audits, test discovery, and disjoint documentation checks. One coordinator must reconcile the public type contract, CFG join rules, and final SSOT wording.
3. **基座：** Reuse the existing compiler/session/type-table, diagnostics, parser-authority, documentation, and test-harness substrate. Any missing general-purpose gate belongs to the `common-foundation` plan before this feature depends on it.
4. The frozen source contract is [STYIO-SYNTAX-DECISION-REVIEW-Draft.md](../review/STYIO-SYNTAX-DECISION-REVIEW-Draft.md), especially `O01-Q02..Q05`, `O05-Q01`, `P01.11-A` / `O05-Q03..Q05, O05-Q07`, and `P01.12-A..B` / `O05-Q06`.
5. This plan must not infer answers for optional-value recovery syntax, implicit defaults, or the separately frozen P01.14-A directional-flow/settlement implementation. Frozen generic Unit/effect/ABI consequences are implemented by the child [Unit Zero-Payload Boundaries Plan](./styio-block-completion-and-bottom-type/unit-zero-payload-boundaries/Plan.md). Frozen publication, cleanup ordering, and bounded multi-failure consequences are implemented by the child [Block Exit Publication and Settlement Plan](./styio-block-completion-and-bottom-type/block-exit-publication-and-settlement/Plan.md). Neither belongs to this parent's nodes.

## Delivery target

The compiler converges on one result model:

- reachable natural Block completion is `() : unit`;
- reachable normal results join only when their canonical types are compatible;
- `T` and `unit` do not silently become `? | T`, a default, or an integer sentinel;
- a non-completing edge has type `never`, and only `join(T, never) = T` removes it from a value join;
- `never` is an ordinary `NAME` token with built-in meaning only in type position;
- `<| expr` and inline `|<| expr |;` are one `BlockYieldAST` that completes
  only the current lexical Block; inline `|;` is mandatory, `<| ()` is legal,
  and only the outer function-body Block result becomes a function return;
- a structurally unreachable sibling after unconditional completion is an
  error, and a Unit-only consumer rejects rather than discards a non-Unit
  result.

## Scope

1. Type identity and display for `unit` and `never`.
2. Parser/AST ownership of direct expression bodies and both frozen lexical
   Block-yield spellings through one node.
3. Sema control-flow summaries, deterministic result joins, structural
   reachability, and explicit value-versus-Unit-only consumer contracts.
4. StyioIR and LLVM lowering without fabricated integer/default return values.
5. Stable diagnostics, feature tests, parser tests, pipeline goldens, editor grammar, formatter, and active language documentation.
6. One-shot removal of misleading return-shaped internal paths and obsolete tests where they encode the old cross-Block behavior.

## Non-goals

- No `Option[T]`, optional-union runtime layout, or ordinary value-recovery
  operator implementation. D02 is decided: both bare binary value `|` and `??`
  are rejected, while leading `?| ... | fallback` remains effect settlement.
  Deletion of the legacy fallback pipeline belongs to its dedicated migration
  plan rather than this Block-completion plan.
- No decision about declarations without initializers or a future `Default` capability.
- No generic-storage, collection-cardinality, public C ABI, or nullable-pointer implementation in this parent; those frozen boundaries belong to the child [Unit Zero-Payload Boundaries Plan](./styio-block-completion-and-bottom-type/unit-zero-payload-boundaries/Plan.md).
- No cleanup-order, multi-completion, or task/resource settlement implementation in this parent; the frozen Block-exit protocol belongs to its child plan, while the accepted operation-completion algebra is delivered by the [Directional Flow and Operation Settlement plan](./Styio-Directional-Flow-and-Settlement-Plan.md).
- No compatibility AST, second parser route, executable legacy spelling, or version-shaped implementation.

## Execution graph

The machine-validated graph is [Checkpoints.json](./styio-block-completion-and-bottom-type/Checkpoints.json). Plan-local delivery contracts are:

- [Requirements.md](./styio-block-completion-and-bottom-type/Requirements.md)
- [Evidence.md](./styio-block-completion-and-bottom-type/Evidence.md)
- [Validation.md](./styio-block-completion-and-bottom-type/Validation.md)
- [Architecture.md](./styio-block-completion-and-bottom-type/Architecture.md)

Child delivery graph:

- [Unit Zero-Payload Boundaries Plan](./styio-block-completion-and-bottom-type/unit-zero-payload-boundaries/Plan.md)
- [Block Exit Publication and Settlement Plan](./styio-block-completion-and-bottom-type/block-exit-publication-and-settlement/Plan.md)

## 验收条件

1. All `REQ-BC-*` requirements map to executable checks in `Validation.md` and to implementation nodes in `Checkpoints.json`.
2. `=> expr`, `=> { expr }`, and `=> { <| expr }` agree for a direct single-expression body; `<| expr` and `|<| expr |;` produce the same current-Block node and target, inline `|;` is mandatory, and `<| ()` is legal.
3. Positive tests prove `unit` fallthrough and `join(T, never) = T`; negative tests prove `T`/`unit` joins, construction/defaulting of `never`, structurally unreachable siblings, malformed inline yield termination, and non-Unit yields at Unit-only consumers are rejected with stable diagnostics.
4. The lexer adds no keyword or dedicated token for `never`; type-position recognition uses the existing contextual-name route.
5. AST, Sema, StyioIR, and codegen contain one current implementation, with no integer-zero return repair, cross-Block return compatibility route, or retained obsolete acceptance fixture.
6. Language-feature, pipeline, security, parser-shadow, syntax-convergence, documentation, lifecycle, local-information, and Better Plan gates pass on one head commit.
