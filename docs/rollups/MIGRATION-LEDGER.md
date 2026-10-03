# Migration Ledger

**Purpose:** Track every active historical-compatibility migration so the project can reduce historical burden checkpoint by checkpoint without losing visibility on the still-open seams. Each row has an explicit completion signal so the seam can be retired the day its closure conditions are met.

**Last updated:** 2026-10-01

> Search code, scripts, and docs for `MIGRATION-NEEDED:` to find every site annotated under this ledger.

## How To Use

1. Before adding a new compatibility shim, alias, fallback engine, or "legacy" branch, register it here with its completion signal.
2. When closing a migration, remove the matching `MIGRATION-NEEDED:` markers from the code, then drop the row from this ledger.
3. Each row links to its owner runbook and to the file it lives in. Owners are responsible for keeping the row honest.
4. This ledger does not replace `NEXT-STAGE-GAP-LEDGER.md`. Migrations carry both an end-state plan (here) and any open implementation gap (there) only when the gap is broader than the migration itself.

## Open Migrations

| ID | Name | Owner | Site(s) | Completion Signal |
|----|------|-------|---------|-------------------|
| M-PARSER-01 | Parser compatibility entrypoints | Frontend | `src/StyioParser/Parser.hpp` retains `Legacy` and `New = Nightly`; parser sources retain `_latest` helpers and bridge counters | Remove the compatibility entrypoints/counters after current parser-authority and parity gates prove they are unnecessary; remove or merge `NewParserExpr.cpp`, or record an explicit ownership decision and validation evidence for retaining it as a separate module. |
| M-PARSER-02 | Profiler legacy-bridge counters | Frontend, Codegen / Runtime | `src/StyioProfiler/FrontendProfiler.hpp` and `src/StyioParser/Parser.hpp` expose `legacy_fallback_statements` / `nightly_internal_legacy_bridges` | M-PARSER-01 closes and the counters are removed from the public profiler surface. |
| M-SEMA-01 | Rejected retired AST forms | Sema / IR | `AstToStyioIRLowerer::toStyioIR(InfiniteAST*)` and `toStyioIR(ReadFileAST*)` explicitly reject retired forms; declarations remain in `src/StyioAST/AST.hpp` | Remove obsolete AST nodes when their remaining references are retired, or implement them only through an accepted feature decision. |
| M-RUNTIME-01 | Runtime implementation ownership | Codegen / Runtime | `src/StyioRuntime/` owns handles, error state, ready-queue and observation support; `src/StyioExtern/ExternLib.cpp` still implements multiple collection/string/file/task families | Move each remaining family behind its owned runtime interface or document why it stays in the C ABI implementation; verify lifetime and execution behavior for each slice. |
| M-RUNTIME-02 | Session lifetime migration boundary | Codegen / Runtime | `src/StyioSession/CompilationSession.hpp`, `SessionAllocation.hpp` and the C.1 shell comment in `src/main.cpp` retain the transitional session boundary | Close the owning runtime/session migration and update the transitional comments to the final ownership model. |
| M-CLI-01 | CLI orchestration extraction | CLI / Nano | `src/main.cpp` retains configuration, nano materialization/publishing, capability output and parser-comparison orchestration; current service owners live in `src/StyioServices/` | Move non-entry responsibilities to their existing owned services while preserving CLI contracts. Maintain the current ceiling enforced by `scripts/monolith-line-ratchet-gate.py`; line count alone is not the completion criterion. |

## Closure and Provenance

Remove a migration row once its closure condition is verified, and keep the
surviving behavior in the owning source contract, tests, and runbook. Do not
append a dated closed-items journal here. Previous migration details remain in
Git history; history/review/audit extraction records remain in the lifecycle
manifest.

The former maturity-checkpoint note is absorbed by the current gap ledger,
repository map and project principles. Its deletion does not close the
remaining compiler or research work.

## Cross-References

1. Active gap ownership lives in [`NEXT-STAGE-GAP-LEDGER.md`](./NEXT-STAGE-GAP-LEDGER.md).
2. Implemented decision provenance lives in [`../adr/IMPLEMENTED-DECISIONS.md`](../adr/IMPLEMENTED-DECISIONS.md).
3. Lifecycle rules for archive state live in [`../archive/README.md`](../archive/README.md).
