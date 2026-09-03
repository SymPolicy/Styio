# PLAN-002 Reuse one observable topology artifact across compilation

Phase: completed · Revision: 1

This document is a render-only projection of `Plan.json`. Edit `Plan.json`; never edit this file.

## Intent

**Goal**: Construct the compiler resource topology once at the typed semantic boundary and reuse that immutable validated artifact during lowering without changing accepted programs, diagnostics, generated code, or ordinary compiler operation.

**In scope**
- Sema-owned resource-topology artifact lifecycle, lowering reuse, focused regression evidence, and repository documentation or plan metadata required by the current contracts.

**Out of scope**
- Persistent semantic IDs, public snapshot or event schemas, serialization, runtime instrumentation, scheduler integration, Vityo, policy engines, external repositories, unrelated cache or topology algorithm redesign, and deferred stash experiments.

**Success**
- Resource-bearing programs build and validate one authoritative topology artifact per semantic analysis, lowering consumes it without rebuilding, scalar fast paths and diagnostics stay intact, focused topology/Sema/lowering/IDE checks pass, and the repository plan remains valid.

**Risk boundary**
- Do not weaken validation, alter language semantics or diagnostic contracts, expose AST pointers or machine paths, add public ABI or wire promises, touch runtime scheduling, or run repeated broad regressions.

## Decisions

Dossier status: not_required

No non-discoverable user decision was required.

### Observed repository facts

- none

## Requirements

| Code | Statement | Sources |
| --- | --- | --- |
| REQ-001 | Every resource-bearing top-level semantic analysis builds and validates exactly one authoritative resource-topology artifact after typed facts are complete. | user-request, repository-contract |
| REQ-002 | The semantic context exclusively owns the validated artifact for the analyzed root, replaces it on a later analysis, exposes it read-only, and never retains a successful artifact after failed analysis. | user-request, observable-language-contract |
| REQ-003 | Top-level lowering requires and consumes the matching Sema result as its validation proof and never rebuilds or revalidates resource topology. | user-request, repository-fact |
| REQ-004 | The existing narrow, import-free scalar subset still skips topology construction, while every program outside that proven subset still receives the full validation. | user-request, repository-contract |
| REQ-005 | Accepted programs, rejection points, diagnostic text and phase, StyioIR or generated behavior, and ordinary compiler operation remain unchanged. | user-request |
| REQ-006 | The artifact remains compiler-internal, adds no persistent identity, serialization, ABI, wire, runtime, scheduler, cache, or policy contract, and exposes neither AST pointers nor machine paths through its new access surface. | user-request, observable-language-contract |
| REQ-007 | Focused topology, Sema, lowering, resource-feature, scalar, and IDE evidence proves the lifecycle and compatibility claims without a permanent production build counter. | user-request |
| REQ-008 | The observable-language SSOT, test catalog, and generated plan index describe the implemented ownership and executable evidence without changing the resource-syntax SSOT. | user-request, repository-docs-contract |

## Architecture

One mutually independent Task forms the complete parallel frontier because the validated value type, its Sema owner, lowering's read-only consumption, compatibility tests, and owning documentation are one causal cutover. Within that Task, the artifact-contract and behavior-baseline Nodes begin independently; later source and test Nodes branch only where their write paths and inputs are independent, and one named join runs focused acceptance.

- Keep `StyioResourceTopology::Graph` as the only topology model and leave its active node kinds, edge kinds, validation rules, cycle algorithm, labels, and build-order IDs unchanged. Do not add a second graph, view model, snapshot schema, cache, or compatibility implementation.
- Add a semantic, move-only validated value in `ResourceTopology.hpp`. The existing `validate_or_throw(MainBlockAST*, phase)` becomes the sole top-level validated-artifact construction gateway by returning that value after the existing `BuildResult` report passes; callers that only need validation may still ignore the return. Raw `build` remains the lower-level topology-test API, not a competing validated lifecycle.
- The validated value owns the one moved `Graph`, deletes copying, and offers only const, pointer-free observations needed by internal callers and tests. Existing process-local `Node::source` pointers stay encapsulated inside the topology implementation and are not returned by the Sema access surface, serialized, logged, or documented as identity.
- `StyioSemaContext` owns an explicit per-root lifecycle with three states: not analyzed, scalar no-op, and validated artifact. Entering top-level semantic analysis invalidates any prior state; the exact existing scalar predicate plus the existing imported-callable condition selects scalar no-op; every other successful analysis stores the returned validated value. Validation failure throws the existing `sema-resource-topology` diagnostic and leaves no consumable result.
- Root matching is an internal lifetime guard only. It may compare the current AST identity inside the owner, but no accessor returns that pointer and no persistent identity is introduced. Reanalysis replaces the prior artifact rather than merging, caching, or sharing it.
- Top-level `AstToStyioIRLowerer::toStyioIR(MainBlockAST*)` asks the inherited semantic context for the matching read-only lifecycle result. Scalar no-op preserves the skip path; validated state supplies the same artifact as an opaque proof token; missing or mismatched state fails the internal typed-AST precondition and never falls back to building. Nested lowering remains unchanged.
- Move the update point for the existing topology profile fields to semantic analysis so the current counter names and profile output remain available while measuring the single real probe or validation. Add no counter, exporter, timer deadline, runtime hook, or scheduler dependency.
- Prove reuse with a white-box test seam that exposes only the const lifecycle state/artifact identity to a test subclass, plus a source-level oracle that forbids `build`, `validate_or_throw`, and the scalar predicate in lowering. Do not add an injected builder strategy or a permanent production build-count field.
- Update `Styio-Observable-Language.md` from the pre-foundation statement to the implemented internal lifecycle and record the exact focused commands in `TEST-CATALOG.md`. Leave `Styio-Resource-Topology.md` untouched because syntax and topology semantics do not change. Update the Sema / IR, IDE / LSP, Test Quality, and Docs / Ecosystem runbooks with only the maintenance consequence of this cutover, refresh their statistics, and regenerate the plan index required by the docs gate.
- No Task touches runtime scheduling, public services or protocols, Vityo, external repositories, deferred experiments, or unrelated topology/cache algorithms. The complete regression remains outside the Designer and runs once after the Task is accepted.

## Tasks

Every Task belongs to the same mutually independent parallel frontier.

### TASK-001 sema-topology-artifact-reuse

Worker: general · Tier: complex · Workload: medium · Verification: code · Frontier: parallel

Outcome: Resource-bearing compilation creates one immutable validated topology at the typed semantic boundary, lowering consumes that exact Sema-owned result without rebuilding, and focused compatibility evidence plus the owning contracts stay green.

Risks: quality, performance, privacy, shared_resource
Requirements: REQ-001, REQ-002, REQ-003, REQ-004, REQ-005, REQ-006, REQ-007, REQ-008
Writes: src/StyioResourceTopology/ResourceTopology.hpp, src/StyioResourceTopology/ResourceTopology.cpp, src/StyioSema/SemaContext.hpp, src/StyioSema/TypeInfer.cpp, src/StyioLowering/AstToStyioIRLowerer.hpp, src/StyioLowering/AstToStyioIR.cpp, tests/resource_topology_test.cpp, tests/typeinfer_internal_test.cpp, tests/lowering_internal_test.cpp, tests/ide/styio_ide_test.cpp, docs/design/Styio-Observable-Language.md, docs/teams/SEMA-IR-RUNBOOK.md, docs/teams/IDE-LSP-RUNBOOK.md, docs/teams/TEST-QUALITY-RUNBOOK.md, docs/teams/DOCS-ECOSYSTEM-RUNBOOK.md, docs/teams/DOC-STATS.md, workflows/TEST-CATALOG.md, docs/plan/INDEX.md
Exclusive resources: configured CMake and CTest tree build/default, generated documentation indexes

In scope
- Introduce the move-only validated topology value using the existing graph and validator.
- Add the reset, scalar-noop, validated, failed, and root-match lifecycle to the semantic context and construct the artifact only after top-level type inference completes.
- Remove lowering's scalar probe and topology validation build, replacing them with a required read-only lookup of the matching semantic result.
- Preserve the existing compiler-profile counter surface while moving its accounting to the one Sema probe or validation.
- Add focused topology, Sema, lowering, compatibility, and IDE tests, including an artifact-identity plus no-builder proof.
- Update the observable-language contract, focused test catalog, four required owner runbooks, runbook statistics, and generated plan index required by repository documentation gates.

Out of scope
- Any new node kind, edge kind, validation rule, topology algorithm, cache, persistent semantic ID, source-anchor design, snapshot, delta, event, query, ABI, or serialization format.
- Any language syntax, accepted-program, diagnostic-contract, StyioIR, code-generation, runtime, scheduler, instrumentation-export, or policy change.
- Public artifact access, AST-pointer or machine-path exposure, Vityo or external-repository integration, and deferred stash experiments.
- Broad source cleanup, compatibility shims, parallel old/new implementations, version-style names, and repeated full regression.

Outputs
- OUT-001 Move-only validated topology value built from the existing Graph (`src/StyioResourceTopology/ResourceTopology.hpp`): A successful validation can be retained without copying the graph or exposing a mutable or pointer-bearing access surface.
- OUT-002 Per-root semantic lifecycle for scalar-noop or validated topology state (`src/StyioSema/SemaContext.hpp`): Exactly one semantic context owns the authoritative result and stale or failed analyses cannot be consumed.
- OUT-003 Top-level lowering dependency on the matching read-only Sema result (`src/StyioLowering/AstToStyioIR.cpp`): Lowering performs no topology build, validation, or duplicate scalar proof.
- OUT-004 White-box topology, Sema, lowering, and IDE regression coverage (`tests/resource_topology_test.cpp`): Artifact immutability, single construction, reuse identity, scalar behavior, diagnostics, and representative lowering behavior are executable claims without a production build counter.
- OUT-005 Current internal artifact ownership and boundary documentation (`docs/design/Styio-Observable-Language.md`): The design SSOT distinguishes this implemented foundation from deferred public snapshots, identities, runtime correlation, and consumer work.
- OUT-006 Focused topology-foundation test commands (`workflows/TEST-CATALOG.md`): Maintainers can rerun the topology, Sema, lowering, resource-feature, scalar, and IDE evidence directly.
- OUT-007 Regenerated plan workspace index (`docs/plan/INDEX.md`): Repository documentation gates index this tracked Better Plan without hand-edited generated content.

Internal Node graph
- NODE-001 define-validated-topology-value · after: none · Convert the existing successful validation result into one move-only, const-observable artifact without changing Graph construction, validation, diagnostics, or raw builder tests.
- NODE-002 capture-compatibility-baselines · after: none · Record the exact current Sema topology diagnostic and representative scalar and resource StyioIR or execution expectations that the new focused tests must preserve.
- NODE-003 install-sema-artifact-lifecycle · after: NODE-001 · Add per-root reset, scalar-noop, validated, and failed lifecycle handling to `StyioSemaContext`; store the sole returned artifact after successful top-level type inference and move existing profile accounting to this boundary.
- NODE-004 consume-sema-result-in-lowering · after: NODE-003 · Replace lowering's independent scalar probe and validator call with a fail-closed read-only requirement for the matching Sema lifecycle state, preserving nested lowering and existing profile output.
- NODE-005 prove-sema-lifecycle-and-diagnostics · after: NODE-002, NODE-003 · Add topology and Sema tests for move-only const access, one successful artifact per resource analysis, scalar-noop state, stale-state replacement, and no artifact after the exact existing failure diagnostic.
- NODE-006 prove-lowering-reuse-and-compatibility · after: NODE-002, NODE-004 · Add the white-box same-artifact identity test, the no-second-probe scalar test, and exact representative IR assertions for resource and scalar programs without introducing a production build counter.
- NODE-007 prove-ide-diagnostic-boundary · after: NODE-002, NODE-003 · Add an IDE semantic-bridge case showing that invalid resource topology still reports the same compiler-owned Sema diagnostic without a machine path or pointer-shaped text.
- NODE-008 update-owning-contracts-and-runbook · after: NODE-005, NODE-006, NODE-007 · Mark the internal foundation implemented in the observable-language SSOT, register the exact focused commands and test names in the test catalog, update the four required owner runbooks and their statistics, and regenerate the plan index; do not edit the resource-syntax SSOT.
- NODE-009 verify-focused-topology-closure · after: NODE-008 · Build the four affected test targets, run the new proof tests and existing topology, Sema, lowering, scalar/resource-feature, and IDE evidence, enforce the no-builder-in-lowering oracle, and pass documentation checks.

Design
- approach
  - Change the `MainBlockAST` overload of `validate_or_throw` to return the new validated value after moving `BuildResult::graph`; preserve the `BlockAST` validation entry and direct `build` tests. This reuses the existing builder and avoids a second construction API.
  - Make the validated value non-copyable and movable. Its public observations are const and do not return `Node::source`, the analyzed root, or mutable Graph storage. The Sema owner exposes only lifecycle status and a const artifact reference for the same root.
  - Reset lifecycle state before each top-level analysis. Publish scalar-noop only when both existing conditions hold; publish validated only after the unchanged report succeeds; leave not-analyzed on every exception. Never retain the prior root's artifact through reanalysis.
  - Treat the artifact as lowering's proof that topology validation already succeeded. Lowering may inspect const counts if genuinely needed, but this delivery requires no graph traversal because existing lowering used validation only for its side effect. Missing or mismatched proof is an internal typed-boundary violation, not permission to rebuild.
  - Preserve user-visible diagnostics by keeping the validation call at the same end-of-Sema position, the phase string `sema-resource-topology`, report ordering, and `StyioTypeError` propagation. The old `lowering-resource-topology` call disappears only because correctly typed programs already passed the authoritative check.
  - Preserve ordinary profile behavior by retaining existing counter names and enablement, recording the scalar predicate duration or one validation duration in Sema, and ensuring lowering adds neither a second duration nor a second skipped event. Add no externally visible metric.
  - Use existing golden and feature suites as compatibility authorities. New focused assertions hard-code the pre-change diagnostic and IR baselines captured by the baseline Node; they do not normalize changed output into acceptance.
  - The test-only white-box subclass reveals const lifecycle state and artifact object identity. A focused `rg` oracle simultaneously proves lowering contains no direct builder, validator, or scalar-predicate call, closing the loophole that identity alone would leave.
  - Update only the current-state paragraph and evidence obligation in the observable-language SSOT, add the focused command to the existing test catalog, update the four owner runbooks with the maintenance consequence, refresh `docs/teams/DOC-STATS.md`, and regenerate `docs/plan/INDEX.md` with the repository generator.
- patterns
  - pattern_catalog: refactoring-guru-catalog-22-v1
  - candidate: Strategy
  - decision: reject
  - pressure: A build-count test could inject a replaceable topology builder, but production has one authoritative algorithm and no runtime selection requirement.
  - expected_benefit: none beyond test interception; it would not improve the ownership or validation contract.
  - simpler_alternative: Retain the direct validator, make its successful result a move-only Sema-owned value, expose const identity through the existing white-box test subclass, and pair that test with a source-level no-builder oracle.
  - application: Use ordinary value ownership and const access only inside ResourceTopology, SemaContext, and top-level lowering.
  - costs_and_rejections: A strategy interface or production counter would add indirection, mutable accounting, and another lifecycle solely for tests; Facade, Proxy, and Observer layers are likewise rejected because there is one owner and one synchronous consumer.

Acceptance
- AC-001 covers REQ-001, REQ-002, REQ-003, OUT-001, OUT-002, OUT-003, OUT-004
  - Given A resource-bearing root and one semantic context
  - When top-level semantic analysis succeeds, is inspected through the test-only const seam, and is followed by lowering
  - Then one move-only validated artifact containing the expected existing Graph facts is installed for that root and the same artifact object remains the lowering proof
  - Oracle: `StyioResourceTopology.ValidatedArtifactIsMoveOnlyAndConstObservable`, `StyioSemaTopology.ResourceAnalysisPublishesOneValidatedArtifact`, and `StyioLoweringInternal.ReusesSemaValidatedTopologyArtifact` all pass
  - Evidence: command from focused artifact lifecycle tests
- AC-002 covers REQ-001, REQ-003, OUT-003
  - Given The lowering implementation after the cutover
  - When its topology dependencies are inspected
  - Then it contains no direct topology build, validation, or scalar eligibility call and no alternate compatibility path
  - Oracle: `! rg -n 'resource_topology::(build|validate_or_throw|validation_is_noop_for_scalar_program)' src/StyioLowering/AstToStyioIR.cpp` exits zero, while `rg -n 'require.*resource.*topology|resource.*topology.*require' src/StyioLowering/AstToStyioIR.cpp` finds the Sema-result precondition
  - Evidence: command from no lowering rebuild structural proof
- AC-003 covers REQ-002, REQ-005, REQ-006, OUT-002, OUT-004
  - Given A failed resource analysis, a successful root followed by reanalysis, and an attempted mismatched-root lookup
  - When the lifecycle tests exercise each transition
  - Then failures and replacement leave no stale consumable artifact, the established diagnostic remains byte-for-byte equal to its captured `sema-resource-topology` baseline, and lowering never rebuilds as fallback
  - Oracle: `StyioSemaTopology.InvalidAnalysisKeepsDiagnosticAndPublishesNoArtifact`, `StyioSemaTopology.ReanalysisReplacesPriorArtifact`, and `StyioLoweringInternal.RejectsMismatchedSemaTopologyState` all pass
  - Evidence: command from focused lifecycle failure tests
- AC-004 covers REQ-004, REQ-005, OUT-002, OUT-004
  - Given A proven narrow scalar program, a scalar program with imported callables, and nested resource-shaped expressions
  - When Sema and lowering run
  - Then only the existing import-free narrow subset records scalar-noop, no graph is built for it, and every negative case receives validated topology
  - Oracle: `StyioSemaTopology.ScalarFastPathRecordsNoopWithoutArtifact`, `StyioLoweringInternal.ScalarFastPathConsumesSemaNoopState`, and the existing `StyioLoweringInternal.ResourceTopologyFastPath*` tests all pass
  - Evidence: command from scalar fast-path tests
- AC-005 covers REQ-005, REQ-004, REQ-007, OUT-004
  - Given Representative scalar, file, state, stream, task, ownership, mutation, commit, backpressure, failure, and ordering programs
  - When the affected test targets and focused feature labels run
  - Then accepted programs still compile and execute, rejected programs keep their exact diagnostics, and expected StyioIR or generated outputs do not change
  - Oracle: `ctest --test-dir build/default -L '^(resource_topology|scalar_expressions|file_resources|state_resources|stream_processing|task_resources)$' --output-on-failure --no-tests=error` exits zero
  - Evidence: command from focused language compatibility matrix
- AC-006 covers REQ-005, REQ-006, REQ-007, OUT-004
  - Given IDE analysis of the captured invalid resource program
  - When the compiler semantic bridge reports diagnostics
  - Then it emits the same compiler-owned Sema topology message and phase as before and contains neither a machine path nor pointer-shaped address
  - Oracle: `StyioSemanticBridge.ResourceTopologyDiagnosticRemainsCompilerOwned` passes under `styio_ide_test`
  - Evidence: command from focused IDE topology diagnostic test
- AC-007 covers REQ-006, REQ-007, REQ-008, OUT-005, OUT-006, OUT-007
  - Given The implementation and focused tests are complete
  - When the observable contract, test catalog, owner runbooks, generated indexes, and documentation gates are checked
  - Then the SSOT says Sema owns one immutable internal artifact, deferred public/runtime work remains deferred, the focused commands and team ownership are documented, runbook statistics are current, and generated content is current
  - Oracle: `python3 scripts/docs-index.py --check`, `python3 scripts/team-docs-gate.py --mode worktree`, and `python3 scripts/docs-audit.py` all exit zero, `rg -n 'Sema-owned|semantic analysis.*owns' docs/design/Styio-Observable-Language.md` finds the current ownership contract, and `rg -n 'ReusesSemaValidatedTopologyArtifact' workflows/TEST-CATALOG.md` finds the focused runbook entry
  - Evidence: command from observable topology documentation checks

Focused regression
- `cmake --build build/default --target styio_resource_topology_test styio_typeinfer_internal_test styio_lowering_internal_test styio_ide_test -j2`
- `ctest --test-dir build/default -R '^(StyioResourceTopology\.ValidatedArtifactIsMoveOnlyAndConstObservable|StyioSemaTopology\.(ResourceAnalysisPublishesOneValidatedArtifact|InvalidAnalysisKeepsDiagnosticAndPublishesNoArtifact|ReanalysisReplacesPriorArtifact|ScalarFastPathRecordsNoopWithoutArtifact)|StyioLoweringInternal\.(ReusesSemaValidatedTopologyArtifact|RejectsMismatchedSemaTopologyState|ScalarFastPathConsumesSemaNoopState)|StyioSemanticBridge\.ResourceTopologyDiagnosticRemainsCompilerOwned)$' --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -L resource_topology --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -L sema_internal --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -L lowering_internal --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -L '^(scalar_expressions|file_resources|state_resources|stream_processing|task_resources)$' --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -R '^StyioSemanticBridge\.ResourceTopologyDiagnosticRemainsCompilerOwned$' --output-on-failure --no-tests=error`
- `test "$(rg -c 'resource_topology::validate_or_throw\(' src/StyioSema/TypeInfer.cpp)" -eq 1 && ! rg -n 'resource_topology::(build|validate_or_throw|validation_is_noop_for_scalar_program)' src/StyioLowering/AstToStyioIR.cpp`
- `python3 scripts/docs-index.py --check && python3 scripts/team-docs-gate.py --mode worktree && python3 scripts/docs-audit.py`
- paths: src/StyioResourceTopology/ResourceTopology.hpp, src/StyioResourceTopology/ResourceTopology.cpp, src/StyioSema/SemaContext.hpp, src/StyioSema/TypeInfer.cpp, src/StyioLowering/AstToStyioIRLowerer.hpp, src/StyioLowering/AstToStyioIR.cpp, tests/resource_topology_test.cpp, tests/typeinfer_internal_test.cpp, tests/lowering_internal_test.cpp, tests/ide/styio_ide_test.cpp, docs/design/Styio-Observable-Language.md, docs/teams/SEMA-IR-RUNBOOK.md, docs/teams/IDE-LSP-RUNBOOK.md, docs/teams/TEST-QUALITY-RUNBOOK.md, docs/teams/DOCS-ECOSYSTEM-RUNBOOK.md, docs/teams/DOC-STATS.md, workflows/TEST-CATALOG.md, docs/plan/INDEX.md

## Full regression

Run inside the sole Reviewer session after every repair is integrated.

- `cmake --build build/default -j2`
- `ctest --test-dir build/default --output-on-failure --no-tests=error`
- `bash scripts/docs-gate.sh --mode worktree`
- `git diff --check`
- paths: src/StyioResourceTopology, src/StyioSema, src/StyioLowering, tests, docs/design/Styio-Observable-Language.md, docs/teams/SEMA-IR-RUNBOOK.md, docs/teams/IDE-LSP-RUNBOOK.md, docs/teams/TEST-QUALITY-RUNBOOK.md, docs/teams/DOCS-ECOSYSTEM-RUNBOOK.md, docs/teams/DOC-STATS.md, workflows/TEST-CATALOG.md, docs/plan
