# Docs / Ecosystem Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of repository documentation, generated indexes, archive/rollup lifecycle, templates, and external Styio ecosystem handoff material.

**Last updated:** 2026-04-26

## Mission

Own documentation structure and cross-repository clarity. This team protects SSOT discipline, generated indexes, archive provenance, external repository boundaries, handoff notes, and reusable templates. It does not redefine language semantics, accepted tests, or package-manager ownership.

## Owned Surface

Primary paths:

1. `docs/`
2. `docs/assets/`
3. `docs/rollups/`
4. `docs/archive/`
5. `templates/`
6. `scripts/docs-index.py`
7. `scripts/docs-audit.py`
8. `scripts/docs-lifecycle.py`
9. `scripts/team-docs-gate.py`
10. `scripts/delivery-gate.sh`

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
6. Run the team runbook maintenance gate before delivery so source/test/docs folder changes cannot land without the mapped runbook update or required runbook format.
7. Use archive lifecycle tooling for raw history/review compression rather than manually moving provenance.
8. Use the unified delivery gate for docs/process deliveries so hygiene, runbook maintenance, and docs audit stay coupled.
9. Keep the ecosystem CLI contract mirror and cross-repo doc gate aligned whenever `styio-spio` or `styio-view` handoff docs change.
10. When a compiler-side machine contract grows, update the owner SSOT and both consumer handoff docs in the same checkpoint instead of leaving one side on preview wording.
11. Keep generated `INDEX.md` files deterministic for empty collections by deriving fallback timestamps from collection metadata instead of local wall-clock date.
12. When CI validates sibling ecosystem repositories, use the downstream `nightly` branch as the shared ecosystem baseline; `ai-dev` remains a writable staging lane in the upstream repo, but cross-repository contract checks still validate against the downstream delivery lane.
13. When syntax-delivery rules change, update the workflow asset, gate scripts, and delivery entrypoints in the same checkpoint; workflow-only prose is not enough.
14. Keep `docs/assets/workflow/WORKFLOW-ORCHESTRATION.md` and `scripts/workflow-scheduler.py` as the registry for workflow separation; new workflow assets must be registered and pass scheduler validation before delivery.

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
./scripts/delivery-gate.sh --mode checkpoint --skip-health
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
