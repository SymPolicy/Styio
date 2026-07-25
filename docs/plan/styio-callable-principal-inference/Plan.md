# Styio Callable Principal Inference Delivery Plan

**Purpose:** Define the product boundary, dependency graph, and single converged implementation for Q02-INF principal callable inference.

**Last updated:** 2026-07-20

**Status:** Pending implementation. Q02-INF and `Q05-LIT-ADD` are approved and
all checkpoints remain pending. Callable inference consumes the Q05 delivery
catalog through a reviewed interface; it does not implement a duplicate numeric
policy or catalog.

**Better Plan ID:** `5a5ec867-9998-4e78-9111-761cad834754`

## 前置条件

1. **语义 owner:** The approved Q02-INF contract is owned only by [Styio-Callable-Principal-Inference.md](../../design/Styio-Callable-Principal-Inference.md); the requirements checkpoint traces implementation to that accepted authority.
2. **Q05 dependency:** The complete contract is now frozen in
   [Styio Exact Literals and Built-in Add](../../design/Styio-Exact-Literals-and-Builtin-Add.md).
   Its sibling delivery plan owns exact-term representation, materialization,
   the finite scalar table, checked execution, and constant evaluation. The
   callable relation node starts only against that reviewed catalog interface
   and owns constraint integration, not a second table.
3. **Q02-SIG boundary:** Existing explicit boundary signatures and finite completion upper bounds remain authoritative. This plan consumes that contract and does not reopen its syntax.
4. **并行:** Evidence and validation discovery may fan out before architecture. After architecture, lowering/IR specialization and Q05 operator-relation work may proceed as sibling branches only after the shared call-instantiation interface is complete; codegen, IDE/cache, convergence, and final validation follow their real prerequisites.
5. **子智能体:** Read-only audits and file-disjoint implementation nodes may use sub-agents. Shared Sema files are deliberately serialized through prerequisites.
6. **Workspace:** The plan uses `docs/plan` as the existing Better Plan workspace. Manifest/INDEX registration is owned outside this file set.

## Product outcome

Users can write concise local callable adapters whose parameter/result relationship is evident from the definition, while receiving deterministic errors when a principal usable scheme cannot be derived. Calls remain independently typed, generated code remains fully concrete, and source order or incremental compilation scope cannot change a definition's scheme.

The two acceptance anchors are:

```styio
# identity := (x) => x
# add_five := (x) => x + 5
```

The first is delivered by equality inference. The second retains and solves the
accepted Q05-owned closed relation through its catalog interface; this plan does
not infer or reimplement that relation from current implementation accidents.

## Dependency tree

- **Parent:** accepted Q02-SIG boundary/operation-summary contract, Q02-INF semantic owner, existing session/type/symbol/diagnostic infrastructure.
- **Current:** principal type terms and constraints, safe rank-1 callable schemes, per-use instantiation, concrete specialization, diagnostics/IDE/cache, and complete removal of first-use/default behavior.
- **Children:** concrete SGIR/backend instances, editor semantic facts, incremental cache entries, active language/tooling documentation, and final acceptance evidence.

The checkpoint graph first establishes product requirements, evidence, validation, and architecture. It then serializes the shared Sema core and call-use contract, fans out Q05 relation solving and concrete specialization, rejoins at tooling/convergence, and ends with one all-requirement validation node. Nodes are commit-sized responsibilities, not acceptable partial product releases.

## Artifacts

- [Requirements.md](./Requirements.md) — user outcomes, functional requirements, constraints, non-goals, and acceptance target.
- [Evidence.md](./Evidence.md) — repository facts, external compiler practice, gaps, and decision prerequisites.
- [Validation.md](./Validation.md) — requirement-to-test/static-evidence mapping fixed before implementation.
- [Architecture.md](./Architecture.md) — module ownership, algorithms, dependency direction, interfaces, caching, monomorphization, and removal design.
- [Checkpoints.json](./Checkpoints.json) — machine-validated pending execution graph.

## Single-final-state rule

No checkpoint may ship or retain a second inference path. The migration is complete only when callable parameters and results are inferred through the new Sema domain, calls consume scheme facts, lowering consumes concrete instance facts, SGIR remains concrete, and the old AST mutation/cache/default routes and tests are absent. Feature flags, compatibility visitors, fallback `Undefined`/`i64`, and parallel old/new caches are outside the plan.

## 验收条件

1. `Requirements.md`, `Evidence.md`, `Validation.md`, and `Architecture.md` remain mutually consistent with the accepted Q02 owner and the cited Q05 owner contract.
2. `Checkpoints.json` validates as an all-pending topological graph whose four foundation roles precede implementation and whose final-validation node covers every implementation requirement label.
3. Equality-only identity inference, Q05-backed operator constraints, independent calls, canonical instance reuse, concrete-only SGIR, deterministic limits, IDE/cache publication, and old-path deletion each have explicit implementation ownership and evidence.
4. The final node records one source snapshot, complete requirement coverage, targeted commands, structural searches, determinism/performance results, removed fixtures, and all relevant documentation/Better Plan gates.
