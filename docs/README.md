# Styio Docs

**Purpose:** Define the boundary of the `docs/` tree and point readers to the generated inventory in [INDEX.md](./INDEX.md); detailed file listings live in directory-level `INDEX.md` files, not here.

**Last updated:** 2026-04-15

## Tree Contract

1. Design-level SSOT lives in `docs/design/`.
2. Contributor, agent, repository-boundary, dependency, and documentation rules live in `docs/specs/`.
3. Team daily-work runbooks live in `docs/teams/`.
4. Review findings and open conflicts live in `docs/review/`.
5. Plans and migration drafts live in `docs/plans/`.
6. Cross-repository handoff notes intended for `styio-spio` live in `docs/for_spio/`.
7. IDE integration, embedding, and LSP usage material lives in `docs/for-ide/`.
8. Reusable workflows and templates live in `docs/assets/`.
9. Compressed active summaries live in `docs/rollups/`.
10. Archived raw provenance lives in `docs/archive/`.
11. Daily history, frozen milestones, and ADRs stay in their dedicated directories.

## Entry Points

1. Directory inventory: [INDEX.md](./INDEX.md)
2. Repository ecosystem map: [specs/REPOSITORY-MAP.md](./specs/REPOSITORY-MAP.md)
3. Documentation policy: [specs/DOCUMENTATION-POLICY.md](./specs/DOCUMENTATION-POLICY.md)
4. Agent/contributor rules: [specs/AGENT-SPEC.md](./specs/AGENT-SPEC.md)
5. Project principles and objectives: [specs/PRINCIPLES-AND-OBJECTIVES.md](./specs/PRINCIPLES-AND-OBJECTIVES.md)
6. Team daily runbooks: [teams/INDEX.md](./teams/INDEX.md)
7. Current-state rollups: [rollups/INDEX.md](./rollups/INDEX.md)
8. Workflow assets: [assets/INDEX.md](./assets/INDEX.md)
9. Design SSOT: [design/INDEX.md](./design/INDEX.md)
10. IDE integration docs: [for-ide/INDEX.md](./for-ide/INDEX.md)
11. Archived provenance: [archive/INDEX.md](./archive/INDEX.md)
12. File-governance alignment plan: [plans/Styio-Ecosystem-File-Governance-Alignment-Plan.md](./plans/Styio-Ecosystem-File-Governance-Alignment-Plan.md)

## Default Read Order

1. Start with [rollups/CURRENT-STATE.md](./rollups/CURRENT-STATE.md).
2. Follow its links into the owning SSOT in `design/`, `specs/`, `review/`, `assets/workflow/`, or the active milestone batch.
3. Read only the newest raw history/review entry that is still kept active.
4. Use `archive/` only when exact historical wording or provenance is required.

## Maintenance Rules

1. `README.md` explains scope, naming, and maintenance rules.
2. `INDEX.md` is the generated inventory for collection directories.
3. Regenerate indexes with `python3 scripts/docs-index.py --write` after docs-tree changes.
4. Validate archive/rollup state with `python3 scripts/docs-lifecycle.py validate`.
5. Verify the tree with `python3 scripts/docs-audit.py` or `ctest --test-dir build -L docs`.
