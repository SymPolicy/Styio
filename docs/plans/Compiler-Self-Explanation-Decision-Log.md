# Compiler Self-Explanation Decision Log

**Purpose:** Record compiler gaps found during the self-explanation audit that were either closed autonomously because they already matched accepted Styio design, or left for maintainer decision because closing them would define or change language semantics.

**Last updated:** 2026-05-10

**Status:** Active decision log for maintainer review.

## Autonomous Closure in This Checkpoint

| Item | Why it was safe to close | Result | Proof |
|------|--------------------------|--------|-------|
| Format strings (`$"..."`) | `Styio-Language-Design.md`, `Styio-EBNF.md`, and active stdio-output fixtures already describe `$"..."`; the compiler had lexer/parser/AST shape but rejected the syntax and lowered `FmtStrAST` to a placeholder | Both legacy and nightly parser routes now parse `$"..."`; embedded expressions are type-inferred and lowered through existing string concatenation/runtime conversion | `ctest --test-dir build/default -R '^(StyioParserEngine\\.LegacyAndNightlyMatchOnStdioOutputFmtStringSample|stdio_output_t06_stdout_fmtstr)$' --output-on-failure` |
| Hash-tag iterator sequence silent placeholder | The parser accepted forms such as `[1, 2] >> #price`, but active syntax docs do not define runnable semantics, and the previous IR path silently produced `SGConstInt(0)` | The path now fails closed with a `TypeError` instead of continuing execution with a bogus placeholder | `ctest --test-dir build/default -R '^StyioDiagnostics\\.IteratorSequenceHashTagRoutingFailsClosed$' --output-on-failure` |
| Hash-tag iterator diagnostics | Parser errors contained informal text and did not explain the accepted iterator body shape | Diagnostics now use maintainer-facing wording: `expected hash tag name after # in iterator sequence`, `iterator sequence expects another #tag after >`, and `expected #(param...) or #tag after >> in iterator` | Covered by build and parser tests; no new syntax added |

## Decisions Needed

| Decision | Current evidence | Options | Recommended next action |
|----------|------------------|---------|-------------------------|
| What should hash-tag iterator sequences mean? | `IterSeqAST` exists and the parser accepts `>> #tag`/`> #tag`, but the active syntax docs teach `#(param) => { ... }` iterator bodies, not tag-routing semantics | Retire this parser form completely; or define tag-routing semantics in the design SSOT, EBNF, sema, lowering, runtime, and tests | Retire unless there is a clear Topology/stream-processing use case; do not implement by inference from the old placeholder |
| Which Sema/IR placeholder cluster is next? | The current checkpoint closed `FmtStrAST` and made `IterSeqAST` fail-closed, but other `SGConstInt(0)` and empty visitor families remain | Inventory all remaining clusters first; then close one cluster per checkpoint by real lowering or typed failure | Approve a P0 inventory checkpoint before more feature work lands |
| How aggressively should stream-processing stream processing expand? | Stream zip has real slices, but source coverage is still narrow and hash-tag sequences now explicitly fail | Pick a single accepted stream-processing path and carry it through parser, sema, lowering, runtime, and tests; or defer stream-processing until Topology v2 migration is clearer | Choose one stream-processing path with existing active syntax and close it end-to-end |
| When should Topology v2 become parser-enforced compiler behavior? | Design docs already describe Topology v2, but implementation remains partially migrated | Make Topology v2 a dedicated migration track; or keep compatibility paths and document partial support | Use a dedicated Topology v2 migration checkpoint; avoid incidental syntax changes inside unrelated bug fixes |

## Guardrails for the Next Checkpoint

1. Do not introduce source syntax that is absent from `docs/design/Styio-Language-Design.md`, `docs/design/Styio-EBNF.md`, or `docs/design/syntax/ACTIVE-SYNTAX.md`.
2. Replace active execution placeholders with either real lowering or explicit typed diagnostics.
3. Preserve the current Sema/lowering visitor split; do not move compiler behavior into parser-only special cases.
4. Every semantic closure should update this log or the next-stage gap ledger in the same merge unit.
