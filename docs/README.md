# Styio Docs

**Purpose:** Define the boundary of the `docs/` tree and point readers to the generated inventory in [INDEX.md](./INDEX.md); detailed file listings live in directory-level `INDEX.md` files, not here.

**Last updated:** 2026-05-09

## Tree Contract

1. Design-level SSOT lives in `docs/design/`.
2. Contributor, agent, repository-boundary, dependency, and documentation rules live in `docs/specs/`.
3. Team daily-work runbooks live in `docs/teams/`.
4. Still-active review findings live in `docs/review/`; absorbed findings use Git history.
5. Current plans and cross-repo contracts live in `docs/plans/`.
6. Cross-repository handoff notes intended for `styio-spio` live in `docs/external/for-spio/`.
7. IDE integration, embedding, and LSP usage material lives in `docs/external/for-ide/`.
8. Reusable workflows and templates live in `docs/assets/`.
9. Compressed active summaries live in `docs/rollups/`.
10. Minimal lifecycle metadata lives in `docs/archive/`; exact old prose is recovered from Git history.
11. Current checkpoints and temporary ADRs stay in their dedicated directories; implemented decisions are compressed and raw daily history is not retained by default.

## Entry Points

1. Directory inventory: [INDEX.md](./INDEX.md)
2. Build and dev environment: [BUILD-AND-DEV-ENV.md](./BUILD-AND-DEV-ENV.md)
3. Repository ecosystem map: [specs/REPOSITORY-MAP.md](./specs/REPOSITORY-MAP.md)
4. Documentation policy: [specs/DOCUMENTATION-POLICY.md](./specs/DOCUMENTATION-POLICY.md)
5. Agent/contributor rules: [specs/AGENT-SPEC.md](./specs/AGENT-SPEC.md)
6. Project principles and objectives: [specs/PRINCIPLES-AND-OBJECTIVES.md](./specs/PRINCIPLES-AND-OBJECTIVES.md)
7. Team daily runbooks: [teams/INDEX.md](./teams/INDEX.md)
8. Current-state rollups: [rollups/INDEX.md](./rollups/INDEX.md)
9. Workflow assets: [assets/INDEX.md](./assets/INDEX.md)
10. Design SSOT: [design/INDEX.md](./design/INDEX.md)
11. IDE integration docs: [external/for-ide/INDEX.md](./external/for-ide/INDEX.md)
12. Archive lifecycle metadata: [archive/INDEX.md](./archive/INDEX.md)
13. Plans scope and status rules: [plans/README.md](./plans/README.md)
14. Implemented decision summary: [adr/IMPLEMENTED-DECISIONS.md](./adr/IMPLEMENTED-DECISIONS.md)

## Default Read Order

1. Start with [rollups/CURRENT-STATE.md](./rollups/CURRENT-STATE.md).
2. Follow its links into the owning SSOT in `design/`, `specs/`, `assets/workflow/`, `teams/`, `plans/`, or current rollups.
3. Use Git history only when exact old history/review wording is required.
4. Use `archive/` only for lifecycle metadata.

## Maintenance Rules

1. `README.md` explains scope, naming, and maintenance rules.
2. `INDEX.md` is the generated inventory for collection directories.
3. Regenerate indexes with `python3 scripts/docs-index.py --write` after docs-tree changes.
4. Validate archive/rollup state with `python3 scripts/docs-lifecycle.py validate`.
5. Verify the tree with `python3 scripts/docs-audit.py` or `ctest --test-dir build/default -L docs`.
