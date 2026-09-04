# Persistent semantic identities for resource topology

## Requirements

- dense-local-node-index: Keep `Node::id` as the zero-based snapshot-local dense index used by edges, cycle detection, diagnostics, and existing topology algorithms; add semantic identity as a separate field. — source: user-request, repository-contract
- qualified-rebuild-stability: Fresh parsers, analyzers, ASTs, and topology artifacts built for the same explicitly qualified project/package and logical module must produce the same semantic identity for the same semantic site. — source: user-request
- nonsemantic-edit-stability: Whitespace, comments, formatting, optimization level, parser file labels, and unrelated semantic edits must not change identities for unaffected sites. — source: user-request
- semantic-change-boundary: Renames, logical module moves, and structural rewrites may change the affected identities without creating a lineage or compatibility promise. — source: user-request
- forbidden-input-exclusion: Identity derivation must not use machine or absolute paths, filenames, working directories, byte offsets, line or column locations, AST/IR/LLVM addresses, graph/build/allocation order alone, raw source or body digests, runtime identifiers, UI or random identifiers, or session-local `SymbolId`, `TypeId`, or HIR identifiers. — source: user-request
- explicit-scope-and-anonymous-fallback: Provide an explicit compiler-owned identity-scope input containing project/package identity and canonical slash-form logical module identity, plus an explicitly unqualified anonymous fallback that cannot be represented as globally comparable or publishable identity and is never inferred from a filename or working directory. — source: user-request
- neutral-module-identity-rules: Reuse the callable module canonical-identity rules from a neutral low-level utility so ResourceTopology depends on neither CallableInterface nor IDE services and existing callable-interface diagnostics remain unchanged. — source: user-request, repository-contract
- canonical-key-and-collision-safety: Derive the smallest sufficient fixed opaque digest from a deterministic versioned, length-prefixed exact key; detect different exact keys yielding the same digest and fail closed without salting, probing, or build-order repair. — source: user-request
- distinct-semantic-sites: Repeated labels, anonymous sites, and every slot of a multi-slot `ResourceDeclAST` must receive distinct site identities even when one declaration AST pointer is reused, without changing language behavior. — source: user-request, repository-contract
- explicit-synthetic-roles: Program roots, state-window ledgers, task failure domains, scope-exit destroy sinks, and other compiler-synthesized nodes must derive identity from explicit semantic roles and owners rather than null pointers, labels, or insertion order. — source: user-request
- sema-owned-immutable-identities: Preserve the existing Sema-owned `ValidatedArtifact` lifecycle and lowering reuse, add only immutable internal identity observation, and create no second topology graph. — source: user-request, repository-contract
- optimization-and-behavior-preservation: Optimization must not mutate semantic identities, while accepted programs, diagnostics, StyioIR/codegen, runtime behavior, scalar fast-path behavior, topology labels, and existing topology algorithms remain unchanged. — source: user-request, repository-contract
- internal-only-surface: Keep identities compiler-internal with no public snapshot, schema, serialization, ABI/SDK, lineage, partial-topology, runtime/scheduler, Vityo, cache, or retained experiment surface. — source: user-request
- focused-evidence-and-contracts: Add deterministic internal tests using fresh compilation objects and fixed logical identities, then update only the owning design contract, Sema runbook, test catalog, and generated plan index required by touched surfaces. — source: user-request, repository-contract

## Architecture
Summary: Add one compiler-internal qualified-or-anonymous semantic identity scope, one neutral deterministic identity primitive, and one topology-specific semantic-site derivation path to the existing Sema-owned graph; keep local graph indices and every external or runtime boundary unchanged.
Notes:
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
- One coupled Task owns utility extraction, compiler input plumbing, topology assignment, tests, and contracts because their interfaces and acceptance are inseparable. The single-Task frontier is parallel-safe by construction; its internal Node DAG branches utility consumers and test suites at their real dependency points.

## Task: compiler-internal-persistent-semantic-identities
Outcome: Every node in the existing Sema-owned resource topology carries a deterministic opaque semantic identity with explicit qualification and fail-closed collision handling, while dense graph behavior and all ordinary compiler behavior remain unchanged and focused evidence proves the approved stability envelope.
Scope in:
- Add the neutral internal identity scope, canonical logical-module validator, length-prefixed key encoder, 128-bit SHA-256-derived value, and per-build exact-key collision guard.
- Reuse the neutral canonical module rule in callable-interface and callable-module validation without changing accepted identifiers or diagnostics.
- Thread an explicit qualified or anonymous identity scope through `StyioSemaContext`, `AstToStyioIRLowerer`, BuildOptions, top-level topology validation, and the existing `ValidatedArtifact` lifecycle.
- Add typed semantic roles, owner paths, local structural discriminators, and semantic IDs to all existing resource-topology node creation paths while preserving dense local IDs and graph semantics.
- Correct multi-slot resource declaration memoization so each semantic slot is a distinct topology site even though the declaration AST pointer is shared.
- Add immutable artifact descriptors and fresh-build metamorphic tests for qualification, stability, change boundaries, repeated and synthetic sites, collision handling, lifecycle visibility, and optimizer immutability.
- Run focused compatibility and repository gates and update only the owning observable-language contract, Sema runbook, test catalog, and generated plan index.
Scope out:
- Public topology snapshots, schemas, serialization, exported text or hex identity formats, ABI/SDK guarantees, compatibility promises, lineage, deltas, queries, source anchors, and partial topology.
- Runtime IDs, scheduler or runtime integration, Vityo or IDE identity integration, telemetry, caches, persistence stores, remote services, and retained stash experiments.
- Canonical ordering of snapshots, nodes, or edges; edge semantic identities; graph replacement; a second graph; or changes to topology node kinds, edge kinds, labels, validation rules, and algorithms.
- New language syntax, changed accepted programs or diagnostics, changed StyioIR/codegen/runtime behavior, scalar-fast-path expansion, and broad refactoring outside the touched identity boundary.
- Deriving scope from the parser filename, source path, repository checkout, process environment, current directory, import resolution path, or any source/body digest.
Outputs:
- neutral-semantic-identity-core: Compiler-internal qualified/anonymous scope, shared canonical logical-module validation, length-prefixed exact-key encoding, 128-bit SHA-256-derived opaque identity, and collision guard — artifact: src/StyioUtil/SemanticIdentity.hpp — guarantee: All identity consumers share one low-level deterministic implementation with explicit qualification and fail closed on digest collisions without retaining a cache or exposing key material.
- semantic-identity-build-wiring: The neutral identity implementation is linked through the existing frontend source boundary — artifact: src/cmake/StyioFrontendSources.cmake — guarantee: ResourceTopology and callable-interface consumers use one implementation without reversing Sema, IDE, or backend dependency directions.
- shared-module-identity-rules: Callable interface and import-module validation delegate canonical slash-form checks to the neutral utility — artifact: src/StyioSema/CallableInterface.cpp — guarantee: Existing canonical module acceptance and exact callable diagnostics remain compatible while ResourceTopology has no CallableInterface dependency.
- topology-semantic-node-contract: Separate opaque semantic identity and typed semantic role on every existing topology node — artifact: src/StyioResourceTopology/ResourceTopology.hpp — guarantee: `Node::id` remains the dense snapshot-local index, semantic descriptors are immutable and order-unspecified, and no public protocol or mutable graph surface is introduced.
- deterministic-site-derivation: Explicit owner, role, typed structural discriminator, synthetic-site, and multi-slot derivation in the existing Builder — artifact: src/StyioResourceTopology/ResourceTopology.cpp — guarantee: Qualified rebuild and non-semantic-edit stability do not depend on forbidden inputs, repeated sites stay distinct, and all nodes remain in the one existing graph.
- sema-identity-scope-owner: Explicit compilation identity input and anonymous fallback retained with the Sema-owned topology lifecycle — artifact: src/StyioSema/SemaContext.hpp — guarantee: Successful artifacts expose qualification and const semantic descriptors for their matching root, scalar no-op remains artifact-free, and lowering continues to reuse rather than rebuild.
- topology-identity-metamorphic-evidence: Fresh-parser topology tests for deterministic keys, forbidden-input independence, clean rebuilds, trivia, unrelated edits, logical-scope changes, renames and rewrites, repeated sites, multi-slot declarations, synthetic roles, and collision failure — artifact: tests/resource_topology_test.cpp — guarantee: Tests compare sets of stable semantic descriptors instead of node-vector positions or diagnostic labels.
- lifecycle-and-optimization-evidence: Sema lifecycle and lowering optimizer tests over the same immutable identity-bearing artifact — artifact: tests/typeinfer_internal_test.cpp — guarantee: Qualified descriptors are visible only through the Sema-owned artifact, anonymous fallback is explicitly unqualified, reanalysis is fresh, and optimization cannot mutate identity.
- persistent-identity-contract: Current compiler-internal identity scope, stability envelope, forbidden inputs, ambiguity boundary, and deferred public/runtime work — artifact: docs/design/Styio-Observable-Language.md — guarantee: Documentation describes only the implemented internal capability and leaves public publication and lineage deferred.
- persistent-identity-runbook: Maintainer rules and exact focused verification commands — artifact: docs/teams/SEMA-IR-RUNBOOK.md — guarantee: Future topology changes preserve identity decomposition, lifecycle ownership, and the one-graph layering boundary.
- persistent-identity-test-catalog: Discoverable focused semantic-identity and compatibility evidence — artifact: workflows/TEST-CATALOG.md — guarantee: Maintainers can rerun the new metamorphic, lifecycle, optimization, layering, and compatibility oracles directly.
- current-plan-index: Regenerated tracked Better Plan index — artifact: docs/plan/INDEX.md — guarantee: Repository documentation gates index PLAN-003 without hand-editing generated content.
Owns:
- src/StyioUtil/SemanticIdentity.hpp
- src/StyioUtil/SemanticIdentity.cpp
- src/cmake/StyioFrontendSources.cmake
- src/StyioSema/CallableInterface.cpp
- src/StyioSema/CallableModuleLoader.cpp
- src/StyioResourceTopology/ResourceTopology.hpp
- src/StyioResourceTopology/ResourceTopology.cpp
- src/StyioSema/SemaContext.hpp
- src/StyioSema/TypeInfer.cpp
- src/StyioLowering/AstToStyioIRLowerer.hpp
- tests/resource_topology_test.cpp
- tests/typeinfer_internal_test.cpp
- tests/lowering_internal_test.cpp
- docs/design/Styio-Observable-Language.md
- docs/teams/SEMA-IR-RUNBOOK.md
- workflows/TEST-CATALOG.md
- docs/plan/INDEX.md
Exclusive:
- configured CMake and CTest tree build/default
- generated documentation and plan indexes
- repository worktree delivery gates
Worker: general
Difficulty: complex
Workload: heavy
Verification: code
Risks:
- quality
- performance
- privacy
- shared_resource
Nodes:
- capture-identity-and-compatibility-baselines: Record the current canonical module validation diagnostics, dense node/debug behavior, topology lifecycle, scalar fast path, representative accepted/rejected programs, and StyioIR/codegen/runtime expectations that focused tests must preserve.
- implement-neutral-identity-core: Add the explicit qualified/anonymous scope value, shared canonical logical-module rule, deterministic length-prefixed encoder, 128-bit SHA-256-derived opaque value, and ephemeral exact-key collision guard under `StyioUtil`, then wire the implementation into the frontend source list.
- reuse-canonical-module-rules: Replace callable-interface and final import-module duplicate canonical checks with the neutral rule while preserving existing normalization ownership and exact diagnostics. — after: implement-neutral-identity-core
- thread-identity-scope-through-sema: Add qualified-scope construction and explicit anonymous default construction to Sema/Lowerer, carry the scope through top-level inference and validation, and preserve root matching, scalar no-op, failed-analysis clearing, and lowering reuse. — after: implement-neutral-identity-core
- assign-identities-to-topology-sites: Add typed roles and canonical site derivation to every node creation path, update AST memoization to include semantic subsites, give synthetic nodes explicit owner/role keys, attach immutable descriptors to the existing artifact, and leave dense IDs, labels, edges, and algorithms untouched. — after: implement-neutral-identity-core
- prove-topology-identity-contract: Add fresh-parser metamorphic and low-level tests for canonical encoding, collision failure, clean rebuilds, trivia and parser-label independence, unrelated edits, distinct qualified scopes, allowed rename/rewrite changes, repeated anonymous sites, multi-slot declarations, synthetic roles, and unchanged dense/debug behavior. — after: capture-identity-and-compatibility-baselines, reuse-canonical-module-rules, assign-identities-to-topology-sites
- prove-sema-lifecycle-and-optimizer-immutability: Extend Sema and lowering internal tests to observe qualified and anonymous descriptors through the root-matched artifact, compare descriptor sets rather than positions, verify fresh reanalysis, and prove opt-level zero and optimized lowering leave the artifact identities unchanged. — after: capture-identity-and-compatibility-baselines, thread-identity-scope-through-sema, assign-identities-to-topology-sites
- update-identity-contracts-and-runbook: Update the observable-language current-state and identity sections, add the Sema/runbook invariant and exact focused catalog entries, and regenerate the plan index while keeping snapshots, serialization, lineage, runtime, scheduler, IDE/Vityo, and caches explicitly deferred. — after: reuse-canonical-module-rules, prove-topology-identity-contract, prove-sema-lifecycle-and-optimizer-immutability
- verify-focused-identity-closure: Build the affected targets; run new identity tests plus existing topology, Sema/lowering, callable-module, scalar, resource, stream, task, IDE diagnostic, and graph-algorithm evidence; then run architecture, docs, local-info, hygiene, and diff gates exactly once for this Task. — after: update-identity-contracts-and-runbook
Requirements:
- dense-local-node-index
- qualified-rebuild-stability
- nonsemantic-edit-stability
- semantic-change-boundary
- forbidden-input-exclusion
- explicit-scope-and-anonymous-fallback
- neutral-module-identity-rules
- canonical-key-and-collision-safety
- distinct-semantic-sites
- explicit-synthetic-roles
- sema-owned-immutable-identities
- optimization-and-behavior-preservation
- internal-only-surface
- focused-evidence-and-contracts
Design:
approach:
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
patterns:
- pattern_catalog: refactoring-guru-catalog-22-v1
- candidate: Strategy
- decision: reject
- pressure: Collision tests could motivate a replaceable hash algorithm, but production has one versioned deterministic derivation and no runtime algorithm-selection requirement.
- expected_benefit: none beyond test interception; replaceability would weaken the fixed identity contract and add dispatch state.
- simpler_alternative: Use one pure encoder and LLVM SHA-256 function plus an ephemeral exact-key collision guard, and exercise the guard directly through a narrow low-level test seam.
- application: Keep ordinary immutable values and pure functions in `StyioUtil`, with topology-specific owner/role derivation inside the existing ResourceTopology Builder.
- costs_and_rejections: A Strategy, Factory, global registry, Facade, or Observer would add indirection, mutable lifecycle, or a second coordination surface without improving current correctness; no GoF pattern is adopted.
Acceptance:
- Given: Canonical and invalid logical module spellings, qualified scopes, the default anonymous compiler construction, and two different exact keys forced through one collision-guard digest — When: the low-level identity and existing callable-module tests run — Then: module acceptance is shared without diagnostic drift, qualified scopes retain explicit project/package and module semantics, anonymous artifacts are explicitly not globally comparable, canonical encoding is deterministic and length-unambiguous, and the forced collision fails once without salt or probing — Oracle: `StyioSemanticIdentity.CanonicalModuleRulesAndQualificationAreExplicit`, `StyioSemanticIdentity.LengthPrefixedDigestIsDeterministic`, and `StyioSemanticIdentity.CollisionGuardFailsClosed` pass, and the `callable_interfaces` plus `portable_generic_interfaces` labels remain green — Evidence: command: identity primitive and callable-module focused tests — Covers: explicit-scope-and-anonymous-fallback, neutral-module-identity-rules, canonical-key-and-collision-safety, internal-only-surface, neutral-semantic-identity-core, semantic-identity-build-wiring, shared-module-identity-rules
- Given: Two fresh compilations of the same typed resource program under one fixed qualified scope, plus variants differing only in whitespace, comments, formatting, parser filename, and path-shaped file-resource label text — When: descriptor sets are collected from each Sema-owned artifact — Then: corresponding node semantic identities are equal while each build has fresh AST and analyzer addresses, and no source location, parser filename, file label, pointer, session-local ID, or raw source digest affects the result — Oracle: `StyioResourceTopology.SemanticIdsSurviveFreshRebuildAndTrivia` and `StyioResourceTopology.SemanticIdsIgnoreSourceLocationsFileNamesAndLabels` pass using descriptor-set equality rather than vector positions — Evidence: command: clean-rebuild and forbidden-input metamorphic tests — Covers: qualified-rebuild-stability, nonsemantic-edit-stability, forbidden-input-exclusion, topology-semantic-node-contract, deterministic-site-derivation, topology-identity-metamorphic-evidence
- Given: A baseline program, a fresh variant with an unrelated differently shaped declaration, variants with a different project/package or module identity, and variants that rename or structurally rewrite one owned site — When: stable descriptors are matched by kind and typed role — Then: unaffected baseline identities survive the unrelated edit, every qualified scope change separates identity domains, and only the affected rename/rewrite region is permitted to differ — Oracle: `StyioResourceTopology.UnrelatedEditsPreserveUnaffectedSemanticIds` and `StyioResourceTopology.ScopeRenameAndRewriteBoundariesAreExplicit` pass — Evidence: command: semantic stability-boundary metamorphic tests — Covers: qualified-rebuild-stability, nonsemantic-edit-stability, semantic-change-boundary, explicit-scope-and-anonymous-fallback, deterministic-site-derivation, topology-identity-metamorphic-evidence
- Given: Repeated anonymous operations with identical diagnostic labels, one multi-slot `ResourceDeclAST`, a state window, a task, and close-capable resources requiring scope cleanup — When: the topology Builder creates nodes — Then: every intended site has a unique semantic identity, all declaration slots remain present with their existing graph relations, and program, state-window ledger, task failure domain, and scope-exit destroy sink nodes report their explicit typed roles — Oracle: `StyioResourceTopology.RepeatedAnonymousSitesRemainDistinct`, `StyioResourceTopology.MultiSlotResourceDeclarationHasDistinctSemanticSites`, and `StyioResourceTopology.SyntheticNodesHaveExplicitSemanticRoles` pass — Evidence: command: repeated, multi-slot, and synthetic-site tests — Covers: distinct-semantic-sites, explicit-synthetic-roles, dense-local-node-index, topology-semantic-node-contract, deterministic-site-derivation, topology-identity-metamorphic-evidence
- Given: A qualified resource-bearing root, an anonymous ordinary root, scalar no-op, failed analysis, reanalysis, lowering, and opt-level-zero and optimized pass paths — When: tests inspect only the matching `ValidatedArtifact` descriptor seam before and after lowering/optimization — Then: qualification is accurate, descriptors remain immutable on the same Sema-owned artifact, scalar no-op remains artifact-free, failed or replaced roots expose no stale result, and lowering creates no graph or identity fallback — Oracle: `StyioSemaTopology.ValidatedArtifactOwnsQualifiedSemanticDescriptors`, `StyioSemaTopology.AnonymousFallbackAndReanalysisStayExplicit`, and `StyioLoweringInternal.OptimizationDoesNotMutateTopologySemanticIds` pass together with the existing lifecycle and no-rebuild structural oracle — Evidence: command: Sema lifecycle and optimizer immutability tests — Covers: sema-owned-immutable-identities, optimization-and-behavior-preservation, explicit-scope-and-anonymous-fallback, internal-only-surface, sema-identity-scope-owner, lifecycle-and-optimization-evidence
- Given: Identity-bearing topology construction over existing representative graph and language fixtures — When: topology, Sema, lowering, scalar, file, state, stream, task, callable-interface, and IDE diagnostic suites run — Then: dense node IDs remain contiguous, edge endpoints and cycle/count results are unchanged, `debug_string()` contains no semantic IDs and matches existing expectations, accepted programs and runtime results stay accepted, rejected programs keep diagnostics, and StyioIR/codegen output remains unchanged — Oracle: `StyioResourceTopology.DenseIdsDebugAndAlgorithmsRemainCompatible` passes and every declared compatibility label exits zero — Evidence: command: focused compiler behavior compatibility matrix — Covers: dense-local-node-index, optimization-and-behavior-preservation, sema-owned-immutable-identities, topology-semantic-node-contract, deterministic-site-derivation, topology-identity-metamorphic-evidence, lifecycle-and-optimization-evidence
- Given: The implementation, tests, and owning contracts are complete — When: architecture, docs, local-information, repository-hygiene, and diff gates run — Then: ResourceTopology has no CallableInterface or IDE dependency, no public or runtime surface was added, the current contract and runbooks describe the exact internal stability boundary, PLAN-003 is indexed, repository-visible text contains no local information, and the worktree has no hygiene or whitespace errors — Oracle: `architecture_layer_gate`, `scripts/docs-gate.sh --mode worktree`, `scripts/local-info-leak-gate.py --mode worktree`, `scripts/repo-hygiene-gate.py --mode worktree`, the ResourceTopology include-direction `rg` assertion, and `git diff --check` all exit zero — Evidence: command: focused architecture and repository contract gates — Covers: neutral-module-identity-rules, internal-only-surface, focused-evidence-and-contracts, semantic-identity-build-wiring, persistent-identity-contract, persistent-identity-runbook, persistent-identity-test-catalog, current-plan-index
Regression:
Commands:
- cmake --build build/default --target styio styio_resource_topology_test styio_typeinfer_internal_test styio_lowering_internal_test styio_test styio_ide_test -j2
- ctest --test-dir build/default -R '^(StyioSemanticIdentity\.(CanonicalModuleRulesAndQualificationAreExplicit|LengthPrefixedDigestIsDeterministic|CollisionGuardFailsClosed)|StyioResourceTopology\.(SemanticIdsSurviveFreshRebuildAndTrivia|SemanticIdsIgnoreSourceLocationsFileNamesAndLabels|UnrelatedEditsPreserveUnaffectedSemanticIds|ScopeRenameAndRewriteBoundariesAreExplicit|RepeatedAnonymousSitesRemainDistinct|MultiSlotResourceDeclarationHasDistinctSemanticSites|SyntheticNodesHaveExplicitSemanticRoles|DenseIdsDebugAndAlgorithmsRemainCompatible)|StyioSemaTopology\.(ValidatedArtifactOwnsQualifiedSemanticDescriptors|AnonymousFallbackAndReanalysisStayExplicit)|StyioLoweringInternal\.OptimizationDoesNotMutateTopologySemanticIds)$' --output-on-failure --no-tests=error
- ctest --test-dir build/default -L '^(resource_topology|sema_internal|lowering_internal)$' --output-on-failure --no-tests=error
- ctest --test-dir build/default -L '^(callable_interfaces|portable_generic_interfaces|scalar_expressions|file_resources|state_resources|stream_processing|task_resources)$' --output-on-failure --no-tests=error
- ctest --test-dir build/default -R '^StyioSemanticBridge\.ResourceTopologyDiagnosticRemainsCompilerOwned$' --output-on-failure --no-tests=error
- test "$(rg -c 'resource_topology::validate_or_throw\(' src/StyioSema/TypeInfer.cpp)" -eq 1 && ! rg -n 'resource_topology::(build|validate_or_throw|validation_is_noop_for_scalar_program)' src/StyioLowering/AstToStyioIR.cpp
- test -z "$(rg -n '#include .*Styio(Sema/(CallableInterface|CallableModuleLoader)|Services/StyioIDE)' src/StyioResourceTopology || true)"
- ctest --test-dir build/default -R '^architecture_layer_gate$' --output-on-failure --no-tests=error
- bash scripts/docs-gate.sh --mode worktree
- python3 scripts/local-info-leak-gate.py --mode worktree
- python3 scripts/repo-hygiene-gate.py --mode worktree
- git diff --check
Paths:
- src/StyioUtil/SemanticIdentity.hpp
- src/StyioUtil/SemanticIdentity.cpp
- src/cmake/StyioFrontendSources.cmake
- src/StyioSema/CallableInterface.cpp
- src/StyioSema/CallableModuleLoader.cpp
- src/StyioResourceTopology/ResourceTopology.hpp
- src/StyioResourceTopology/ResourceTopology.cpp
- src/StyioSema/SemaContext.hpp
- src/StyioSema/TypeInfer.cpp
- src/StyioLowering/AstToStyioIRLowerer.hpp
- tests/resource_topology_test.cpp
- tests/typeinfer_internal_test.cpp
- tests/lowering_internal_test.cpp
- docs/design/Styio-Observable-Language.md
- docs/teams/SEMA-IR-RUNBOOK.md
- workflows/TEST-CATALOG.md
- docs/plan/INDEX.md

## Full regression
Commands:
- cmake --build build/default -j2
- ctest --test-dir build/default --output-on-failure --no-tests=error
- ctest --test-dir build/default -R '^architecture_layer_gate$' --output-on-failure --no-tests=error
- bash scripts/docs-gate.sh --mode worktree
- python3 scripts/local-info-leak-gate.py --mode worktree
- python3 scripts/repo-hygiene-gate.py --mode worktree
- git diff --check
Paths:
- src/StyioUtil
- src/cmake/StyioFrontendSources.cmake
- src/StyioSema
- src/StyioResourceTopology
- src/StyioLowering
- tests
- docs/design/Styio-Observable-Language.md
- docs/teams/SEMA-IR-RUNBOOK.md
- workflows/TEST-CATALOG.md
- docs/plan
