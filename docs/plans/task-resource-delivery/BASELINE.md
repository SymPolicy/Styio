# Current-source task/resource baseline

**Purpose:** Record current-source reproductions, input provenance and bounded coverage before repair.

**Last updated:** 2026-10-11

**Observed:** 2026-10-11  
**Owner:** NODE-BASELINE / TASK-001  
**Registered check:** `styio-task-resource-baseline`

This is engineering evidence for the existing programme, not candidate acceptance
or Task/Tree delivery. Confirmed defects below remain for NODE-REPAIR. No compiler,
runtime, test source, or public contract was changed by this baseline.

## Candidate inputs and build

- Revision: `09f8fbdf6af9e27e590def6731eaa660d595a36c`.
- Reused branch: `agent/downstream-styio-docs-20261005`.
- Starting tracked modifications: `docs/INDEX.md`, `docs/plan/INDEX.md`,
  `docs/plan/README.md`; `docs/plans/` was untracked. These inputs were preserved.
  No implementation source modifications were present before the build.
- Reused `build/default`: Debug, Unix Makefiles, Apple Clang 21.0.0,
  LLVM 18, CMake 4.3.1, Tree-sitter enabled, ICU disabled. The host was macOS
  arm64. This is an observed local configuration, not the standardized Debian
  toolchain or evidence for other platforms.
- Executed `bash docs/plans/task-resource-delivery/checks.sh baseline` once:
  exit 0, configuring current sources and building `styio`, `styio_lspd`,
  `styio_test`, and `styio_resource_topology_test`, then emitting machine-info.
  Executables are at `build/default/bin/<target>`. Build warnings remain in the
  private log; successful compilation does not establish warning-free code.
- Full input status, compiler/configuration details and actual command are in
  `build/task-resource-baseline-20261011/provenance.json`.

All artifact references below are relative to the repository. Raw logs, runtime
outputs, generated plans with local paths, and detailed provenance stay in the
ignored `build/task-resource-baseline-20261011/` directory. Reuse these results
only while relevant source, test, configuration and environment inputs remain
unchanged. The baseline made only the evidence/Node/README changes described here.

## Executed checks

| Command or selector | Actual outcome | Evidence under the private artifact directory |
| --- | --- | --- |
| `bash docs/plans/task-resource-delivery/checks.sh baseline` | Exit 0; four targets built and JSON handshake emitted | `baseline.log`, `baseline.exit`, `machine-info.json` |
| `python3 tests/run_styio_case.py run --styio build/default/bin/styio --source docs/plans/task-resource-delivery/fixtures/task-history.styio --cwd . --expect-stdout docs/plans/task-resource-delivery/fixtures/task-history.stdout` | Exit 1; actual parent/task `9 / 0`, expected `9 / 7` | `task-history.log`, `task-history.exit` |
| `build/default/bin/styio_test --gtest_filter='StyioDiagnostics.MachineInfo*:StyioDiagnostics.CompilePlan*:StyioResourceEffects.TaskAwaitFallbackRunsAfterFailedTaskPull:StyioResourceEffects.FailedTaskAwaitWithoutFallbackStopsAtSettlementSite'` | 32 passed, exit 0 | `c1-and-failure.log`, `c1-and-failure.xml` |
| `ctest --test-dir build/default --output-on-failure --no-tests=error -R '^(task_resources_\|state_resources_\|styio_lspd_stdio_framing$)'` (ERE alternatives use plain `|`) | 30 passed, exit 0: 11 task, 18 state, 1 LSP framing | `task-state-lsp.log`, `task-state-lsp.exit` |
| `build/default/bin/styio_test --gtest_filter='StyioResourceTopology.*'` | 7 passed, exit 0 | `resource-runtime.log`, `resource-runtime.xml` |
| Real CLI diagnostic programs and repeated compile-plan invocations below | Defect observations and limited successful probes; not acceptance passes | `language-probes.json`, `extra-language-probes.json`, individual `.styio`/`.stdout`/`.stderr`, `c1-sequence/` |

The GoogleTest commands additionally wrote the listed XML using
`--gtest_output=xml:<relative-artifact-path>`. A total of 69 existing focused tests
passed. The historical fixture failed independently. No focused registered check
for another Node, full regression, ecosystem integration, cost benchmark, native
installation, or live acceptance was run.

## Finite issue ledger

| ID | Classification and actual evidence | Source/contract and repair requirement |
| --- | --- | --- |
| B1 task history | Confirmed defect. The authoritative plan fixture returns `9 / 0` instead of `9 / 7`. `history-wrap.styio` captures history `[6,7]`, then the parent advances to `[9,10]`; captured sum is `0`, expected `13`. | D4 inventory lines 349–374 requires creation-time task snapshots. `src/StyioCodeGen/CodeGenIO.cpp:533` collects scalar captures, then clears ring metadata at line 655 without reconstructing captured history. Implement owned capture of committed history; preserve wraparound, alias identity, repeated reads, lifetime and failure cleanup. Promote the existing fixture to one permanent authority. |
| B2 nested commit | Confirmed ownership defect against the existing block snapshot rule. `nested-empty.styio` produces `7,7,9,9`: entering and leaving a child that writes nothing commits the parent's pending `9` before the parent exits. Under the existing parent snapshot rule, the parent's read after that empty child must still see `7`; final outside read sees `9`. | `src/StyioCodeGen/CodeGenG.cpp:2441` commits every pending ring in the current map; block/loop exits call it. D4 lines 353–374 assigns writes to their current context and commits each stage to its source context. Establish owner-specific pending writes and nested snapshots throughout the actual lowering/codegen path; do not replace this with a global flush. |
| B3 task order | Confirmed silent loss of an accepted edge. Both default scheduling and `STYIO_TASK_THREADS=1` execute `first` before `second` despite `second => first`; exit 0, output `1,2,3`. | `docs/design/syntax/features/task-resource-order.md:59` promises an explicit execution edge. `src/StyioLowering/AstToStyioIR.cpp:3829` lowers `ResourceOrderAST` to `SGNoOp`; Sema at `src/StyioSema/TypeInfer.cpp:8681` only visits operands. The linked golden case tests `job -> answer`, not `t1 => t2`. Implement/preserve the existing order contract or identify the exact supported diagnostic under D5; silent acceptance is incorrect. |
| B4 C1 current invocation | Confirmed stale output defect. Reusing one output directory: parse failure exits 3 but leaves the exact previous success receipt; repaired source exits 0 with a new receipt but old error diagnostics remain. Invalid intent and wrong outer object type exit 6 and also leave the old receipt. | `src/main.cpp:3906`, `3924`, `3950`, `6551` select an append-only diagnostic sink. Receipt publication at `7515` occurs only on success; earlier failures retain the previous receipt. Repair current-invocation output ownership without deleting user data; include success → compiler failure → repaired success and invalid-plan transitions in permanent CLI tests. Pafio/Vityo still own their consumer-side isolation. |
| C1 identity/capabilities | No defect reproduced in the observed full build. Machine handshake and four configuration-selection tests passed within the 32-test group. | `machine-info.json` advertises Styio 0.0.1/nightly/full, compile-plan/receipt/diagnostics v1, runtime-events v2; syntax-check and project-execution flags are true. These values are protocol identity, not proof of source provenance or unsupported capabilities. |
| C1 outer compile-plan validation | No malformed-plan acceptance reproduced by the existing focused cases or the wrong-type `toolchain: []` probe. Public exit 6 and `STYIO_SERVICE_COMPILE_PLAN_INVALID` were observed. Output freshness remains B4. | `src/StyioServices/StyioConfig/CompilePlanContract.cpp:725` validates a JSON object and required outer fields. Existing tests cover malformed JSON, producer, intent, profile, packages, entry/output fields and conflicts. The plan-version-marker test deliberately accepts marker 9; do not invent version rejection as a fix. |
| C1 f64 precision | Historical precision issue not reproduced. `16777217.25 - 16777216.0` prints `1.250000`; `(1.000000000000001 - 1.0) * 1000000000000000.0` prints `1.110223`, consistent with double precision and the existing six-decimal output. | `src/StyioCodeGen/CodeGenG.cpp:1403` uses `std::stod` and double `APFloat`. `f64-precision.styio` is diagnostic evidence, not a permanent precision regression test; REPAIR should preserve an appropriate permanent assertion. |
| Task failure settlement | No failure reproduced in the two existing real-CLI tests: fallback recovers and continues; no-fallback stops at await with the file-write runtime error. | `tests/styio_test.cpp:13565` and `13602`. These do not cover ownership/cleanup of newly captured histories and must not be stretched into that claim. |

The nested-write variant `nested-write-separated.styio` also shows the parent's
pending `9` entering history with the child's `11` at the child boundary. This
supports B2; it does not settle all nested-write visibility choices. REPAIR must
reconcile D4 block snapshots, the current `LogicalResourceWritesCommitAtPulseEnd`
test (`tests/styio_test.cpp:9302`), and mandatory read barriers before changing
behavior. In particular, that permanent test intentionally reads the previous
committed value inside a pulse. Changing all such reads to see pending writes
would change supported behavior and is not an approved repair.

There is no request for a new language decision from this baseline. If a proposed
repair cannot preserve those existing obligations, document the minimal conflict,
options and consequences before changing that dependent path. Multiwriter merge,
external-I/O rollback, cancellation and future syntax remain outside this node.

## Minimal programs

History: the sole current fixture is
`tests/features/state_resources/t13_task_history_capture.styio` with its adjacent
`expected/t13_task_history_capture.out`. The identical planning fixture used by
the historical command above was removed during review after byte comparison;
the command is retained as execution history, not a runnable current path.

Empty child exposes the pending-write ownership error:

```styio
@x : i64|..4|
[7] >> #(v) => { v -> @x }
[9] >> #(v) => {
  v -> @x
  >_(@x[-1])
  [0] >> #(unused) => { >_(@x[-1]) }
  >_(@x[-1])
}
>_(@x[-1])
```

Reverse creation-order edge exposes the dropped order instruction:

```styio
first = ||> { >_(1) <| 1 }
second = ||> { >_(2) <| 2 }
second => first
?| first -> a: i64
?| second -> b: i64
>_(a + b)
```

Run these with `build/default/bin/styio --error-format=jsonl --file <program>`.
The reverse-order probe also ran with `STYIO_TASK_THREADS=1`; its artifacts are
`order-one-worker.*`. This is a controlled scheduler input, not a timing test.

For B4, `c1-sequence/plan.json` is the generated private plan. It uses `intent=run`,
Pafio producer identity, one synthetic package, and the same output directories.
Compile `>_(51)`, then invalid source `>_(`, then `>_(52)` through
`build/default/bin/styio --compile-plan <plan>`: exits `0,3,0`. Preserved artifacts
are `clean-success.*`, `parse-failure.*`, `repaired-success.*` and
`source-transition-results.json`. The parse-failure receipt equals the immediately
preceding success receipt byte for byte; repaired-success diagnostics still include
`STYIO_PARSE_UNEXPECTED_TOKEN`. Earlier invalid-plan cases left two CLI diagnostics
in this deliberately reused directory as well.

Probe construction caveats are retained honestly: `nested-write.styio` without an
intervening statement was parsed as a resource selector continuation and rejected;
the corrected separated variant is the nested-write evidence. An initial
`>_(missing_name)` source probe exited 0 and printed `0`, so it was not used as
compiler-failure evidence; its intended diagnostic contract was not investigated
in this bounded baseline. The explicit parse error above supplies B4's failure.

## Supported-capability and platform preservation inventory

The source inventory in `capability-inventory.json` contains all 46 feature
contracts with their owner paths and declared test/evidence references: 44 are
labelled `converged`, while rank-2 callbacks and user-extensible callable constraints
are `not_started`. Those labels describe repository declarations, not observed
correctness (B3 is itself labelled converged). `ctest-inventory.json` records 780
configured tests via `ctest --show-only=json-v1`; it does not claim they ran.

| Existing surface and owner contracts | Existing coverage to preserve | Evidence/limit in this node |
| --- | --- | --- |
| Core syntax, scalar/container types, bindings, conditionals, match, iteration, imports; `docs/design/Styio-Language-Design.md` and `docs/design/syntax/features/core-*.md` | `tests/features/scalar_expressions`, `final_bindings`, `functions`, collection/control/import suites and parser/pipeline tests in `tests/CMakeLists.txt` | Inventory only, except the concrete f64 probe. No broad language-preservation pass claimed. |
| Callable inference, constraints, defaults, affine captures, effects, monomorphic values, recursive groups, generic interfaces, reachable/native specialization cache; corresponding `core-*` contracts | `tests/features/callable_*`, `tests/styio_test.cpp`, `tests/run_callable_specialization_case.py` | Not exercised by this baseline. Rank-2 and user-extensible constraints are future boundaries. |
| Resource declaration/write/selector/copy; bounded i64/f64/bool/char/string/list/dict/matrix history; D4 and `resource-*` contracts | 18 state-resource cases and 7 `StyioResourceTopology` runtime tests; `tests/resource_topology_test.cpp` for structural semantics | 25 existing runtime/golden cases passed. Structural test executable built but not run. Task capture and nested ownership are defective; alias/repeated-read/lifetime capture tests are missing. |
| File/stdin/stdout resources, methods, cleanup, typed effects, match handlers, fallback; D4 | `tests/features/file_resources`, `stdio_input`, `stdio_output`, `StyioResourceEffects.*` | Only the two task failure cases ran from the effects suite. No broad I/O/cleanup acceptance. |
| Tasks, await, group launch, captures, ordering; D5 and `task-*` contracts | 11 task golden/negative cases, task runtime/security and observation suites | 11 passed; existing scalar/string captures work in these cases. B1/B3 reveal missing history/order assertions. |
| Streams, pulse frames, range/container iteration and zip; D5 and stream/resource-zip contracts | `tests/features/stream_processing`, `StyioStreamZip.*`, resource topology/security tests | Inventory only beyond state/pulse tests above. Future pressure/merge implementation is not promoted to supported runtime capability. |
| Compiler CLI/machine-info, compile-plan, diagnostics, receipts, observation-off/v2; machine-contract matrix | `StyioDiagnostics.*`, `tests/main_contract_test.cpp`, observable runtime/static-snapshot suites | 30 C1 tests passed; repeated-output probes reveal B4. Observation correlation/cancellation/cooperative producers were not tested; machine-info explicitly lists unavailable runtime capabilities. |
| IDE syntax/semantic services and stdio LSP; `docs/external/for-ide/BUILD.md` | `styio_ide_test`, `styio_lspd_stdio_framing` | Daemon built; framing test passed. No UI, real Agent, or product E2E inspected. |
| Native executable/FFI, nano profiles/packages, security and algorithm equivalence; `docs/BUILD-AND-DEV-ENV.md`, `docs/teams/CLI-NANO-RUNBOOK.md` | native-interop/platform/internal tests, nano tests, security and algorithm-equivalence targets | Inventory only. Nano configured ON does not mean nano was built or tested here. |
| macOS, Linux and Windows source-build support; `docs/BUILD-AND-DEV-ENV.md:7` and platform CI definitions | macOS/Linux CI gates; Windows smoke/advisory lane; platform tests | Only this macOS arm64 Debug configuration was exercised. Linux, Windows, other architectures, ICU ON, Tree-sitter OFF, release and sanitizer configurations remain unverified. |

## Repair handoff and evidence boundaries

NODE-REPAIR can reuse this build directory and input record, incrementally build
its current sources, then fix B1–B4 and add permanent cases at their actual
production entrypoints. The current registered focused script stops at the failed
history fixture and otherwise omits C1 and new nested/order assertions; extend
its focused coverage without importing a full regression into this graph.

COST must compare the repaired, correct capture implementation with a correct
full-snapshot baseline; no cost result or optimization benefit is established here.
HANDOFF must review the final source and exact candidate provenance. Pafio/Vityo
integration, all-source reviews and same-candidate common E2E remain later
coordinator obligations. Repository FINAL/CLOSE definitions stay pending.

The evidence document contains synthetic results and relative artifact references.
It does not reproduce local paths, environment values, private endpoints or raw
runtime payloads. Private artifacts must not be copied into public sources or the
shared blackboard.
