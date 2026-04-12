# Styio Docs

**Purpose:** Define the boundary of the `docs/` tree and point readers to the generated inventory in [INDEX.md](./INDEX.md); detailed file listings live in directory-level `INDEX.md` files, not here.

**Last updated:** 2026-04-12

## Tree Contract

1. Design-level SSOT lives in `docs/design/`.
2. Contributor, agent, repository-boundary, dependency, and documentation rules live in `docs/specs/`.
3. Review findings and open conflicts live in `docs/review/`.
4. Plans and migration drafts live in `docs/plans/`.
5. Cross-repository handoff notes intended for `styio-spio` live in `docs/for_spio/`.
6. Reusable workflows and templates live in `docs/assets/`.
7. Daily history, frozen milestones, and ADRs stay in their dedicated directories.

## Entry Points

1. Directory inventory: [INDEX.md](./INDEX.md)
2. Repository ecosystem map: [specs/REPOSITORY-MAP.md](./specs/REPOSITORY-MAP.md)
3. Documentation policy: [specs/DOCUMENTATION-POLICY.md](./specs/DOCUMENTATION-POLICY.md)
4. Agent/contributor rules: [specs/AGENT-SPEC.md](./specs/AGENT-SPEC.md)
5. Workflow assets: [assets/INDEX.md](./assets/INDEX.md)
6. Design SSOT: [design/INDEX.md](./design/INDEX.md)

## Maintenance Rules

1. `README.md` explains scope, naming, and maintenance rules.
2. `INDEX.md` is the generated inventory for collection directories.
3. Regenerate indexes with `python3 scripts/docs-index.py --write` after docs-tree changes.
4. Verify the tree with `python3 scripts/docs-audit.py` or `ctest --test-dir build -L docs`.
