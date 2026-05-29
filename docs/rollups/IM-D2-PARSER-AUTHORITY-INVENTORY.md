# IM-D2 Parser Authority Inventory

**Purpose:** Record the implementation contract for IM-D2 so accepted Styio grammar is judged by the compiler-owned parser instead of legacy fallback, editor snapshot drift, or consumer-local syntax approximations.

**Last updated:** 2026-05-30

**Status:** Active contract inventory. This document supports [NEXT-STAGE-GAP-LEDGER.md](./NEXT-STAGE-GAP-LEDGER.md) §5.7 `IM-D2`.

## Contract Manifest

| Contract item | Implemented position | Evidence |
|---------------|----------------------|----------|
| Parser authority | The hand-written nightly compiler parser is the only accepted grammar authority | [../../src/StyioParser/Parser.cpp](../../src/StyioParser/Parser.cpp) |
| Public syntax service | `styio check --syntax --json --file` uses nightly parser only; `legacy` is rejected | [../../src/StyioServices/StyioCLI/SyntaxCheck.cpp](../../src/StyioServices/StyioCLI/SyntaxCheck.cpp), `StyioDiagnostics.SyntaxCheckRejectsNonAuthoritativeParserEngine` |
| Accepted grammar no-fallback | Nightly top-level statement declines now fail with a syntax diagnostic instead of calling `parse_stmt_or_expr_legacy` | [../../src/StyioParser/Parser.cpp](../../src/StyioParser/Parser.cpp) |
| Internal bridge no-fallback | List, dict, block, statement, match, iterator, and hash-statement internal fallback points now reject unsupported syntax instead of routing to legacy parser helpers | [../../src/StyioParser/NewParserExpr.cpp](../../src/StyioParser/NewParserExpr.cpp), [../../src/StyioParser/Parser.cpp](../../src/StyioParser/Parser.cpp) |
| Resource-effect file rebind route | Statement-shaped `?| f = @file(...) | fallback` is accepted through nightly `ResourceEffectAST` / `FlexBindAST` parsing without legacy fallback; scalar flex binds and value-required file rebind expressions stay rejected | `StyioSecurityNightlyParserStmt.ParsesResourceEffectFileRebindStatement`, `StyioSecurityNightlyParserStmt.RejectsScalarFlexBindResourceEffectStatement`, `StyioSecurityNightlyParserStmt.RejectsFileRebindResourceEffectExpression` |
| Resource-method returned resource-effect route | A resource method block that returns a value-producing `?| ... | ...` expression now routes that block through the authoritative nightly subset parser, preserves `ResourceEffectAST`, and keeps returned `?| op | ...` discard rejected as statement-only | `StyioSecurityNightlyParserStmt.ParsesResourceMethodReturnedResourceEffectFallbackExpression`, `StyioSecurityNightlyParserStmt.RejectsResourceMethodReturnedResourceEffectDiscardExpression` |
| IDE boundary | IDE syntax snapshots are non-authoritative editor data; semantic bridge uses strict nightly parser facts and no longer recovers later semantic facts from malformed source | [../../src/StyioServices/StyioIDE/CompilerBridge.cpp](../../src/StyioServices/StyioIDE/CompilerBridge.cpp), `StyioSemanticBridge.RejectsMalformedInputWithoutRecovery` |
| Unsupported syntax | Unsupported syntax is a parser error, not a subset gap that can pass through fallback | `StyioDiagnostics.MalformedStatementPrefixReportsParseErrorWithoutCrash`, `StyioSyntaxDrift.CorpusMatchesApprovedEnvelope` |

## Accepted Grammar Rules

1. Accepted Styio source must parse through the nightly parser without top-level legacy fallback.
2. Accepted Styio source must not require internal nightly-to-legacy expression, block, statement, match, list, dict, iterator, or hash-statement bridges.
3. Public syntax validation may accept `--parser-engine nightly` only as an explicit no-op spelling.
4. Public syntax validation must reject `legacy`, `new`, generated parser names, or consumer-specific parser names.
5. Syntax-check recovery diagnostics are diagnostic collection only. Any recovered parse diagnostic still makes the result `syntax_error`.

## IDE Boundary

`StyioIDE` may expose token, CST, matching-token, folding, completion-context, and syntax-diagnostic snapshots for editing. Those snapshots do not define accepted grammar. IDE hosts that need a syntax-validity answer must call the compiler parser surface through `StyioCLI` or a future compiler-owned parser API.

The current Tree-sitter-backed and fallback editor snapshot code is therefore an editing aid, not a language specification. It must not be used as evidence that a source form is accepted.

## Legacy Parser Boundary

The legacy parser remains present as migration and parity tooling, but it is not the accepted grammar authority. Existing shadow gates may continue to prove that retained compatibility fixtures do not accidentally route through legacy fallback. New language acceptance must be demonstrated by authoritative nightly parsing and no-fallback evidence.

## IM-D2 Closure Position

The parser-authority contract part of IM-D2 is implemented: public syntax validation is locked to nightly, nightly parser fallback points fail closed, IDE semantic analysis uses strict compiler parse facts, and the service manifest documents that editor syntax snapshots are not grammar authority.

Remaining parser work is feature implementation debt, not an IM-D2 decision: a future syntax form must either be accepted by the nightly parser with no fallback and tests, or rejected with a stable diagnostic until it is intentionally designed.
