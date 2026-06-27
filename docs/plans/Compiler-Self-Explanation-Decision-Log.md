# Compiler Self-Explanation Decision Log

**Purpose:** Record compiler gaps found during the self-explanation audit that were either closed autonomously because they already matched accepted Styio design, or closed by maintainer decision with an explicit follow-up order before implementation changes define language semantics.

**Last updated:** 2026-06-28

**Status:** Active decision log for maintainer review.

## 前置条件

1. 并行: run this plan as parallel inventory and evidence lanes first. Placeholder clusters, retired-syntax diagnostics, resource-topology readiness, parser-performance fixtures, stream boundaries, and IDE readiness may be audited independently; only shared Sema, IR, codegen, diagnostic, or parser-authority edits are serial merge gates.
2. 子智能体: sub-agents may own one placeholder family, decision row, test family, or docs-sync lane at a time, then return evidence, risk notes, and proposed owner gates for maintainer review.
3. 基座: shared visitor, diagnostic, workflow, or test-harness changes must be moved first into [Styio-Common-Foundation-Plan.md](./Styio-Common-Foundation-Plan.md) before this decision log drives feature closure.

## Autonomous Closure in This Checkpoint

| Item | Why it was safe to close | Result | Proof |
|------|--------------------------|--------|-------|
| Format strings (`$"..."`) | `Styio-Language-Design.md`, `Styio-EBNF.md`, and active stdio-output fixtures already describe `$"..."`; the compiler had lexer/parser/AST shape but rejected the syntax and lowered `FmtStrAST` to a placeholder | Both legacy and nightly parser routes now parse `$"..."`; embedded expressions are type-inferred and lowered through existing string concatenation/runtime conversion | `ctest --test-dir build/default -R '^(StyioParserEngine\\.LegacyAndNightlyMatchOnStdioOutputFmtStringSample|stdio_output_t06_stdout_fmtstr)$' --output-on-failure` |
| Hash-tag iterator sequence silent placeholder | The parser accepted forms such as `[1, 2] >> #price`, but active syntax docs do not define runnable semantics, and the previous IR path silently produced `SGConstInt(0)` | The path now fails closed with `STYIO_TYPE_STREAM_HASH_TAG_ROUTE_UNSUPPORTED` instead of continuing execution with a bogus placeholder; this is diagnostic-only and does not define route semantics | `ctest --test-dir build -R '^StyioDiagnostics\\.IteratorSequenceHashTagRoutingReportsFeatureCode$' --output-on-failure` |
| Hash-tag iterator diagnostics | Parser errors contained informal text and did not explain the accepted iterator body shape | Diagnostics now use maintainer-facing wording: `expected hash tag name after # in iterator sequence`, `iterator sequence expects another #tag after >`, and `expected #(param...) or #tag after >> in iterator` | Covered by build and parser tests; no new syntax added |

## Owner Decisions And Execution Order

| Decision | Closed answer | Implementation consequence | Required evidence |
|----------|---------------|----------------------------|-------------------|
| Hash-tag iterator sequences | Retire `>> #tag` / `> #tag` as an old parser-route draft. Do not define route semantics from the previous placeholder behavior. | Future work may remove or narrow the parser shell, keep `STYIO_TYPE_STREAM_HASH_TAG_ROUTE_UNSUPPORTED`, and sync migration diagnostics/tests. It must not add runtime behavior for hash-tag routes unless a new design decision reopens the syntax. | Focused hash-tag diagnostic test plus parser/no-fallback evidence for the accepted `#(param) => { ... }` iterator body shape. |
| Next Sema/IR checkpoint | Start with a P0 inventory checkpoint before more feature closure lands. | Inventory all remaining placeholder clusters, empty visitors, unsupported inline-clone surfaces, and generated/default `SGConstInt(0)` uses, then close one cluster per checkpoint by real lowering or typed failure. | Inventory diff, StyioIR verifier regression, affected feature/security tests, and docs-audit. |
| Resource topology migration | After P0, use a canonical-only topology migration track. | Advance active `@name : Type`, selectors, `expr -> @name`, and explicit `<<` copy behavior without reviving retired state-resource spellings or compatibility probes. | Resource-topology feature/security tests, active-syntax docs update, and stable negative migration diagnostics for retired forms. |
| Performance priority | Prioritize Algorithmic Optimization C: parser Pratt / precedence table after the P0 inventory and canonical topology prerequisites are represented. | Parser performance work must preserve accepted grammar, no-fallback parser authority, and stable diagnostics; it may not make undecided syntax executable for speed. | Legacy/nightly equivalence, precedence fixtures, parser shadow/no-fallback gates, and large/deep expression perf evidence. |
| Stream-processing expansion | Do not expand stream/runtime behavior through retired hash-tag routes or duplicate stdin consumption. | Future stream slices must use active syntax and pass parser, sema, lowering, runtime, feature, and five-layer gates. `@stdin & @stdin` stays fail-closed until an explicit tee/buffer or duplicate-driver design is accepted. | Stream-processing feature case, duplicate-stdin diagnostic regression, five-layer evidence, and parser shadow gates. |

## Parallel Execution Model

| Lane | Can run in parallel | Serial merge gate |
|------|---------------------|-------------------|
| P0 placeholder inventory | Audit `SGConstInt(0)` defaults, empty visitors, unsupported clone surfaces, direct unsupported AST lowering, verifier coverage, and affected tests in separate sub-agent lanes. | One owner merges the inventory into IM-D1, this log, the test catalog, and the first implementation-cluster pick. |
| Hash-tag retirement | Audit parser shell removal options, diagnostic stability, migration wording, and feature/security tests independently. | Any parser or diagnostic-code edit that changes accepted/rejected syntax must merge after parser-authority review. |
| Duplicate stdin boundary | Audit diagnostics, stream-source docs, and future tee/buffer design notes independently. | No runtime or driver behavior may merge until an explicit tee/buffer or duplicate-driver design is accepted. |
| Canonical resource topology | Audit active syntax, resource-topology fixtures, negative migration diagnostics, and docs drift in parallel with P0 inventory. | Sema/lowering implementation for a touched AST/IR node waits for the relevant P0 inventory slice to land. |
| Parser Pratt performance | Build precedence fixture inventory, perf harness inputs, and equivalence expectations in parallel with topology/P0 work. | Parser implementation merges only after no-fallback authority, accepted grammar, and diagnostic preservation gates are named. |
| IDE rename readiness | Audit semantic identity, workspace-index freshness, stale publication suppression, and diagnostics-publication tests independently. | `textDocument/rename` capability advertising waits for all readiness gates to pass in one IDE/LSP merge checkpoint. |

## Guardrails for the Next Checkpoint

1. Do not introduce source syntax that is absent from `docs/design/Styio-Language-Design.md`, `docs/design/Styio-EBNF.md`, or `docs/design/syntax/ACTIVE-SYNTAX.md`.
2. Replace active execution placeholders with either real lowering or explicit typed diagnostics.
3. Preserve the current Sema/lowering visitor split; do not move compiler behavior into parser-only special cases.
4. Every semantic closure should update this log or the next-stage gap ledger in the same merge unit.
5. Retired hash-tag routes may keep a stable diagnostic and migration test, but they must not become runnable semantics.
6. Performance checkpoints must keep the parser-authority contract intact; speed work cannot promote unsupported syntax.

## 验收条件

1. Every autonomous closure row names proof commands or explains why no new syntax or behavior was added.
2. Every owner decision row names the closed answer, implementation consequence, and required evidence.
3. Parallel lanes produce evidence independently, but each semantic or contract change names the serial merge gate before implementation lands.
4. Any shared foundation work discovered by the decision review is moved to the common foundation plan before feature implementation begins.
5. Follow-up implementation is not considered accepted until the owning design, test catalog, runbook, and narrow feature gate agree.
