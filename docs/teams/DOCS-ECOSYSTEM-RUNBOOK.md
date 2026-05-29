# Docs / Ecosystem Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of repository documentation, generated indexes, archive/rollup lifecycle, templates, and external Styio ecosystem handoff material.

**Last updated:** 2026-05-29

## Mission

Own documentation structure and cross-repository clarity. This team protects SSOT discipline, generated indexes, archive provenance, external repository boundaries, handoff notes, and reusable templates. It does not redefine language semantics, accepted tests, or package-manager ownership.

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

Key SSOTs:

1. [../specs/DOCUMENTATION-POLICY.md](../specs/DOCUMENTATION-POLICY.md)
2. [../specs/REPOSITORY-MAP.md](../specs/REPOSITORY-MAP.md)
3. [../../workflows/DOCS-MAINTENANCE-WORKFLOW.md](../../workflows/DOCS-MAINTENANCE-WORKFLOW.md)

## Daily Workflow

1. Check whether the content already has an owning SSOT before adding a new file.
2. Prefer linking and short summaries over copying rules across documents.
3. Add `Purpose` and `Last updated` metadata to every active docs file.
4. Use [../assets/templates/TEAM-RUNBOOK-TEMPLATE.md](../assets/templates/TEAM-RUNBOOK-TEMPLATE.md) for team runbook structure changes.
5. Regenerate `INDEX.md` files after collection changes.
6. Use `python3 scripts/docs-scaffold.py ...` for new docs files or new docs collection directories so metadata, collection registration, and generated indexes are created together.
7. Keep repository-level bootstrap/build entrypoints under `docs/BUILD-AND-DEV-ENV.md`, and push subsystem-only details back down into the owning docs collection instead of overloading `README.md` or `docs/external/for-ide/`.
8. Run the team runbook maintenance gate before delivery so source/test/docs folder changes cannot land without the mapped runbook update or required runbook format.
9. Use archive lifecycle tooling for raw history/review metadata and cleanup rather than manually moving provenance.
10. Use the default unified delivery gate for docs/process deliveries so worktree hygiene, branch-range hygiene, runbook maintenance, docs audit, and external audit stay coupled behind one command.
11. When refactoring `scripts/delivery-gate.sh`, keep literal scheduler profile invocations visible for `delivery-checkpoint` and `delivery-push` so released `styio-audit` can verify that the unified entrypoint still delegates to the approved scheduler profiles.
12. Keep the ecosystem CLI contract mirror and cross-repo doc gate aligned whenever `styio-spio` or `styio-view` handoff docs change.
13. When a compiler-side machine contract grows, update the owner SSOT and both consumer handoff docs in the same checkpoint instead of leaving one side on preview wording.
14. Keep generated `INDEX.md` files deterministic for empty collections by deriving fallback timestamps from collection metadata instead of local wall-clock date.
15. When CI validates sibling ecosystem repositories, use the downstream `nightly` branch as the shared ecosystem baseline; `ai-dev` remains a writable staging lane in the upstream repo, but cross-repository contract checks still validate against the downstream delivery lane.
16. When syntax-delivery rules change, update the workflow asset, gate scripts, and delivery entrypoints in the same checkpoint; workflow-only prose is not enough.
17. Keep `workflows/WORKFLOW-ORCHESTRATION.md` and `scripts/workflow-scheduler.py` as the registry for workflow separation; new workflow assets must be registered and pass scheduler validation before delivery.
18. Keep repository-level build bootstrap docs aligned with the standardized shared baseline: Debian 13, LLVM 18.1.x, CMake/CTest 3.31.6, Python 3.13.5, and Node.js v24.15.0 LTS where Node-backed tooling exists; when CI mirrors differ by host OS, document the mirror explicitly instead of drifting the toolchain version text.
19. Keep `docs/external/for-spio/` aligned with the current `binary` vs `build` split: `--machine-info=json` remains the binary handshake, while `--source-build-info=json` owns the official source-layout contract for `spio build`.
20. When a compiler/runtime/contract adjustment spans multiple checkpoints, add or update an explicit `docs/plans/` implementation plan instead of leaving the execution order only in handoff or runbook prose.
21. When the compiler-side source-build helper changes, keep `scripts/source-build-minimal.sh`, `docs/BUILD-AND-DEV-ENV.md`, and the `--source-build-info=json` handoff wording aligned in the same checkpoint.
22. When a plan remains in `docs/plans/` after one stage closes, make the file say whether it is still `Active`, `Repo-local baseline completed`, or ready for deletion after promotion; do not force readers to infer status from scattered stage tables.
23. Keep repository entry docs honest about maturity: if repo-local baselines are complete but ecosystem closure is still open, say that explicitly instead of leaving stale `early stage` wording in top-level entrypoints.
24. When an evidence-backed closure retires a gap, move it out of the open gap table, add the smallest closed-evidence note, update the matching checkpoint tree entry, and regenerate affected generated indexes.
25. Keep [../specs/POST-COMMIT-CI-CHECKS.md](../specs/POST-COMMIT-CI-CHECKS.md) aligned with actual GitHub Actions monitoring practice whenever commit, push, or CI handoff rules change.
26. Keep GitHub Actions sibling checkouts for `styio-spio` and `styio-view` pinned to the same branch ref as `styio-nightly` when a workflow runs cross-repository gates.
27. Keep compact syntax references under `docs/design/syntax/` short and defer semantic detail to the owning design SSOT.
28. When syntax tokens change, update the compact syntax page, EBNF, and symbol reference together before regenerating indexes.
29. When standard-stream or resource identifier declarations change, keep `src/StyioPrelude/resources.styio`, `docs/design/syntax/RESOURCE_IDENTIFIERS.md`, `docs/design/syntax/CONTINUATION_TRANSFER.md`, EBNF, symbol reference, and the language design aligned on accepted source forms, canonical/compatibility status, and parser-implementation status.
30. Keep root `workflows/`, `workflows/skills/`, `workflows/workflows.toml`, and generated workflow indexes aligned whenever reusable workflows or repo-local skills are added. The historical `docs/assets/workflow/` mirror was retired on 2026-05-22 and should not be reintroduced.
31. Keep repo-local skills concise: workflow docs own sequencing, while `skill.toml` owns reusable execution discipline and references.
32. Workflow and skill machine-readable definitions must use TOML (`*.toml`, `skill.toml`, and `agents/openai.toml`); Markdown remains explanatory only.
33. When test coverage changes require `workflows/TEST-CATALOG.md`, keep the catalog as an evidence index and point behavior ownership back to the implementation or test-quality runbook instead of embedding new language semantics there.
34. When language SSOT docs change token-count semantics, update the design page, EBNF, symbol reference, and test catalog in one checkpoint so docs readers do not see conflicting operator depth rules.
35. When syntax is retired, document the cutover in active SSOT docs and tests. Do not keep retired examples as runnable acceptance cases or recreate old feature-test pages in the current tree.
36. When the eBioRing Styio repository set changes, refresh the inventory with `gh repo list eBioRing --limit 200`, then update root `README.md`, [../specs/REPOSITORY-MAP.md](../specs/REPOSITORY-MAP.md), and any active ecosystem plan that names the old repository set.
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
47. When a feature test catalog changes for new resource-management syntax, update only the evidence index there and keep lifecycle rules in the owning team runbooks; then refresh `DOC-STATS.md` before rerunning the team-docs gate.
48. Keep the Styio / `styio-benchmark` boundary explicit: Styio docs may describe probes and compatibility wrappers, but benchmark workloads, runners, baselines, reports, and regression records must point to `styio-benchmark`.
49. When compiler-owned resource topology code changes, keep the design SSOT, test catalog, Sema / IR runbook, Test Quality runbook, and `DOC-STATS.md` aligned in the same delivery so source, evidence, and ownership do not drift.
50. When retired syntax is removed from active compiler examples, keep active design docs, EBNF, symbol reference, language design, standard-library notes, implementation plans, and test catalogs aligned on the same retired/negative-test wording.
51. When the compiler-side native artifact command changes, keep root README usage, the ecosystem CLI contract matrix, and the `styio-benchmark` boundary language aligned: `styio build <file_path> -o <artifact_name>` owns artifact production, while benchmark workloads and scoreboards remain in `styio-benchmark`.
52. Public resource-management statements must cite primary language or platform sources and map each borrowed practice to Styio's actual compiler evidence. Keep the wording at "modern resource-management language" unless formal proof, async/task stress evidence, and driver-family validation justify a stronger statement.
53. Keep public wording concise and evidence-scoped. Do not speculate about companies, organizations, individuals, projects, products, or their capabilities; do not use absolute marketing superlatives or unsupported superiority language in repository docs.
54. README showcase examples must point to repository-local Styio source that can be run from the repository root, and the documented command/output pair must be verified before publication.
55. Root community files such as `CONTRIBUTING.md`, `SECURITY.md`, `SUPPORT.md`, `CODE_OF_CONDUCT.md`, `CHANGELOG.md`, and release-policy documents are approved public docs only when `scripts/docs-audit.py` classifies them explicitly and their wording stays evidence-scoped.
56. Active `example/` and `tests/` directories must not carry non-runnable language drafts. Delete drafts from the current tree after durable rules are promoted, and keep active README links pointed at CTest-covered examples.
57. Do not describe Styio as equivalent to another language's resource model. State the exact compiler evidence Styio has, then list external practices only as references.
58. Historical deprecated syntax belongs in Git history, not active docs; do not recreate old syntax catalogs or copy historical syntax into active examples without checking the active SSOT.
59. When an audit creates a maintainer decision log under `docs/plans/`, keep it separate from language SSOTs: record autonomous closures, unresolved decisions, and verification evidence without defining new syntax there.
60. When closing a rollup ledger item for sanitizer, fuzz, or fail-closed compiler work, cite the exact local command that proved the closure, preserve the GitHub run or artifact id and backflow seed when the finding came from CI, and keep broader open-gap rows intact unless the whole class is actually retired.
61. Keep `docs/external/SERVICES.md` as the consumer-neutral catalog for package-manager, IDE, CI, and editor-facing compiler services. Consumer-specific pages under `for-spio/` or `for-ide/` should point to that catalog when they describe shared service contracts such as syntax checking.
62. Source-level Markdown remains disallowed under `src/` except for `src/StyioServices/**/README.md` and `src/StyioServices/MANIFEST.md`, which document service usage at the code boundary. Do not broaden the docs-audit exception without updating this runbook and the service catalog.
63. Keep `scripts/ecosystem-cli-doc-gate.py` aligned with service-contract section numbering and controlled source graph names. When a downstream layout change lands before released `styio-audit` policy catches up, CI may normalize the checked-out policy in `.github/workflows/styio-audit.yml`; that workflow patch must remain downstream-local and must not imply an upstream policy push.
64. When shared service diagnostics add consumer-facing JSON fields, update `docs/external/SERVICES.md`, the source-level service README, and the module manifest in the same checkpoint so external users see the same contract shape as `--machine-info=json`.
65. When a broad compiler-language maturity gap cannot be implemented in the current checkpoint, record it in [../rollups/NEXT-STAGE-GAP-LEDGER.md](../rollups/NEXT-STAGE-GAP-LEDGER.md) with a named pending decision, mature architecture reference, owner-facing stop condition, and follow-up gate instead of leaving it as prose in a chat or long-lived plan file.
66. When a rollup gap is narrowed rather than fully retired, update the evidence row and affected test catalog/runbooks with the precise closed value families, commands, and remaining unsupported families so readers do not infer broader feature closure than the tests prove.
66. Keep the industrial-maturity decision register sorted by descending priority: `IM-D1` is the highest-priority blocker, and larger decision numbers are lower priority.
67. When describing `IM-D1`, treat `StyioIR` as the existing canonical typed mid-level IR; the remaining maturity work is to freeze its contract, complete accepted AST coverage, and reject unsupported forms with typed diagnostics instead of placeholder lowering.
68. When an industrial-maturity item moves from pending decision to accepted implementation rule, keep the rollup row and the owning team runbooks in the same staged delivery so staged gates can prove the decision and operational rule landed together.
69. Keep the IM-D1 contract inventory in [../rollups/IM-D1-STYIOIR-CONTRACT-INVENTORY.md](../rollups/IM-D1-STYIOIR-CONTRACT-INVENTORY.md) when placeholder behavior changes. The rollup ledger may call the contract closed only when the inventory, runbooks, focused tests, and docs indexes all describe the same accepted/no-op/fail-closed split, including any value-carrying scalar cast slice that retires a former placeholder or fail-closed entry.
70. Keep IM-D4 scoped to resource management: resource-subject identity, capability vocabulary, typestate transitions, block snapshot/commit behavior, cleanup policy, and fallible operation semantics. Resource recovery uses `?| resource_operation | fallback`, named handlers such as `?| resource_operation | backpressure => handler`, and the statement-only discard `?| resource_operation | ...`; the discard form produces no value and still requires resource settlement. When only a resource-effect implementation slice lands, such as statement-level catch-all fallback, named-handler dispatch over current runtime subcode families, statement file-acquire recovery, statement resource-method settlement for direct file close, or explicit file-close cleanup-failure matching, record the exact closed runtime path and the remaining implicit cleanup/pressure/value-model gap in the rollups instead of marking IM-D4 closed. Do not introduce user-visible `borrow`, `shared`, `own`, or `pure` syntax, and do not restate the IM-D1 verifier placement, no-op, active AST, or codegen-gate contract inside the IM-D4 row or inventory.
71. Keep IM-D5 scoped to stream runtime and concurrency: deterministic pulse frames, zip barrier synchronization, snapshot joins, unequal-rate liveness, backpressure scheduling, happens-before edges, and parallel resource worktree merge/conflict handling. Do not move single-resource capability, typestate, cleanup, or `?|` fallback rules out of IM-D4, and do not let scheduler order define observable stream semantics.
72. When an IM-D5 stream-source slice lands without closing the whole stream model, record the exact accepted source combination, element families, termination behavior, and adjacent unsupported combination. Keep broader pressure, timeout, snapshot-join, EOF/failure, and merge/conflict wording open unless those paths have their own runtime evidence.
73. Keep IM-D6 scoped to release evidence and promotion policy: L0/L1/L2/L3 gate tiers, lane status, block policy, affected-area gates, nightly/release matrices, conformance families, and package-boundary reporting. Do not turn IM-D6 into feature semantics, and do not treat skipped or unavailable lanes as success without an explicit `skip-with-reason` or `pending` owner.
74. Keep IM-D7 scoped to native interop ABI evidence: host C/C++ compiler delegation, explicit binding names, native signature extraction, symbol resolution, function-pointer calls, native effect classification, and toolchain/cache records. Do not replace the compiler-owned native route with a private C/C++ parser, and do not document unsupported C++ symbol behavior as accepted without signature and symbol evidence.
75. Keep IM-D8 scoped to library ownership: language-core, compiler-intrinsic, and standard library are the retained layers; examples, benchmarks, and domain libraries are external projects. Standard-library implementation may remain temporarily in this repository, but future official package distribution belongs through Spio / Styio-Platform, and benchmark/example/domain content must not become a language or standard-library commitment without an explicit promotion record.
76. Keep IM-D9 scoped to IDE/LSP service contracts: compiler-owned parser and semantic pipeline remain authoritative, while editor syntax snapshots are non-authoritative interaction data. Vityo, Spio, and other first-party projects may have deep convenience adapters over `StyioServices`, but those adapters must reuse shared service facts, capability states, parser/grammar evidence, document revisions, and workspace/config identity; they must not become separate grammar, diagnostic, or semantic authorities. LSP capabilities must be advertised only when backed by `StyioIDE` service behavior and tests.
77. Keep IM-D10 scoped to package, module, and release compatibility boundaries. Local `styio-spio` or `styio-platform` checkouts are not authoritative for externally maintained state; if the active owning repository cannot be read, record manifest, lockfile, resolver, registry, trust, hosted workspace, standard-library package, and compatibility-matrix questions as external confirmations instead of turning them into `styio` commitments.
78. When recovering Codex or VM change records, promote only durable repository evidence into tracked docs: ADRs, plans, audit summaries, generated indexes, and team runbook rules. Do not restore transient `docs/audit/defects/` records into pushable history; move stable audit findings into approved audit summaries or `docs/audit/agent-findings/` with owner-facing scope and follow-up gates.
79. Keep the dedicated `repo-hygiene` GitHub Actions workflow aligned with `scripts/repo-hygiene-gate.py` whenever the gate modes, push-range calculation, Python baseline, or documented delivery floor changes.
80. When updating the active rollup ledger for package or service contract hardening, keep the change evidence-scoped: record the exact local test command, preserve the `styio` compiler-side boundary, and leave unresolved package-manager or platform lifecycle questions in IM-D10 instead of turning them into `styio` commitments. For nano static repository hardening, keep marker, SHA256, size, and remote-publish guard evidence together when the baseline changes. For compile-plan v1 hardening, describe only request-envelope shape, machine-readable diagnostics, and entry-package consistency unless Spio has explicitly confirmed broader resolver or lifecycle ownership.
81. Keep the Draft PR checkpoint rule in [../../workflows/REPO-HYGIENE-COMMIT-STANDARD.md](../../workflows/REPO-HYGIENE-COMMIT-STANDARD.md) and [../../workflows/CHECKPOINT-WORKFLOW.md](../../workflows/CHECKPOINT-WORKFLOW.md) aligned whenever commit, push, or recovery-branch practice changes; the minimum push unit is an engineering slice such as a syntax/semantic slice, feature-closure unit, test-evidence group, or docs/governance closure, and exists to protect remote recoverability before long-running tasks are fully polished.
82. Branch-consolidation pushes must run whitespace checks over the full push range, not only the worktree. If an older tracked docs file fails range hygiene, fix the active file in the consolidation commit and refresh `DOC-STATS.md` when team runbooks changed.
83. Current implementation-vs-design gap audits belong in `docs/rollups/` only when they are evidence-backed active summaries. Keep them tied to `CURRENT-STATE.md`, `NEXT-STAGE-GAP-LEDGER.md`, exact local verification commands, and explicit non-gap boundaries so they do not become a parallel language SSOT or a stale audit bundle.
84. When an accepted industrial-maturity design slice is implemented while adjacent questions remain undecided, update the owning IM inventory with the accepted behavior, keep the unresolved question as a named pending decision in [../rollups/NEXT-STAGE-GAP-LEDGER.md](../rollups/NEXT-STAGE-GAP-LEDGER.md), and do not let the ledger become the authority for already accepted syntax.
85. When a resource-topology gap closure updates rollups and `workflows/TEST-CATALOG.md`, refresh generated indexes and record the docs-maintenance rule here so future checkpoints can distinguish evidence indexing from language semantics.
86. When a resource selector closure broadens an already-accepted bounded history family, update rollups and the test catalog with the precise family list and keep open-gap wording intact for unsupported families, unbounded snapshots, and explicit copy semantics.
87. When explicit resource-copy evidence lands for only one selector family, describe the exact accepted copy shape and keep the broader `<<` clone/copy model listed as open unless type-directed source/sink semantics are also implemented and tested.
88. When an IM-D5 stream-source closure broadens an existing accepted slice, update the gap audit, IM-D5 inventory, next-stage ledger, test catalog, and owning runbooks with the precise source orders, element families, termination behavior, and remaining unsupported stream semantics before refreshing generated doc stats.
89. For stdin stream-zip closures, keep documentation scoped to the accepted finite line-stream slice: record stdin/list and stdin/file source orders, stdin EOF termination, `string` element binding, and the duplicate `@stdin & @stdin` fail-closed boundary; do not present this as true snapshot joins, arbitrary stream-driver support, pressure scheduling, timeout policy, or duplicate external-input consumption.
90. When a bounded resource-selector snapshot becomes accepted as a materialized stream-zip input, keep rollups and `workflows/TEST-CATALOG.md` framed as evidence indexes: name the selector families and finite-barrier behavior, cite the adjacent scalar-selector negative, and keep true snapshot joins listed as open unless they have separate IM-D5 runtime evidence.
91. When a parser/runtime slice turns an already-documented type family into executable syntax, update the grammar/status SSOTs and evidence rollups together. For example, accepting single-quoted `char` literals and bounded `char` selector snapshots requires EBNF literal coverage, Resource Topology implementation-status wording, current gap/IM inventory evidence, test catalog wording, and a shifted adjacent negative for the next unsupported family.
92. When compiler-owned IM-D10 nano negative-path evidence changes, update the gap audit, next-stage ledger, IM-D10 inventory, `workflows/TEST-CATALOG.md`, CLI / Nano runbook, Test Quality runbook, and `DOC-STATS.md` together. Keep the wording scoped to compiler-side producer/verifier and static repository contracts; package-manager lifecycle UX, resolver policy, remote registry semantics, and trust/auth remain Spio or Platform confirmation items.
93. When range literal semantics change, keep `ACTIVE-SYNTAX.md`, IM-D1, the current gap audit, the next-stage ledger, owning runbooks, and the unit/security test evidence aligned. Record the exact accepted operand family and materialized value shape instead of implying broader sequence/range-handle semantics.
94. When closing a function-return fallback gap, keep IM-D1, the current gap audit, the next-stage ledger, `workflows/TEST-CATALOG.md`, and owning runbooks aligned. Distinguish positive scalar/inferred return evidence from fail-closed tuple return annotations so tuple value IR remains an explicit open language/runtime gap.
95. When closing an accepted match-case or function-match-sugar Sema gap, keep IM-D1, the current gap audit, the next-stage ledger, `workflows/TEST-CATALOG.md`, Sema / IR, Test Quality, and `DOC-STATS.md` aligned. Record only the tested scrutinee/pattern subset, branch-scope isolation, scalar/string result families, and fail-closed negatives; leave broader match result families open unless they have separate runtime evidence.
96. When closing an IM-D4 clone/copy slice, update active syntax, Resource Topology, IM-D4, the current gap audit, the next-stage ledger, `workflows/TEST-CATALOG.md`, and owning runbooks with the exact accepted source families. Keep rejected compatibility paths such as `name <- list_or_dict_or_matrix` visible, and leave file/topology-resource or future resource-family clone semantics open unless they have their own runtime evidence.
97. When closing an IM-D4 file lifecycle slice, record the exact source-reachable edge in the current gap audit, IM-D4 inventory, next-stage ledger, `workflows/TEST-CATALOG.md`, and owning runbooks. For cleanup slices, distinguish tracked file-handle scope-pop, explicit-return cleanup, loop break/continue cleanup, and default file flex-rebind cleanup settlement from source-level fallback recovery for implicit or reassignment cleanup; keep non-file resource cleanup listed as open unless it has its own runtime evidence. If the slice closes alias invalidation for one operation shape, state that exact shape and keep broader cleanup/recovery families open.
98. When closing a task_await resource-effect compatibility slice, update IM-D4, the current gap audit, the next-stage ledger, `workflows/TEST-CATALOG.md`, and Test Quality evidence rules with the exact task success/failure boundary. Keep non-task `?|` sources and bare continuation freeze fallback visibly fail-closed, and leave arbitrary value-producing resource-operation recovery, implicit cleanup, and pressure observers open unless they have separate runtime evidence.
99. When narrowing IM-D4 plain resource-operation settlement, update the current gap audit, IM-D4 inventory, next-stage ledger, `workflows/TEST-CATALOG.md`, Codegen / Runtime runbook, and Test Quality runbook with the exact file acquire/write/release/iterator boundary. Keep evidence-only closures framed as closed evidence for already implemented behavior, keep the explicit `?| ... | fallback` recovery wrapper distinct from default statement-boundary fail-fast behavior, and leave arbitrary value-producing recovery, source-level fallback recovery for implicit/reassignment cleanup, pressure observers, and non-file resource families open unless those paths have their own runtime evidence.
100. When broadening statement-shaped resource-effect settlement, update the current gap audit, IM-D4 inventory, next-stage ledger, `workflows/TEST-CATALOG.md`, and owning runbooks with the exact operation shape, effect family, success/fallback/no-fallback evidence, and adjacent expression-context rejection. If file handle acquire is accepted under `?|`, distinguish failure recovery, successful acquire followed by each tested later file operation such as iteration, close-method use, guarded write-method use, or acquired-handle instant-pull use, close-method receiver invalidation when covered, and recovered failure followed by zero-handle fail-closed behavior instead of implying every post-acquire resource operation is complete.
101. When closing a value-producing resource-effect slice, record the exact resource operation family, success value type, fallback/handler behavior, no-fallback settlement, and adjacent rejected expression forms. Keep arbitrary resource-operation recovery, pressure observers, implicit cleanup, and non-file cleanup families open unless they have separate parser/Sema/lowering/codegen/runtime evidence.
102. When broadening an already-closed value-producing resource-effect slice, update the gap audit, IM-D4 inventory, next-stage ledger, test catalog, owning team runbooks, and `DOC-STATS.md` with the precise new operation family, target value family, and effect family. For file/stdin instant-pull closures, keep the wording limited to the tested value families, such as file `i64`, acquired file-handle `i64`, untyped stdin `i64`, explicit-target stdin `f64`/`string`/typed-list values, and the exact `io`/`closed`/`parse` recovery evidence. For materialized container-bounds closures, record the `STYIO_RUNTIME_LIST_INDEX`, `STYIO_RUNTIME_DICT_KEY`, and `STYIO_RUNTIME_MATRIX_INDEX` / `bounds` evidence, the default plain-expression guards, matrix row and row-range behavior when accepted, list-slice behavior when accepted, and the still-open dict slice-shaped bounds boundary. For value-returning resource-method closures, record whether the accepted body shape is only a single `<| expr`, whether direct calls and `?| method() | fallback` both return the value, whether returned file instant-pull failures recover through catch-all fallback or matched `io` handlers, whether returned canonical stdin instant-pull failures recover through catch-all fallback or matched `parse` handlers, whether returned list index/list-slice, inline dict-index, or typed-parameter matrix cell/row or row-range slice failures recover through catch-all fallback or matched `bounds` handlers, and which lexical/global capture, dict slice, multi-statement, or other failing-method recovery paths remain open.
103. When shrinking the IM-D1 state/resource-method inline clone gap, update IM-D1, the current gap audit, the next-stage ledger, `workflows/TEST-CATALOG.md`, Frontend, Sema / IR, Test Quality, and `DOC-STATS.md` with the exact AST family and source path that now execute. Distinguish parser-path acceptance from inline-clone coverage, require an adjacent fail-closed negative such as invalid `CharAST` or malformed `FmtStrAST` syntax, and leave the broader unsupported clone fallback open until every accepted source-reachable helper-body family has evidence.

## Change Classes

1. Small: typo, link fix, or local README wording. Run docs audit.
2. Medium: new docs collection, generated index config, SSOT table change, external handoff doc, or CLI/runtime contract matrix update. Update policy and run generated-index checks.
3. High: repository boundary, archive lifecycle, docs audit rule, or ecosystem ownership change. Use checkpoint workflow and coordinate affected implementation teams.

## Required Gates

Documentation gates:

```bash
python3 scripts/docs-index.py --write
python3 scripts/workflow-scheduler.py check
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
