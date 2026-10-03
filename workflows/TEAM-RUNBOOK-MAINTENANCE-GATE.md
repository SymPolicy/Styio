# Team Runbook Maintenance Gate

**Purpose:** Define the delivery gate that requires team runbooks under `docs/teams/` to be updated and kept in the standard template shape when files in corresponding team-owned folders are added, modified, renamed, or deleted.

**Last updated:** 2026-10-01

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
| `FRONTEND-RUNBOOK.md` | `src/StyioToken/`, `src/StyioUnicode/`, `src/StyioParser/`, `src/Deprecated/`, parser legacy-entry audit scripts |
| `SEMA-IR-RUNBOOK.md` | `src/StyioAST/`, `src/StyioSema/`, `src/StyioLowering/`, `src/StyioIR/`, `src/StyioResourceTopology/`, `src/StyioToString/`, `src/StyioSession/`, Sema / IR source fragment, `StyioObservable/`, `StyioObservableProducer/`, `StyioUtil/SemanticIdentity.*` |
| `CODEGEN-RUNTIME-RUNBOOK.md` | `src/StyioCodeGen/`, `src/StyioJIT/`, `src/StyioExtern/`, `src/StyioRuntime/`, `scripts/runtime-surface-gate.py`, `StyioNative/`, `StyioObservable/RuntimeCorrelation.*`, backend source map |
| `CLI-NANO-RUNBOOK.md` | `src/main.cpp`, `src/StyioServices/StyioCLI/`, `src/StyioServices/StyioConfig/`, `configs/`, `scripts/gen-styio-nano-profile.py`, `scripts/source-build-minimal.sh`, `docs/external/for-pafio/`, `StyioObservableProducer/` |
| `IDE-LSP-RUNBOOK.md` | `src/StyioServices/StyioIDE/`, `src/StyioServices/StyioLSP/`, `docs/external/for-ide/`, `tests/ide/` |
| `GRAMMAR-RUNBOOK.md` | `grammar/tree-sitter-styio/`, `src/StyioServices/StyioIDE/TreeSitterBackend.*` |
| `TEST-QUALITY-RUNBOOK.md` | `tests/`, `src/StyioTesting/`, `tests/workflow_scheduler_test.py`, parser shadow suite gates, fuzz pack script, `scripts/coverage-gate.sh`, `scripts/checkpoint-health.sh` |
| `PERF-STABILITY-RUNBOOK.md` | `benchmark/`, `src/StyioProfiler/` |
| `DOCS-ECOSYSTEM-RUNBOOK.md` | `README.md`, `docs/`, `library/`, `workflows/`, `templates/`, docs maintenance scripts, `scripts/delivery-gate.sh`, `scripts/stdlib-manifest-gate.py`, `scripts/team-docs-gate.py`, `scripts/workflow-scheduler.py` |

Generated `docs/**/INDEX.md` files do not themselves require runbook updates. They are regenerated inventory, not a maintenance decision.

## Target-Scoped CMake Mapping

`src/CMakeLists.txt` only assembles the existing target modules and requires
Coordination. Edit the owning file for normal source or target-setting changes;
do not move those changes back into the assembly entry point. New unmapped
`src/cmake/*.cmake` files fall back to Coordination and fail the inventory
regression until their explicit ownership is added.

Source lists are explicit (no glob), target names and dependency directions are
unchanged, and nano reuses the same ordered source lists. A transitive consumer
is not an additional maintenance owner. The following exceptions are genuinely
shared contracts, so all listed roles still update their runbooks.

| Files under `src/cmake/` | Required roles |
|---|---|
| `StyioSymbolSources.cmake`, `StyioFrontendFoundationSources.cmake`, `targets/StyioSymbolCore.cmake` | Frontend |
| `StyioSemaIRSources.cmake` | Sema / IR |
| `StyioFrontendProfilerSources.cmake` | Performance / Stability |
| `StyioBackendSources.cmake`, `StyioRuntimeSources.cmake`, `StyioNativeInteropSources.cmake`, `targets/StyioRuntimeCore.cmake` | Codegen / Runtime |
| `StyioTestingSources.cmake` | Test Quality |
| `StyioCoreSources.cmake`, `targets/StyioCore.cmake` | Codegen / Runtime + Test Quality |
| `StyioFrontendSources.cmake`, `targets/StyioFrontendCore.cmake` | Frontend + Sema / IR + Codegen / Runtime + Performance / Stability |
| `StyioObservableSources.cmake`, `StyioRuntimeCorrelationSources.cmake`, `targets/StyioObservableCore.cmake` | Sema / IR + Codegen / Runtime |
| `StyioObservableProducerSources.cmake`, `StyioCLIContractSources.cmake`, `targets/StyioCLIContractCore.cmake` | Sema / IR + CLI / Nano |
| `StyioIDESources.cmake`, `StyioLSPSources.cmake`, `targets/StyioLSPD.cmake` | IDE / LSP |
| `targets/StyioIDECore.cmake` | IDE / LSP + Grammar |
| `StyioNanoCoreSources.cmake`, `targets/Styio.cmake`, `targets/StyioNanoCore.cmake`, `targets/StyioNano.cmake` | CLI / Nano |
| `StyioTargetHelpers.cmake` | Frontend + Sema / IR + Codegen / Runtime + CLI / Nano + IDE / LSP |

`scripts/team-docs-gate.py:CMAKE_ROLES` is the executable mapping.
`tests/cmake_ownership_test.py` asserts exact required-role sets, absence of
unrelated roles, missing shared-owner failure, inventory coverage, and both
source and destination roles for renames. Copies only trigger the destination.
The configure-only `cmake_target_contract` CTest freezes pre-split target source
order, links, settings, and the nano alias. It is not a substitute for building
and testing the actual compiler, nano, Observable, and IDE/LSP targets.


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
