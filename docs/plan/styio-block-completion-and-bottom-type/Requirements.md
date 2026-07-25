# Styio Block Completion and Bottom Type — Requirements

**Purpose:** Define the source-observable requirements and explicit non-goals for the frozen Styio Block completion and bottom-type contract.

**Plan:** `styio-block-completion-and-bottom-type`

**Last updated:** 2026-07-16

## User problem

Styio currently parses `<| expr` and its inline spelling into a
function-return-shaped AST, infers Block results by inspecting selected tails,
and contains backend fallbacks that can fabricate integer/default return
values. Those mechanisms cannot express the frozen rule that both spellings
complete only their current lexical Block, that reachable fallthrough is
`unit`, that Unit-only consumers cannot discard values, and that
non-completion is `never`. Without one explicit flow algebra, nested Blocks,
branches, functions, diagnostics, and optimization can disagree about whether
a path produced a value.

## Target users and workflows

- Language authors writing value Blocks and functions with direct or explicit results.
- Compiler maintainers extending branch, match, loop, resource, or task control flow.
- IDE and formatter consumers that need one parse tree and one public type spelling.
- Test and release maintainers validating that missing values are rejected instead of repaired.

## Functional requirements

### REQ-BC-001 — Reachable Block completion is `unit`

Every lexical Block has a result contract. Reaching its closing brace normally produces the unique value `()` of type `unit`. A statement-only or empty function body therefore has result type `unit`; it does not produce integer zero, absence, or an undefined compiler type.

### REQ-BC-002 — Normal-path joins fail closed

Every reachable normal exit from a value-producing Block must have a compatible canonical result type. A `T` exit and a reachable `unit` fallthrough are a compile-time error. The compiler must not insert a default, infer `? | T`, or treat a separator or formatter choice as a result-policy switch. Deliberate absence remains an explicit `(?)` path governed by the separately frozen optional-union contract.

### REQ-BC-003 — `never` is the public bottom type

`never` is an uninhabited built-in type name, accepted wherever a type is syntactically allowed and never constructible, literal-producing, or defaultable. A control-flow edge that cannot complete has type `never`; its only value-join law is `join(T, never) = T`. `never` is not an inference fallback and never changes to `unit`.

### REQ-BC-004 — Lexical Block results compose into functions

`<| expr` and inline `|<| expr |;` are the same lexically owned completion of
the current Block; inline `|;` is mandatory, and `<| ()` is legal. The outermost
function-body Block result becomes the function result. For a direct
single-expression function body, `=> expr`, `=> { expr }`, and
`=> { <| expr }` have the same type and runtime behavior. Multi-item Blocks use
explicit `<|` when they produce a non-`unit` value.

### REQ-BC-005 — Contextual type name, no keyword

The tokenizer continues to emit `StyioTokenType::NAME` for `never`. Type parsing/resolution recognizes `never` contextually through the built-in type table. No lexer keyword, dedicated token, statement form, or control-flow punctuation is added.

### REQ-BC-006 — Stable diagnostics and single implementation

The compiler emits stable, source-located diagnostics for incompatible Block
exits, missing non-`unit` function results, construction/defaulting of `never`,
and a sibling or region whose every structural incoming edge has already
completed. Structural unreachability is a compile-time error independent of
constant folding, optimization, or backend terminator omission. The AST and IR
represent lexical Block completion distinctly from function return. Obsolete
cross-Block behavior and tests are removed rather than kept as executable
compatibility.

### REQ-BC-007 — Strict current-Block yield surface and consumers

`<| expr` and inline `|<| expr |;` parse to the same `BlockYieldAST` and target
the immediately owning lexical Block. The inline `|;` terminator is mandatory
and creates no semantic node or second exit boundary. `<| ()` is legal explicit
Unit completion. Statements that structural flow proves unreachable after an
unconditional completion are compile-time errors independent of optimization.
A Unit-only Block consumer accepts only `unit` and rejects a non-Unit yield
rather than evaluating and discarding its value.

## Non-functional constraints

1. Parser authority remains the nightly hand-written parser; editor grammars mirror but do not redefine acceptance.
2. Flow analysis must be linear in the size of the analyzed control-flow region, with cached summaries rather than repeated subtree scans.
3. `unit` and `never` carry no fabricated runtime payload. Optimization may erase zero-payload facts only after preserving their static type identity.
4. Diagnostics must use the shared compiler diagnostic contract and preserve exact source spans for the conflicting exits, missing inline terminator, structurally unreachable region, or Unit-only mismatch.
5. The change converges in one implementation. No old `ReturnAST` alias, legacy lowering route, or fallback integer result remains.

## Scope

`src/StyioToken`, `src/StyioParser`, `src/StyioAST`, `src/StyioSema`, `src/StyioIR`, `src/StyioLowering`, `src/StyioCodeGen`, affected editor/formatter surfaces, `tests/features/functions`, `tests/features/control_flow`, parser/unit/security/pipeline tests, and the active language documentation set.

## Non-goals

- Optional-union recovery operators, implicit initialization, `Default`, or P01.14-A directional-flow/settlement implementation. Frozen generic Unit storage, no-payload task/effect state, and C ABI adaptation are owned by the child `unit-zero-payload-boundaries` plan. Frozen publication, cleanup ordering, and bounded multi-failure behavior are owned by the sibling child `block-exit-publication-and-settlement` plan rather than this parent.
- General user-defined bottom types or arbitrary subtyping.
- Rust-style general tail-expression Blocks.

## Final acceptance target

On one head commit, all mapped tests and gates pass; every `REQ-BC-*` behavior
has positive or negative executable evidence; the tokenizer has no `never`
keyword token; nested Block fixtures prove lexical completion; AST tests prove
the two yield spellings normalize identically; Sema rejects structural
unreachability and Unit-only value loss; IR has no second inline-yield or
type-repair/default-return path; and all active language SSOT documents state
the same algebra.
