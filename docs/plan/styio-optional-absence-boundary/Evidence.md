# Styio Optional Absence Boundary Evidence

**Purpose:** Ground the Optional representation and value-fallback deletion in
repository facts, accepted decisions, and other-language failure experience.

**Last updated:** 2026-07-16

## Accepted source contract

- `docs/design/Styio-Language-Design.md` §3.5 makes absence a branch of static
  `? | T`, excludes it from ordinary `T`, and prohibits hidden sentinels.
- The same document §7.4 assigns every accepted `|` from an already-open grammar
  production and rejects general binary value pipe before Sema.
- §13.4 records D02's final answer: no value fallback/coalescing operator;
  leading `?| operation | fallback` remains effect settlement.
- `docs/review/STYIO-SYNTAX-DECISION-REVIEW-Draft.md` records the frozen owner answers
  and closes `O02-Q01..Q08` plus `O28-Q01`.

## Repository findings

| Finding | Evidence | Delivery consequence |
|---|---|---|
| The tokenizer recognizes `??` although the target grammar rejects it. | `src/StyioParser/Tokenizer.cpp:323`; `src/StyioToken/Token.hpp:1557`; `src/StyioToken/Token.cpp:891`; `src/StyioServices/StyioIDE/Syntax.cpp:291` | Delete `DBQUESTION` end to end; two adjacent question marks receive ordinary syntax rejection rather than a reserved compound token. |
| The old parser treats bare `|` as a left-associated value fallback. | `src/StyioParser/Parser.cpp:2817,3003,3149,3155,3251-3266` | Replace every `parse_fallback_expr` call with the correct non-pipe expression entry and delete the helper; do not port it into the Nightly parser. |
| The token/operator tables can revive `a | b` as bitwise OR. | `src/StyioToken/Token.hpp:681,730,771,807` | Remove the source `Bitwise_OR` map/precedence entry while preserving `TOK_PIPE` for anchored grammar roles. Named bitwise policy remains owned by D14. |
| A complete value-fallback AST family survives despite having no accepted syntax. | `src/StyioAST/ASTDecl.hpp:90`; `src/StyioAST/AST.hpp:1829-1860`; `src/StyioToken/Token.hpp:949` | Delete the node, enum member, factories, visitors, ownership tests, cloning, repr, and topology traversal. |
| Empty/undefined concepts overlap and neither is a canonical Optional value. | `src/StyioAST/AST.hpp:306-314,1727-1734`; `src/StyioSema/TypeInfer.cpp:1880-1884,2562-2564`; `src/StyioLowering/AstToStyioIR.cpp:2569-2572,3140-3143` | Introduce one semantic empty-Optional node for `(?)`, `[?]`, and `{?}`; keep compiler “unknown type” state separate and remove source-value `UndefinedLitAST`/`SGUndef`. |
| Sema visits fallback operands but proves no domain, result type, or ownership rule. | `src/StyioSema/TypeInfer.cpp:2581-2584` | Deletion is safer than trying to repair or reuse this path. Optional typing belongs to the type/branch-injection layer, not fallback Sema. |
| StyioIR has `SGUndef` and `SGFallback` as ordinary values. | `src/StyioIR/GenIR/SGIR.hpp:893-918`; `src/StyioIR/Verifier.cpp:259-261`; `src/StyioIR/StyioIRWalker.hpp:290-295,677-681` | Replace absence with typed Optional constructors and delete both retired IR nodes plus walkers/verifiers/optimizers. |
| Codegen uses the legal integer `i64::MIN` as undefined and eagerly evaluates both fallback operands. | `src/StyioCodeGen/CodeGenG.cpp:65-69,3633-3670`; `src/StyioCodeGen/CodeGenPulse.cpp:14-16` | Optional lowering needs a real discriminant; present `i64::MIN` must not compare equal to empty. Deleting fallback also deletes its eager evaluation behavior. |
| `TypeTable` has O(1) interned equality but no structural Optional key. | `src/StyioSession/TypeTable.hpp:16-105`; `src/StyioSession/TypeTable.cpp:35-70`; `src/StyioToken/Token.hpp:15-36,117-180` | Extend the canonical type key with an Optional form and payload `TypeId`; normalize Optional-of-Optional at construction rather than encoding it in strings. |
| The Nightly parser already treats single pipe as context-sensitive grammar punctuation rather than a generic logic operator. | `src/StyioParser/NewParserExpr.cpp:332,381,448,1529,1821-1824,2170-2207,2308-2377` | Preserve and tighten these syntax-selected paths; a deletion must not remove settlement or guard separators. |
| Many tests named “Fallback” exercise accepted resource/effect settlement, while a smaller set directly instantiates retired value nodes. | Resource cases in `tests/styio_test.cpp:9191-13181` and `tests/security/styio_security_test.cpp:7762-9041`; retired-node cases in `tests/lowering_internal_test.cpp:472,963,2678`, `tests/codegen_internal_test.cpp:395-410,1251-1259`, and `tests/security/styio_security_test.cpp:1172-1191,11484-11486,14994-14998` | Classify by AST/grammar owner, not by the English word “fallback”: preserve effect settlement coverage and remove value-node positive tests. |

## Other-language failure lessons

1. Rust exposes separate eager and lazy default APIs on
   [`Option`](https://doc.rust-lang.org/core/option/enum.Option.html). That
   distinction shows that a “small” fallback convenience permanently commits
   evaluation and ownership behavior. Styio avoids the operator entirely.
2. JavaScript's nullish-coalescing proposal created a dedicated grammar and
   forbids unparenthesized mixing with Boolean operators; see the
   [TC39 grammar](https://tc39.es/proposal-nullish-coalescing/). This supports
   syntax-first rejection rather than admitting a pipe and deciding later from
   types or truthiness.
3. TypeScript documented how Boolean `||` defaults incorrectly replace valid
   `0`, `false`, and empty text in its
   [3.7 release notes](https://www.typescriptlang.org/docs/handbook/release-notes/typescript-3-7).
   Styio therefore never derives absence from truthiness or a payload sentinel.
4. Swift's nested-Optional evolution recorded usability and compatibility costs
   in [SE-0230](https://forums.swift.org/t/accepted-se-230-flatten-nested-optionals-resulting-from-try/17376).
   Styio states its different set-like normalization law up front instead of
   allowing accidental layers to escape into APIs.
5. C++'s early `optional` design discussion describes the semantic ambiguity of
   nested engaged/empty states in
   [N3672](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3672.html).
   Styio requires an explicit tagged domain when several empty meanings matter.

## Representation conclusion

The semantic model is a discriminated sum. The safest initial native lowering is
an explicit presence tag plus a payload for non-Unit `T`, and only the tag for
`? | unit`. A niche optimization is not required for delivery and cannot be
enabled until a type-specific proof shows that the chosen bit pattern is outside
the complete legal payload domain and does not change FFI layout. A legal
`i64::MIN` value can never satisfy that proof for `i64`.

## Dependency boundary

`styio_undef_i64()` is also used by arithmetic code in
`src/StyioCodeGen/CodeGenG.cpp:1090-1433`. Those uses violate the general rule
if they leak an invalid marker through ordinary `i64`, but choosing overflow,
division-by-zero, or numeric failure behavior belongs to D08/P04. This plan:

- removes every Optional/fallback interpretation of that sentinel;
- proves Optional `Some(i64::MIN)` remains present;
- deletes `SGUndef` as a source absence value; and
- records remaining numeric-only consumers for the D08 plan rather than silently
  choosing trap, effect, saturation, or another source-observable policy.
