# PLAN-003 Persistent semantic identities for resource topology

Phase: completed · Revision: 2

This document is a render-only projection of `Plan.json`. Edit `Plan.json`; never edit this file.

## Intent

**Goal**: Give every compiler-owned resource-topology node an opaque persistent semantic identity that survives clean rebuilds and non-semantic edits without changing accepted programs, diagnostics, code generation, or ordinary compiler execution.

**In scope**
- Internal semantic identity type and derivation for resource-topology nodes, logical identity scope input at the Sema boundary, immutable artifact observation needed for compiler tests, metamorphic and focused verification, and repository documentation required by existing contracts.

**Out of scope**
- Public snapshot schemas or serialization, lineage protocols, partial topology, public SDK or ABI promises, runtime or scheduler integration, Vityo integration, caches, and retained low-completion stash experiments.

**Success**
- Fresh compilations of the same logical module and semantic program produce identical node semantic identities across whitespace, comments, formatting, optimization settings, and unrelated edits; a semantic rename may change the affected identity; identities encode no path, pointer, source offset, allocation order, or runtime identifier; existing compiler behavior and the final complete regression remain green.

**Risk boundary**
- Keep snapshot-local graph indices separate, expose no absolute path, source-content literal, pointer, runtime identity, public wire promise, or compatibility implementation, and run the complete regression only once after all implementation changes.

## Decisions

Dossier status: not_required

No non-discoverable user decision was required.

### Observed repository facts

- none

## Requirements

| Code | Statement | Sources |
| --- | --- | --- |
| REQ-001 | Keep `Node::id` as the zero-based snapshot-local dense index used by edges, cycle detection, diagnostics, and existing topology algorithms; add semantic identity as a separate field. | user-request, repository-contract |
| REQ-002 | Fresh parsers, analyzers, ASTs, and topology artifacts built for the same explicitly qualified project/package and logical module must produce the same semantic identity for the same semantic site. | user-request |
| REQ-003 | Whitespace, comments, formatting, optimization level, parser file labels, and unrelated semantic edits must not change identities for unaffected sites. | user-request |
| REQ-004 | Renames, logical module moves, and structural rewrites may change the affected identities without creating a lineage or compatibility promise. | user-request |
| REQ-005 | Identity derivation must not use machine or absolute paths, filenames, working directories, byte offsets, line or column locations, AST/IR/LLVM addresses, graph/build/allocation order alone, raw source or body digests, runtime identifiers, UI or random identifiers, or session-local `SymbolId`, `TypeId`, or HIR identifiers. | user-request |
| REQ-006 | Provide an explicit compiler-owned identity-scope input containing project/package identity and canonical slash-form logical module identity, plus an explicitly unqualified anonymous fallback that cannot be represented as globally comparable or publishable identity and is never inferred from a filename or working directory. | user-request |
| REQ-007 | Reuse the callable module canonical-identity rules from a neutral low-level utility so ResourceTopology depends on neither CallableInterface nor IDE services and existing callable-interface diagnostics remain unchanged. | user-request, repository-contract |
| REQ-008 | Derive the smallest sufficient fixed opaque digest from a deterministic versioned, length-prefixed exact key; detect different exact keys yielding the same digest and fail closed without salting, probing, or build-order repair. | user-request |
| REQ-009 | Repeated labels, anonymous sites, and every slot of a multi-slot `ResourceDeclAST` must receive distinct site identities even when one declaration AST pointer is reused, without changing language behavior. | user-request, repository-contract |
| REQ-010 | Program roots, state-window ledgers, task failure domains, scope-exit destroy sinks, and other compiler-synthesized nodes must derive identity from explicit semantic roles and owners rather than null pointers, labels, or insertion order. | user-request |
| REQ-011 | Preserve the existing Sema-owned `ValidatedArtifact` lifecycle and lowering reuse, add only immutable internal identity observation, and create no second topology graph. | user-request, repository-contract |
| REQ-012 | Optimization must not mutate semantic identities, while accepted programs, diagnostics, StyioIR/codegen, runtime behavior, scalar fast-path behavior, topology labels, and existing topology algorithms remain unchanged. | user-request, repository-contract |
| REQ-013 | Keep identities compiler-internal with no public snapshot, schema, serialization, ABI/SDK, lineage, partial-topology, runtime/scheduler, Vityo, cache, or retained experiment surface. | user-request |
| REQ-014 | Add deterministic internal tests using fresh compilation objects and fixed logical identities, then update only the owning design contract, Sema runbook, test catalog, and generated plan index required by touched surfaces. | user-request, repository-contract |
| REQ-015 | Register the already documented architecture layer script as the CTest oracle named `architecture_layer_gate` so focused and complete regression commands execute the real repository gate instead of reporting no tests. | repository-contract, focused-acceptance |
| REQ-016 | Update the Docs / Ecosystem and Test Quality runbooks plus generated documentation statistics required by the repository docs gate for the authorized plan, test, and documentation changes. | repository-contract, focused-acceptance |

## Architecture

Add one compiler-internal qualified-or-anonymous semantic identity scope, one neutral deterministic identity primitive, and one topology-specific semantic-site derivation path to the existing Sema-owned graph; keep local graph indices and every external or runtime boundary unchanged.

- `StyioUtil` owns canonical logical-module validation, the qualified/anonymous scope value, deterministic key encoding, a 128-bit opaque identity derived by truncating the repository's existing LLVM SHA-256 implementation, and a per-build exact-key collision guard. The digest is one identity mechanism, not a checksum pair or cache key.
- A qualified scope contains an explicit logical project/package identity and canonical slash-form module identity. The anonymous scope carries a distinct domain marker and an explicit unqualified status; default compiler construction selects it without consulting source filenames, process state, or the working directory.
- ResourceTopology owns semantic owner paths, topology semantic roles, and local structural discriminators. Exact keys are the versioned domain, scope fields, owner components, role, and discriminator components encoded as fixed-width counts plus byte-length-prefixed semantic strings.
- Owner paths use declaration and binding spellings at typed semantic boundaries. Local structural discriminators use typed AST kind, normalized type shape, operator or callee semantics, child-relation tags, and an occurrence number only among otherwise indistinguishable siblings under the same semantic owner. Trivia and semantically different siblings do not participate in that occurrence number.
- Literal payloads, diagnostic labels, and source text are not identity fields. In particular, path-bearing file-resource labels remain diagnostic-only and never enter the exact key.
- AST pointers remain permitted only in the snapshot-local memoization key that prevents revisiting one AST site. The memoization key also includes the semantic subsite so all slots sharing one `ResourceDeclAST*` remain distinct; pointers never enter the canonical preimage.
- Every node stores `semantic_id` and a typed semantic role alongside the unchanged dense `id`. `Graph::add_node` still assigns `id = nodes_.size()`, edges still reference dense IDs, `debug_string()` remains byte-compatible and omits semantic identities, and cycle/count/validation algorithms remain unchanged.
- The existing Builder derives identities inline while creating the one graph. It retains an ephemeral digest-to-exact-key registry only until that build completes; this is collision detection, not a reusable cache or a second graph.
- `StyioSemaContext` owns the identity scope for a compilation. Its default constructor delegates to the explicit anonymous scope, while an overload accepts a qualified scope. Top-level validation passes that scope into the existing artifact construction gateway, and lowering continues to require the same root-matched Sema artifact without rebuilding.
- `ValidatedArtifact` exposes only const, order-unspecified semantic node descriptors containing node kind, typed semantic role, opaque identity, and qualification status. It exposes no AST pointer, canonical preimage, source label, mutable graph, serialization, or ordering promise.
- For indistinguishable repeated anonymous siblings, the same-signature occurrence is the minimal deterministic discriminator. Inserting, deleting, or reordering an indistinguishable sibling may change that local ambiguity class; this is a structural rewrite within the approved instability boundary, not an unrelated edit guarantee.
- One coupled compiler Task owns utility extraction, compiler input plumbing, topology assignment, tests, and primary contracts because their interfaces and acceptance are inseparable. A disjoint repository-contract Task registers the already documented architecture gate and updates the team documentation required by the repository docs gate; the two Tasks have no overlapping write paths.

## Tasks

Every Task belongs to the same mutually independent parallel frontier.

### TASK-001 compiler-internal-persistent-semantic-identities

Worker: general · Tier: complex · Workload: heavy · Verification: code · Frontier: parallel

Outcome: Every node in the existing Sema-owned resource topology carries a deterministic opaque semantic identity with explicit qualification and fail-closed collision handling, while dense graph behavior and all ordinary compiler behavior remain unchanged and focused evidence proves the approved stability envelope.

Risks: quality, performance, privacy, shared_resource
Requirements: REQ-001, REQ-002, REQ-003, REQ-004, REQ-005, REQ-006, REQ-007, REQ-008, REQ-009, REQ-010, REQ-011, REQ-012, REQ-013, REQ-014
Writes: src/StyioUtil/SemanticIdentity.hpp, src/StyioUtil/SemanticIdentity.cpp, src/cmake/StyioFrontendSources.cmake, src/StyioSema/CallableInterface.cpp, src/StyioSema/CallableModuleLoader.cpp, src/StyioResourceTopology/ResourceTopology.hpp, src/StyioResourceTopology/ResourceTopology.cpp, src/StyioSema/SemaContext.hpp, src/StyioSema/TypeInfer.cpp, src/StyioLowering/AstToStyioIRLowerer.hpp, tests/resource_topology_test.cpp, tests/typeinfer_internal_test.cpp, tests/lowering_internal_test.cpp, docs/design/Styio-Observable-Language.md, docs/teams/SEMA-IR-RUNBOOK.md, workflows/TEST-CATALOG.md, docs/plan/INDEX.md
Exclusive resources: configured CMake and CTest tree build/default, generated documentation and plan indexes, repository worktree delivery gates

In scope
- Add the neutral internal identity scope, canonical logical-module validator, length-prefixed key encoder, 128-bit SHA-256-derived value, and per-build exact-key collision guard.
- Reuse the neutral canonical module rule in callable-interface and callable-module validation without changing accepted identifiers or diagnostics.
- Thread an explicit qualified or anonymous identity scope through `StyioSemaContext`, `AstToStyioIRLowerer`, BuildOptions, top-level topology validation, and the existing `ValidatedArtifact` lifecycle.
- Add typed semantic roles, owner paths, local structural discriminators, and semantic IDs to all existing resource-topology node creation paths while preserving dense local IDs and graph semantics.
- Correct multi-slot resource declaration memoization so each semantic slot is a distinct topology site even though the declaration AST pointer is shared.
- Add immutable artifact descriptors and fresh-build metamorphic tests for qualification, stability, change boundaries, repeated and synthetic sites, collision handling, lifecycle visibility, and optimizer immutability.
- Run focused compatibility and repository gates and update only the owning observable-language contract, Sema runbook, test catalog, and generated plan index.

Out of scope
- Public topology snapshots, schemas, serialization, exported text or hex identity formats, ABI/SDK guarantees, compatibility promises, lineage, deltas, queries, source anchors, and partial topology.
- Runtime IDs, scheduler or runtime integration, Vityo or IDE identity integration, telemetry, caches, persistence stores, remote services, and retained stash experiments.
- Canonical ordering of snapshots, nodes, or edges; edge semantic identities; graph replacement; a second graph; or changes to topology node kinds, edge kinds, labels, validation rules, and algorithms.
- New language syntax, changed accepted programs or diagnostics, changed StyioIR/codegen/runtime behavior, scalar-fast-path expansion, and broad refactoring outside the touched identity boundary.
- Deriving scope from the parser filename, source path, repository checkout, process environment, current directory, import resolution path, or any source/body digest.

Outputs
- OUT-001 Compiler-internal qualified/anonymous scope, shared canonical logical-module validation, length-prefixed exact-key encoding, 128-bit SHA-256-derived opaque identity, and collision guard (`src/StyioUtil/SemanticIdentity.hpp`): All identity consumers share one low-level deterministic implementation with explicit qualification and fail closed on digest collisions without retaining a cache or exposing key material.
- OUT-002 The neutral identity implementation is linked through the existing frontend source boundary (`src/cmake/StyioFrontendSources.cmake`): ResourceTopology and callable-interface consumers use one implementation without reversing Sema, IDE, or backend dependency directions.
- OUT-003 Callable interface and import-module validation delegate canonical slash-form checks to the neutral utility (`src/StyioSema/CallableInterface.cpp`): Existing canonical module acceptance and exact callable diagnostics remain compatible while ResourceTopology has no CallableInterface dependency.
- OUT-004 Separate opaque semantic identity and typed semantic role on every existing topology node (`src/StyioResourceTopology/ResourceTopology.hpp`): `Node::id` remains the dense snapshot-local index, semantic descriptors are immutable and order-unspecified, and no public protocol or mutable graph surface is introduced.
- OUT-005 Explicit owner, role, typed structural discriminator, synthetic-site, and multi-slot derivation in the existing Builder (`src/StyioResourceTopology/ResourceTopology.cpp`): Qualified rebuild and non-semantic-edit stability do not depend on forbidden inputs, repeated sites stay distinct, and all nodes remain in the one existing graph.
- OUT-006 Explicit compilation identity input and anonymous fallback retained with the Sema-owned topology lifecycle (`src/StyioSema/SemaContext.hpp`): Successful artifacts expose qualification and const semantic descriptors for their matching root, scalar no-op remains artifact-free, and lowering continues to reuse rather than rebuild.
- OUT-007 Fresh-parser topology tests for deterministic keys, forbidden-input independence, clean rebuilds, trivia, unrelated edits, logical-scope changes, renames and rewrites, repeated sites, multi-slot declarations, synthetic roles, and collision failure (`tests/resource_topology_test.cpp`): Tests compare sets of stable semantic descriptors instead of node-vector positions or diagnostic labels.
- OUT-008 Sema lifecycle and lowering optimizer tests over the same immutable identity-bearing artifact (`tests/typeinfer_internal_test.cpp`): Qualified descriptors are visible only through the Sema-owned artifact, anonymous fallback is explicitly unqualified, reanalysis is fresh, and optimization cannot mutate identity.
- OUT-009 Current compiler-internal identity scope, stability envelope, forbidden inputs, ambiguity boundary, and deferred public/runtime work (`docs/design/Styio-Observable-Language.md`): Documentation describes only the implemented internal capability and leaves public publication and lineage deferred.
- OUT-010 Maintainer rules and exact focused verification commands (`docs/teams/SEMA-IR-RUNBOOK.md`): Future topology changes preserve identity decomposition, lifecycle ownership, and the one-graph layering boundary.
- OUT-011 Discoverable focused semantic-identity and compatibility evidence (`workflows/TEST-CATALOG.md`): Maintainers can rerun the new metamorphic, lifecycle, optimization, layering, and compatibility oracles directly.
- OUT-012 Regenerated tracked Better Plan index (`docs/plan/INDEX.md`): Repository documentation gates index PLAN-003 without hand-editing generated content.

Internal Node graph
- NODE-001 capture-identity-and-compatibility-baselines · after: none · Record the current canonical module validation diagnostics, dense node/debug behavior, topology lifecycle, scalar fast path, representative accepted/rejected programs, and StyioIR/codegen/runtime expectations that focused tests must preserve.
- NODE-002 implement-neutral-identity-core · after: none · Add the explicit qualified/anonymous scope value, shared canonical logical-module rule, deterministic length-prefixed encoder, 128-bit SHA-256-derived opaque value, and ephemeral exact-key collision guard under `StyioUtil`, then wire the implementation into the frontend source list.
- NODE-003 reuse-canonical-module-rules · after: NODE-002 · Replace callable-interface and final import-module duplicate canonical checks with the neutral rule while preserving existing normalization ownership and exact diagnostics.
- NODE-004 thread-identity-scope-through-sema · after: NODE-002 · Add qualified-scope construction and explicit anonymous default construction to Sema/Lowerer, carry the scope through top-level inference and validation, and preserve root matching, scalar no-op, failed-analysis clearing, and lowering reuse.
- NODE-005 assign-identities-to-topology-sites · after: NODE-002 · Add typed roles and canonical site derivation to every node creation path, update AST memoization to include semantic subsites, give synthetic nodes explicit owner/role keys, attach immutable descriptors to the existing artifact, and leave dense IDs, labels, edges, and algorithms untouched.
- NODE-006 prove-topology-identity-contract · after: NODE-001, NODE-003, NODE-005 · Add fresh-parser metamorphic and low-level tests for canonical encoding, collision failure, clean rebuilds, trivia and parser-label independence, unrelated edits, distinct qualified scopes, allowed rename/rewrite changes, repeated anonymous sites, multi-slot declarations, synthetic roles, and unchanged dense/debug behavior.
- NODE-007 prove-sema-lifecycle-and-optimizer-immutability · after: NODE-001, NODE-004, NODE-005 · Extend Sema and lowering internal tests to observe qualified and anonymous descriptors through the root-matched artifact, compare descriptor sets rather than positions, verify fresh reanalysis, and prove opt-level zero and optimized lowering leave the artifact identities unchanged.
- NODE-008 update-identity-contracts-and-runbook · after: NODE-003, NODE-006, NODE-007 · Update the observable-language current-state and identity sections, add the Sema/runbook invariant and exact focused catalog entries, and regenerate the plan index while keeping snapshots, serialization, lineage, runtime, scheduler, IDE/Vityo, and caches explicitly deferred.
- NODE-009 verify-focused-identity-closure · after: NODE-008 · Build the affected targets; run new identity tests plus existing topology, Sema/lowering, callable-module, scalar, resource, stream, task, IDE diagnostic, and graph-algorithm evidence; then run architecture, docs, local-info, hygiene, and diff gates exactly once for this Task.

Design
- approach
  - Introduce `styio::semantic_identity::Scope` as an immutable value with two constructors: `qualified(project_package_identity, logical_module_identity)` and `anonymous()`. Qualified construction rejects empty or path-shaped project/package identities and validates the module through the shared canonical slash-form rule. Anonymous construction has no caller-supplied pseudo-name and reports `is_globally_comparable() == false`.
  - Keep the default `StyioSemaContext` and `AstToStyioIRLowerer` source-compatible by delegating explicitly to `Scope::anonymous()`. Add constructor overloads for a caller-provided qualified scope. Do not add a CLI flag or infer a scope in `main.cpp`, CompilerBridge, PipelineCheck, or tests from their parser filenames.
  - Move only the canonical module predicate/error classification into `StyioUtil`; keep callable-interface and import-loader wrappers responsible for their established diagnostic wording and dotted-import normalization. ResourceTopology includes the utility directly and never includes CallableInterface, CallableModuleLoader, or IDE headers.
  - Represent the opaque digest as exactly 16 digest bytes plus explicit qualification metadata, with equality and hashing but no production text, hex, serialization, parse, ordering, or source-key API. Derive the bytes from the first 128 bits of LLVM SHA-256, already used in the repository, over one canonical preimage.
  - Encode the preimage as a fixed version-domain field followed by scope kind, qualified project/package and module fields when present, owner-component count and components, typed semantic role, and discriminator-component count and components. Encode counts and byte lengths as fixed-width big-endian integers; reject oversize fields instead of truncating. Treat semantic spellings as exact parser-owned bytes without locale folding or filesystem normalization.
  - Keep a per-Builder registry from the 128-bit digest to the exact canonical preimage. Reuse is allowed only when the same AST pointer and same semantic subsite memo key prove a repeated visit. A different exact key with the same digest, or a second intended node with the same exact key, raises one privacy-safe internal compiler failure; never salt, retry, append graph order, or disclose the preimage.
  - Extend `Node` with `semantic_id` and a topology-owned `SemanticRole` enum. Require both at `Graph::add_node`; continue assigning the dense `id` from vector size. Do not include either field in `debug_string()`, diagnostics, edge addressing, cycle detection, node counts, or ordering.
  - Build owner paths from explicit typed semantic boundaries such as program, named function or resource method, named task/block, state, binding, snapshot, handle, and resource slot. Include semantic declaration, callee, resource-family, operator, and normalized type spellings where they identify the site; never reuse human diagnostic labels as keys.
  - Build local discriminators from child relation (`initializer`, `lhs`, `rhs`, `argument`, `handler`, `driver`, `slot`, `body`, or equivalent), typed AST/operator shape, and same-signature sibling occurrence within that owner. Ignore comment/no-op trivia when assigning occurrences. Do not include literal payloads, complete subtree text, a raw body digest, source position, vector index alone, or graph creation order.
  - Change AST memoization from one node per AST pointer to one node per `(AST pointer, semantic subsite)` for the duration of the build. The pointer remains only a local lookup key. Resource declaration slots use their slot semantic name and normalized declared type as distinct subsites, preventing the current second-and-later slot collapse without changing parser, Sema, lowering, or resource behavior.
  - Assign explicit roles and owners to non-AST and shared-AST creations: root program or standalone block, state-window ledger owned by its state, task failure domain owned by its task, and scope-exit destroy sink owned by its root scope. Give series ledgers and other synthesized helpers their own typed roles as part of the same exhaustive node-site audit.
  - Carry the scope into existing `BuildOptions` for raw topology tests and into `validate_or_throw` for Sema. Preserve `ValidatedArtifact` move-only ownership, root-match lifecycle, scalar-noop state, failed-analysis clearing, and lowering's exact artifact reuse. Do not annotate AST, clone Graph, or construct identity during lowering.
  - Add an order-unspecified const descriptor observation on `ValidatedArtifact` containing only `NodeKind`, `SemanticRole`, opaque identity, and qualification. Tests normalize descriptors into sets or maps keyed by semantic descriptor fields; they never correlate vector positions, dense IDs, labels, AST addresses, or source locations across builds.
  - Each metamorphic case creates new tokens, parser context, AST, analyzer, and artifact with a fixed qualified scope. Vary whitespace, comments, formatting, parser file labels, optimization level, and unrelated declarations independently; separately vary project/package, module, declaration spelling, and typed structure to prove the approved positive and negative stability boundaries.
  - Test repeated labels and anonymous expressions as unordered identity sets. For indistinguishable same-owner siblings, assert uniqueness and rebuild stability without promising stability after insert/delete/reorder inside that ambiguity class. Test one multi-slot `ResourceDeclAST` directly to prove all slots survive as distinct nodes and preserve existing edges.
  - Test the collision guard through a narrow internal unit seam that records a supplied derived digest and exact preimage; do not inject a production hash strategy or add mutable global counters. The production path still has exactly one fixed algorithm.
  - Capture descriptor sets before lowering, after opt-level-zero pass construction, and after optimized lowering; assert the same Sema artifact and descriptors remain unchanged. Existing IR and execution expectations are compatibility oracles, not regenerated outputs.
  - Update `Styio-Observable-Language.md` from “persistent identity deferred” to the implemented internal-only capability and retain all public snapshot, ordering, source-anchor, serialization, lineage, runtime, scheduler, and consumer work as deferred. Add only the corresponding Sema/runbook rule, test-catalog command, and generated plan index entry.
- patterns
  - pattern_catalog: refactoring-guru-catalog-22-v1
  - candidate: Strategy
  - decision: reject
  - pressure: Collision tests could motivate a replaceable hash algorithm, but production has one versioned deterministic derivation and no runtime algorithm-selection requirement.
  - expected_benefit: none beyond test interception; replaceability would weaken the fixed identity contract and add dispatch state.
  - simpler_alternative: Use one pure encoder and LLVM SHA-256 function plus an ephemeral exact-key collision guard, and exercise the guard directly through a narrow low-level test seam.
  - application: Keep ordinary immutable values and pure functions in `StyioUtil`, with topology-specific owner/role derivation inside the existing ResourceTopology Builder.
  - costs_and_rejections: A Strategy, Factory, global registry, Facade, or Observer would add indirection, mutable lifecycle, or a second coordination surface without improving current correctness; no GoF pattern is adopted.

Acceptance
- AC-001 covers REQ-006, REQ-007, REQ-008, REQ-013, OUT-001, OUT-002, OUT-003
  - Given Canonical and invalid logical module spellings, qualified scopes, the default anonymous compiler construction, and two different exact keys forced through one collision-guard digest
  - When the low-level identity and existing callable-module tests run
  - Then module acceptance is shared without diagnostic drift, qualified scopes retain explicit project/package and module semantics, anonymous artifacts are explicitly not globally comparable, canonical encoding is deterministic and length-unambiguous, and the forced collision fails once without salt or probing
  - Oracle: `StyioSemanticIdentity.CanonicalModuleRulesAndQualificationAreExplicit`, `StyioSemanticIdentity.LengthPrefixedDigestIsDeterministic`, and `StyioSemanticIdentity.CollisionGuardFailsClosed` pass, and the `callable_interfaces` plus `portable_generic_interfaces` labels remain green
  - Evidence: command from identity primitive and callable-module focused tests
- AC-002 covers REQ-002, REQ-003, REQ-005, OUT-004, OUT-005, OUT-007
  - Given Two fresh compilations of the same typed resource program under one fixed qualified scope, plus variants differing only in whitespace, comments, formatting, parser filename, and path-shaped file-resource label text
  - When descriptor sets are collected from each Sema-owned artifact
  - Then corresponding node semantic identities are equal while each build has fresh AST and analyzer addresses, and no source location, parser filename, file label, pointer, session-local ID, or raw source digest affects the result
  - Oracle: `StyioResourceTopology.SemanticIdsSurviveFreshRebuildAndTrivia` and `StyioResourceTopology.SemanticIdsIgnoreSourceLocationsFileNamesAndLabels` pass using descriptor-set equality rather than vector positions
  - Evidence: command from clean-rebuild and forbidden-input metamorphic tests
- AC-003 covers REQ-002, REQ-003, REQ-004, REQ-006, OUT-005, OUT-007
  - Given A baseline program, a fresh variant with an unrelated differently shaped declaration, variants with a different project/package or module identity, and variants that rename or structurally rewrite one owned site
  - When stable descriptors are matched by kind and typed role
  - Then unaffected baseline identities survive the unrelated edit, every qualified scope change separates identity domains, and only the affected rename/rewrite region is permitted to differ
  - Oracle: `StyioResourceTopology.UnrelatedEditsPreserveUnaffectedSemanticIds` and `StyioResourceTopology.ScopeRenameAndRewriteBoundariesAreExplicit` pass
  - Evidence: command from semantic stability-boundary metamorphic tests
- AC-004 covers REQ-009, REQ-010, REQ-001, OUT-004, OUT-005, OUT-007
  - Given Repeated anonymous operations with identical diagnostic labels, one multi-slot `ResourceDeclAST`, a state window, a task, and close-capable resources requiring scope cleanup
  - When the topology Builder creates nodes
  - Then every intended site has a unique semantic identity, all declaration slots remain present with their existing graph relations, and program, state-window ledger, task failure domain, and scope-exit destroy sink nodes report their explicit typed roles
  - Oracle: `StyioResourceTopology.RepeatedAnonymousSitesRemainDistinct`, `StyioResourceTopology.MultiSlotResourceDeclarationHasDistinctSemanticSites`, and `StyioResourceTopology.SyntheticNodesHaveExplicitSemanticRoles` pass
  - Evidence: command from repeated, multi-slot, and synthetic-site tests
- AC-005 covers REQ-011, REQ-012, REQ-006, REQ-013, OUT-006, OUT-008
  - Given A qualified resource-bearing root, an anonymous ordinary root, scalar no-op, failed analysis, reanalysis, lowering, and opt-level-zero and optimized pass paths
  - When tests inspect only the matching `ValidatedArtifact` descriptor seam before and after lowering/optimization
  - Then qualification is accurate, descriptors remain immutable on the same Sema-owned artifact, scalar no-op remains artifact-free, failed or replaced roots expose no stale result, and lowering creates no graph or identity fallback
  - Oracle: `StyioSemaTopology.ValidatedArtifactOwnsQualifiedSemanticDescriptors`, `StyioSemaTopology.AnonymousFallbackAndReanalysisStayExplicit`, and `StyioLoweringInternal.OptimizationDoesNotMutateTopologySemanticIds` pass together with the existing lifecycle and no-rebuild structural oracle
  - Evidence: command from Sema lifecycle and optimizer immutability tests
- AC-006 covers REQ-001, REQ-012, REQ-011, OUT-004, OUT-005, OUT-007, OUT-008
  - Given Identity-bearing topology construction over existing representative graph and language fixtures
  - When topology, Sema, lowering, scalar, file, state, stream, task, callable-interface, and IDE diagnostic suites run
  - Then dense node IDs remain contiguous, edge endpoints and cycle/count results are unchanged, `debug_string()` contains no semantic IDs and matches existing expectations, accepted programs and runtime results stay accepted, rejected programs keep diagnostics, and StyioIR/codegen output remains unchanged
  - Oracle: `StyioResourceTopology.DenseIdsDebugAndAlgorithmsRemainCompatible` passes and every declared compatibility label exits zero
  - Evidence: command from focused compiler behavior compatibility matrix
- AC-007 covers REQ-007, REQ-013, REQ-014, OUT-002, OUT-009, OUT-010, OUT-011, OUT-012
  - Given The implementation, tests, and owning contracts are complete
  - When architecture, docs, local-information, repository-hygiene, and diff gates run
  - Then ResourceTopology has no CallableInterface or IDE dependency, no public or runtime surface was added, the current contract and runbooks describe the exact internal stability boundary, PLAN-003 is indexed, repository-visible text contains no local information, and the worktree has no hygiene or whitespace errors
  - Oracle: `architecture_layer_gate`, `scripts/docs-gate.sh --mode worktree`, `scripts/local-info-leak-gate.py --mode worktree`, `scripts/repo-hygiene-gate.py --mode worktree`, the ResourceTopology include-direction `rg` assertion, and `git diff --check` all exit zero
  - Evidence: command from focused architecture and repository contract gates

Focused regression
- `cmake --build build/default --target styio styio_resource_topology_test styio_typeinfer_internal_test styio_lowering_internal_test styio_test styio_ide_test -j2`
- `ctest --test-dir build/default -R '^(StyioSemanticIdentity\.(CanonicalModuleRulesAndQualificationAreExplicit|LengthPrefixedDigestIsDeterministic|CollisionGuardFailsClosed)|StyioResourceTopology\.(SemanticIdsSurviveFreshRebuildAndTrivia|SemanticIdsIgnoreSourceLocationsFileNamesAndLabels|UnrelatedEditsPreserveUnaffectedSemanticIds|ScopeRenameAndRewriteBoundariesAreExplicit|RepeatedAnonymousSitesRemainDistinct|MultiSlotResourceDeclarationHasDistinctSemanticSites|SyntheticNodesHaveExplicitSemanticRoles|DenseIdsDebugAndAlgorithmsRemainCompatible)|StyioSemaTopology\.(ValidatedArtifactOwnsQualifiedSemanticDescriptors|AnonymousFallbackAndReanalysisStayExplicit)|StyioLoweringInternal\.OptimizationDoesNotMutateTopologySemanticIds)$' --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -L '^(resource_topology|sema_internal|lowering_internal)$' --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -L '^(callable_interfaces|portable_generic_interfaces|scalar_expressions|file_resources|state_resources|stream_processing|task_resources)$' --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -R '^StyioSemanticBridge\.ResourceTopologyDiagnosticRemainsCompilerOwned$' --output-on-failure --no-tests=error`
- `test "$(rg -c 'resource_topology::validate_or_throw\(' src/StyioSema/TypeInfer.cpp)" -eq 1 && ! rg -n 'resource_topology::(build|validate_or_throw|validation_is_noop_for_scalar_program)' src/StyioLowering/AstToStyioIR.cpp`
- `test -z "$(rg -n '#include .*Styio(Sema/(CallableInterface|CallableModuleLoader)|Services/StyioIDE)' src/StyioResourceTopology || true)"`
- `ctest --test-dir build/default -R '^architecture_layer_gate$' --output-on-failure --no-tests=error`
- `bash scripts/docs-gate.sh --mode worktree`
- `python3 scripts/local-info-leak-gate.py --mode worktree`
- `python3 scripts/repo-hygiene-gate.py --mode worktree`
- `git diff --check`
- paths: src/StyioUtil/SemanticIdentity.hpp, src/StyioUtil/SemanticIdentity.cpp, src/cmake/StyioFrontendSources.cmake, src/StyioSema/CallableInterface.cpp, src/StyioSema/CallableModuleLoader.cpp, src/StyioResourceTopology/ResourceTopology.hpp, src/StyioResourceTopology/ResourceTopology.cpp, src/StyioSema/SemaContext.hpp, src/StyioSema/TypeInfer.cpp, src/StyioLowering/AstToStyioIRLowerer.hpp, tests/resource_topology_test.cpp, tests/typeinfer_internal_test.cpp, tests/lowering_internal_test.cpp, docs/design/Styio-Observable-Language.md, docs/teams/SEMA-IR-RUNBOOK.md, workflows/TEST-CATALOG.md, docs/plan/INDEX.md

### TASK-002 close-architecture-and-documentation-gates

Worker: general · Tier: standard · Workload: light · Verification: code · Frontier: parallel

Outcome: The documented architecture CTest oracle is executable and all repository-required team documentation is current for PLAN-003 without changing the compiler implementation.

Risks: quality, shared_resource
Requirements: REQ-015, REQ-016
Writes: tests/CMakeLists.txt, docs/teams/DOCS-ECOSYSTEM-RUNBOOK.md, docs/teams/TEST-QUALITY-RUNBOOK.md, docs/teams/DOC-STATS.md
Exclusive resources: configured CMake and CTest tree build/plan-003-contract

In scope
- Register the existing architecture layer script under the documented CTest name.
- Update only the owning team runbooks and generated documentation statistics required by docs-gate.
- Run the focused repository-contract gates.

Out of scope
- Compiler identity implementation, compiler tests, language behavior, new architecture policy, public protocols, runtime integration, and unrelated documentation.

Outputs
- OUT-013 Executable architecture layer CTest registration (`tests/CMakeLists.txt`): The configured CTest tree contains exactly one `architecture_layer_gate` test that invokes the existing repository script.
- OUT-014 Current Docs / Ecosystem ownership record (`docs/teams/DOCS-ECOSYSTEM-RUNBOOK.md`): Docs / Ecosystem maintenance guidance records the tracked PLAN-003 contract and generated-index responsibilities.
- OUT-015 Current Test Quality ownership record (`docs/teams/TEST-QUALITY-RUNBOOK.md`): Test Quality maintenance guidance records the semantic-identity metamorphic tests and architecture gate registration.
- OUT-016 Regenerated documentation statistics (`docs/teams/DOC-STATS.md`): Generated documentation statistics match the final tracked documentation tree.

Internal Node graph
- NODE-010 register-architecture-layer-gate · after: none · Register the existing architecture-layer script as the exact CTest gate already named by repository documentation and PLAN-003.
- NODE-011 update-required-team-docs · after: none · Update the two owning team runbooks and regenerate documentation statistics for the current authorized test and docs changes.
- NODE-012 verify-repository-contract-closure · after: NODE-010, NODE-011 · Reconfigure once and prove the architecture, docs, privacy, hygiene, and diff gates pass.

Design
- approach
  - Add one CTest registration in `tests/CMakeLists.txt` that invokes the existing `scripts/architecture-layer-gate.py` with the configured Python interpreter and repository working directory; do not duplicate or wrap the script.
  - Update only the required Docs / Ecosystem and Test Quality runbook maintenance records, then regenerate `docs/teams/DOC-STATS.md` through the repository documentation tooling.
  - Configure a dedicated contract-check build tree once so CTest discovers the new gate, then run the architecture and documentation gates without rebuilding or rerunning compiler suites.

Acceptance
- AC-008 covers REQ-015, REQ-016, OUT-013, OUT-014, OUT-015, OUT-016
  - Given The repository catalog documents `architecture_layer_gate`, the script exists, and PLAN-003 changes compiler tests and tracked documentation
  - When the configured test tree and repository documentation gates run
  - Then CTest invokes the real architecture script and the owning team runbooks and documentation statistics accurately cover the authorized changes without altering compiler behavior
  - Oracle: `ctest --test-dir build/default -R '^architecture_layer_gate$' --output-on-failure --no-tests=error`, `bash scripts/docs-gate.sh --mode worktree`, `python3 scripts/local-info-leak-gate.py --mode worktree`, `python3 scripts/repo-hygiene-gate.py --mode worktree`, and `git diff --check` all exit zero
  - Evidence: command from architecture registration and repository documentation gates

Focused regression
- `cmake -S . -B build/plan-003-contract`
- `ctest --test-dir build/plan-003-contract -R '^architecture_layer_gate$' --output-on-failure --no-tests=error`
- `bash scripts/docs-gate.sh --mode worktree`
- `python3 scripts/local-info-leak-gate.py --mode worktree`
- `python3 scripts/repo-hygiene-gate.py --mode worktree`
- `git diff --check`
- paths: tests/CMakeLists.txt, docs/teams/DOCS-ECOSYSTEM-RUNBOOK.md, docs/teams/TEST-QUALITY-RUNBOOK.md, docs/teams/DOC-STATS.md

## Full regression

Run inside the sole Reviewer session after every repair is integrated.

- `cmake --build build/default -j2`
- `ctest --test-dir build/default --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -R '^architecture_layer_gate$' --output-on-failure --no-tests=error`
- `bash scripts/docs-gate.sh --mode worktree`
- `python3 scripts/local-info-leak-gate.py --mode worktree`
- `python3 scripts/repo-hygiene-gate.py --mode worktree`
- `git diff --check`
- paths: src/StyioUtil, src/cmake/StyioFrontendSources.cmake, src/StyioSema, src/StyioResourceTopology, src/StyioLowering, tests, docs/design/Styio-Observable-Language.md, docs/teams/SEMA-IR-RUNBOOK.md, docs/teams/DOCS-ECOSYSTEM-RUNBOOK.md, docs/teams/TEST-QUALITY-RUNBOOK.md, docs/teams/DOC-STATS.md, tests/CMakeLists.txt, workflows/TEST-CATALOG.md, docs/plan
