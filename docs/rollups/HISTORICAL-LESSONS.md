# Styio Historical Lessons

**Purpose:** Compress the first-wave dated history and review material into a few durable themes that future agents can load before opening raw archived docs.

**Last updated:** 2026-04-15

## Parser Migration And Gates

1. The parser migration only stayed manageable once the team stopped treating “new parser works on samples” as enough and instead froze route-level evidence: shadow compare, summary artifacts, zero-fallback gates, zero-internal-bridges gates, and then five-layer goldens for high-value resource paths.
2. Parser rollout succeeded because the team accepted staged subset takeover rather than one-shot replacement: hash forms first, then block/control bodies, then resource writes, iterators, snapshot/state entry, and finally nightly-first cutover with explicit legacy decline boundaries.
3. Every parser boundary fix that mattered ended up needing a reproducible failing bookmark plus a durable gate. The pattern was: red test or artifact, narrow subset fix, gate expansion, ADR, then keep the checkpoint reproducible.
4. Benchmark work later confirmed the same rule: performance baselines were only meaningful after parser shadow fallback was driven back to zero on the blocking milestone cases.

## Ownership And Runtime Hardening

1. The compiler stopped being “probably okay” only after ownership moved from raw ambient allocation to explicit lifecycle structures: `CompilationSession`, AST RAII migration, scoped cleanup, handle-table takeover, and targeted sanitizer/soak coverage.
2. The 2026-03-30 review bundle identified the recurring failure classes that stayed relevant afterward: unchecked lexer boundaries, leaked context/token/IR ownership, unsafe FFI ownership, shell-based command construction, and overly wide JIT symbol exposure. Later checkpoints largely followed that review’s map.
3. Runtime hardening worked best when paired with classification cleanup. Replacing active-path `StyioNotImplemented` throws with `ParseError` or `TypeError`, stabilizing stderr/jsonl diagnostics, and making invalid-handle behavior explicit removed a large class of ambiguous failures.
4. String ownership and file-handle ownership both required “consume after use” discipline plus regression coverage. Any future runtime rewrite should assume that implicit ownership conventions are not trustworthy unless they are frozen by tests.

## StdIO And Five-Layer Formalization

1. Standard streams only became coherent after the docs, milestones, fixtures, and implementation were forced back into one contract: compiler-recognized `@stdout/@stderr/@stdin`, canonical `->` writes, accepted `>>` shorthand, borrowed stdin line buffers, and a separate numeric instant-pull contract.
2. The team explicitly chose “freeze the actual contract first, redesign later” for stdio. That is why format strings were removed from M9 acceptance and why `(<< @stdin)` remained numeric even though string semantics may still be desirable later.
3. Five-layer pipeline cases turned previously fuzzy resource behavior into durable acceptance: redirect/file writes, instant pull, stdin echo/transform/pull, mixed stdio, snapshot/state, zip/file-stream, and full pipeline paths all became inspectable across tokens, AST, StyioIR, LLVM IR, and runtime output.
4. The stable lesson is to separate canonical user-facing syntax from accepted compatibility shorthand, then freeze both intentionally with different tests instead of letting compatibility survive by accident.

## Docs And Benchmark Workflow

1. The repo only became operable at scale after document roles were separated: design/specs/review/assets for durable SSOT, dated history for raw execution logs, milestones for frozen acceptance, rollups for compressed loading, and archive for provenance only.
2. Generated indexes, docs audit, lifecycle validation, and explicit SSOT routing matter because without them the same story reappears in plans, history, README files, and milestone notes with drifted wording.
3. Benchmarking needed the same discipline as docs: one route, structured artifacts, local reports ignored by default, and a clear rule that conclusions belong in tracked docs while raw logs stay local.
4. The most reusable workflow pattern across both docs and performance work was: write the stable rule once, keep the raw trace elsewhere, and automate the boundary so future contributors do not have to rediscover it manually.
