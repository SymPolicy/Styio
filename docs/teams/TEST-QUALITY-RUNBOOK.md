# Test Quality Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of feature tests, golden files, five-layer pipeline cases, security tests, fuzz smoke, parser shadow gates, and test documentation.

**Last updated:** 2026-05-26

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
2. Choose the smallest useful test layer: feature stdout, semantic failure, five-layer, C++ unit, security, fuzz, shadow gate, or soak.
3. Register every new automated test in CMake.
4. Update [../../workflows/TEST-CATALOG.md](../../workflows/TEST-CATALOG.md) when adding or changing acceptance tests.
5. Keep generated or temporary outputs out of the repository unless the test framework explicitly treats them as goldens.
6. Treat compile-plan negative-path coverage and machine-readable diagnostics as contract evidence, not optional smoke coverage.
7. When compile-plan artifacts grow, add assertions for receipt fields and auxiliary artifacts such as `runtime-events.jsonl`, not just exit codes.
8. Keep five-layer Layer 4 LLVM goldens semantic, not implementation-bound: when stdout lowering moves between legacy `printf/puts` and runtime helpers such as `styio_stdout_write_cstr`, or when LLVM stops printing unused `declare` lines and renumbers transient `%<n>` temporaries, update the pipeline canonicalization before touching large golden sets.
9. Treat workflow scheduler tests as gate-level regression coverage; changes to scheduler profiles, phase ordering, or registry validation must update `tests/workflow_scheduler_test.py`.
10. Treat `StyioTaskSchedulerPerf.SleepTasksRunConcurrently` as the task_resources task_runtime performance sentinel. It must compare against an in-process sequential baseline rather than a fixed absolute timeout so CI variance does not hide loss of concurrency.
11. When compiler handoff contracts grow, add or update regression coverage for both `--machine-info=json` and `--source-build-info=json` so `spio`-facing metadata cannot drift silently.
12. When language service contracts grow, add focused CLI regression coverage for the public entrypoint and its machine-readable fields; syntax-only services must prove that parser success is independent from later type-checking or execution.
13. When the compiler-side source-build helper changes, keep a lightweight regression on `scripts/source-build-minimal.sh --help` or an equivalent smoke path so the published helper entry does not silently rot.
14. When a coverage gap is marked closed, make the CTest registration, catalog entry, and exact passing command visible in the owning ledger or checkpoint document.
15. New syntax surfaces need focused lexer/parser coverage plus the smallest runtime smoke that proves any supported lowering path.
16. When standard-stream syntax changes, include both parser-only shorthand coverage and a runtime stdin/stdout smoke so symbolic declarations cannot parse while the executable path stays broken.
17. When generic/container function type annotations change, cover both parser-route acceptance and a lowering/codegen case for the smallest supported runtime family, so `list[T]` or `dict[K,V]` annotations cannot parse while call lowering regresses.
18. When a collection annotation adds contextual validation, pair the positive runtime smoke with a negative semantic test and an untyped-control case proving ordinary nested lists keep their prior behavior.
19. When control-flow spellings change, keep feature stdout goldens and security/codegen regressions together: `^...` must prove nearest-loop behavior, and nested `<| expr` returns must prove they exit the enclosing function.
20. When a syntax revision retires old retired syntax, delete the active `.styio` fixture and golden instead of marking it expected-red. Then remove the `TEST-CATALOG` row, add a revision note to the feature-test/design docs, and rerun the affected label plus `ctest -L language_feature`.
21. Native interop acceptance must include parser-only top-level guards and executable feature goldens that prove C/C++ source is compiled, linked, loaded, and called through the JIT.
22. When tests create custom AST nodes or compiler-stage visitors, use the split visitor signatures: `typeInfer(StyioSemaContext*)` and `toStyioIR(AstToStyioIRLowerer*)`.
23. Put C++ reference equivalence cases under `tests/algorithms/<case>/`; keep the C++ oracle, Styio program, and per-case random-input test driver in that directory, with only shared runner code under `tests/algorithms/.common/`.
24. When post-push CI reports five-layer typed-AST or diagnostic expectation drift, rebuild the local test binary before trusting a prior pass, reproduce the exact failing CTest filters, then update only the stale golden or stable diagnostic fragment.
25. Syntax aliases that assert canonical equivalence need both runtime equivalence and exact lowered or LLVM IR comparison where the backend contract is part of the statement; include at least one non-example-shaped case so optimizer coverage cannot be a one-off source rewrite.
26. Internal resource declarations need parser coverage for the prelude source file plus negative tests for undeclared local names and not-allowed hidden pseudo-primitives such as `file(path)`.
27. Task_resource syntax needs both positive stdout goldens and semantic negatives: cover `answer <- job`, `job -> answer -> @stdout`, `?| job -> answer: T`, `?| job -> answer: T | fallback`, string and numeric results, failed-task fallback recovery, no-fallback await fail-fast behavior, undeclared flow targets, double-pull rejection, non-task `?|` sources, and reserved bare continuation freeze fallback rejection in the same feature or focused runtime registration.
28. Profiler changes must keep `styio_profiler_frontend_smoke` on a task_using fixture and assert the JSON keys that prove scheduler counters and expanded phase names are wired, not just that a profile file exists. Native executable profiling changes must keep `styio_build_native_executable_stdin_echo` green and preserve opt-in `STYIO_NATIVE_PROFILE_OUT` behavior.
29. Expression-oriented statement semantics need one runtime smoke that covers function match sugar, a block final expression returning from a function, a match-arm final expression returning from a branch, and a statement-only tail returning the default value.
30. Match Sema changes need both executable and fail-closed evidence: keep a runtime smoke for branch-local final-expression tails, keep recursive function match sugar executable, and add security negatives for undefined arm tail values plus branch-local binding leakage in both ordinary match expressions and function match sugar.
31. Resource-topology safety tests live in `tests/resource_topology_test.cpp`. They must cover capability rejection, close-capable ownership, stream backpressure edges, hidden-ledger scope, and handle-table release/recycle before a resource lifecycle change is considered accepted.
32. state-resource retirement coverage keeps positive feature fixtures on Topology v2 syntax and preserves retired state-family spellings only as registered negative tests with stable migration diagnostics.
33. Native executable artifact coverage must build through `styio build <file_path> -o <artifact_name>`, assert the produced file is executable, and run the artifact against an existing golden so the test proves both artifact creation and runtime behavior.
34. Resource method tests must cover static method resolution, consuming receiver invalidation, transitive consuming method calls, final binding override rejection, property-as-method rejection, method arity rejection, repeated consuming call rejection, non-consuming overrides that must not lower to release, task outer-resource consume rejection, explicit `=>` ordering for exclusive borrows, and lowering evidence for file `write`/`close` methods before the topology model is considered regression-covered.
35. File resource flex-rebind lifecycle changes need both a topology/Sema pair proving use-after-close still fails without a new occupant and succeeds after accepted `name = @file(...)`, plus a runtime smoke that closes and reopens the same literal path so singleton-slot reopening is exercised.
36. README showcase examples that are wired into CTest must run repository-local Styio source from the repository root and compare stdout against a checked-in golden, so public examples cannot drift away from executable compiler behavior.
37. Semantic negative tests must assert a stable diagnostic fragment from `tests/features/<feature>/expected/*.err`; a nonzero exit code alone is not enough evidence.
38. Lit/FileCheck-style fixture trees belong under active `tests/` only when they are registered in CTest and have real check lines. Otherwise archive them until a live runner owns them.
39. LibFuzzer runtime probes must compile a minimal `LLVMFuzzerTestOneInput` entrypoint. Do not probe `-fsanitize=fuzzer` with a custom `main`, because the sanitizer runtime provides `main` and the check will fail for the wrong reason.
40. Fuzz targets that exercise tokenizer, parser, AST, or compiler session objects must run each input inside `CompilationSession` or an equivalent arena owner so sanitizer deep runs catch real memory bugs instead of expected session-lifetime allocations.
41. When replacing a placeholder with accepted behavior, pair the positive fixture with the smallest semantic negative that proves adjacent undefined syntax fails closed; for format strings this means a real `$"..."` stdio-output smoke plus a hash-tag iterator sequence diagnostic.
42. Nightly fuzz leak artifacts must become durable regression evidence. Add each minimized lexer/parser seed to `tests/fuzz/corpus/`, add the smallest security or ASan-targeted regression that exercises the same recovery path, preserve embedded NUL bytes in deterministic parser regression inputs when the artifact has them, and record any local libFuzzer toolchain blocker separately from the code fix.
43. Parser lifetime regressions that pass through typed annotations need both routes covered when the seed can reach them. Keep the raw fuzz corpus byte-for-byte, then add a deterministic security test that builds the same embedded-NUL input through `CompilationSession` so ASan/LSan validates parser recovery ownership without depending on the local libFuzzer runtime.
44. Parser DoS or OOM fuzz artifacts follow the same evidence path as lifetime bugs. Preserve the raw seed byte-for-byte, add a deterministic resource-limit regression, replay the artifact and the tracked corpus seed with the local libFuzzer target, and verify the relevant security tests under both default and ASan builds.
45. Parser timeout artifacts caused by nightly-to-legacy bridge loops need explicit closure evidence. Keep the minimized seed in `tests/fuzz/corpus/parser/`, add a deterministic security regression that exercises the same malformed nest through `CompilationSession`, replay the isolated artifact, and replay the full parser corpus so a single fixed seed does not hide another fallback loop nearby.
46. When fuzz minimizes a previously tracked parser timeout into a smaller bridge-loop seed, keep the derivative corpus file too. Add a second deterministic regression for the smaller shape and prove that both the original artifact and the minimized derivative replay cleanly, or bridge-budget fixes can look closed while a nearby cursor-starter path still times out.
47. Leak artifacts from parser fuzzing need session-backed regressions, not just direct parser helper calls. If the parser runs under `CompilationSession` arenas, reproduce the exact seed bytes through the session path, replay the isolated `leak-*` artifact with leak detection enabled, and keep the raw corpus seed so later timeout fixes do not leave an exception-path destructor leak behind.
48. Parser leak artifacts can migrate from nightly literal fallback into legacy block recovery. When a new `leak-*` seed walks through `parse_block_only`, `parse_main_block_legacy`, or shadow-mode legacy fallback, add a session-backed regression that covers every engine the fuzz target executes and keep the raw corpus seed so recovered statements still run their nested AST destructors.
49. Parameter-list leak artifacts need the same owner-path closure as block recovery. When a `leak-*` seed reaches `parse_params(...)`, keep the raw corpus seed, replay it under leak detection, and add a session-backed regression that covers every engine and caller family the fuzz target exercises, because hash functions, iterators, and resource methods can all abandon partially typed parameters before AST adoption.
50. Print and similar statement wrappers need their own leak regressions when the inner expression already built a heap-owning AST. If fuzz finds a `leak-*` seed where a malformed outer delimiter drops a completed call or nested expression from `parse_print(...)` or a nightly statement subset, keep the raw corpus seed and add a session-backed regression that covers both legacy and nightly engines.
51. Iterator hash-tag leak artifacts need the same treatment. If fuzz finds a `leak-*` seed where `parse_iterator_tail(...)` or the nightly iterator subset builds `HashTagNameAST` nodes before a later delimiter failure, keep the raw corpus seed, replay it under leak detection, and add a session-backed regression that covers both legacy and nightly engines.
52. Iterator and forward-clause leaks can migrate outward after the inner `#tag` ownership is fixed. When fuzz shows a later failure in legacy fallback or nightly subset recovery after an iterator was already constructed, preserve that second raw seed too, add a second session-backed regression, and rerun the isolated artifact plus the full parser corpus so the next owner boundary is proved closed.
53. Statement-prefix leaks need the same two-engine closure. If fuzz reaches `parse_stmt_or_expr_legacy(...)` or shadow-mode statement recovery and leaks a created `NameAST` or bind target before the right-hand side finishes parsing, keep that raw seed, add a session-backed regression that covers both engines, and replay the isolated artifact plus the tracked parser corpus so legacy entry points cannot silently rot behind nightly shadow green status.
54. `@resource` leak seeds need three-route confirmation even when the test loop only drives two engines. If fuzz leaks a `NameAST` built in `parse_resource_ref_after_at_latest(...)`, preserve the raw seed, add a session-backed regression that runs both legacy and nightly engines, and verify the replay stack closes legacy main-block parsing plus nightly subset and shadow recovery before treating the corpus backflow as complete.
55. Syntax-check diagnostics that add recoverable parser behavior or source context need focused CLI tests covering nonzero exit codes, multiple recovered `STYIO_PARSE_*` entries when applicable, and machine-readable code/phase/notes/caret/source fields for both parse and lex failures.
56. Native interop syntax tests must cover both compatibility and preferred binding forms. Keep legacy `@export { name }` plus `@extern(...) => ...` only in parser compatibility coverage; feature fixtures, artifact tests, and new examples must use explicit `# name[, other] := @ extern(...) { ... }` bindings for referenced C, referenced C++, inline C, mixed inline/reference execution, `styio build` artifact linking, and multi-symbol exposure from one native source file.
57. IR contract tests must call new node-level invariants through `StyioIR*` or another public base surface, not only through concrete classes. The `is_active()` and verifier contract needs focused coverage for SG, SC, and SIO domains (the former IOIR domain has been merged into `GenIR/SIOIR.hpp`), explicit `SGNoOp` lowering from no-op AST forms, inactive-node rejection, and codegen gate rejection before LLVM emission.
58. Placeholder-retirement coverage must protect both sides of the contract: accepted metadata paths should prove `SGNoOp` or real lowering, while unsupported value syntax should assert `StyioTypeError`. Pair focused `StyioIRContract` unit tests with `security`, `language_feature`, and `styio_pipeline` labels before marking an IM-D1 slice closed.
59. Value-carrying cast closure needs paired IR contract and codegen regression coverage: `TypeConvertAST -> SGCast` must preserve the value/type edge, and `SGCast` LLVM conversion must prove numeric promotion semantics instead of only asserting no throw.
60. Parser-authority coverage must protect IM-D2: public syntax-check tests must prove `legacy` and other non-authoritative engines are rejected, parser tests must prove unsupported syntax fails closed instead of using fallback, and IDE tests must prove malformed-source token snapshots do not publish recovered semantic facts.
61. Fuzz corpus recovery must preserve semantic seed content while removing incidental transport whitespace when it would fail repository whitespace checks. Treat this as seed hygiene, not a parser behavior change, unless the byte sequence itself is the minimized reproducer.
62. Nano/static repository negative-path coverage is compiler-side contract evidence, not package-manager UX. Keep marker contract rejection, malformed entry-schema rejection, malformed cloud manifest rejection, blob SHA256 mismatch, blob size mismatch, remote-publish rejection, create/publish mutual exclusion, missing nano mode selection, and create-only/publish-only option guards covered before extending static repository behavior; add the narrowest test that asserts the stable diagnostic text for each new guard.
63. Resource-effect parser/runtime changes need paired evidence: focused parser/security tests that prove non-task `?| resource_operation` forms stay out of task_await ASTs, catch-all fallback and named handlers parse through the resource-effect route, unknown or duplicate handler names fail closed, `effect => @()` remains invalid, and non-resource member calls are rejected when method-call syntax is admitted as a resource-operation candidate. Runtime evidence must cover the smallest behavior being changed: discard still commits the underlying topology write, fallback or a matched named handler runs only after a materialized resource failure is cleared, successful operations skip recovery, unmatched named handlers without catch-all fallback keep fail-fast behavior, handler chains can fall through to a final catch-all fallback, direct resource method calls such as `@file(...).close()` prove success/fallback/named-handler/no-fallback settlement, cleanup handlers catch only cleanup-family runtime subcodes, adjacent effect families do not miscatch, and no-fallback settlement stops before the following statement. Value-producing expression slices also need a positive success-value smoke, a fallback or named-handler recovery smoke, a no-fallback fail-fast smoke, and adjacent negatives for expression discard, statement-shaped operations in value context, fallback/handler type mismatch, and nearby unsupported operation families. For materialized container indexing, include plain `xs[i]`, `d[key]`, and `m[row][col]` JSONL fail-fast tests outside `?|`, `bounds` handler recovery under `?|` for `STYIO_RUNTIME_LIST_INDEX`, `STYIO_RUNTIME_DICT_KEY`, and `STYIO_RUNTIME_MATRIX_INDEX`, matrix row recovery when row reads are accepted, and a slice boundary negative until slice-shaped bounds recovery is implemented.
64. Plain resource-operation settlement changes need adjacent evidence outside the explicit `?|` wrapper. For file acquire/write/iterator guards, include JSONL CLI tests that prove the runtime diagnostic is emitted and a following statement is not executed, then rerun representative `StyioResourceEffects` fallback/handler tests to prove explicit recovery still owns `?| ... | fallback` behavior. File iterator EOF/error changes also need a normal singleton-read smoke, a stale alias after close regression, a direct zero-handle runtime helper test, and the affected five-layer LLVM golden cases so EOF is not confused with closed-handle failure.
65. Topology v2 selector changes need both positive and adjacent negative evidence. When `@name[-n..]` or `@name[...]` is accepted for a resource family, add a runtime fixture that proves the selector materializes a distinct typed slice/snapshot value instead of the latest scalar, plus fail-closed fixtures for the nearest unsupported boundaries such as depth beyond the declared history window or an unsupported bounded value family. Keep the state-resource catalog current when a new family such as bounded `bool`, `char`, or `string` is closed, and move the adjacent unsupported fixture to the next real boundary such as `matrix` rather than preserving an obsolete negative.
66. Explicit selector-copy changes need the same boundary evidence: prove `name << @resource[-n..]` or `name << @resource[...]` binds the materialized snapshot value, and add an adjacent negative for scalar latest reads such as `name << @resource[-1]` so copy semantics do not collapse into ordinary scalar binding or resource-write compatibility.
67. Stream zip source-combination changes need a positive fixture that proves the accepted source shape reaches runtime with the expected finite-barrier behavior and typed element families, plus an adjacent negative that keeps the next unsupported mixed source shape fail-closed. Run the affected stream-processing feature tests and the diagnostic regression before recording an IM-D5 closure.
68. Mixed file/list zip evidence needs both source orders when both are accepted: one parser-shadow-safe feature fixture for catalog coverage, one focused unit or pipeline test for the opposite order and shorter-input termination, and the stream-processing shadow gates so parser-route AST drift cannot masquerade as runtime coverage.
69. Resource-selector zip evidence must avoid conflating materialized selector snapshots with IM-D5 snapshot joins. Pair a parser-shadow-safe feature fixture for `@name[...]` / `@name[-n..]` finite zip with a negative scalar-selector case such as `@name[-1] >> ...`, and keep the catalog wording on materialized bounded selector snapshots.
70. Range literal expression-bound coverage needs three proofs in the same slice: an executable positive that materializes and prints a `list[i64]`, an adjacent non-integer-bound failure that stops in Sema, and a focused codegen regression that proves the dynamic range path emits the runtime list loop rather than falling back to constant literal lowering.
71. Function return fallback closure needs paired evidence: an executable positive for scalar and inferred return types, a CLI JSONL negative for unsupported tuple return annotations, and a security/lowering regression proving tuple return metadata fails closed before it can become an `i64` function type.
72. Materialized container clone coverage needs both sides of the IM-D4 copy rule: prove `name << list_or_dict_or_matrix` lowers to explicit clone IR and produces an independent runtime container after source mutation, and prove `name <- list_or_dict_or_matrix` fails closed so acquire/pull syntax does not remain a hidden clone compatibility path.
73. Stdin value-producing resource-effect coverage must prove all three settlement paths for the exact accepted family: successful untyped `?| (<- @stdin) | fallback` returns the parsed `i64`, invalid numeric input recovers through catch-all fallback or a matched `parse` handler after clearing `STYIO_RUNTIME_NUMERIC_PARSE`, explicit-target `f64` success/fallback/handler paths preserve f64 output, explicit-target `string` pulls clone the stdin line, explicit-target typed-list pulls prove success, fallback, and no-fallback `STYIO_RUNTIME_LIST_PARSE`, and unsupported targets such as `bool` fail closed in Sema before codegen.
74. Compile-plan malformed-input tests must assert the CLI exit code, `compile_plan_invalid` subcode, stderr/stdout diagnostic fragment, and `diag_dir/diagnostics.jsonl` fragment when the plan exposes an absolute diagnostics directory. Package-list consistency tests should prove only compiler-visible shape and entry-package invariants, not resolver or package-manager lifecycle policy.

## Change Classes

1. Small: new fixture for already accepted behavior, expected-output fix, or test naming cleanup. Run targeted test.
2. Medium: new feature-test area, five-layer case, security regression, parser shadow gate update, or compile-plan artifact assertion expansion. Update docs and run affected labels.
3. High: new test framework, changed oracle policy, fuzz corpus backflow, or checkpoint-health gate change. Use checkpoint workflow and add ADR if the gate becomes required.

## Required Gates

Common commands:

```bash
ctest --test-dir build/default -L language_feature
ctest --test-dir build/default -L styio_pipeline
ctest --test-dir build/default -L security
ctest --test-dir build/default -L resource_topology
ctest --test-dir build/default -R '^(StyioDiagnostics\.SyntaxCheckRejectsNonAuthoritativeParserEngine|StyioSemanticBridge\.RejectsMalformedInputWithoutRecovery|StyioSyntaxDrift\.CorpusMatchesApprovedEnvelope)$'
ctest --test-dir build/default -R '^parser_shadow_gate_'
ctest --test-dir build/default -L algorithm_equivalence
```

Fuzz smoke:

```bash
ctest --test-dir build/fuzz -L fuzz_smoke
```

`fuzz_smoke` 当前走独立 corpus-replay smoke binaries，而不是直接把 PR 门禁绑在 libFuzzer main 的启动行为上；真正的 libFuzzer 目标仍保留给手动/夜间深跑。

Docs and recovery:

```bash
python3 tests/workflow_scheduler_test.py
python3 scripts/team-docs-gate.py
python3 scripts/docs-audit.py
./scripts/checkpoint-health.sh --no-asan
```

`checkpoint-health.sh` is allowed to reconfigure the requested build dir; maintenance changes to that recovery path must preserve a clean build-dir handoff instead of leaking configure logs into later commands. The default local variant is `build/default/`; use `--build-dir build/<variant>` for another configured variant.
同一脚本在 normal leg 里必须显式构建 `styio_security_test` 后再跑 `ctest -L security`；空标签返回 0 不能算通过。
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
