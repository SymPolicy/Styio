# Styio — Research Innovation Points & Paper Roadmap

**Purpose:** Record research questions and evaluation evidence for a possible Styio paper. This is not a language-semantics, implementation-status, or product-comparison authority; those remain in the language/feature contracts and their tests.

**Last updated:** 2026-10-01

**Version:** 1.0-draft  
**Date:** 2026-03-28  
**Target Venues:** PLDI, OOPSLA, POPL, EuroSys, VLDB (systems track)

---

## Proposed Title

> **Styio: A General-Purpose Symbolic Language for Visual Program Understanding**

Alternative:

> **From Symbols to Runtime: Stream Topology for Multi-Source Programs**

---

## Abstract (Draft)

Styio is an experimental general-purpose visual programming language with
symbolic syntax for expressing data flow. This research agenda evaluates its
notation, inference, program views, execution model, and source-access planning.

Styio explores five design areas: (1) **intent-aware compilation** that carries field-access analysis to resource drivers; (2) **pulse frame locking** for deterministic committed snapshots; (3) **explicit tagged absence** at compiler-owned value boundaries; (4) **virtual state mounting** with anonymous ledgers; and (5) **dual-track stream synchronization** that distinguishes push-aligned and pull-snapshot joins at the AST level.

Any latency, code-size, safety, or related-work statement must be backed by repository tests, `styio-benchmark` reports, or cited primary sources before publication.

---

## Research Questions and Acceptance Evidence

The [language design](./Styio-Language-Design.md#24-visual-design-intent)
combines compiler-generated program views with runtime execution overlays in
an independent editor. Evaluate comprehension and authoring on both human-written
and AI-generated programs. The [current observable interface](./Styio-Observable-Language.md#3-current-compiler-foundation)
covers resource-oriented facts and selected runtime events.

For end-to-end diagnosis, evaluate whether correlated compiler and runtime
evidence reduces the time and error rate of locating data-flow, scheduling,
I/O, and resource-lifecycle failures. Each scenario records available OS/runtime
observations, their program-site correlation, missing evidence, and collection
overhead. Compare against a stated debugging baseline on the same workload.
Each compiler and tooling layer retains its own representation and owner;
explicit identifiers and completeness states connect the evidence.

| Question | Evaluation needed before making a claim |
|---|---|
| Do the symbolic forms help people read intent across natural-language backgrounds? | Matched comprehension and error-localization tasks, with recorded prior experience, training, accuracy, time, and symbol-confusion cases. Compare identical semantics; do not assume traffic-sign intuition is universal or sufficient without learning. |
| Does a compiler-backed visual view improve understanding of a large program? | Tasks covering module dependencies, data flow, branches, loops, and failure paths; compare text-only and text-plus-graph conditions, include collapsed/expanded hierarchy, and report incorrect/missing edges and incomplete facts. |
| Can inference and recommended composition reduce unnecessary authoring without hiding errors? | Checked classic algorithms and practical programs with equivalent inputs/results; record annotation and expression burden, failed attempts, diagnostics and ambiguity. Shorter source alone does not establish readability, expressiveness, or type safety. |
| Does the graph remain faithful during execution at acceptable cost? | Static-site/runtime-instance correlation fixtures, repeated loops and branch outcomes, loss/completeness checks, privacy boundaries, and approved time/memory/event-volume budgets. An attractive rendering is not semantic evidence. |

Use the algorithm suite and focused language fixtures as experimental cases.
Distinguish missing teaching material, missing library support, and language
constraints when classifying authoring failures. User-facing tutorials and
programming skills should exercise accepted idioms at a fixed compiler version.

Report formal properties proved for a specified language subset, behaviors
covered by regression tests, and usability or performance results measured
experimentally as separate evidence categories. Section 6.2 defines the first
proposed formalization scope.

---

## Innovation Point 1: Intent-Aware Compilation

### The Problem

Resource drivers may expose fields that a program never reads. The research question is whether compiler-derived field usage can be passed to drivers without weakening type or resource checks.

### Styio's Contribution

The research target is to derive required fields and admissible filtering from
compiler-owned semantic facts and pass that intent to capable resource drivers.
This is not a statement that current drivers perform whole-program pushdown;
the [resource driver contract](./Styio-Resource-Driver.md) and executable
resource paths determine the implemented scope.

**Formal model:**

Let \(R\) be a resource, \(F(R) = \{f_1, f_2, \ldots, f_n\}\) its full schema, and \(U(R) \subseteq F(R)\) the set of fields transitively accessed by user code. A candidate contract would communicate \(U(R)\), or an explicitly conservative
approximation when complete static knowledge is unavailable. Potential uses,
subject to the source format and driver capability, include:

- SQL drivers to generate `SELECT f1, f2` instead of `SELECT *`
- Columnar file drivers to seek directly to relevant column chunks
- Network drivers to subscribe to minimal data channels

**Evidence required before external comparison:**

- cite primary documentation for each external optimizer or runtime being discussed
- record the exact workload and driver contract being measured
- state Styio behavior only for compiler paths covered by tests

### Evaluation Criteria

Measure bytes transferred from source to runtime for identical analytical queries. Do not publish reduction ratios until the workload, inputs, and measured outputs are recorded in `styio-benchmark`.

---

### Source-Proximal Acquisition and Processing Research

Evaluate capability-aware projection and filtering on a delimited-text source
and a columnar or indexed source. Separate file discovery, byte scanning,
decoding/materialization, and query execution. A columnar source may skip
columns or ranges; a text source may still scan all bytes while avoiding some
parsing and copying.

Compare the current execution path, a selective C++ baseline using the same
parser, and each proposed transformation with identical inputs. Record logical
and physical bytes read, syscall count, decoding time, copies/allocations,
time to first result, total time, and peak memory. Verify result, order, effect,
error-timing, and buffer-lifetime equivalence. Unsupported driver capabilities
retain the baseline path.

Current file iteration uses line-oriented reads, and native interop maps pointer
results to strings. Binary buffers, explicit lengths, borrow lifetimes, opaque
handles, and OS error propagation need an interface contract before zero-copy
or low-level I/O experiments. Correlate source operation, rewrite justification,
physical scan, runtime instance, and available OS evidence with explicit IDs;
unavailable observations remain visible in the result.

---

## Innovation Point 2: Pulse-Scoped Snapshot Consistency

### Research Question

Determine which committed-state reads can share a consistent observation within
a pulse, and at what collection/scheduling cost. Specify the source-update,
commit and read ordering before comparing consistency models.

### Current Boundary and Proposed Extension

The resource-topology contract defines current committed selectors and the
supported history/snapshot slices. Their parser, Sema, lowering and runtime
evidence lives in [Resource Topology](./Styio-Resource-Topology.md) and its tests.
This does not establish a universal frame-entry snapshot for every asynchronous
source or every resource family.

For a selected supported state set, a candidate frame rule would require repeated
reads during one pulse to observe the same captured committed value. Define when
the capture occurs, which writes become visible, and what happens on absence,
failed reads or unsupported resource capabilities. Any live-read alternative
must follow its owning source contract; the research rule does not assign a
new meaning to existing operators.

### Evaluation Criteria

Use repeated-read cases with controlled commit and source-update schedules.
Check observed values and failure ordering against a reference event trace,
then measure capture work, memory traffic and pulse latency. Report the tested
state set and scheduler assumptions; extend coverage only with matching producer
and runtime evidence. External comparisons need versioned primary references
and the same observation model.

---

## Innovation Point 3: Tagged Runtime Absence Without Sentinel Collisions

### The Problem

Missing data is pervasive in real-world streams (network drops, sensor failures, data gaps). Common representations include:

1. **Reserved scalar sentinels:** lose one legitimate value and leak checks into every hot scalar operation
2. **Option/Maybe-style wrappers:** make absence explicit in the type
3. **NaN** (IEEE 754): Propagates silently but only works for floats, and `NaN != NaN` breaks equality

### Styio's Contribution

Styio represents runtime `@` as a compiler-owned **tagged absence value** that:

- uses an explicit `(is_defined, value)` representation only on compiler paths that can carry absence
- leaves ordinary `i64` as an untagged machine integer with the complete signed 64-bit value domain
- is not authored as a bare source literal and must be intercepted before ordinary arithmetic, comparison, logic, or callable/runtime arguments
- can be intercepted lazily through `|` (value fallback). Resource-effect fallback is separate: `?| resource_operation` settles in place and raises immediately on failure, while `?| resource_operation | fallback` recovers through type inference.
- records reason/source metadata and the `??` diagnostic-extract surface as deferred work; neither is part of the implemented runtime contract

**Current value rule:**

For present value \(a\), fallback \(d\), and runtime absence \(@\):

\[@ \mid d = d\]
\[a \mid d = a\]

Ordinary \(\oplus\) is defined only on present scalar operands; there is no
implicit `@` propagation through the raw integer fast path.

**Evidence required before external comparison:**

- specify the exact absence semantics being compared
- cite the referenced language or runtime behavior
- measure syntax and runtime cost with reproducible cases

### Evaluation Criteria

Compare source size and runtime overhead for a data pipeline with controlled missing-value inputs. Keep external baselines in `styio-benchmark` and cite primary language references for each absence model.

---

## Innovation Point 4: Resource State Layout and Checkpointing

### Research Question

Determine whether compatible resource-state allocations can be grouped to reduce
allocation and access cost while preserving resource identity and lifecycle.

### Current Boundary and Proposed Extension

Current lowering creates resource bindings individually, and code generation
allocates their ring/head/pending state separately. A whole-program contiguous
ledger, constant-offset rewriting for every selector, and a single allocation
for all program state are proposed transformations rather than current compiler
guarantees. See [AstToStyioIR](../../src/StyioLowering/AstToStyioIR.cpp) and
[CodeGenG](../../src/StyioCodeGen/CodeGenG.cpp) for the implemented paths.

Start with a fixed set of bounded scalar resources. Define layout alignment,
padding, lifetimes and valid selector offsets; prove or test that allocation
coalescing preserves reads, writes, commit order and failures. Container handles,
pointers and external resources require explicit serialization and reconstruction
rules before checkpoint/restart is meaningful. A raw memory copy is not a general
state-restoration contract.

### Evaluation Criteria

Compare allocation count, bytes, cache behavior and state-access latency against
the current per-resource layout. For checkpointing, compare logical state before
and after restore, including absent/history values and failure cases. Keep
measured workloads and reports in `styio-benchmark`.

---

## Innovation Point 5: Zip and Snapshot Execution Strategies

### Research Question

Determine how explicit zip and snapshot semantics affect scheduling, data
freshness, buffering and latency for the supported source combinations.

### Current Boundary and Proposed Extension

The owning language contracts distinguish aligned iteration and snapshot reads.
Implementation strategy depends on source capabilities: current materialized-list
zip uses length/index iteration, so a universal two-queue/barrier model would
misdescribe that path. Snapshot reads likewise do not imply background atomic
writes, atomic reads for every value representation, or zero synchronization
cost.

Extend one source combination at a time. Record its existing accepted source
form, blocking/termination behavior, ordering, absent-value handling and resource
lifecycle. Propose queues, barriers or atomic state only when required by that
combination and justified by its memory model.

### Evaluation Criteria

Use the existing [stream-processing fixtures](../../tests/features/stream_processing/)
and [resource-topology contract](./Styio-Resource-Topology.md) to choose accepted
programs. Compare zip and snapshot workloads with controlled rates and values;
record freshness, output sequence, buffer growth, CPU cost and latency
percentiles. Use current syntax and reject unsupported combinations explicitly.
External baselines must implement the same timing and observation model.

---

## 6. Broader Impact & Future Directions

### 6.1 Beyond Quantitative Finance

Candidate application areas for evaluation:

- **IoT edge computing:** Sensor fusion with heterogeneous sampling rates
- **Autonomous systems:** Real-time decision pipelines with fail-safe `@` propagation
- **Log analytics:** High-throughput ETL with intent-pushed column pruning
- **Game engines:** Frame-locked state updates with deterministic replay

### 6.2 Formal Verification

The proposed first proof project uses Lean 4 and a fixed compiler revision.
Model the existing closed, pure, final callable subset with `i64`, `bool`,
homogeneous lists, immutable local bindings, direct non-recursive calls, and
rank-1 equality-based type relations. Exclude implicit conversions, higher-order
values, captures, mutation, I/O, and constrained overloads initially. Resolved
core constructors describe existing AST forms; they introduce no source syntax.

Define global scheme and function environments, a local monomorphic type
environment, values, and capture-avoiding substitution. Execution uses a
left-to-right call-by-value small-step relation under the fixed function table.
Integer literals have fixed `i64` type; initially admit empty lists only at
listed positions with concrete expected element types. Same-call joint
constraint propagation is a separate inference experiment. Use
64-bit values for integer operations and verify their overflow behavior against
both code generation and constant folding before freezing the model. Separate
the source core with inferred schemes from the elaborated, concrete core used
for execution.

Deliver the proof obligations in dependency order:

1. Substitution and environment lemmas; unifier soundness, occurs-check safety,
   and most-general-unifier factorization for first-order equality constraints.
2. Inference soundness: successful inference produces a well-typed annotated
   source core under the substituted environment, with the resulting
   substitution applied to both result types and core annotations. Generalization quantifies only
   variables free in the inferred type but not free in the global or local type
   environment, under the closed/pure eligibility rule.
3. Preservation and progress for well-typed closed concrete-core programs with
   total primitives and an abstract allocation model.
4. Elaboration correspondence: produce a well-typed monomorphic function table
   and concrete term with ground entry and reachable-call types. Relate their
   values and multi-step execution to the rank-1 source. Core safety alone does
   not establish source-pipeline correctness.
5. Relative completeness and principal schemes for the stated declarative
   subset, separately from fixed-default selection and C++ implementation claims.

A test-only adapter should compare resolved ASTs, inferred schemes, and concrete
instances from the pinned C++ frontend with the reference model. Start with
named positive/negative fixtures, then generated cases and minimized mismatches.
Treat parser extraction, C++ inference, lowering, LLVM, runtime, and machine
execution as explicit trust gaps until independently validated. A model theorem
and differential tests do not constitute a C++ refinement proof.

Keep the proof project and its conformance tooling in the research workspace.
Lock the Lean toolchain and dependencies; build every theorem, inspect its axiom
dependencies, and reject unfinished `sorry` proofs or new axioms standing in for
the target property. The existing observable evidence model is not a typing
certificate. A compiler-emitted certificate/checker would be a later interface.

References: [Lean inductive definitions](https://lean-lang.org/theorem_proving_in_lean4/Inductive-Types/),
[recursive definitions](https://lean-lang.org/doc/reference/latest/Definitions/Recursive-Definitions/),
[axiom inspection](https://lean-lang.org/theorem_proving_in_lean4/Axioms-and-Computation/).

Later resource proofs can extend this core with explicit absence, state,
ordering, and failure semantics before addressing frame locking or ledger
consistency.

### 6.3 Distributed Styio

The current design targets single-machine execution. Extending to distributed clusters requires:

- **Ledger partitioning:** Splitting the state ledger across nodes
- **Pulse coordination:** Distributed frame locking (similar to Chandy-Lamport snapshots)
- **Driver federation:** Resource drivers that abstract multi-node data sources

---

## 7. Related Work Evidence Ledger

Related-work tables must be built from cited primary sources and reproducible baseline programs. This draft does not assert external system behavior. A publishable ledger must include:

1. source link and version for each external system
2. exact feature or API under discussion
3. Styio compiler path or runtime path used for comparison
4. benchmark or test artifact that supports the statement

---

## Appendix: Consultant's Additional Thoughts

### On Paper Structure

Possible paper structure:

1. **Introduction** — The performance-expressiveness gap in stream processing
2. **Motivating Example** — The golden cross strategy (as developed in the Gemini discussion)
3. **Language Design** — Core syntax, focusing on the five innovations
4. **Compilation Pipeline** — Lexer → Parser → State Analysis → Intent Extraction → LLVM CodeGen
5. **Evaluation** — reproducible measurements and cited baselines
6. **Discussion** — Limitations (symbol density, learning curve, single-machine only)
7. **Related Work** — Positioning table above
8. **Conclusion**

### On Evaluation Strategy

Required evidence should include a three-way study:

1. **Styio** — the complete system
2. **Styio-minus** — ablation studies removing each innovation one at a time (e.g., Styio without frame lock, Styio without intent pushdown)
3. **Baselines** — source-linked external implementations with recorded workloads

This keeps each design area tied to measurable behavior.

### On Intellectual Honesty

The paper should openly acknowledge:

- **Symbol density** requires usability evaluation
- **Single-machine limitation** restricts applicability to scenarios where data fits in memory
- **Pulse frame locking** adds O(k) overhead per pulse — quantify this precisely
- **The "thick library" model** means the standard library is a significant engineering investment before the language becomes practically useful

Acknowledging limitations keeps the paper evidence-scoped.

### On Research Statement Discipline

Research statements about originality, performance, safety, or external systems require a literature review, source citations, and repository evidence. Until that evidence exists, this document records hypotheses only.

The same rule applies to algebraic absence and diagnostic tainting: describe the implemented semantics first, then publish only measured benefits.

### On Potential Reviewers' Concerns

Questions to answer with evidence:

1. scope of the language beyond the initial financial examples
2. usability of dense symbolic syntax
3. limits of the single-machine execution model
