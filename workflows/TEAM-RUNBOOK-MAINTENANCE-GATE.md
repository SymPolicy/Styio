# Team Runbook Maintenance Gate

**Purpose:** Define the delivery gate that requires team runbooks under `docs/teams/` to be updated and kept in the standard template shape when files in corresponding team-owned folders are added, modified, renamed, or deleted.

**Last updated:** 2026-04-16

## Goal

Every delivery that changes an owned folder must update the daily-work document for the affected team. The gate also validates the required runbook shape so maintainers can use a documented format instead of reading gate source. It does not judge whether the prose is sufficient; review still verifies that the update describes the real maintenance consequence.

## Command

Local worktree gate:

```bash
python3 scripts/team-docs-gate.py
```

Staged-only gate:

```bash
python3 scripts/team-docs-gate.py --mode staged
```

Branch or CI gate:

```bash
python3 scripts/team-docs-gate.py --base origin/main
```

`scripts/docs-audit.py` runs the worktree gate automatically, so the existing docs CTest and `checkpoint-health` docs step include this check.
`scripts/delivery-gate.sh` also runs this gate as part of the common delivery floor.

## Format Gate

Team runbooks must follow [../docs/assets/templates/TEAM-RUNBOOK-TEMPLATE.md](../docs/assets/templates/TEAM-RUNBOOK-TEMPLATE.md). The gate checks:

1. The first heading is an H1 ending in `Runbook`.
2. Top-level `**Purpose:** ...` metadata exists.
3. Top-level `**Last updated:** YYYY-MM-DD` metadata exists.
4. The H2 sections are exactly these, in order:
   1. `Mission`
   2. `Owned Surface`
   3. `Daily Workflow`
   4. `Change Classes`
   5. `Required Gates`
   6. `Cross-Team Dependencies`
   7. `Handoff / Recovery`

[../docs/teams/COORDINATION-RUNBOOK.md](../docs/teams/COORDINATION-RUNBOOK.md) is checked against a coordinator-specific H2 shape:

1. `Mission`
2. `Module Map`
3. `Ownership Table`
4. `Review Matrix`
5. `Escalation Rules`
6. `Checkpoint Policy`
7. `Release / Cutover Gates`
8. `Handoff / Recovery`

Gate failures print the missing, duplicate, extra, or out-of-order section and point back to this workflow document and the template.

## Folder Mapping

| Team doc | Watched paths |
|----------|---------------|
| `FRONTEND-RUNBOOK.md` | `src/StyioToken/`, `src/StyioUnicode/`, `src/StyioParser/`, `src/Deprecated/` |
| `SEMA-IR-RUNBOOK.md` | `src/StyioAST/`, `src/StyioSema/`, `src/StyioLowering/`, `src/StyioIR/`, `src/StyioToString/`, `src/StyioSession/` |
| `CODEGEN-RUNTIME-RUNBOOK.md` | `src/StyioCodeGen/`, `src/StyioJIT/`, `src/StyioExtern/`, `src/StyioRuntime/` |
| `CLI-NANO-RUNBOOK.md` | `src/main.cpp`, `src/StyioConfig/`, `configs/`, `scripts/gen-styio-nano-profile.py`, `docs/external/for-spio/` |
| `IDE-LSP-RUNBOOK.md` | `src/StyioIDE/`, `src/StyioLSP/`, `docs/external/for-ide/`, `tests/ide/` |
| `GRAMMAR-RUNBOOK.md` | `grammar/tree-sitter-styio/`, `src/StyioIDE/TreeSitterBackend.*` |
| `TEST-QUALITY-RUNBOOK.md` | `tests/`, `src/StyioTesting/`, `extend_tests.py`, parser shadow scripts, fuzz pack script, `scripts/checkpoint-health.sh` |
| `PERF-STABILITY-RUNBOOK.md` | `benchmark/` |
| `DOCS-ECOSYSTEM-RUNBOOK.md` | `README.md`, `docs/`, `workflows/`, `templates/`, docs maintenance scripts, `scripts/delivery-gate.sh`, `scripts/team-docs-gate.py` |

Generated `docs/**/INDEX.md` files do not themselves require runbook updates. They are regenerated inventory, not a maintenance decision.

## Stats Requirement

When any team runbook or `COORDINATION-RUNBOOK.md` changes, [../docs/teams/DOC-STATS.md](../docs/teams/DOC-STATS.md) must also be refreshed in the same delivery. The statistics file uses the `scripts/docs-audit.py` word-count and character-count rules.

## Failure Handling

If the gate fails:

1. Open the required runbook named in the error.
2. If the error is structural, align the file with [../docs/assets/templates/TEAM-RUNBOOK-TEMPLATE.md](../docs/assets/templates/TEAM-RUNBOOK-TEMPLATE.md) or the coordinator shape above.
3. Update only what the delivery actually changed: owned surface, workflow, gates, cross-team dependencies, or handoff notes.
4. If the runbook content changes, refresh [../docs/teams/DOC-STATS.md](../docs/teams/DOC-STATS.md).
5. Re-run `python3 scripts/team-docs-gate.py` and `python3 scripts/docs-audit.py`.

Do not bypass the gate by editing unrelated prose. The runbook update should make the actual maintenance consequence discoverable for the next owner.
