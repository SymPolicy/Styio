# Restore nightly CI green and complete the docs/plan v3 migration

## Requirements

- pipeline-suite-green: The styio_pipeline label runs with zero failures after every previously failing case is repaired by moving forward, without deleting or weakening a test. — source: user-request, ctest evidence
- failure-verdicts-recorded: Each of the 23 recorded pipeline failures carries an explicit implementation-regression or stale-expectation verdict with its evidence source. — source: user-request, decision Q-003
- sanitizer-build-and-test-green: The ASan plus UBSan configuration links every test target that previously failed and its security label passes. — source: user-request, nightly-sanitizers evidence
- fuzz-crashes-cleared: Every crash input from the three failed nightly fuzz runs is reproduced, fixed, and preserved in the tracked corpus so the fuzz smoke label replays it. — source: user-request, nightly-fuzz evidence
- internal-test-library-boundary: Internal test binaries consume a declared library boundary instead of recompiling frontend sources into each test executable. — source: user-request, tests build evidence
- nightly-required-checks-enforced: The downstream nightly branch ruleset requires the named status checks with strict enforcement, readable back through the repository ruleset API. — source: user-request, ruleset evidence
- issue-tracking-enabled: The downstream repository has Issues enabled and carries one tracking issue each for the fuzz and sanitizer regressions. — source: user-request, repository settings evidence
- governance-owner-references-corrected: Public badges and the post-commit check specification name the actual downstream owner and the actual enforced checks. — source: user-request, README evidence
- upstream-promotion-handoff: Upstream promotion is either performed inside the authorized boundary or reported as authority-blocked with verified divergence facts and executable steps. — source: user-request, decision Q-002
- external-audit-policy-handoff: External policy and benchmark-asset changes that this delivery needs are reported as authority-blocked with the exact required content and executable steps. — source: user-request, external repository evidence
- migration-ledger-truth-restored: The migration ledger reports measured current values instead of stale counts and states its refresh date. — source: user-request, ledger evidence
- stale-audit-reports-closed: The four dated audit reports are closed through the documented four-step archive path and no longer exist in the current tree. — source: user-request, ledger row M-AUDIT-01
- main-entry-line-ratchet: A registered gate fails when the compiler entry file grows beyond its recorded ceiling. — source: user-request
- windows-smoke-job-present: A native Windows smoke job exists in CI, builds the compiler and test binaries, and runs the two Windows-only filtered tests. — source: user-request, tests build evidence
- coverage-report-job-present: A report-only coverage job exists in CI, publishes its report, and is excluded from required status checks. — source: user-request
- benchmark-linkage-job-present: A report-only benchmark job exists in CI, resolves the external benchmark checkout explicitly, and reports the exact missing-asset list when the external repository cannot satisfy the integration contract. — source: user-request, external repository evidence
- audit-normalization-fails-loudly: The released audit policy normalization step fails with the unmatched pattern instead of silently becoming a no-op. — source: user-request, workflow evidence
- hygiene-residue-detected: The hygiene gate detects forbidden build and profiling residue that ignore rules currently hide, and the local tree carries none. — source: user-request
- plan-workspace-v3-only: The plan workspace contains only the v3 structure, and every legacy artifact of the previous generation is gone. — source: user-request
- legacy-plan-references-retargeted: Every repository script, registry, gate, and document that referenced the legacy plan structure now references the v3 structure and passes its own gate. — source: user-request
- open-work-registered: Every unclosed item of the deleted plan is registered explicitly in the surviving ledger and plan index without being implemented. — source: user-request
- legacy-plan-artifacts-proven-removed: A one-time targeted command proves that no legacy plan artifact or reference remains, without leaving a permanent gate behind. — source: user-request

## Architecture
Summary: One delivery, five mutually parallel Tasks split by real write ownership rather than by problem number, because the compiler source tree, the CI workflow tree, the documentation and gate tree, the downstream repository settings, and the external repositories are five surfaces that never write each other. All causally coupled repairs stay inside one Task, and every Task exposes its own internal concurrency through a minimal Node DAG.
Notes:
- The compiler repairs are one Task, not four. The pipeline failures, the sanitizer link failure, the fuzz crashes, and the test build model all edit the same source tree and all need a compile of that tree; splitting them would let one Task break another Task's build through a shared header while claiming disjoint ownership.
- The three verified failure families are already distinguishable. Container and matrix resource-effect cases fail LLVM module verification with a non-dominating release call, which is an implementation regression. The five-layer golden cases differ at the LLVM IR layer where a runtime error probe replaced a direct return. The match diagnostic case fails because the emitted text changed to a broader integer-or-char rule that the expectation never followed, which is stale expectation.
- The sanitizer link failure is an archive-level layering cycle, not a missing file. The IR base header pulls the codegen visitor header, so the templated IR dispatch helper needs the complete codegen visitor type and every frontend translation unit that builds IR nodes emits a reference to codegen run-time type information whose only strong definition lives in the backend objects that depend back on the frontend library. A single-pass linker cannot close that cycle.
- GitHub administration does not own repository files. The downstream governance Task owns only the two public readme files and proves its branch-ruleset and issue outcomes with API read-back. The external-authority Task owns only its handoff document.
- The authority boundary, not the token, decides. Administration stays limited to the downstream repository. Upstream promotion, the released audit policy, and the external benchmark workload assets are recorded as authority-blocked with verified current state and executable steps, and the blocked terminal state does not hold any completable Task hostage.
- Generated documentation projections have exactly one owner. Only the migration Task writes generated indexes, the archive ledger, and the documentation statistics page, so no other Task can make them stale. The compiler Task keeps its verdict and fuzz records inside the tests tree it already owns, which removes the last documentation-generation race.
- The team documentation gate only requires that the mapped runbook appears in the same change set. The migration Task therefore refreshes every runbook this delivery touches, which satisfies the gate for the compiler Task's source changes without either Task writing the other's files.
- No Task performs staging, commit, branch, or push operations, because concurrent Tasks share one worktree and one index. Deletions use plain file removal and the documented lifecycle commands. Committing on the temporary branch, pushing to the downstream remote, and opening the pull request happen once after every Task is integrated.
- Remote observation is a full-regression concern. Task-level acceptance for new workflows is local and static, because GitHub Actions cannot run before the branch is pushed and the compiler-side repairs must land first for the Windows and pipeline jobs to be meaningful.
- Refactor completeness is proven once. The legacy plan generation is removed, its references retargeted, and its absence proven by a one-time command rather than a new permanent gate.

## Task: compiler-signal-green
Outcome: Every compiler-side red signal on nightly turns green in one forward-only repair: the pipeline label passes, the sanitizer configuration links and passes, the fuzz corpus replays without a crash, and internal test binaries stop recompiling frontend sources.
Scope in:
- Classify and repair all 23 recorded styio_pipeline failures, updating an expectation only after confirming the new behavior against the design authority.
- Remove the archive-level layering cycle that breaks the sanitizer link of the internal type-inference test.
- Replace source-direct compilation in the test build with a declared internal library boundary.
- Download, reproduce, and fix the crash inputs from the three failed nightly fuzz runs and preserve them in the tracked corpus.
- Record one verdict per failing test and one regression note per fuzz crash inside the tests tree.
Scope out:
- Reverting any feature delivered by the two suspect merges.
- Deleting, skipping, or relaxing a failing test to obtain a green result.
- Any syntax or semantic extension, including new diagnostics beyond the confirmed current contract.
- Splitting the compiler entry file or any other monolith refactor.
- Editing documentation, gate scripts, CI workflows, or repository settings.
Outputs:
- failure-verdict-record: Per-test verdict record for the recorded pipeline failures — artifact: tests/PIPELINE-FAILURE-VERDICTS.md — guarantee: Each repair is traceable to a verdict and an evidence source instead of an unexplained expectation change.
- layered-visitor-boundary: Frontend sources free of backend run-time type dependencies — artifact: src/StyioIR — guarantee: The frontend library no longer requires backend symbols, so single-pass linkers close the link.
- internal-test-library: Declared internal library boundary for test binaries — artifact: tests/CMakeLists.txt — guarantee: Internal tests link one compiled boundary instead of duplicating frontend translation units.
- fuzz-regression-corpus: Crash inputs and regression notes preserved in the tracked corpus — artifact: tests/fuzz — guarantee: Every fixed crash is replayed by the fuzz smoke label on every future run.
Owns:
- src
- tests
- cmake
- CMakeLists.txt
Exclusive:
- local default build directory build/default
- local sanitizer build directory build/asan-ubsan
- local fuzz build directory build/fuzz
Difficulty: complex
Workload: heavy
Verification: code
Risks:
- quality
- schema
- concurrency
- shared_resource
Nodes:
- reproduce-pipeline-failures: Reproduce the full failure set in the default build and capture each failure's exact diagnostic, golden diff, or verifier message.
- reproduce-sanitizer-link: Reconfigure and build the sanitizer directory with the nightly compiler and flag set, reproduce the undefined run-time type reference, and name the producing and consuming objects with a symbol listing.
- collect-fuzz-regressions: Download the regression packs of the three failed fuzz runs, configure and build the local fuzz targets, and replay every artifact until each crash is reproduced.
- inventory-test-build-model: Inventory every test target that compiles frontend sources directly, and record which sources each target actually needs.
- classify-pipeline-failures: Judge every failure as implementation regression or stale expectation by comparing the emitted behavior against the design authority and the two suspect merges. — after: reproduce-pipeline-failures
- repair-codegen-dominance-family: Fix the resource-effect and slice families whose emitted module fails verification because a release call does not dominate its allocation. — after: classify-pipeline-failures
- realign-confirmed-expectations: Update only the expectations whose current behavior was confirmed intended, including the broadened match scrutinee diagnostic and the five-layer golden lines. — after: classify-pipeline-failures
- rebuild-internal-test-boundary: Remove the frontend-to-backend header dependency where it is achievable without changing semantics, otherwise declare the true library boundary, and replace source-direct test compilation with that boundary. — after: reproduce-sanitizer-link, inventory-test-build-model
- repair-frontend-fuzz-crashes: Fix each reproduced lexer and parser crash at its root cause instead of guarding the fuzz entry point. — after: collect-fuzz-regressions
- preserve-fuzz-corpus: Add each fixed crash input to the tracked corpus and write its regression note from the existing template. — after: repair-frontend-fuzz-crashes
- record-failure-verdicts: Write the per-test verdict record with the verdict, root cause, and evidence source for every recorded failure. — after: repair-codegen-dominance-family, realign-confirmed-expectations
- integrate-compiler-signals: Rebuild the default, sanitizer, and fuzz directories and confirm the pipeline, security, and fuzz smoke labels together with the architecture layer gate. — after: record-failure-verdicts, rebuild-internal-test-boundary, preserve-fuzz-corpus
Requirements:
- pipeline-suite-green
- failure-verdicts-recorded
- sanitizer-build-and-test-green
- fuzz-crashes-cleared
- internal-test-library-boundary
Design:
approach:
- Diagnose before repairing. The verdict record is written from measured behavior, so a green result can never be reached by editing an expectation that nobody confirmed.
- Treat the non-dominating release call as one shared defect of the cleanup emission path rather than as fifteen separate test failures, and confirm the shared root cause before touching individual cases.
- Prefer removing the frontend dependency on backend run-time type information over teaching the linker to tolerate a cycle. Reject linker grouping flags and interface-multiplicity bumps, because they preserve the violation as a permanent compatibility layer.
- If the dependency cannot be removed without changing IR or codegen semantics, declare the real boundary in the library definitions, keep the architecture layer gate passing, and record the residual layering debt as an out-of-scope finding rather than widening this Task.
- Make the internal test boundary a compiled library that the test targets link, so the duplicated translation units disappear and the sanitizer link path matches the production link path.
- Use a platform-independent symbol oracle in addition to a successful link, because the failing linker behavior is single-pass and the local linker is not.
- Keep every fixed fuzz input in the tracked corpus so the existing fuzz smoke replay becomes the permanent guard, and add no new gate.
patterns:
- pattern_catalog: refactoring-guru-catalog-22-v1
- candidate: Visitor
- decision: reject
- pressure: The IR dispatch already uses double dispatch through a codegen visitor, and that visitor's complete type leaks into a frontend header, which creates the archive cycle behind the sanitizer failure.
- expected_benefit: none for a redesign; the existing double dispatch is correct and a reshaped visitor would not remove the cycle by itself.
- simpler_alternative: Keep the visitor and cut the header dependency to a forward declaration, or declare the real library boundary. Either is sufficient and far smaller.
- application: Limit the change to the declaration seam and the library definitions; do not restructure element or visitor roles.
- costs_and_rejections: Reshaping the visitor would touch every IR node class and every codegen entry point during a red-to-green repair, so it is rejected together with adjacent Adapter and Bridge layers over the same seam.
Acceptance:
- Given: The default build directory is configured from the delivery branch — When: The pipeline label runs after a full build — Then: No pipeline test fails and none was removed or relaxed — Oracle: ctest --test-dir build/default -L styio_pipeline --output-on-failure --no-tests=error exits zero and reports 352 tests passing — Evidence: command: ctest pipeline label — Covers: pipeline-suite-green
- Given: The verdict record exists — When: The record is inspected — Then: Every recorded failing test carries one verdict and one evidence source — Oracle: rg -c "^- \[x\] " tests/PIPELINE-FAILURE-VERDICTS.md reports 23, and every entry names one failing test with the verdict implementation-regression or stale-expectation plus its evidence source — Evidence: command: verdict record count — Covers: failure-verdicts-recorded, failure-verdict-record
- Given: The sanitizer directory is configured with the nightly sanitizer flags — When: The previously failing internal test target is built and the security label runs — Then: The link succeeds and the label passes — Oracle: cmake --build build/asan-ubsan --target styio styio_test styio_security_test styio_typeinfer_internal_test exits zero, then ctest --test-dir build/asan-ubsan -L security --output-on-failure --no-tests=error exits zero — Evidence: command: sanitizer build and security label — Covers: sanitizer-build-and-test-green, layered-visitor-boundary
- Given: The test build no longer compiles frontend sources directly — When: The build model and the layer gate are inspected — Then: The direct-source list is gone and the layer gate still passes — Oracle: rg -n "STYIO_INTERNAL_PARSER_SUPPORT_SOURCES" tests/CMakeLists.txt finds no match and python3 scripts/architecture-layer-gate.py exits zero — Evidence: command: build model and layer gate — Covers: internal-test-library-boundary, internal-test-library
- Given: The fuzz directory is configured with fuzzing enabled and the crash inputs are in the tracked corpus — When: The fuzz smoke label replays the corpus — Then: No target crashes and every downloaded artifact is represented — Oracle: cmake --build build/fuzz --target styio_fuzz_lexer styio_fuzz_parser exits zero, then ctest --test-dir build/fuzz -L fuzz_smoke --output-on-failure --no-tests=error exits zero — Evidence: command: fuzz smoke replay — Covers: fuzz-crashes-cleared, fuzz-regression-corpus
Regression:
Commands:
- cmake --build build/default --target styio styio_test styio_security_test
- ctest --test-dir build/default -L styio_pipeline --output-on-failure --no-tests=error
- ctest --test-dir build/default -L security --output-on-failure --no-tests=error
- ctest --test-dir build/default -L golden_standard --output-on-failure --no-tests=error
- ctest --test-dir build/fuzz -L fuzz_smoke --output-on-failure --no-tests=error
- python3 scripts/architecture-layer-gate.py
- python3 scripts/runtime-surface-gate.py
Paths:
- src
- tests
- cmake
- CMakeLists.txt

## Task: ci-workflow-signals
Outcome: The workflow tree gains the three missing observability jobs and loses its silent external-policy patch, so native Windows, coverage, and benchmark signals exist and the always-on audit workflow can no longer normalize nothing without saying so.
Scope in:
- Add a native Windows smoke job that configures, builds, and runs the two Windows-only filtered tests using the documented native Windows recipe.
- Add a report-only coverage job that publishes its report and stays out of required checks.
- Add a report-only benchmark job that resolves the external benchmark checkout explicitly and reports the exact missing-asset list.
- Make the released audit policy normalization assert each declared replacement and fail with the unmatched pattern.
Scope out:
- A full Windows matrix or Windows-specific source and build-system changes.
- Enforcing any coverage threshold in CI.
- Publishing or editing anything in an external repository.
- Adding a new job to the required status checks.
- Editing gate scripts, documentation, or repository settings.
Outputs:
- windows-smoke-workflow: Native Windows smoke job — artifact: .github/workflows/windows-smoke.yml — guarantee: The Windows-only build and test branches stop being unreachable code and produce an observable remote result.
- coverage-report-workflow: Report-only coverage job — artifact: .github/workflows/nightly-coverage.yml — guarantee: Coverage is measured and published without becoming a merge gate.
- benchmark-linkage-workflow: Report-only external benchmark linkage job — artifact: .github/workflows/nightly-benchmark.yml — guarantee: The external benchmark integration seam is exercised and its missing assets are named instead of silently unused.
- audit-normalization-guard: Fail-loud normalization of the released audit policy — artifact: .github/workflows/styio-audit.yml — guarantee: Policy drift in the external repository surfaces as an explicit failure instead of a vacuous audit scope.
Owns:
- .github/workflows
Exclusive:
- repository workflow definitions
Difficulty: complex
Workload: medium
Verification: code
Risks:
- operations
- observability
- release
Requirements:
- windows-smoke-job-present
- coverage-report-job-present
- benchmark-linkage-job-present
- audit-normalization-fails-loudly
Nodes:
- inventory-workflow-contracts: Record every current trigger, job name, check name, and reusable step so new jobs match the existing contract and no existing check name changes.
- add-windows-smoke-job: Add the Windows smoke workflow that installs a Windows LLVM development package per the repository build guide, configures, builds the compiler and test binaries, and runs the two Windows-only filtered tests. — after: inventory-workflow-contracts
- add-coverage-report-job: Add the scheduled report-only coverage workflow that runs the existing coverage gate script, uploads the report, and never fails the branch. — after: inventory-workflow-contracts
- add-benchmark-linkage-job: Add the scheduled report-only benchmark workflow that checks out the external benchmark repository at its default branch, reports which required integration assets are present or missing, then attempts the required-mode configure and probe build. — after: inventory-workflow-contracts
- harden-audit-normalization: Replace the silent policy rewrite with one that asserts each declared replacement applied and fails with the exact unmatched pattern. — after: inventory-workflow-contracts
- validate-workflow-definitions: Parse every workflow definition and assert the new job identities, runner images, report-only settings, and preserved check names. — after: add-windows-smoke-job, add-coverage-report-job, add-benchmark-linkage-job, harden-audit-normalization
Design:
approach:
- Put the expensive coverage and benchmark work on a schedule with manual dispatch, matching how the sanitizer and fuzz workflows already isolate long jobs, so pull-request feedback stays fast.
- Trigger the Windows smoke job on push so the branch produces a remote result before merge, and keep it out of required checks until it has a green history.
- Keep report-only literally report-only: continue on error and upload the artifact, rather than lowering a threshold to zero and losing the signal.
- Reuse the existing coverage gate script and the existing external benchmark integration contract instead of writing new measurement logic in YAML.
- Verify the external benchmark asset list in the job itself, because the external default branch currently publishes only part of the contract and a bare configure failure would hide which assets are missing.
- Make the audit normalization fail loudly rather than tolerant, because a silent no-op leaves the audit scoped over paths that no longer exist and turns a required check into a vacuous one.
- Do not touch existing job names. They are the identities the branch ruleset will require.
Acceptance:
- Given: The workflow tree contains the new definitions — When: Every workflow definition is parsed — Then: All definitions load and the new jobs declare their intended runners and report-only behavior — Oracle: python3 -c "import glob,yaml;[yaml.safe_load(open(p)) for p in glob.glob('.github/workflows/*.yml')]" exits zero, rg -n "windows-2022" .github/workflows/windows-smoke.yml matches, and rg -n "continue-on-error" .github/workflows/nightly-coverage.yml .github/workflows/nightly-benchmark.yml matches both files — Evidence: command: workflow definition validation — Covers: windows-smoke-job-present, coverage-report-job-present, windows-smoke-workflow, coverage-report-workflow
- Given: The Windows smoke job exists — When: Its test step is inspected — Then: It runs exactly the two Windows-only filtered tests already defined by the test build — Oracle: rg -n "styio_test_windows_five_layer_pipeline" .github/workflows/windows-smoke.yml and rg -n "ClassifiersCoverFallbackAndPhaseFamilies" .github/workflows/windows-smoke.yml both match — Evidence: command: Windows job test selection — Covers: windows-smoke-job-present
- Given: The benchmark job exists — When: Its asset precondition step is inspected — Then: It names the required integration assets and reports the missing ones before configuring — Oracle: rg -n "styio-benchmark" .github/workflows/nightly-benchmark.yml matches and rg -n "STYIO_REQUIRE_EXTERNAL_BENCHMARK" .github/workflows/nightly-benchmark.yml matches — Evidence: command: benchmark job contract — Covers: benchmark-linkage-job-present, benchmark-linkage-workflow
- Given: The audit workflow normalizes the released policy — When: A declared replacement does not apply — Then: The step fails and prints the unmatched pattern — Oracle: rg -n "unmatched" .github/workflows/styio-audit.yml matches and the normalization step raises on a non-applied replacement instead of writing the file unchanged — Evidence: command: audit normalization guard — Covers: audit-normalization-fails-loudly, audit-normalization-guard
Regression:
Commands:
- python3 -c "import glob,yaml;[yaml.safe_load(open(p)) for p in glob.glob('.github/workflows/*.yml')]"
- rg -n "name: platform-adaptation / linux-ci-gate|name: platform-adaptation / macos-ci-gate|name: test / smoke|name: test / golden-standard|name: styio-audit" .github/workflows
Paths:
- .github/workflows

## Task: plan-workspace-and-gate-migration
Outcome: The plan workspace exists only in its v3 form with every legacy artifact removed, every dependent script, registry, gate, and document retargeted, the migration ledger and audit archive back to measured truth, a line ratchet on the compiler entry file, hygiene residue detected and cleared, and unclosed legacy work registered rather than implemented.
Scope in:
- Replace the legacy plan generation with the v3 workspace and delete the legacy capability catalog, roadmap document, and roadmap tree.
- Retarget every repository reference to the legacy plan structure, including the repo-local plan validator, the tool registry, the runbooks, and the documents that link into the deleted tree.
- Exempt the generated plan workspace from the document metadata rule that its generated files cannot satisfy.
- Refresh the migration ledger with measured values and complete the four-step closure of the four dated audit reports, extending the lifecycle tool with the audit family it currently lacks.
- Add a registered line ratchet gate for the compiler entry file and wire it into the existing scheduler profiles so it runs without touching CI definitions.
- Detect forbidden ignored build and profiling residue in the hygiene gate and clear the residue that exists locally.
- Register every unclosed item of the deleted plan in the surviving gap ledger and the plan index.
- Correct the post-commit check specification owner reference and document the checks the downstream ruleset enforces.
- Refresh the runbooks and generated documentation projections this delivery requires.
Scope out:
- Implementing any registered backlog item.
- Restoring or preserving any legacy plan artifact for compatibility.
- Adding a permanent gate for legacy-artifact absence.
- Editing compiler sources, test sources, CI workflow definitions, the public readme files, or repository settings.
Outputs:
- v3-plan-workspace: Plan workspace in v3 form only — artifact: docs/plan — guarantee: One current plan structure remains, with no legacy generation to interpret or maintain.
- refreshed-migration-ledger: Migration ledger with measured values — artifact: docs/rollups/MIGRATION-LEDGER.md — guarantee: Ledger rows can be trusted as current evidence rather than stale claims.
- audit-archive-closure: Closure rows for the four dated audit reports — artifact: docs/archive/ARCHIVE-MANIFEST.json — guarantee: Removed audit reports keep provenance without retaining the reports.
- monolith-line-ratchet: Registered line ratchet gate for the compiler entry file — artifact: scripts/monolith-line-ratchet-gate.py — guarantee: The entry file cannot grow further while its split remains future work.
- v3-workspace-validator: Repo-local plan workspace validator for the v3 structure — artifact: scripts/manifest_tool.py — guarantee: The registered repo-local gate validates the structure that actually exists.
- hygiene-residue-detection: Hygiene detection for forbidden ignored artifacts — artifact: scripts/repo-hygiene-gate.py — guarantee: Residue hidden by ignore rules is reported instead of accumulating.
- open-work-register: Explicit register of unclosed legacy work — artifact: docs/rollups/NEXT-STAGE-GAP-LEDGER.md — guarantee: Nothing the deleted plan left open is lost, and nothing is silently implemented.
Owns:
- docs/plan
- docs/rollups
- docs/teams
- docs/audit
- docs/archive
- docs/history
- docs/specs
- docs/INDEX.md
- docs/README.md
- docs/design/README.md
- docs/external/README.md
- docs/external/for-pafio/README.md
- scripts
- workflows
- .gitignore
Exclusive:
- generated documentation index projections
- documentation lifecycle archive state
- repo-local tool and workflow registries
Difficulty: complex
Workload: heavy
Verification: code
Risks:
- migration
- removal
- schema
- operations
- quality
Nodes:
- inventory-legacy-plan-surface: List every legacy plan artifact, every document that links into it, and every unclosed item the deleted plan still owns.
- inventory-gate-coupling: List every script, registry, profile, and gate that reads the plan workspace or constrains new files, including the document metadata rule that the generated plan files cannot satisfy and the registration rules for a new script.
- verify-audit-findings-closed: Verify each finding of the four dated audit reports against the implemented-decision record and its matching inventory, and record which evidence closes it.
- measure-ledger-drift: Measure the current values the ledger misreports, including the compiler entry file length, and record the measured baseline for the ratchet.
- write-v3-plan-workspace: Write the v3 plan workspace, including the index and boundary documents that describe the current structure and its state model. — after: inventory-legacy-plan-surface
- remove-legacy-plan-artifacts: Delete the legacy capability catalog, roadmap document, and roadmap tree. — after: write-v3-plan-workspace
- register-open-work: Register every unclosed legacy item in the surviving gap ledger and the plan index, without implementing any of them. — after: inventory-legacy-plan-surface
- retarget-legacy-references: Retarget every remaining reference to the legacy structure in documents, runbook tool lists, and registries. — after: inventory-legacy-plan-surface, inventory-gate-coupling
- replace-plan-workspace-validator: Replace the stale array-manifest validator with a v3 workspace structure validator at the registered path, and refresh its registry entry. — after: inventory-gate-coupling
- exempt-generated-plan-workspace: Exempt the generated plan workspace files from the document metadata rule and from the runbook trigger set, so the framework's own generated documents cannot fail the documentation audit. — after: inventory-gate-coupling
- close-audit-archive: Extend the lifecycle tool with the audit family, record the closure rows, regenerate the archive ledger, and remove the four reports from the current tree. — after: verify-audit-findings-closed, inventory-gate-coupling
- refresh-migration-ledger: Rewrite the drifted ledger rows with measured values and record the refresh date. — after: measure-ledger-drift
- add-line-ratchet-gate: Add the line ratchet gate at the measured ceiling, register it in the tool registry, and add it to the scheduler profiles that already run in CI. — after: measure-ledger-drift
- harden-hygiene-residue: Add detection for forbidden ignored artifacts to the hygiene gate without changing its existing modes. — after: inventory-gate-coupling
- clear-local-residue: Remove the stray configured build root, profiling output, and test-discovery residue from the local tree.
- update-team-runbooks: Refresh every runbook this delivery touches and the documentation statistics page. — after: remove-legacy-plan-artifacts, register-open-work, retarget-legacy-references, replace-plan-workspace-validator, exempt-generated-plan-workspace, close-audit-archive, refresh-migration-ledger, add-line-ratchet-gate, harden-hygiene-residue
- regenerate-doc-projections: Regenerate the documentation indexes, validate the lifecycle state, and confirm the documentation audit, team gate, and registry gates. — after: update-team-runbooks
- prove-legacy-removal: Run the one-time targeted search that proves no legacy plan artifact or reference remains, and record its result without adding a permanent gate. — after: regenerate-doc-projections
- verify-hygiene-state: Confirm the hardened hygiene gate reports a clean tracked tree and no remaining residue. — after: harden-hygiene-residue, clear-local-residue
Requirements:
- migration-ledger-truth-restored
- stale-audit-reports-closed
- main-entry-line-ratchet
- hygiene-residue-detected
- plan-workspace-v3-only
- legacy-plan-references-retargeted
- open-work-registered
- legacy-plan-artifacts-proven-removed
Design:
approach:
- Treat the migration as one complete transition. Remove the legacy generation, retarget its consumers, and prove absence once, rather than leaving a compatibility reader for the old array manifest.
- Keep the registered validator path and replace its content, because deleting a registered gate would force a registry removal and leave the repository with no local structural check at all.
- Validate only current v3 invariants in that validator. Legacy-absence belongs to the one-time proof, not to a permanent gate.
- Extend the lifecycle tool rather than hand-editing the archive manifest, because validation rejects families the tool does not know and the archive policy retains no copies.
- Reach CI enforcement for the line ratchet through the existing scheduler profile that CI already runs, so the ratchet needs no workflow edit and no ownership overlap.
- Register the new gate in both the tool registry and the scheduler registry in the same change, because each has its own completeness check.
- Detect residue that ignore rules hide as an additional hygiene mode rather than by changing the existing tracked and push modes that CI depends on.
- Register unclosed legacy work in the ledger that already carries the next-stage queue instead of creating a second competing backlog document.
- Exempt the generated plan workspace explicitly, because its plan projection is generated from the plan data and cannot carry repository document metadata by hand.
- Keep the workspace lock and transient session state untracked while tracking the plan and design documents themselves.
- Update the runbooks this delivery touches in the same change set, which satisfies the team gate for the other Tasks' owned surfaces without writing their files.
Acceptance:
- Given: The migration is complete — When: The one-time proof command runs over the tracked tree — Then: No legacy plan artifact and no reference to one remains — Oracle: git ls-files docs/plan lists no legacy capability catalog, roadmap document, or roadmap directory entry, and rg -n "Capabilities.json|Styio-Project-Roadmap|docs/plan/roadmap" over tracked files finds no match outside deleted history — Evidence: command: one-time legacy removal proof — Covers: plan-workspace-v3-only, legacy-plan-artifacts-proven-removed, v3-plan-workspace
- Given: Every dependent script, registry, and document is retargeted — When: The documentation and registry gates run — Then: All of them pass against the v3 structure — Oracle: python3 scripts/docs-index.py --check, python3 scripts/docs-audit.py, python3 scripts/docs-lifecycle.py validate, python3 scripts/team-docs-gate.py, python3 scripts/tool-skill-registry-gate.py, python3 scripts/workflow-scheduler.py check, and python3 scripts/manifest_tool.py validate docs/plan all exit zero — Evidence: command: documentation and registry gate set — Covers: legacy-plan-references-retargeted, v3-workspace-validator
- Given: The ledger has been refreshed and the audit reports closed — When: The ledger and archive state are inspected — Then: The ledger reports measured values with a current refresh date and the four dated reports are gone with provenance recorded — Oracle: rg -n "src/main.cpp" docs/rollups/MIGRATION-LEDGER.md reports the measured current length, the four dated audit report paths no longer exist, and python3 scripts/docs-lifecycle.py validate exits zero with their closure rows present — Evidence: command: ledger and archive verification — Covers: migration-ledger-truth-restored, stale-audit-reports-closed, refreshed-migration-ledger, audit-archive-closure
- Given: The line ratchet gate is registered — When: The gate runs and the entry file is inspected — Then: The gate passes at the recorded ceiling and would fail above it — Oracle: python3 scripts/monolith-line-ratchet-gate.py exits zero, the recorded ceiling equals the measured current length, and rg -n "monolith-line-ratchet" workflows/TOOL-SKILL-REGISTRY-GATE.toml scripts/workflow-scheduler.py matches both files — Evidence: command: line ratchet gate — Covers: main-entry-line-ratchet, monolith-line-ratchet
- Given: The hygiene gate detects ignored residue and the local tree was cleared — When: The hygiene modes run — Then: The tracked tree is clean and no forbidden residue remains — Oracle: python3 scripts/repo-hygiene-gate.py --mode tracked exits zero and the new residue mode exits zero on the cleared tree while reporting a seeded forbidden artifact — Evidence: command: hygiene gate modes — Covers: hygiene-residue-detected, hygiene-residue-detection
- Given: The deleted plan had unclosed work — When: The surviving ledger and plan index are inspected — Then: Every unclosed item is registered and none is implemented — Oracle: rg -n "M7|Topology v2|W1|W8|callable" docs/rollups/NEXT-STAGE-GAP-LEDGER.md matches every registered item and the delivery changes no compiler source — Evidence: command: open work register — Covers: open-work-registered, open-work-register
Regression:
Commands:
- python3 scripts/manifest_tool.py validate docs/plan
- python3 scripts/docs-index.py --write
- python3 scripts/docs-audit.py
- python3 scripts/docs-lifecycle.py validate
- python3 scripts/team-docs-gate.py
- python3 scripts/tool-skill-registry-gate.py
- python3 scripts/workflow-scheduler.py check
- python3 scripts/repo-hygiene-gate.py --mode tracked
- python3 scripts/local-info-leak-gate.py --mode worktree
- python3 scripts/monolith-line-ratchet-gate.py
Paths:
- docs/plan
- docs/rollups
- docs/teams
- docs/audit
- docs/archive
- docs/specs
- scripts
- workflows

## Task: downstream-governance-alignment
Outcome: The downstream repository enforces the named status checks on its protected integration branch, accepts issues and carries the two regression tracking issues, and its public badges point at the repository that actually runs the checks.
Scope in:
- Add a required status check rule with strict enforcement to the existing branch ruleset for the integration branch, using check names verified against real check runs.
- Enable issues on the downstream repository and open one tracking issue each for the fuzz and sanitizer regressions.
- Correct the owner reference in the public badges.
Scope out:
- Administering any repository other than the downstream one.
- Merging, closing, or approving any pull request.
- Adding the new report-only jobs to the required checks.
- Editing workflow definitions, gate scripts, or documentation.
Outputs:
- corrected-public-badges: Public badges pointing at the downstream repository — artifact: README.md — guarantee: The advertised build and license state reflects the repository that actually runs the checks.
Owns:
- README.md
- README_zh.md
Exclusive:
- downstream repository branch ruleset
- downstream repository issue settings
Difficulty: standard
Workload: light
Verification: code
Risks:
- operations
- public_interface
- persistent_state
Nodes:
- verify-check-identities: Read the actual check-run names produced by the required workflows on a recent commit and confirm each intended required check exists under that exact name.
- read-current-ruleset: Read the current ruleset for the integration branch and record which rules it already enforces.
- correct-public-badges: Correct the owner reference in the public badges and confirm no other owner mismatch remains in the public readme files.
- enable-issue-tracking: Enable issues on the downstream repository.
- add-required-status-checks: Add the strict required status check rule with the verified check names, preserving the existing pull-request, deletion, and non-fast-forward rules. — after: verify-check-identities, read-current-ruleset
- open-regression-tracking-issues: Open one tracking issue each for the fuzz and sanitizer regressions, naming the failed runs and the owning surface. — after: enable-issue-tracking
- confirm-governance-state: Read back the ruleset and repository settings and confirm the enforced rules, the required check names, the issue setting, and the two open tracking issues. — after: add-required-status-checks, open-regression-tracking-issues, correct-public-badges
Requirements:
- nightly-required-checks-enforced
- issue-tracking-enabled
- governance-owner-references-corrected
Design:
approach:
- Extend the existing ruleset instead of creating a second one or reviving classic branch protection, because the repository specification names rulesets as the only authority and warns that the classic endpoint misreports state.
- Require only checks that already exist and already run on pull requests: the two platform adaptation gates, the smoke job, the golden-standard job, the released audit gate, and the hygiene gate. Verify each name against real check runs before writing it, because a required name that never reports blocks every merge.
- Keep the new report-only Windows, coverage, and benchmark jobs out of the required set until they have a green history, so observability does not become an unproven merge gate.
- Enable strict up-to-date enforcement, which the repository specification already requires for protected branches.
- Prove every outcome by reading state back through the repository API, since none of these outcomes is a file.
- Do not change the purpose or title line of any document, so no generated index becomes stale in another Task's surface.
Acceptance:
- Given: The integration branch has an active ruleset — When: The ruleset is read back — Then: It enforces strict required status checks with the verified check names alongside the existing rules — Oracle: gh api repos/Unka-Malloc/styio-nightly/rulesets returns an active branch ruleset for the integration branch whose rule types include required_status_checks with strict enforcement and whose contexts match the verified check names — Evidence: command: ruleset read-back — Covers: nightly-required-checks-enforced
- Given: Issue tracking was disabled — When: The repository settings and issue list are read back — Then: Issues are enabled and both tracking issues are open — Oracle: gh api repos/Unka-Malloc/styio-nightly reports has_issues true and gh issue list returns one open fuzz regression issue and one open sanitizer regression issue — Evidence: command: repository settings and issue read-back — Covers: issue-tracking-enabled
- Given: The public badges referenced a non-existent owner — When: The public readme files are inspected — Then: No stale owner reference remains — Oracle: rg -n "styio-org" README.md README_zh.md finds no match and the badge targets name the downstream repository — Evidence: command: badge owner check — Covers: governance-owner-references-corrected, corrected-public-badges
Regression:
Commands:
- rg -n "styio-org" README.md README_zh.md
- gh api repos/Unka-Malloc/styio-nightly/rulesets
Paths:
- README.md
- README_zh.md

## Task: external-authority-handoff
Outcome: Every action this delivery needs in a repository outside the authorized administration boundary is recorded with verified current state, the exact required content, and an executable step sequence, and none of them is performed outside that boundary.
Scope in:
- Verify and record the current upstream divergence and the branch relationship the repository contract requires for promotion.
- Record the executable promotion sequence, including the ordering constraint that the downstream integration merge must land first.
- Record the exact released audit policy change that would remove the need for runtime normalization.
- Record the exact benchmark integration assets the external benchmark repository must publish for the report-only benchmark job to configure.
Scope out:
- Performing any write action in an external repository.
- Merging the downstream integration pull request.
- Weakening the branch or ordering contract to make promotion possible sooner.
- Editing compiler sources, workflow definitions, gate scripts, or repository settings.
Outputs:
- external-authority-handoff: Handoff document for external-repository actions — artifact: docs/external/upstream/EXTERNAL-AUTHORITY-HANDOFF.md — guarantee: The maintainer can execute each blocked action without rediscovering its state, ordering, or exact content.
Owns:
- docs/external/upstream
Exclusive:
- upstream repository promotion
- released audit policy repository
- external benchmark repository assets
Difficulty: standard
Workload: light
Verification: code
Risks:
- release
- operations
Nodes:
- verify-upstream-divergence: Read and record the current divergence between the downstream integration branch and each upstream managed branch.
- verify-audit-policy-drift: Read the released audit policy and record which scope entries the workflow currently rewrites at runtime and what the corrected policy content is.
- verify-benchmark-asset-gap: Read the external benchmark repository default branch and record exactly which required integration assets are present and which are missing.
- write-authority-handoff: Write the handoff document with verified state, required content, executable steps, and the ordering constraint for each blocked action. — after: verify-upstream-divergence, verify-audit-policy-drift, verify-benchmark-asset-gap
Requirements:
- upstream-promotion-handoff
- external-audit-policy-handoff
Design:
approach:
- Treat the blocked terminal state as a real outcome with a real artifact, not as a failure. The authority boundary limits administration to the downstream repository, and the repository contract additionally requires the downstream integration merge before any upstream promotion, so promotion cannot complete inside this delivery.
- Verify state before recording it, so the handoff carries measured divergence and a measured asset list instead of restated assumptions.
- Record the corrected policy content precisely enough to apply without rediscovery, and keep the local fail-loud normalization as the interim guard owned by the workflow Task.
- Place the document in a new external handoff subdirectory, which keeps it out of every generated index and away from every other Task's owned paths.
- Keep the document free of machine identity, credentials, and permission detail; it records repository state and steps only.
Acceptance:
- Given: Actions are required in repositories outside the authorized boundary — When: The handoff document is inspected — Then: Each blocked action carries verified current state, required content, executable steps, and its ordering constraint, and no external write was performed — Oracle: rg -n "upstream|audit policy|benchmark" docs/external/upstream/EXTERNAL-AUTHORITY-HANDOFF.md matches all three blocked actions, each section names a verified current state and an executable step sequence, and gh api reports the external repositories unchanged by this delivery — Evidence: command: handoff document and external state check — Covers: upstream-promotion-handoff, external-audit-policy-handoff, external-authority-handoff
Regression:
Commands:
- rg -n "^## " docs/external/upstream/EXTERNAL-AUTHORITY-HANDOFF.md
- python3 scripts/local-info-leak-gate.py --mode worktree
Paths:
- docs/external/upstream

## Full regression
Commands:
- cmake --build build/default --target styio styio_lspd styio_test styio_security_test
- ctest --test-dir build/default -L docs --output-on-failure --no-tests=error
- ctest --test-dir build/default -L security --output-on-failure --no-tests=error
- ctest --test-dir build/default -L ide --output-on-failure --no-tests=error
- ctest --test-dir build/default -L styio_pipeline --output-on-failure --no-tests=error
- ctest --test-dir build/default -L language_feature --output-on-failure --no-tests=error
- ctest --test-dir build/default -L golden_standard --output-on-failure --no-tests=error
- cmake --build build/fuzz --target styio_fuzz_lexer styio_fuzz_parser
- ctest --test-dir build/fuzz -L fuzz_smoke --output-on-failure --no-tests=error
- cmake --build build/asan-ubsan --target styio styio_test styio_security_test styio_typeinfer_internal_test
- ctest --test-dir build/asan-ubsan -L security --output-on-failure --no-tests=error
- ctest --test-dir build/asan-ubsan -L styio_pipeline --output-on-failure --no-tests=error
- python3 scripts/architecture-layer-gate.py
- python3 scripts/runtime-surface-gate.py
- python3 scripts/repo-hygiene-gate.py --mode tracked
- python3 scripts/local-info-leak-gate.py --mode tracked
- python3 scripts/team-docs-gate.py
- python3 scripts/docs-index.py --write
- python3 scripts/docs-audit.py
- python3 scripts/docs-lifecycle.py validate
- python3 scripts/syntax-feature-state-gate.py
- python3 scripts/tool-skill-registry-gate.py
- python3 scripts/workflow-scheduler.py check
- python3 scripts/manifest_tool.py validate docs/plan
- python3 scripts/monolith-line-ratchet-gate.py
- python3 -c "import glob,yaml;[yaml.safe_load(open(p)) for p in glob.glob('.github/workflows/*.yml')]"
- gh api repos/Unka-Malloc/styio-nightly/rulesets
Paths:
- src
- tests
- cmake
- CMakeLists.txt
- .github/workflows
- docs
- scripts
- workflows
- README.md
- README_zh.md
- .gitignore
