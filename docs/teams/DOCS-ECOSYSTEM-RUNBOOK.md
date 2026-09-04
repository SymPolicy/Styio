# Docs / Ecosystem Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of repository documentation, generated indexes, archive/rollup lifecycle, templates, and external Styio ecosystem handoff material.

**Last updated:** 2026-09-04

## Mission

Own documentation structure and cross-repository clarity. This team protects SSOT discipline, generated indexes, archive provenance, external repository boundaries, handoff notes, and reusable templates. It does not redefine language semantics, accepted tests, or package-manager ownership.

The active benchmark boundary is external: current instructions point directly to `styio-benchmark`, while `benchmark/` documents only the explicit optional CMake seam. Redirect wrappers and duplicate benchmark ownership are not maintained.

## Owned Surface

Primary paths:

1. `docs/`
2. `docs/assets/`
3. `docs/rollups/`
4. `docs/archive/`
5. `workflows/`
6. `templates/`
7. `scripts/docs-index.py`
8. `scripts/docs-audit.py`
9. `scripts/docs-lifecycle.py`
10. `scripts/team-docs-gate.py`
11. `scripts/workflow-scheduler.py`
12. `scripts/delivery-gate.sh`
13. `scripts/manifest_tool.py`

Key SSOTs:

1. [../specs/DOCUMENTATION-POLICY.md](../specs/DOCUMENTATION-POLICY.md)
2. [../specs/REPOSITORY-MAP.md](../specs/REPOSITORY-MAP.md)
3. [../../workflows/DOCS-MAINTENANCE-WORKFLOW.md](../../workflows/DOCS-MAINTENANCE-WORKFLOW.md)
4. [../design/Styio-Observable-Language.md](../design/Styio-Observable-Language.md)

## Daily Workflow

1. Check whether the content already has an owning SSOT before adding a new file.
2. Prefer linking and short summaries over copying rules across documents.
3. Add `Purpose` and `Last updated` metadata to every active docs file.
4. Use [../assets/templates/TEAM-RUNBOOK-TEMPLATE.md](../assets/templates/TEAM-RUNBOOK-TEMPLATE.md) for team runbook structure changes.
5. Regenerate `INDEX.md` files after collection changes. Generated indexes stamp their `Last updated` date in UTC (`datetime.now(timezone.utc)` in `scripts/docs-index.py`), so a CI runner and any maintainer timezone regenerate byte-identical content on the same UTC day; never hand-edit the stamp to a local date.
6. Use `python3 scripts/docs-scaffold.py ...` for new docs files or new docs collection directories so metadata, collection registration, and generated indexes are created together.
7. Keep repository-level bootstrap/build entrypoints under `docs/BUILD-AND-DEV-ENV.md`, and push subsystem-only details back down into the owning docs collection instead of overloading `README.md` or `docs/external/for-ide/`.
8. Run the team runbook maintenance gate before delivery so source/test/docs folder changes cannot land without the mapped runbook update or required runbook format.
9. Use archive lifecycle tooling for raw history/review metadata and cleanup rather than manually moving provenance.
10. Use the default unified delivery gate for docs/process deliveries so worktree hygiene, branch-range hygiene, runbook maintenance, docs audit, and external audit stay coupled behind one command.
11. When refactoring `scripts/delivery-gate.sh`, keep literal scheduler profile invocations visible for `delivery-checkpoint` and `delivery-push` so released `styio-audit` can verify that the unified entrypoint still delegates to the approved scheduler profiles.
12. Keep the ecosystem CLI contract mirror and cross-repo doc gate aligned whenever `pafio-nightly` or `vityo-nightly` handoff docs change.
13. When a compiler-side machine contract grows, update the owner SSOT and both consumer handoff docs in the same checkpoint instead of leaving one side on preview wording.
14. Keep generated `INDEX.md` files deterministic for empty collections by deriving fallback timestamps from collection metadata instead of local wall-clock date.
15. When CI validates sibling ecosystem repositories, resolve the sibling checkout to the PR target branch when it is `release`, `stable`, or `nightly`; otherwise use `nightly` as the shared temporary-branch baseline.
16. When syntax-delivery rules change, update the workflow asset, gate scripts, and delivery entrypoints in the same checkpoint; workflow-only prose is not enough.
17. Keep `workflows/WORKFLOW-ORCHESTRATION.md` and `scripts/workflow-scheduler.py` as the registry for workflow separation; new workflow assets must be registered and pass scheduler validation before delivery.
18. Keep repository-level build bootstrap docs aligned with the standardized shared baseline: Debian 13, LLVM 18.1.x, CMake/CTest 3.31.6, Python 3.13.5, and Node.js v24.15.0 LTS where Node-backed tooling exists; when CI mirrors differ by host OS, document the mirror explicitly instead of drifting the toolchain version text.
19. Keep `docs/external/for-pafio/` aligned with the system-compiler boundary: `--machine-info=json` is capability discovery and `--compile-plan` is the resolved request entry. Compiler source-build metadata is not a Pafio contract.
20. When a compiler/runtime/contract adjustment spans multiple checkpoints, add or update an explicit `docs/plan/` implementation plan instead of leaving the execution order only in handoff or runbook prose.
21. When the compiler-side source-build helper changes, keep `scripts/source-build-minimal.sh`, `docs/BUILD-AND-DEV-ENV.md`, and the `--source-build-info=json` handoff wording aligned in the same checkpoint.
22. When a plan remains in `docs/plan/` after one stage closes, make the file say whether it is still `Active`, `Repo-local baseline completed`, or ready for deletion after promotion; do not force readers to infer status from scattered stage tables.
23. Keep repository entry docs honest about maturity: if repo-local baselines are complete but ecosystem closure is still open, say that explicitly instead of leaving stale `early stage` wording in top-level entrypoints.
24. When an evidence-backed closure retires a gap, move it out of the open gap table, add the smallest closed-evidence note, update the matching checkpoint tree entry, and regenerate affected generated indexes.
25. Keep [../specs/POST-COMMIT-CI-CHECKS.md](../specs/POST-COMMIT-CI-CHECKS.md) aligned with actual GitHub Actions monitoring practice whenever commit, push, or CI handoff rules change.
26. Keep GitHub Actions sibling checkouts for `pafio-nightly` and `vityo-nightly` pinned to the same branch ref as `styio-nightly` when a workflow runs cross-repository gates.
27. Keep compact syntax references under `docs/design/syntax/` short and defer semantic detail to the owning design SSOT.
28. When syntax tokens change, update the compact syntax page, EBNF, and symbol reference together before regenerating indexes.
29. When standard-stream or resource identifier declarations change, keep `src/StyioPrelude/resources.styio`, `docs/design/syntax/RESOURCE_IDENTIFIERS.md`, `docs/design/syntax/CONTINUATION_TRANSFER.md`, EBNF, symbol reference, and the language design aligned on accepted source forms, canonical/compatibility status, and parser-implementation status.
30. Keep root `workflows/`, `workflows/skills/`, `workflows/workflows.toml`, generated workflow indexes, and `workflows/` mirrors aligned whenever reusable workflows or repo-local skills are added.
31. Keep repo-local skills concise: workflow docs own sequencing, while `skill.toml` owns reusable execution discipline and references.
32. Workflow and skill machine-readable definitions must use TOML (`*.toml`, `skill.toml`, and `agents/openai.toml`); Markdown remains explanatory only.
33. When test coverage changes require `workflows/TEST-CATALOG.md`, keep the catalog as an evidence index and point behavior ownership back to the implementation or test-quality runbook instead of embedding new language semantics there.
34. When language SSOT docs change token-count semantics, update the design page, EBNF, symbol reference, and test catalog in one checkpoint so docs readers do not see conflicting operator depth rules.
35. When syntax is retired, document the cutover in active SSOT docs and tests. Do not keep retired examples as runnable acceptance cases or recreate old milestone pages in the current tree.
36. When the SymPolicy Styio repository set changes, refresh the inventory with `gh repo list SymPolicy --limit 200`, then update root `README.md`, [../specs/REPOSITORY-MAP.md](../specs/REPOSITORY-MAP.md), and any active ecosystem plan that names the old repository set.
37. Keep [../specs/TECHNOLOGY-COMPONENT-INVENTORY.md](../specs/TECHNOLOGY-COMPONENT-INVENTORY.md) aligned with `styio-audit` whenever the technology stack, internal components, open-source components, dependency manifests, Apache-2.0 evidence, or commercial-risk boundaries change.
38. Keep external `styio-audit` execution wired through the repository delivery gate and dedicated GitHub Actions workflow whenever audit policy or cross-repo CI ownership changes.
39. Maintain GitHub merge gates through Rulesets rather than legacy classic branch protection; audit effective branch rules when required status-check governance changes.
40. Keep `docs/audit/defects/` ignored and out of tracked commits; promote audit findings into an approved docs collection or issue before they become repository-owned records.
41. When compiler source directories are renamed or split, update active path references across `AGENT-SPEC`, technology inventory, workflow mirrors, rollups, audit findings, and team-runbook mappings in the same checkpoint; old paths remain recoverable through Git history.
42. When C++ reference equivalence tests add algorithm cases, keep `workflows/TEST-CATALOG.md` as the concise evidence index and leave case layout details in `tests/algorithms/README.md` plus the Test Quality runbook.
43. When a user-driven syntax correction exposes a compiler/spec mismatch, record the reusable closure path in the workflow directory and make the workflow entrypoint tell agents to read the applicable workflow before editing.
44. Keep CMake build output conventions under `build/<variant>` across scripts, GitHub Actions, workflow docs, and external handoff docs; root `build-*` directories are legacy generated artifacts only and should not be introduced by new commands.
45. When a typed syntax addition changes design SSOTs and test catalogs together, keep the docs update concise: record the accepted source form, point catalog entries to evidence coverage, refresh generated indexes, and update team stats in the same delivery.
46. When source spellings are declared canonically equivalent, keep the ADR, design SSOT, workflow mirror, and test catalog aligned while leaving behavior ownership in parser, Sema / IR, Codegen / Runtime, and Test Quality runbooks.
47. When a milestone test catalog changes for new resource-management syntax, update only the evidence index there and keep lifecycle rules in the owning team runbooks; then refresh `DOC-STATS.md` before rerunning the team-docs gate.
48. Keep the Styio / `styio-benchmark` boundary explicit: Styio docs may describe probes and compatibility wrappers, but benchmark workloads, runners, baselines, reports, and regression records must point to `styio-benchmark`.
49. When compiler-owned resource topology or structured-result contracts change, keep the design SSOT, StyioIR contract inventory, test catalog, Sema / IR runbook, Test Quality runbook, and `DOC-STATS.md` aligned in the same delivery so source, evidence, and ownership do not drift.
50. When retired syntax is removed from active compiler examples, keep active design docs, EBNF, symbol reference, language design, standard-library notes, implementation plans, and test catalogs aligned on the same retired/negative-test wording.
51. When the compiler-side native artifact command changes, keep root README usage, the ecosystem CLI contract matrix, and the `styio-benchmark` boundary language aligned: `styio build <file_path> -o <artifact_name>` owns artifact production, while benchmark workloads and scoreboards remain in `styio-benchmark`.
52. Public resource-management statements must cite primary language or platform sources and map each borrowed practice to Styio's actual compiler evidence. Keep the wording at "modern resource-management language" unless formal proof, async/task stress evidence, and driver-family validation justify a stronger statement.
53. Keep public wording concise and evidence-scoped. Do not speculate about companies, organizations, individuals, projects, products, or their capabilities; do not use absolute marketing superlatives or unsupported superiority language in repository docs.
54. README showcase examples must point to repository-local Styio source that can be run from the repository root, and the documented command/output pair must be verified before publication.
55. Root community files such as `CONTRIBUTING.md`, `SECURITY.md`, `SUPPORT.md`, `CODE_OF_CONDUCT.md`, `CHANGELOG.md`, and release-policy documents are approved public docs only when `scripts/docs-audit.py` classifies them explicitly and their wording stays evidence-scoped.
56. Active `example/` and `tests/` directories must not carry non-runnable language drafts. Delete drafts from the current tree after durable rules are promoted, and keep active README links pointed at CTest-covered examples.
57. Do not describe Styio as equivalent to another language's resource model. State the exact compiler evidence Styio has, then list external practices only as references.
58. Historical deprecated syntax belongs in Git history, not active docs; do not recreate old syntax catalogs or copy historical syntax into active examples without checking the active SSOT.
59. When local divergent history is reintroduced after an upstream governance update, preserve the old branch or stash reference in a rollup ledger and migrate only slices that fit current repository ownership. Do not restore deleted plan trees, old service paths, or superseded ecosystem names as active docs.
60. Keep rollup ledgers and migration notes free of developer-machine absolute paths; describe portability behavior generically unless an owned contract explicitly requires a literal path form.
61. Track valid generated-index support files and installed shared assets even when broad ignore patterns match their directories. When `share/styio/prelude/` files become required by docs audit, install layout, or source-build metadata, force-track the owned files and refresh `DOC-STATS.md` in the same change.
62. When `>>` language docs distinguish pulse transfer from standalone continue, keep EBNF, language design, active syntax, resource docs, symbol reference, rollups, agent specs, examples, and the owning implementation runbooks aligned in the same checkpoint.
63. When writable-resource iterable writes change between whole-value serialization and per-item pulse emission, keep resource identifier docs, EBNF, language design, active syntax, symbol reference, handle capability wording, Sema / IR ownership, and security evidence aligned in the same checkpoint.
64. Keep one durable SSOT under `docs/design/syntax/features/` for each syntax feature. Update its lifecycle, dependencies, prerequisites, implementation owner, and golden evidence first; regenerate `SYNTAX-FEATURE-GRAPH.json` with `scripts/syntax-feature-state-gate.py` instead of editing the composed graph by hand or creating a parallel registry.
65. Callable binding docs must teach the unified binding model consistently across its feature SSOT, active syntax, EBNF, language design, and symbol reference: `=` is mutable, `:=` is final, `#` marks a callable/operation-channel binding, and direct resource atoms such as `# sink = @stdout` stay invalid because resources remain in the visible `@` family.
66. Range docs must keep naked `start..end` as the expression-level range form, bracketed `[start..end]` as the canonical materialized range source, and `[start..end..step]` as reserved/non-active wording across active syntax, EBNF, language design, symbol reference, examples, test catalog, and editor grammar notes.
67. Keep native macOS build and CI documentation reproducible without recording machine-specific paths: resolve keg-only Homebrew package prefixes with `brew --prefix`, resolve the active SDK with `xcrun`, and keep the selected LLVM 18.1.x, CMake/CTest, Python, ICU, and Node lines aligned across the root READMEs, repository build guide, IDE build guide, bootstrap plan, and macOS CI lane.
68. Treat the keyword-free lexical contract as its own feature SSOT: every word remains `NAME`, exact spelling checks belong only to symbol-anchored contexts, and feature or teaching docs must not invent a keyword class.
69. Keep callable type-system decisions distributed by feature: definition-site principal relations, recursive SCC inference, and context-driven use-site instantiation each own a separate syntax-feature SSOT and dependency edge. A consolidated decision agenda may compose unresolved questions and external references, but it must link to the owning draft feature SSOTs instead of becoming a competing language authority.
70. When the language owner approves a composed decision agenda, update every owning feature SSOT from `review` to `accepted` first, preserve independent delivery states and dependency edges, then revise the agenda into a non-authoritative approval record and regenerate the syntax-feature graph.
71. When an approved callable-type feature converges, advance only its owning feature SSOT, attach checked implementation and golden evidence, update the compact language/test views and owning team runbooks, then regenerate the dependency graph so downstream readiness follows from the distributed authorities.
72. Keep callable constraints and literal defaulting in separate feature SSOTs even when they share one solver pipeline. The constraint document owns the closed capability vocabulary and satisfiability evidence; the defaulting document owns canonical scalar defaults and the rule that empty collections require context.
73. Keep higher-order callable policy and its monomorphic callable-value child in separate feature SSOTs. Shared language views must distinguish direct named scheme instantiation from contextual freezing under one complete invariant `#(...): ...` type, and describe the allocation-free function-item boundary independently from the affine-closure child. Do not infer rank-2 values, mutable callable slots, address equality, native-pointer interop, generalized storage, or callable topology from the noncapturing item implementation.
74. Keep capability-polymorphic handle policy in its own feature SSOT and link it to both handle-capability and resource-topology authorities. Shared views must distinguish admitted pure materialized collections from rejected stateful handles and state that checking precedes relation normalization; do not imply lifetime, linearity, send/sync, or matrix-shape polymorphism.
75. Keep callable interface publication, canonical effect rows, and portable generic bodies in their separate feature SSOTs. Shared views must distinguish authored `@import`/`@export` syntax from compiler-owned `.styioi` facts, identify schema-v4 `labels` plus nullable `open_tail` as effect identity rather than legacy bits or stored canonical strings, keep sorted per-variable usage requirements under their capability-usage SSOT, and assign canonical `styio.portable-styioir` schema-v1 encoding and verification only to the portable-body SSOT. State the explicit dependency-first publication order, no imported-source reparse or schema-v3 fallback, older-schema/source/contract/body/dependency/ABI rejection, direct-export-only visibility, private body helpers, and whole module-cycle rejection as the current conservative recursive boundary rather than a permanent source-language theorem.
76. Keep callable specialization policy in its own feature SSOT. Shared views must state that demand-driven mono items and their full content digests are compiler metadata, enumerate transitive callable/module/backend identity facts and both hard ceilings, and scope unconditional single ownership and reuse to one compiler invocation. Persistent local native reuse belongs only to its opt-in cache child SSOT; preserve explicit instantiation, distributed caching, normal warning thresholds, and stable callable addresses as separate future decisions.
77. Collect undecided syntax evolution only after primary-source research. A consolidated question set must show implementation lessons, known pitfalls, Styio-specific functional constraints, dependencies, options, and one recommendation; it remains a non-authoritative agenda until the language owner answers, after which each accepted branch receives its own feature SSOT and graph edge.
78. Record a batch owner answer in the researched question set, then create one child SSOT per question before implementation. Use `accepted/not_started` for approved directions whose delivery floor is unmet, `deferred/not_started` plus a concrete `reopen_when` for postponed directions, and `accepted/converged` only when existing implementation symbols and goldens already prove the exact child boundary.
79. Keep capability/usage polymorphism separate from the closed handle-admission baseline. Shared views must identify the canonical copy/borrow/consume relation facts, original-type instance revalidation, and fact-specific diagnostics while stating that task-transfer, resource-state-family, topology, and matrix-shape admission remain disabled. Do not describe compiler-owned usage facts as authored lifetime/capability syntax or as permission to generalize every handle representation.
80. Keep affine capturing closures in their own feature SSOT. Shared language views must place the exact nonempty `$(...)` list between the callable signature and binding/body operator, distinguish shared escape from exclusive direct-call and rejected consume modes, and scope the converged environment to program-static scalar storage. Do not imply resource/container capture, imported environments, heap boxes, garbage collection, implicit free-name capture, or generalized closures.
81. Keep persistent callable specialization reuse in its own compiler-cache feature SSOT. Compact language views should say only that the operation is opt-in, keyed by the fresh full specialization digest, isolated by compiler/LLVM/codegen/target/channel/backend facts, bounded by explicit age/byte/file limits, and semantics-neutral on miss or corruption. Put binary entry layout, ORC partitioning, verification, atomic-write, pruning, and statistics details in the child SSOT and owning runbooks; do not turn CLI cache controls into source syntax or imply distributed trust.
82. Converged prerequisites do not auto-promote a deferred language feature. Refresh its non-authoritative question set from specifications, official compiler documentation, accepted proposals, or primary papers; record concrete implementation lessons and failure modes; map them to Styio's functional, keyword-free, ownership, interface, and specialization constraints; and keep every unanswered subquestion linked from the existing deferred child SSOT. Do not create a parallel feature SSOT or describe a recommended answer as approved.
83. Keep project planning on the Better Plan v3 model: `Manifest.json` indexes Plans, each `Plan.json` is semantic state, each `Checkpoints.json` is execution-only state, and `Plan.md` is a render-only projection. Validate the workspace with `python3 scripts/manifest_tool.py validate docs/plan`; do not add compatibility readers for removed planning generations.
84. Keep durable unfinished product work in `docs/rollups/NEXT-STAGE-GAP-LEDGER.md` and point the plan workspace boundary to that register. Generated delivery Markdown is exempt from authored metadata and runbook triggers, while the tracked Manifest, Plan, Checkpoints, projection, and design provenance remain reviewable.
85. Treat EBNF as the token-spelling authority for standalone break syntax, and migrate the language design, symbol reference, active plans, evidence, and executable tests in the same checkpoint. Keep the downstream Brainfuck example as the canonical consumer proving the structured-result and stream-input contracts rather than duplicating its source in compiler docs.
86. When the maintainer freezes a Styio-only release boundary, record it in the current v3 Plan and rendered projection, keep future product work in the gap ledger, state Styio-only functional acceptance as the sole gate, and do not imply participation by Pafio, Platform, Vityo, or any other repository excluded from that boundary.
87. Manage dated audit reports through the lifecycle tool's `audit` family. Verify closure against active decision and inventory documents, mark each source with durable extracted value and targets, run cleanup under the no-retained-copy policy, and regenerate the archive ledger and indexes.
88. Run `python3 scripts/repo-hygiene-gate.py --mode residue` before delivery to detect ignored build roots, profiling output, test-discovery files, caches, and platform metadata that tracked-only scans cannot see.
89. Keep the compiler-entry line ceiling in `scripts/monolith-line-ratchet-gate.py`, the tool registry, and scheduler profiles synchronized; raising the ceiling is not a substitute for the separately registered split backlog.
90. When weekly fuzz cadence or corpus-backflow docs change, refresh the Frontend and Test Quality runbooks, regenerate `docs/teams/INDEX.md`, and update `DOC-STATS.md` in the same delivery so generated inventory and size snapshots stay aligned.
91. Describe runtime absence as the implemented tagged value boundary, not as an integer sentinel or implicit algebraic propagation. Keep the language design, research note, intrinsic table, resource-driver contract, performance route, owning runbooks, and test catalog aligned whenever the value ABI changes; distinguish implemented lazy `|` recovery from deferred diagnostic metadata and `??` extraction.
92. Keep IDE snapshot-storage and incremental-parser performance evidence in the test catalog, IDE/LSP runbook, grammar runbook, performance runbook, and Test Quality runbook as one documentation unit; refresh `DOC-STATS.md` whenever that unit changes.
93. Keep observable-language semantics in one cross-feature design SSOT. Resource syntax remains owned by the resource-topology design, implementation sequencing remains in `docs/plan/`, runtime samples remain distinct from compiler facts, and imported planning bundles must be reconciled with the current repository before any content becomes authoritative.
94. When the internal topology artifact lifecycle changes, update the observable-language SSOT, Sema/IR ownership rule, IDE diagnostic boundary, focused test evidence, and test catalog together. Keep persistent IDs, public snapshots, serialization, runtime correlation, scheduler integration, and external consumers explicitly deferred until their own contracts are authorized.
95. For PLAN-003, keep the tracked semantic-identity contract, `architecture_layer_gate` registration, and generated documentation indexes and statistics aligned; regenerate `docs/plan/INDEX.md` with `python3 scripts/docs-index.py --write` and refresh `DOC-STATS.md` from the repository docs-audit export, while keeping public snapshots, serialization, lineage, runtime, scheduler, cache, and external-consumer surfaces deferred.

## Change Classes

1. Small: typo, link fix, or local README wording. Run docs audit.
2. Medium: new docs collection, generated index config, SSOT table change, external handoff doc, or CLI/runtime contract matrix update. Update policy and run generated-index checks.
3. High: repository boundary, archive lifecycle, docs audit rule, or ecosystem ownership change. Use checkpoint workflow and coordinate affected implementation teams.

## Required Gates

Documentation gates:

```bash
python3 scripts/docs-index.py --write
python3 scripts/workflow-scheduler.py check
python3 scripts/syntax-feature-state-gate.py
python3 scripts/team-docs-gate.py
python3 scripts/docs-lifecycle.py validate
python3 scripts/ecosystem-cli-doc-gate.py
python3 scripts/docs-audit.py
```

Unified docs/process delivery floor:

```bash
./scripts/delivery-gate.sh --skip-health
```

Optional inventory commands:

```bash
python3 scripts/docs-audit.py --manifest valid --format tree
python3 scripts/docs-audit.py --manifest invalid --format list
python3 scripts/docs-lifecycle.py candidates --family all --format tree
```

Checkpoint-grade:

```bash
./scripts/checkpoint-health.sh --no-asan --no-fuzz
```

## Cross-Team Dependencies

1. Frontend, Sema / IR, Codegen / Runtime, IDE / LSP, and CLI / Nano must review docs that describe their behavior.
2. Test Quality must review test catalog, workflow, and oracle documentation changes.
3. Perf / Stability must review benchmark, soak, and report lifecycle docs.
4. Coordination owner must review repository-boundary and external ecosystem handoff changes.

## Handoff / Recovery

Record unfinished docs/ecosystem work with:

1. Owning SSOT and files changed.
2. Generated indexes that still need refresh.
3. Link or metadata audit failures.
4. Team runbook gate failures, required runbook paths, and template/format violations.
5. External repository or handoff owner affected.
6. Archive/rollup lifecycle action still pending.

2026-09-04: Registered PLAN-004, PLAN-005, and PLAN-006 as ready but unapproved
future observable-language deliveries, refreshed the plan inventory and gap
ledger, and linked the fixture-gated Vityo, conditional Pafio, and benchmark
handoffs. No execution checkpoints, Workers, product implementation, backend
scope, or authority were created; the imported evolution archive remains
reference material only.
