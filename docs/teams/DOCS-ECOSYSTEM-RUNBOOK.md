# Docs / Ecosystem Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of repository documentation, generated indexes, archive/rollup lifecycle, templates, and external Styio ecosystem handoff material.

**Last updated:** 2026-10-01

## Mission

Maintain current documentation, generated inventories, lifecycle metadata,
workflow entrypoints, and cross-repository handoffs. Language features, public
interfaces, tests, and repository roles retain their own authoritative documents.
Use this runbook to find the owner and verification path for a documentation
change.

## Owned Surface

Target-scoped CMake maintenance routing is authoritative in `scripts/team-docs-gate.py` (`CMAKE_ROLES`) and described in `workflows/TEAM-RUNBOOK-MAINTENANCE-GATE.md`. Preserve exact positive and negative assertions in `tests/cmake_ownership_test.py`. Do not reinstate the old blanket service-manifest mapping or treat transitive consumers as maintainers; source membership, target configuration, shared composition, and the Coordination entry point have distinct review boundaries.

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

### 1. Select the authority and change scope

1. Start from [CURRENT-STATE](../rollups/CURRENT-STATE.md), then the
   [documentation authority table](../specs/DOCUMENTATION-POLICY.md#04-常见单一事实来源ssot速查)
   and [change-routing workflow](../../workflows/DOCS-MAINTENANCE-WORKFLOW.md#change-routing).
2. Update the existing owner document. Create a new document only after the
   reuse check, using `scripts/docs-scaffold.py --reuse-reviewed`. Keep English
   authoritative and translation companions explicit.
3. Distinguish accepted semantics, implemented capability scope, research
   proposals, and unverified evidence. Update statements in place rather than
   appending a dated correction below a contradictory statement. Apply the
   [public wording rules](../specs/DOCUMENTATION-POLICY.md#012-public-wording-discipline).
4. Route behavior review to the affected source owner through the
   [coordination matrix](./COORDINATION-RUNBOOK.md#review-matrix). Documentation
   review does not replace compiler or consumer acceptance.

### 2. Maintain documents, generated views, and tracked assets

1. Maintain `Purpose` and `Last updated`, relative links, and the standard
   [runbook headings](../assets/templates/TEAM-RUNBOOK-TEMPLATE.md). Replace
   developer-machine details with documented placeholders.
2. Regenerate indexes with `scripts/docs-index.py --write`. The generator derives
   dates from collection entries, then collection metadata for empty directories;
   UTC today is a fallback. Do not hand-edit generated inventories.
3. Refresh [DOC-STATS](./DOC-STATS.md) from the docs-audit JSON export whenever
   runbooks change. Counts describe maintenance scope, not document quality.
4. Keep installed shared assets tracked even when broad ignore rules match them.
   The resource prelude lives at `share/styio/prelude/resources.styio`; its
   ownership is defined by [library/manifest.json](../../library/manifest.json)
   and the [library guide](../../library/README.md).
5. Follow [repository hygiene](../../workflows/REPO-HYGIENE-COMMIT-STANDARD.md)
   for approved document roots, ignored audit output, generated files, and
   residue checks. Current examples reference executable source and test oracles.

### 3. Synchronize language contracts and evidence

| Change | Owner to update first | Supporting documents |
|---|---|---|
| Syntax, tokens, inference, capabilities or callable behavior | The owning [syntax feature SSOT](../design/syntax/features/README.md) | Its declared grammar, token, semantics, diagnostics, compatibility, teaching, implementation and evidence roles |
| Language model and program views | [Language Design](../design/Styio-Language-Design.md#24-visual-design-intent) | The single `toml design-intent` block, concise README descriptions, research evaluation criteria |
| Resource definitions, state, selectors and transfer | [Resource Topology](../design/Styio-Resource-Topology.md) and the owning feature | [Resource identifiers](../design/syntax/RESOURCE_IDENTIFIERS.md), shared grammar and accepted/compatibility/retired fixtures |
| Snapshot, delta, query, identity or runtime-event fields | [Observable decoder contract](../../src/StyioServices/StyioObservable/README.md) | [Observable semantics](../design/Styio-Observable-Language.md), producer/consumer fixtures, CLI/Runtime/IDE handoffs as affected |
| Test registration, labels and oracles | [Test Catalog](../../workflows/TEST-CATALOG.md) and [Test Quality](./TEST-QUALITY-RUNBOOK.md) | CMake registration and the feature evidence map; algorithm layout stays in [tests/algorithms/README.md](../../tests/algorithms/README.md) |

Use [Add Syntax](../../workflows/ADD-SYNTAX-WITH-SKILLS.md) or
[Correct Syntax Contract](../../workflows/CORRECT-SYNTAX-CONTRACT.md) for language
changes. Accepted decisions, delivery state and derived readiness remain
separate. Update feature documents before regenerating their graph; prerequisite
convergence alone does not activate a deferred feature.

Compact syntax pages and runbooks link to the owning feature instead of copying
callable schemes, effect rows, usage facts, closure lifetimes, interface schemas,
or cache formats. Proposed source forms remain outside executable examples.
Runtime observation completeness and default-enablement limits come from the
current contract and [observable follow-up register](../rollups/OBSERVABLE-DELIVERY-FOLLOW-UPS.md).

### 4. Maintain build, workflow, and ecosystem entrypoints

1. Keep common setup in [BUILD-AND-DEV-ENV](../BUILD-AND-DEV-ENV.md), subsystem
   build details with their owner, and dependencies in [THIRD-PARTY](../specs/THIRD-PARTY.md).
   Use `build/<variant>` paths and portable toolchain discovery. Validate documented
   versions against actual configuration instead of copying a frozen tool list.
2. Register workflow changes through [Workflow Orchestration](../../workflows/WORKFLOW-ORCHESTRATION.md)
   and `scripts/workflow-scheduler.py`. When editing `delivery-gate.sh`, preserve
   literal `delivery-checkpoint` and `delivery-push` scheduler invocations because
   the released external audit checks those entrypoints.
3. Maintain repo-local skills through [workflows/skills/README.md](../../workflows/skills/README.md)
   and the [tool/skill registry](../../workflows/TOOL-SKILL-REGISTRY-GATE.md).
   `skill.toml` owns discovery metadata and declared workflow references;
   a skill's detailed execution instructions have one documented owner.
   The performance skill uses `SKILL.md` for execution and TOML for registry metadata.
4. Preserve [Performance Research](../../workflows/PERFORMANCE-RESEARCH-WORKFLOW.md)
   role separation: Benchmark owns workload/evidence and independent evaluation;
   Modification owns authorized compiler changes. Workloads, measurements, and
   dossiers stay in `styio-benchmark`; this repository owns compiler probes.
5. Route repository roles and external teaching material through the
   [repository map](../specs/REPOSITORY-MAP.md). Compiler-maintenance and user
   programming skills have different audiences and repositories.
6. Maintain compiler/Pafio/Vityo handoffs through the [machine-contract matrix](../external/for-pafio/Styio-Ecosystem-Machine-Contract-Matrix.md)
   and [nano coordination contract](../external/for-pafio/Styio-Nano-Pafio-Coordination.md).
   These consumer documents reference the owner schema; they do not duplicate it.
7. Follow [Post-Commit CI Checks](../specs/POST-COMMIT-CI-CHECKS.md) for sibling
   checkout refs, evidence reuse, required checks and CI handoffs. Preserve an
   explicitly scoped single-repository release boundary in its owning plan;
   cross-repository capability does not expand that release scope.

### 5. Maintain current state and remove absorbed history

1. Keep open gaps with owner, evidence and closure condition in the
   [gap ledger](../rollups/NEXT-STAGE-GAP-LEDGER.md). Link current implementation
   inventories rather than repeating historical test counts or old stage status.
2. Follow [plan state ownership](../plan/README.md#state-ownership): the semantic
   plan, execution checkpoints and rendered projection have separate writers.
   Update plan inventory only from verified acceptance; keep external blockers
   explicit. Do not hand-edit generated plan projections during docs cleanup.
3. Retain interrupted recovery information only while it is needed. Promote
   surviving decisions to their owner, unresolved work to the gap ledger, and
   use `docs-lifecycle.py mark` / `cleanup` for supported history/review/audit
   families. Git history retains exact old prose; archive directories retain
   lifecycle metadata rather than duplicate content.
4. Preserve [local divergence migration](../rollups/LOCAL-DIVERGENCE-MIGRATION-2026-07-04.md)
   until its remaining checkpoints close. A date in a filename is not evidence
   that an active migration or unresolved design decision can be removed.
5. Review current rollups separately: remove absorbed narrative, correct stale
   facts from source/test evidence, and preserve still-open constraints. The
   lifecycle tool does not classify ordinary rollups as dated history.

### 6. Verify and hand off

1. Run the focused contract or tool regression, then the Required Gates below.
2. Include source revision, affected owners, commands and passed/failed/not-run
   results. An absent external audit, consumer repository or platform is an
   explicit verification limit, not a passing result.
3. Before a commit, use [Functional Commit Readiness](../../workflows/FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md)
   and the applicable staged delivery gate. A committer self-check reuses
   existing authorization and evidence; separate publication decisions retain
   their own scope.

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

For observable work, resume from the [current delivery sequence](../rollups/NEXT-STAGE-GAP-LEDGER.md#81-observable-language-delivery-sequence) and owning contracts, not earlier milestone deferrals. The [follow-up register](../rollups/OBSERVABLE-DELIVERY-FOLLOW-UPS.md) records gaps, not authorization to implement them or enable runtime observation by default.
