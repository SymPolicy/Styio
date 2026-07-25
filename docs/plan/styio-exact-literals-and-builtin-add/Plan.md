# Styio Exact Literals and Built-in Add Delivery Plan

**Purpose:** Define the dependency graph and one converged implementation for approved Q05-LIT-ADD exact literals and scalar `Add`.

**Last updated:** 2026-07-20

**Status:** Pending implementation; all checkpoints are intentionally `pending`.

**Better Plan ID:** `a0063a94-8e76-4121-937a-0a43fa94b8d1`

## 前置条件

1. **Semantic owner:** [Styio-Exact-Literals-and-Builtin-Add.md](../../design/Styio-Exact-Literals-and-Builtin-Add.md) is the sole detailed Q05-LIT-ADD authority. The review record is decision audit, not a competing specification.
2. **Inference boundary:** Q02 owns `TypeTerm`, unification, scheme generalization/instantiation, and specialization. This plan owns exact literal facts, materialization, and the closed relation Q02 queries.
3. **Control-flow boundary:** The existing operation-completion model owns generic completion propagation/settlement and CFG structure. This plan attaches `{overflow}` to integer `Add` and lowers the operation's checked branch into that structure.
4. **No adjacent-policy capture:** Removing accidental string, matrix, container, or mixed-concrete acceptance does not establish their future syntax or semantics. Tests distinguish removal of unauthorized scalar-`Add` paths from unrelated independently owned operations.
5. **Deterministic limits:** Resource gates use checked counts and versioned fixed thresholds, never elapsed time, host word size, allocation failure, or source-order heuristics. Threshold evidence is recorded before the production route is enabled.
6. **Workspace:** `docs/plan` is the existing Better Plan workspace. Parent coordination owns `Manifest.json`, indexes, design/review documents, and runbooks outside this seven-file package.

## Product outcome

For every numeric literal and scalar `+`, parsing, Sema, constant evaluation, typed IR, LLVM emission, runtime behavior, diagnostics, IDE facts, and caches agree on one exact source value, one selected concrete row, and one finite completion summary. Large hostile literals fail predictably. Mixed concrete operands do not promote. Checked integer overflow is ordinary completion control flow. Floating addition remains IEEE-correct under every supported optimization level.

## Dependency tree

- **Authority:** accepted Q05-LIT-ADD semantic owner; existing Q02 and completion/settlement owners.
- **Foundation:** requirements, repository/external evidence, validation matrix, then architecture and interfaces.
- **Core:** exact literal/resource domain, materialization/defaulting, and immutable `Add` catalog.
- **Execution:** shared constant semantics and concrete SGIR, then checked integer/strict-float backend emission.
- **Consumers:** Q02 adapter, diagnostics, IDE/cache publication, fixtures, docs, performance receipts, and old-path deletion.
- **Closure:** one final node reruns all requirement, determinism, structural-removal, and Better Plan gates on one commit.

The graph serializes shared numeric contracts, allows backend and tooling consumers to follow frozen interfaces, rejoins at convergence, and never treats a partial dual-pipeline state as deliverable.

## Artifacts

- [Requirements.md](./Requirements.md) — users, observable requirements, constraints, and ownership boundaries.
- [Evidence.md](./Evidence.md) — current-path inventory, external implementation evidence, risks, and migration facts.
- [Architecture.md](./Architecture.md) — value domains, budgets, relation/evaluator interfaces, IR/backend design, caching, and deletion design.
- [Validation.md](./Validation.md) — requirement-to-test, differential, IR-audit, performance, and structural evidence mapping.
- [Checkpoints.json](./Checkpoints.json) — machine-validated all-pending execution graph.

## Single-final-state rule

The production switch is acceptable only after AST spelling, semantic literal facts, materialization, relation selection, constant evaluation, SGIR, backend emission, IDE facts, and caches all consume the new contracts. `getMaxType`-based `Add`, string/numeric coercion, host `stol`/`stoll`/`stod` folding, generic `Undefined` repair, raw unchecked language integer `CreateAdd`, permissive mixed floating emission, and width-erasing `i64`/`f64` defaults are removed together with tests that require them.

## 验收条件

1. The four foundation checkpoints precede implementation, every implementation checkpoint carries a complete design contract, and all statuses remain `pending` until execution evidence exists.
2. Requirements, evidence, architecture, validation, and the checkpoint graph consistently name the sole Q05 owner, conform to accepted Q03-F, and preserve the external implementation ownership of Q02/Q03-F plus the active Q06/Q08/F02 boundaries.
3. Boundary, adversarial, differential, const/runtime, optimization-mode, target-capability, CLI/IDE/cache, performance/memory, and structural-deletion evidence covers every `REQ-LITADD-*` label.
4. The final validation receipt records one source revision, compiler/LLVM target facts, versioned limit configuration, full command results, catalog fingerprint, IR audits, removed fixtures, and documentation/Better Plan gates.
