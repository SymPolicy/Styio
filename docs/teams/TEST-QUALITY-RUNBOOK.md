# Test Quality Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of milestone tests, golden files, five-layer pipeline cases, security tests, fuzz smoke, parser shadow gates, and test documentation.

**Last updated:** 2026-07-31

## Mission

Own the evidence that Styio behavior is accepted, reproducible, and recoverable. This team protects CTest registration, fixture layout, golden oracles, C++ reference equivalence cases, fuzz/security coverage, and test catalog accuracy. It does not decide language semantics without the design SSOT.

## Owned Surface

Primary paths:

1. `tests/`
2. `tests/CMakeLists.txt`
3. `tests/fuzz/`
4. `tests/algorithms/`
5. `tests/security/`
6. `src/StyioTesting/`
7. [../../workflows/TEST-CATALOG.md](../../workflows/TEST-CATALOG.md)
8. [../../workflows/FIVE-LAYER-PIPELINE.md](../../workflows/FIVE-LAYER-PIPELINE.md)
9. [../../workflows/TEAM-RUNBOOK-MAINTENANCE-GATE.md](../../workflows/TEAM-RUNBOOK-MAINTENANCE-GATE.md)

## Daily Workflow

1. Identify the behavior owner before adding an oracle.
2. Choose the smallest useful test layer: milestone stdout, semantic failure, five-layer, C++ unit, security, fuzz, shadow gate, or soak.
3. Register every new automated test in CMake.
4. Update [../../workflows/TEST-CATALOG.md](../../workflows/TEST-CATALOG.md) when adding or changing acceptance tests.
5. Keep generated or temporary outputs out of the repository unless the test framework explicitly treats them as goldens.
6. Treat compile-plan negative-path coverage and machine-readable diagnostics as contract evidence, not optional smoke coverage.
7. When compile-plan artifacts grow, add assertions for receipt fields and auxiliary artifacts such as `runtime-events.jsonl`, not just exit codes.
8. Keep five-layer Layer 4 LLVM goldens semantic, not implementation-bound: when stdout lowering moves between legacy `printf/puts` and runtime helpers such as `styio_stdout_write_cstr`, or when LLVM stops printing unused `declare` lines and renumbers transient `%<n>` temporaries, update the pipeline canonicalization before touching large golden sets.
9. Treat workflow scheduler tests as gate-level regression coverage; changes to scheduler profiles, phase ordering, or registry validation must update `tests/workflow_scheduler_test.py`.
10. Treat `StyioTaskSchedulerPerf.SleepTasksRunConcurrently` as the M12 task_runtime performance sentinel. It must compare against an in-process sequential baseline rather than a fixed absolute timeout so CI variance does not hide loss of concurrency.
11. When compiler handoff contracts grow, add or update regression coverage for both `--machine-info=json` and `--source-build-info=json` so `pafio`-facing metadata cannot drift silently.
12. When the compiler-side source-build helper changes, keep a lightweight regression on `scripts/source-build-minimal.sh --help` or an equivalent smoke path so the published helper entry does not silently rot.
13. When a coverage gap is marked closed, make the CTest registration, catalog entry, and exact passing command visible in the owning ledger or checkpoint document.
14. New syntax surfaces need focused lexer/parser coverage plus the smallest runtime smoke that proves any supported lowering path.
15. When standard-stream syntax changes, include both parser-only shorthand coverage and a runtime stdin/stdout smoke so symbolic declarations cannot parse while the executable path stays broken.
16. When generic/container function type annotations change, cover both parser-route acceptance and a lowering/codegen case for the smallest supported runtime family, so `list[T]` or `dict[K,V]` annotations cannot parse while call lowering regresses.
17. When a collection annotation adds contextual validation, pair the positive runtime smoke with a negative semantic test and an untyped-control case proving ordinary nested lists keep their prior behavior.
18. When control-flow spellings change, keep milestone stdout goldens and security/codegen regressions together: `^...` must prove nearest-loop behavior, and nested `<| expr` returns must prove they exit the enclosing function.
19. When a syntax revision retires old milestone syntax, delete the active `.styio` fixture and golden instead of marking it expected-red. Then remove the `TEST-CATALOG` row, add a revision note to the milestone/design docs, and rerun the affected label plus `ctest -L golden_standard`.
20. Native interop acceptance must include parser-only top-level guards and executable milestone goldens that prove C/C++ source is compiled, linked, loaded, and called through the JIT.
21. When tests create custom AST nodes or compiler-stage visitors, use the split visitor signatures: `typeInfer(StyioSemaContext*)` and `toStyioIR(AstToStyioIRLowerer*)`.
22. Put C++ reference equivalence cases under `tests/algorithms/<case>/`; keep the C++ oracle, Styio program, and per-case random-input test driver in that directory, with only shared runner code under `tests/algorithms/.common/`.
23. When post-push CI reports five-layer typed-AST or diagnostic expectation drift, rebuild the local test binary before trusting a prior pass, reproduce the exact failing CTest filters, then update only the stale golden or stable diagnostic fragment.
24. Syntax aliases that assert canonical equivalence need both runtime equivalence and exact lowered or LLVM IR comparison where the backend contract is part of the statement; include at least one non-example-shaped case so optimizer coverage cannot be a one-off source rewrite.
25. Internal resource declarations need parser coverage for the prelude source file plus negative tests for undeclared local names and not-allowed hidden pseudo-primitives such as `file(path)`.
26. Task_resource syntax needs both positive stdout goldens and semantic negatives: cover `answer <- job`, `job -> answer -> @stdout`, string and numeric results, undeclared flow targets, and double-pull rejection in the same milestone registration.
27. Profiler changes must keep `styio_profiler_frontend_smoke` on a task_using fixture and assert the JSON keys that prove scheduler counters and expanded phase names are wired, not just that a profile file exists. Native executable profiling changes must keep `styio_build_native_executable_stdin_echo` green and preserve opt-in `STYIO_NATIVE_PROFILE_OUT` behavior.
28. Expression-oriented statement semantics need one runtime smoke that covers function match sugar, a block final expression returning from a function, a match-arm final expression returning from a branch, and a statement-only tail returning the default value.
29. Resource-topology safety tests live in `tests/resource_topology_test.cpp`. They must cover capability rejection, close-capable ownership, stream backpressure edges, hidden-ledger scope, and handle-table release/recycle before a resource lifecycle change is considered accepted.
30. M6 retirement coverage keeps positive milestone fixtures on Topology v2 syntax and preserves retired state-family spellings only as registered negative tests with stable migration diagnostics.
31. Native executable artifact coverage must build through `styio build <file_path> -o <artifact_name>`, assert the produced file is executable, and run the artifact against an existing golden so the test proves both artifact creation and runtime behavior.
32. Resource method tests must cover static method resolution, consuming receiver invalidation, transitive consuming method calls, final binding override rejection, property-as-method rejection, method arity rejection, repeated consuming call rejection, non-consuming overrides that must not lower to release, task outer-resource consume rejection, explicit `=>` ordering for exclusive borrows, and lowering evidence for file `write`/`close` methods before the topology model is considered regression-covered.
33. README showcase examples that are wired into CTest must run repository-local Styio source from the repository root and compare stdout against a checked-in golden, so public examples cannot drift away from executable compiler behavior.
34. Semantic negative tests must assert a stable diagnostic fragment from `tests/milestones/<milestone>/expected/*.err`; a nonzero exit code alone is not enough evidence.
35. Lit/FileCheck-style fixture trees belong under active `tests/` only when they are registered in CTest and have real check lines. Otherwise archive them until a live runner owns them.
36. LibFuzzer runtime probes must compile a minimal `LLVMFuzzerTestOneInput` entrypoint. Do not probe `-fsanitize=fuzzer` with a custom `main`, because the sanitizer runtime provides `main` and the check will fail for the wrong reason.
37. Windows-native CTest registration may replace broad GTest discovery with explicit focused slices when discovery requires host runtime details. Keep those slices labelled with their behavior family and add a matching non-Windows registration path.
38. LSP transport tests should launch the real `styio_lspd` binary and inspect raw bytes, not only call `Server::handle`, so stdio framing regressions are observable on every host.
39. When source directories move, update C++ test include paths to the current owned headers instead of adding compatibility shims or keeping obsolete short include roots alive.
40. Algorithm equivalence helper targets must keep both the repository root and `src/` on their private include path, because generated reference slices include `tests/...` helpers while the shared C++ harness includes owned implementation headers.
41. Algorithm equivalence helper targets must link the frontend core when they execute the compiler through shared platform process helpers; do not duplicate platform sources in test targets.
42. Continue-depth compatibility changes must update parser, clone/lowering, codegen, IDE tolerant tokenization, and security/lowering regression tests together so multi-character `>>...` spellings do not leave stale depth assumptions in fixtures.
43. Writable-resource iterable write changes must assert the lowered runtime shape, not just parse/typecheck success. Cover list/string-line and dict-value sources so tests can prove `>> @stdout` and `>> @file(...)` emit per-item pulse writes instead of collapsing the container through whole-value stringification.
44. Scalar file-write fixture changes must keep `-> @file(...)` for whole-value writes and reserve `>> @file(...)` for iterable pulse writes. Expression-match fixture branches must end in `<| expr` or an equivalent final value so AST, IR, and LLVM tests follow the current return contract.
45. Range syntax coverage must keep `[start..end]` as the positive materialized range source, assert that `[start..end]` parses as `RangeAST` rather than a one-element `ListAST`, and preserve `[start..end..step]` only as reserved-syntax negative coverage.
46. Windows-native tests that execute `styio.exe` through `_popen` must use `windows_popen_command_latest(...)` or an equivalent `cmd.exe` wrapper instead of POSIX single-quote commands. Compile-plan JSON fixtures must write Windows paths with `generic_string()` so early diagnostic-sink probing sees valid JSON.
47. Callable binding changes need paired parser and semantic evidence: parse `# name = #(args) => ...` and `# name := #(args) => ...`, reject direct resource RHS forms such as `# sink = @stdout`, execute mutable rebinding so the latest callable body wins, and assert final-binding redefinition failures. Do not treat anonymous function-value IR support as covered by these binding tests.
48. Every focused CTest command used as CI or checkpoint evidence must name a currently registered test or label and pass `--no-tests=error`; selecting zero tests is a gate failure, not a successful smoke result.
49. macOS coverage must select clang, clang++, llvm-cov, and llvm-profdata from one validated LLVM 18.1.x prefix and configure the SDK returned by `xcrun`. Do not mix AppleClang profile data with upstream LLVM coverage tools or hardcode a Homebrew installation path.
50. Native macOS acceptance must build and execute the platform internal test, native interop fixtures, bootstrap plan smoke, LSP framing smoke, and the selected compiler/service labels. A configuration-only result does not close platform adaptation.
51. Syntax-feature lifecycle tests must cover a ready projection, dependency-cycle rejection, downstream staleness after a prerequisite feature blocks, and rejection of delivery progress before language-owner acceptance. Each converged feature SSOT must name a checked-in golden case with its expected oracle.
52. Keyword-free lexical coverage must pair a tokenizer classification test for contextual word spellings with an executable source fixture that binds keyword-like names outside symbol-anchored contexts.
53. Inferred-callable coverage must execute independent scalar/string instances, expected-result inference for an empty container, self and mutual recursive SCCs, and paired negative fixtures for polymorphic recursion, underconstrained results, authored generic binders, and call-site type arguments. Keep every failure diagnostic in Sema or the authoritative parser so unresolved relations never become backend failures.
54. Effect-aware callable coverage must pair a successful single effectful instance with rejection of a conflicting second instance, prove canonical-row propagation through at least one direct-call edge, and cover a captured free environment. Keep the expected diagnostic anchored to the derived canonical effect row rather than incidental type-inference internals, and unit-test label sorting, deduplication, open-tail identity, and fail-closed `unknown`.
55. Callable-constraint coverage must execute integer and floating numeric instances, scalar and lexical string comparisons, list and dictionary indexing, and at least one transitive scheme edge. Pair each currently emitted constraint family with an unsatisfied-instance golden whose oracle names the canonical constraint and concrete rejected type.
56. Literal-defaulting coverage must pair canonical numeric execution with contextual empty list/dictionary acceptance and direct missing-context failures. Keep empty-collection tests non-defaulting, and prove that a relation fixed by an integer literal rejects a floating instance instead of widening silently.
57. Callable-value boundary coverage must retain successful direct named multi-instance calls and execute concrete plus contextually frozen inferred callable items through binding, passing, returning, and indirect invocation. Pair that evidence with separate failures for context-free stored, passed, returned, and task-captured schemes, plus signature mismatch, mutable callable slots, capturing items, non-callable right sides, and callable-containing list/dict/topology storage. Keep nested canonical-signature parsing covered and require every invalid case to fail in parser or Sema before backend lowering.
58. Capability-boundary coverage for inferred callables must execute scalar/list/dict instances plus local and transitive usage facts, then reject matrix, task, stream, file, topology-resource, nested sensitive-handle, and repeated noncopyable instances. Keep at least one topology resource case so a later normalization change cannot erase resource shape before validation. Negative oracles must distinguish copy, task transfer, resource state family, topology identity, and materialized shape; unit-test all four canonical usage labels including exclusive borrow.
59. Callable-interface coverage must publish dependencies in a temporary tree, execute downstream scalar/string/constrained specializations including a private concrete helper edge, and round-trip schema-v4 closed/open effect rows plus sorted per-variable usage requirements with transitive open-tail and usage edges. A dedicated portable-body label must prove replay after replacing imported source with invalid Styio text and updating only its source digest, then independently reject unknown opcodes, unbound symbols, mismatched encoded types, noncanonical payload bytes, stale semantic digests, and older portable schemas. The broader interface label must also reject private access, non-canonical module IDs, module cycles, missing interfaces, stale source/schema/dependency facts, and an exported body that fails without a local instance. Assert that emitted effect rows contain sorted `labels` plus nullable `open_tail`, usage requirements contain sorted variables and labels, and neither retains legacy bit/canonical fields. Negative cases must assert stable diagnostic fragments and may not leave generated `.styioi` artifacts in the repository.
60. Callable-specialization coverage must compare LLVM symbols across repeat compilation and source call-order changes, require exactly one local definition for every referenced full-digest mono symbol, prove unreachable inferred definitions emit nothing, and prove a reachable callee-body change invalidates its caller. Keep focused graph tests for deduplicated edges and configurable depth/growth ceilings, plus a language golden for the production recursion ceiling and concrete instance path.
61. Canonical literal-width migrations must update typed-AST parent/result oracles and stable diagnostics from legacy `int` names to `i64` without rewriting raw literal-token reprs. Contextual empty-collection coverage must include a typed declaration and a mutable rebind whose already established list/dict target type reaches the empty literal.
62. Whenever a frontend translation unit becomes reachable from `styio_nano`, keep both local-subset package creation paths as link-level regression tests. Header closure alone is insufficient evidence because an omitted `.cpp` compiles the bundle and then fails with unresolved symbols.
63. Affine-closure coverage must execute a shared program-static scalar through both direct and escaped invariant callable calls, execute repeated direct mutation through an exclusive capture, preserve native `bool`/`f64`/`char` widths, and prove that a same-spelled local in a noncapturing function cannot alias the capture slot. Pair it with stable failures for a missing free name, an unused declaration, duplicate capture syntax, exclusive escape, unsupported representation/drop storage, and consume mode. Run the same positive fixtures through full and nano compilers, and keep imported environments, resources, containers, heap allocation, and generalized closure values outside the proven slice.
64. Persistent callable-cache coverage must retain an unchanged cache-disabled execution, then prove cold per-specialization object writes and warm native hits with identical stdout. Use a fixture that combines an inter-specialization call and a private string constant. In one isolated temporary root, cover transitive callee-body invalidation and backend namespace separation; independently cover a truncated-entry miss/rewrite, simultaneous writers converging without temporary files, and deterministic age, aggregate-byte, and file-count pruning. Statistics assertions must require the path-free schema-v1 JSON and all four nonnegative timing fields. Invalid limits without an explicit cache root remain CLI errors, while cache I/O/corruption must never become a language or runtime failure.

## Change Classes

1. Small: new fixture for already accepted behavior, expected-output fix, or test naming cleanup. Run targeted test.
2. Medium: new milestone area, five-layer case, security regression, parser shadow gate update, or compile-plan artifact assertion expansion. Update docs and run affected labels.
3. High: new test framework, changed oracle policy, fuzz corpus backflow, or checkpoint-health gate change. Use checkpoint workflow and add ADR if the gate becomes required.

## Required Gates

Common commands:

```bash
ctest --test-dir build/default -L golden_standard --output-on-failure --no-tests=error
ctest --test-dir build/default -L styio_pipeline --output-on-failure --no-tests=error
ctest --test-dir build/default -L security --output-on-failure --no-tests=error
ctest --test-dir build/default -L resource_topology --output-on-failure --no-tests=error
ctest --test-dir build/default -R '^parser_shadow_gate_' --output-on-failure --no-tests=error
ctest --test-dir build/default -L algorithm_equivalence --output-on-failure --no-tests=error
```

Fuzz smoke:

```bash
ctest --test-dir build/fuzz -L fuzz_smoke --output-on-failure --no-tests=error
```

`fuzz_smoke` 当前走独立 corpus-replay smoke binaries，而不是直接把 PR 门禁绑在 libFuzzer main 的启动行为上；真正的 libFuzzer 目标仍保留给手动/夜间深跑。

Native macOS platform and coverage evidence:

```bash
LLVM_PREFIX="$(brew --prefix llvm@18)"
ctest --test-dir build/macos -R '^styio_platform_internal_test$|^native_interop_' --output-on-failure --no-tests=error
scripts/coverage-gate.sh --build-dir build/coverage-macos --llvm-prefix "$LLVM_PREFIX"
```

Docs and recovery:

```bash
python3 tests/workflow_scheduler_test.py
python3 tests/syntax_feature_state_gate_test.py
python3 scripts/syntax-feature-state-gate.py
python3 scripts/team-docs-gate.py
python3 scripts/docs-audit.py
./scripts/checkpoint-health.sh --no-asan
```

`checkpoint-health.sh` is allowed to reconfigure the requested build dir; maintenance changes to that recovery path must preserve a clean build-dir handoff instead of leaking configure logs into later commands. The default local variant is `build/default/`; use `--build-dir build/<variant>` for another configured variant.
同一脚本在 normal leg 里必须显式构建 `styio_security_test` 后再跑 `ctest -L security`；所有定向 CTest 都必须使用 `--no-tests=error`，空标签或空正则选择不能算通过。
离线恢复时，`tests/CMakeLists.txt` 和顶层 `CMakeLists.txt` 现在会优先复用本地已有的 `googletest` / `tree_sitter_runtime` source checkout，避免首次恢复因 FetchContent 远端不可达而卡死。

## Cross-Team Dependencies

1. Frontend must review parser, lexer, and shadow gate expectations.
2. Sema / IR must review AST, type, lowering, and repr goldens.
3. Codegen / Runtime must review LLVM, runtime, security, and soak expectations.
4. Perf / Stability must review benchmark or soak threshold changes.
5. Docs / Ecosystem must review test catalog and workflow documentation changes.

## Handoff / Recovery

Record unfinished quality work with:

1. Test name, label, and fixture path.
2. Input and oracle path.
3. Whether failure is expected-red or unexpected regression.
4. Owning implementation team.
5. Required team runbook when the team-docs gate fails.
6. Exact command that reproduces the failure.
