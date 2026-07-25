# Styio Block Completion and Bottom Type — Evidence

**Purpose:** Ground the Block completion and bottom-type delivery plan in frozen owner decisions, current repository behavior, and primary language/compiler evidence.

**Plan:** `styio-block-completion-and-bottom-type`

**Last updated:** 2026-07-16

## Frozen owner evidence

The decision register freezes `O01-Q02..Q05`, `O05-Q01`, and `P01.11-A` /
`O05-Q03..Q05, O05-Q07`: first-class `unit`, no general implicit tail
expression, lexical current-Block yield, reachable fallthrough as `unit`,
incompatible `T`/`unit` exits as errors, public contextual type name `never`,
one AST/target for both yield spellings, legal `<| ()`, structural
unreachability errors, and no value discard at Unit-only consumers.

Source: [STYIO-SYNTAX-DECISION-REVIEW-Draft.md](../../review/STYIO-SYNTAX-DECISION-REVIEW-Draft.md).

## Repository evidence

| Finding | Evidence | Delivery consequence |
|---|---|---|
| Identifiers are tokenized uniformly as `NAME`; type names are resolved contextually. | `src/StyioParser/Tokenizer.cpp:505-514`, `src/StyioParser/Parser.cpp:1298-1375`, `src/StyioToken/Token.hpp:627-663,1165-1210` | Add `never` to the built-in type model without adding a lexer keyword/token. |
| `BlockAST` has statement/following lists but no result or completion summary. | `src/StyioAST/AST.hpp:514-573` | Sema needs an explicit per-Block completion summary; AST ownership must distinguish lexical yield from function return. |
| `<|` currently becomes `ReturnAST`, and nested return discovery scans subtrees. | `src/StyioAST/AST.hpp:713-739`, `src/StyioParser/Parser.cpp:3149-3155,3850`, `src/StyioParser/NewParserExpr.cpp:2516,2949-2953` | Replace the misleading AST concept in one migration; do not retain a compatibility alias. |
| `<| expr` and `|<| expr |;` already converge on a return-shaped parse route, but the node has no lexical owner and the inline terminator is not expressed as a frozen surface contract. | `src/StyioParser/NewParserExpr.cpp:2947`, `src/StyioAST/AST.hpp:713` | Normalize both spellings to one lexically owned `BlockYieldAST`; require inline `|;` without adding a second semantic form. |
| Function result inference recursively inspects selected final AST nodes and treats empty Blocks as `undefined`. | `src/StyioSema/TypeInfer.cpp:972-1017,4441-4449` | Replace tail heuristics with one linear completion/join analysis whose reachable fallthrough is `unit`. |
| Sema has no explicit value-versus-Unit-only Block consumer contract or structural reachability authority. | `src/StyioSema/TypeInfer.cpp:3520` | Type-check every yield against its consumer and diagnose structural unreachability before lowering; never rely on value discard or optimizer cleanup. |
| Task result inference defaults missing results to `i64`. | `src/StyioSema/TypeInfer.cpp:1709-1740` | The common flow contract must not permit an integer fallback; generic task settlement/directional composition is owned by the P01.14-A delivery plan. |
| Function lowering wraps selected tail expressions in `SGReturn` and reports a missing tail before a general Block result exists. | `src/StyioLowering/AstToStyioIR.cpp:220-282,4196-4318` | Lower lexical Block results first; only the function-body adapter emits a function return. |
| Codegen repairs failed return coercion with a default runtime value and returns integer zero for an empty `SGBlock`. | `src/StyioCodeGen/CodeGenG.cpp:2798-2869` | Remove default repair from Block/function-result paths and represent `unit`/`never` without payload fabrication. |
| Codegen stops emitting siblings after a terminator, while existing documentation/tests still describe nested `<|` as exiting a function. | `src/StyioCodeGen/CodeGenG.cpp:2827`, `tests/security/styio_security_test.cpp:13367`, `docs/teams/TEST-QUALITY-RUNBOOK.md:44` | Make unreachability a Sema error, then migrate/delete old cross-Block expectations atomically rather than treating backend omission as validation. |

## Other-language experience

1. [Rust's unit type](https://doc.rust-lang.org/stable/core/primitive.unit.html) is a real one-value type. This preserves generic/type identity even when no payload is emitted.
2. [Rust's never type](https://doc.rust-lang.org/reference/types/never.html) is uninhabited and coerces at control-flow joins. Rust also had a historical never-type fallback to `()`; Styio avoids that class of surprise by making `never` only a proven CFG fact, never a fallback inference.
3. [Rust 2024 never-type fallback guidance](https://doc.rust-lang.org/edition-guide/rust-2024/never-type-fallback.html) documents compatibility and unsoundness problems caused by guessing a fallback type for diverging expressions. This supports explicit join rules and diagnostics.
4. LLVM's [`unreachable` instruction](https://llvm.org/docs/LangRef.html#unreachable-instruction) marks a path that cannot continue but produces no value. It is a backend fact, not permission for the source type checker to invent a normal result.
5. Languages that implicitly return a null-like value on fallthrough make accidental missing returns indistinguishable from deliberate absence. Styio's explicit `? | T` boundary requires the opposite rule: missing normal values fail at compile time.
6. [Rust value-carrying loop exits](https://doc.rust-lang.org/reference/expressions/loop-expr.html) require an explicit target when control would otherwise be nonlocal. Styio avoids importing a labeled/nonlocal model by making both yield spellings resolve to the same immediate lexical Block.
7. [Kotlin returns](https://kotlinlang.org/docs/returns.html) distinguish local, labeled, and inline-lambda nonlocal exits. That history warns against allowing compact punctuation to select a second target-resolution rule.
8. Java's structural unreachable-statement rule makes invalid flow a compile-time property. Styio likewise diagnoses after semantic CFG analysis, independent of whether codegen or optimization would omit the sibling.

## Confirmed gaps

1. No canonical `Unit`/`Never` alternatives exist in `StyioDataTypeOption` or the built-in type table.
2. No reusable control-flow result lattice exists in Sema.
3. AST `ReturnAST` conflates lexical Block completion with function return.
4. StyioIR has `SGReturn` and `SGBlock` but no lexical Block-result boundary.
5. Codegen contains default-value repair that can hide an invalid earlier result analysis.
6. Active docs/tests contain contradictory nonlocal-return wording.
7. No single contract proves inline-yield termination, legal explicit Unit yield,
   Unit-only consumer rejection, and structural unreachable diagnostics together.

## Risk evidence retained outside this plan

Optional layout, generic `unit` storage, public ABI mapping, cleanup effects,
and task settlement can change representation constraints. This plan defines
only the logical completion/type contract. Generic Unit/ABI work belongs to
`unit-zero-payload-boundaries`; the frozen publication and exit-failure protocol
belongs to `block-exit-publication-and-settlement`; decided D02 excludes ordinary
value fallback, and P01.14-A directional-flow/settlement convergence belongs to
its dedicated plan.
