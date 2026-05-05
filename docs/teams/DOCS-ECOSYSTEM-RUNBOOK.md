# Docs / Ecosystem Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of repository documentation, generated indexes, archive/rollup lifecycle, templates, and external Styio ecosystem handoff material.

**Last updated:** 2026-05-04

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
3. [../assets/workflow/DOCS-MAINTENANCE-WORKFLOW.md](../assets/workflow/DOCS-MAINTENANCE-WORKFLOW.md)

## Daily Workflow

1. Check whether the content already has an owning SSOT before adding a new file.
2. Prefer linking and short summaries over copying rules across documents.
3. Add `Purpose` and `Last updated` metadata to every active docs file.
4. Use [../assets/templates/TEAM-RUNBOOK-TEMPLATE.md](../assets/templates/TEAM-RUNBOOK-TEMPLATE.md) for team runbook structure changes.
5. Regenerate `INDEX.md` files after collection changes.
6. Use `python3 scripts/docs-scaffold.py ...` for new docs files or new docs collection directories so metadata, collection registration, and generated indexes are created together.
7. Keep repository-level bootstrap/build entrypoints under `docs/BUILD-AND-DEV-ENV.md`, and push subsystem-only details back down into the owning docs collection instead of overloading `README.md` or `docs/external/for-ide/`.
8. Run the team runbook maintenance gate before delivery so source/test/docs folder changes cannot land without the mapped runbook update or required runbook format.
9. Use archive lifecycle tooling for raw history/review compression rather than manually moving provenance.
10. Use the default unified delivery gate for docs/process deliveries so worktree hygiene, branch-range hygiene, runbook maintenance, docs audit, and external audit stay coupled behind one command.
11. When refactoring `scripts/delivery-gate.sh`, keep literal scheduler profile invocations visible for `delivery-checkpoint` and `delivery-push` so released `audit` can verify that the unified entrypoint still delegates to the approved scheduler profiles.
12. Keep the ecosystem CLI contract mirror and cross-repo doc gate aligned whenever `styio-spio` or `styio-view` handoff docs change.
13. When a compiler-side machine contract grows, update the owner SSOT and both consumer handoff docs in the same checkpoint instead of leaving one side on preview wording.
14. Keep generated `INDEX.md` files deterministic for empty collections by deriving fallback timestamps from collection metadata instead of local wall-clock date.
15. When CI validates sibling ecosystem repositories, use the downstream `nightly` branch as the shared ecosystem baseline; `ai-dev` remains a writable staging lane in the upstream repo, but cross-repository contract checks still validate against the downstream delivery lane.
16. When syntax-delivery rules change, update the workflow asset, gate scripts, and delivery entrypoints in the same checkpoint; workflow-only prose is not enough.
17. Keep `docs/assets/workflow/WORKFLOW-ORCHESTRATION.md` and `scripts/workflow-scheduler.py` as the registry for workflow separation; new workflow assets must be registered and pass scheduler validation before delivery.
18. Keep repository-level build bootstrap docs aligned with the standardized shared baseline: Debian 13, LLVM 18.1.x, CMake/CTest 3.31.6, Python 3.13.5, and Node.js v24.15.0 LTS where Node-backed tooling exists; when CI mirrors differ by host OS, document the mirror explicitly instead of drifting the toolchain version text.
19. Keep `docs/external/for-spio/` aligned with the current `binary` vs `build` split: `--machine-info=json` remains the binary handshake, while `--source-build-info=json` owns the official source-layout contract for `spio build`.
20. When a compiler/runtime/contract adjustment spans multiple checkpoints, add or update an explicit `docs/plans/` implementation plan instead of leaving the execution order only in handoff or runbook prose.
21. When the compiler-side source-build helper changes, keep `scripts/source-build-minimal.sh`, `docs/BUILD-AND-DEV-ENV.md`, and the `--source-build-info=json` handoff wording aligned in the same checkpoint.
22. When a plan remains in `docs/plans/` after one stage closes, make the file say whether it is still `Active`, `Repo-local baseline completed`, or ready for archive; do not force readers to infer status from scattered stage tables.
23. Keep repository entry docs honest about maturity: if repo-local baselines are complete but ecosystem closure is still open, say that explicitly instead of leaving stale `early stage` wording in top-level entrypoints.
24. When an evidence-backed closure retires a gap, move it out of the open gap table, add the smallest closed-evidence note, update the matching checkpoint tree entry, and regenerate affected generated indexes.
25. Keep [../specs/POST-COMMIT-CI-CHECKS.md](../specs/POST-COMMIT-CI-CHECKS.md) aligned with actual GitHub Actions monitoring practice whenever commit, push, or CI handoff rules change.
26. Keep GitHub Actions sibling checkouts for `styio-spio` and `styio-view` pinned to the same branch ref as `styio-nightly` when a workflow runs cross-repository gates.
27. Keep compact syntax references under `docs/design/syntax/` short and defer semantic detail to the owning design SSOT.
28. When syntax tokens change, update the compact syntax page, EBNF, and symbol reference together before regenerating indexes.
29. When standard-stream or resource identifier declarations change, keep `src/StyioPrelude/resources.styio`, `docs/design/syntax/RESOURCE_IDENTIFIERS.md`, `docs/design/syntax/CONTINUATION_TRANSFER.md`, EBNF, symbol reference, and the language design aligned on accepted source forms, canonical/compatibility status, and parser-implementation status.
30. Keep root `workflows/`, `workflows/skills/`, `workflows/workflows.toml`, generated workflow indexes, and `docs/assets/workflow/` mirrors aligned whenever reusable workflows or repo-local skills are added.
31. Keep repo-local skills concise: workflow docs own sequencing, while `skill.toml` owns reusable execution discipline and references.
32. Workflow and skill machine-readable definitions must use TOML (`*.toml`, `skill.toml`, and `agents/openai.toml`); Markdown remains explanatory only.
33. When test coverage changes require `docs/assets/workflow/TEST-CATALOG.md`, keep the catalog as an evidence index and point behavior ownership back to the implementation or test-quality runbook instead of embedding new language semantics there.
34. When language SSOT docs change token-count semantics, update the design page, EBNF, symbol reference, and test catalog in one checkpoint so docs readers do not see conflicting operator depth rules.
35. When syntax is retired, document the cutover in active SSOT docs and add a dated revision note to affected archived milestone pages. Archive pages may preserve provenance, but active catalogs and tests must not keep retired examples as runnable acceptance cases.
36. When the SymPolicy Styio repository set changes, refresh the inventory with `gh repo list SymPolicy --limit 200`, then update root `README.md`, [../specs/REPOSITORY-MAP.md](../specs/REPOSITORY-MAP.md), and any active ecosystem plan that names the old repository set.
37. Keep [../specs/TECHNOLOGY-COMPONENT-INVENTORY.md](../specs/TECHNOLOGY-COMPONENT-INVENTORY.md) aligned with `audit` whenever the technology stack, internal components, open-source components, dependency manifests, Apache-2.0 evidence, or commercial-risk boundaries change.
38. Keep external `audit` execution wired through the repository delivery gate and dedicated GitHub Actions workflow whenever audit policy or cross-repo CI ownership changes.
39. Maintain GitHub merge gates through Rulesets rather than legacy classic branch protection; audit effective branch rules when required status-check governance changes.
40. Keep `docs/audit/defects/` ignored and out of tracked commits; promote audit findings into an approved docs collection or issue before they become repository-owned records.
41. When compiler source directories are renamed or split, update active path references across `AGENT-SPEC`, technology inventory, workflow mirrors, rollups, audit findings, and team-runbook mappings in the same checkpoint; archive pages may preserve historical paths.
42. When C++ reference equivalence tests add algorithm cases, keep `docs/assets/workflow/TEST-CATALOG.md` as the concise evidence index and leave case layout details in `tests/algorithms/README.md` plus the Test Quality runbook.
43. When a user-driven syntax correction exposes a compiler/spec mismatch, record the reusable closure path in the workflow directory and make the workflow entrypoint tell agents to read the applicable workflow before editing.
44. Keep CMake build output conventions under `build/<variant>` across scripts, GitHub Actions, workflow docs, and external handoff docs; root `build-*` directories are legacy generated artifacts only and should not be introduced by new commands.
45. When a typed syntax addition changes design SSOTs and test catalogs together, keep the docs update concise: record the accepted source form, point catalog entries to evidence coverage, refresh generated indexes, and update team stats in the same delivery.
46. When source spellings are declared canonically equivalent, keep the ADR, design SSOT, workflow mirror, and test catalog aligned while leaving behavior ownership in parser, Sema / IR, Codegen / Runtime, and Test Quality runbooks.

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
