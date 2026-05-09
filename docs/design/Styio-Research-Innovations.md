# Styio — Research Innovation Points & Paper Roadmap

**Purpose:** 论文向研究假设与证据清单；**不**作为语言实现或语义的规范依据（实现见 `../specs/AGENT-SPEC.md`，语义见 `Styio-Language-Design.md`），也不作为产品、性能或外部系统比较声明。

**Last updated:** 2026-05-09

**Version:** 1.0-draft  
**Date:** 2026-03-28  
**Target Venues:** PLDI, OOPSLA, POPL, EuroSys, VLDB (systems track)

---

## Proposed Title

> **Styio: An Intent-Aware Stream Processing Language with Static State Allocation and Algebraic Absence**

Alternative:

> **From Symbols to Runtime: Stream Topology for Multi-Source Programs**

---

## Abstract (Draft)

Stream processing language design involves tradeoffs among expressiveness, runtime cost, state layout, synchronization, and error propagation. This draft records Styio research hypotheses and the evidence needed to evaluate them.

Styio explores five design areas: (1) **intent-aware compilation** that carries field-access analysis to resource drivers; (2) **pulse frame locking** for deterministic committed snapshots; (3) **algebraic absence propagation** at the value level; (4) **virtual state mounting** with anonymous ledgers; and (5) **dual-track stream synchronization** that distinguishes push-aligned and pull-snapshot joins at the AST level.

Any latency, code-size, safety, or related-work statement must be backed by repository tests, `styio-benchmark` reports, or cited primary sources before publication.

---

## Innovation Point 1: Intent-Aware Compilation

### The Problem

Resource drivers may expose fields that a program never reads. The research question is whether compiler-derived field usage can be passed to drivers without weakening type or resource checks.

### Styio's Contribution

The compiler performs **static field-access analysis** on the AST. For every `@resource` reference, it traces all downstream `.field` or `["key"]` accesses, constructing a **ResourceIntent** object that is passed to the driver at connection time.

**Formal model:**

Let \(R\) be a resource, \(F(R) = \{f_1, f_2, \ldots, f_n\}\) its full schema, and \(U(R) \subseteq F(R)\) the set of fields transitively accessed by user code. The compiler guarantees that the driver receives exactly \(U(R)\), enabling:

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

## Innovation Point 2: Pulse Frame Locking

### The Problem

In asynchronous stream systems, reading a shared variable twice in the same computation step can yield different values if the source updates between reads. This "time-tearing" is a well-known problem in hardware design (metastability) but has no standard solution in software stream languages.

### Styio's Contribution

Styio introduces **pulse frame locking**: when a primary stream (`>>`) triggers a closure, committed resource snapshots such as `@price[-1]` are captured at the frame boundary. Within the closure, repeated reads observe the same committed value — a compile-time guarantee.

**Formal invariant:**

For a closure \(C\) triggered by pulse \(p_t\) at time \(t\):

\[\forall \text{ reads } r_1, r_2 \text{ of } @v[-1] \text{ within } C: r_1 = r_2\]

**Implementation:**

At frame entry, the runtime performs one snapshot of declared shadow slots. The cost is O(k), where k is the number of declared shadows.

**Evidence required before external comparison:**

- cite primary documentation for each external consistency model being discussed
- define the pulse/frame scenario before comparing behavior
- separate measured behavior from design intent

**Hot pull escape hatch:** The `(<< @resource)` syntax explicitly bypasses frame lock for latency-critical live reads, giving developers fine-grained control.

### Evaluation Criteria

Construct a scenario where two state references are read in the same expression. Measure whether covered Styio paths preserve committed-snapshot consistency, then compare only against explicitly defined and cited baseline programs.

---

## Innovation Point 3: Algebraic Absence Without Monads

### The Problem

Missing data is pervasive in real-world streams (network drops, sensor failures, data gaps). Common representations include:

1. **Nullable sentinels:** require explicit checks at use sites
2. **Option/Maybe-style wrappers:** make absence explicit in the type
3. **NaN** (IEEE 754): Propagates silently but only works for floats, and `NaN != NaN` breaks equality

### Styio's Contribution

Styio introduces runtime `@` as an **algebraic absence value** that:

- Propagates through supported value families as runtime absence, not as a user-authored bare source literal
- Keeps the common value representation outside user-authored wrapper types where the compiler path supports it
- Carries **diagnostic metadata** in debug mode (reason code, source location)
- Can be intercepted at any point via `|` (fallback) or `??` (diagnostic extract)

**Formal algebra:**

For any supported binary operation \(\oplus\), values \(a, b\), and runtime absence \(@\):

\[a \oplus @ = @\]
\[@ \oplus b = @\]
\[@ \oplus @ = @\]

The `|` operator provides recovery:

\[@ \mid d = d\]
\[a \mid d = a \quad (\text{when } a \neq @)\]

**Evidence required before external comparison:**

- specify the exact absence semantics being compared
- cite the referenced language or runtime behavior
- measure syntax and runtime cost with reproducible cases

### Evaluation Criteria

Compare source size and runtime overhead for a data pipeline with controlled missing-value inputs. Keep external baselines in `styio-benchmark` and cite primary language references for each absence model.

---

## Innovation Point 4: Virtual State Mounting with Anonymous Ledgers

### The Problem

Stream processing requires persistent state (accumulators, buffers, counters). This design studies how much state layout can be decided from source-level declarations.

The comparison surface includes manual buffers, managed heaps, and external state backends, but each external system must be described from primary sources before publication.

### Styio's Contribution

Styio's Topology v2 syntax lets developers declare resources **at the top level** while keeping reads and writes close to the logic that uses them. The compiler still globally optimizes the memory layout:

1. **Explicit resource table:** Top-level `@name : Type|..n|` declarations establish durable slots during analysis
2. **Contiguous allocation:** Hoisted states are packed into a single memory block (the "anonymous ledger")
3. **Offset rewriting:** Resource selectors such as `@name[-1]` compile to `base_ptr + constant_offset`

**Formal model:**

Let \(S = \{s_1, s_2, \ldots, s_m\}\) be the set of all state declarations in a program. The compiler computes:

\[\text{offset}(s_i) = \sum_{j=1}^{i-1} \text{sizeof}(s_j)\]

\[\text{total\_size} = \sum_{i=1}^{m} \text{sizeof}(s_i)\]

A single `mmap` or stack allocation of `total_size` bytes serves the entire program's state needs.

**Implementation targets:**

- **No per-pulse allocation in covered paths:** verified through compiler/runtime tests
- **Contiguous slot layout:** selectors lower to offsets in the resource ledger
- **Snapshot copy path:** `memcpy(disk, ledger_base, total_size)` is the candidate implementation for fixed-layout snapshots
- **Hot restart:** Load snapshot, remap base pointer, resume execution with full history

**Evidence required before external comparison:**

- cite primary documentation for each external state model
- measure allocation count, cache behavior, and snapshot cost on fixed workloads
- avoid statements about external design goals unless sourced

### Evaluation Criteria

Measure L1/L2 cache miss rates for state access patterns and snapshot/restore latency for a system with 100+ state variables. External baselines require source-linked implementations.

---

## Innovation Point 5: Dual-Track Stream Synchronization

### The Problem

When combining streams of different frequencies (e.g., 100Hz market data + 1Hz fundamental data), developers must choose between:

- **Inner join (zip):** Only process when both arrive — loses high-frequency resolution
- **Latest-value join:** Process on every high-frequency tick, using stale low-frequency data — risk of using arbitrarily old data

The research question is whether making synchronization mode part of the AST gives the compiler a clearer lowering contract than library-only call sites.

### Styio's Contribution

Styio encodes synchronization mode **directly in the AST** via two distinct syntactic constructs:

1. **Zip (`&`):** `A >> #(a) & B >> #(b) => { ... }` — generates synchronized barrier code
2. **Snapshot (`snapshot << @res[...]` or `ref = (<< @res)`):** Generates async shadow update + atomic read

Because the synchronization mode is known at compile time, the code generator produces fundamentally different LLVM IR:

- **Zip:** Two input queues + barrier synchronization + merged dispatch
- **Snapshot:** Background atomic write + foreground atomic load (no synchronization overhead on the hot path)

**Evidence required before external comparison:**

- cite external API semantics from primary documentation
- define the same stream timing model for all programs under comparison
- report Styio behavior only for AST and lowering paths covered by tests

### Evaluation Criteria

Benchmark a cross-exchange arbitrage strategy that combines a 100Hz price feed with a 1Hz risk_factor feed. Compare latency percentiles (p50, p99, p99.9) across:
- Styio with `&` (zip mode)
- Styio with `$` snapshot mode
- cited external baseline implementations

---

## 6. Broader Impact & Future Directions

### 6.1 Beyond Quantitative Finance

Candidate application areas for evaluation:

- **IoT edge computing:** Sensor fusion with heterogeneous sampling rates
- **Autonomous systems:** Real-time decision pipelines with fail-safe `@` propagation
- **Log analytics:** High-throughput ETL with intent-pushed column pruning
- **Game engines:** Frame-locked state updates with deterministic replay

### 6.2 Formal Verification

The algebraic properties of `@` and the determinism of pulse frame locking define candidate proof obligations:

- stale-data checks for frame-locked reads
- missing-data propagation checks for accumulators
- snapshot consistency checks for contiguous ledgers

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
