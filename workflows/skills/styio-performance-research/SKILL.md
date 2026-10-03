---
name: styio-performance-research
description: Separate Styio performance research into Benchmark and Modification agent tasks, with reproducible profiling, bounded implementation, and independent performance evaluation. Use for deep performance research and optimization follow-ups.
---

# Styio Performance Research

**Purpose:** Separate Benchmark evidence ownership from Modification implementation ownership so each optimization follows a reported finding and receives independent measurement.

**Last updated:** 2026-09-08

## Start From The Assignment

Select the task's role before taking action. An unspecified deep-research or profiling assignment defaults to **Benchmark**. An authorized optimization campaign follows **Benchmark -> Modification -> Benchmark**, using separate agent tasks. One agent switching labels inside its own task does not provide this separation. An optimization request does not make a Benchmark task an implementation task.

| Role | Writable ownership | Boundary | Deliverable |
|------|--------------------|----------|-------------|
| **Benchmark** | Workloads, benchmark tooling, correctness oracles, sampling configuration, profiles, raw measurements, and the research dossier in the selected `styio-benchmark` checkout | Read and execute selected Styio versions; do not edit compiler/runtime source, compiler build configuration, semantic tests, or language/implementation owner documents | Measured findings, a bounded implementation handoff when justified, and independent candidate evaluation |
| **Modification** | Authorized Styio compiler/runtime implementation, supporting build changes, semantic tests, and implementation owner documents | Read benchmark evidence; do not edit benchmark workloads, oracles, sampling rules, thresholds, exclusions, or performance conclusions | One identified candidate, mechanism explanation, source review, focused correctness results, and implementation handoff |

Benchmark may inspect source, IR, and machine code, use supported profiler options, and build into separate output directories. If diagnosis requires new compiler counters, probes, or source instrumentation, request a bounded diagnostic change from Modification; do not use instrumentation as an exception to the source-write boundary. Keep that diagnostic artifact separate from timed release artifacts. Modification may run builds, semantic tests, and IR/assembly checks; formal performance comparisons and the acceptance verdict belong to Benchmark.

Use existing assignment and implementation authority. This skill does not itself authorize extra agents, task creation, publication, deployment, new dependencies, or background scheduling. When only a Benchmark task is available, produce findings and handoffs while remaining in that role. A separate Modification task can proceed under existing authorization without an extra user-approval ceremony.

Select the run mode independently of the role. **Single** is the default when no continuation is requested: finish one investigation, evaluation, or implementation handoff. **Continuous** applies when the assignment asks to keep researching, loop, or continue until stopped. Preserve that mode across turns and resumptions. In continuous mode, a completed experiment, negative result, or implementation handoff is a checkpoint; continue with the next actionable item under the [continuous loop](#continuous-research-loop). Do not quietly downgrade the assignment to a single run.

Identify the selected Styio checkout, the explicit external benchmark checkout, the target scenario, and existing evidence. Resolve paths locally; write portable repository-relative references and parameterized reproduction instructions. Do not assume a sibling checkout or a particular build directory exists. Inspect working changes and preserve other contributors' work. If the benchmark checkout is unavailable, continue source analysis and prepare concrete cases; record the missing measurement instead of creating a second benchmark system inside Styio.

Read these owners once, then follow only the links needed by the selected scenario:

- [Project priorities](../../../docs/specs/PRINCIPLES-AND-OBJECTIVES.md): performance first, usability second; recommended code should also be fast code.
- [Current state](../../../docs/rollups/CURRENT-STATE.md) and [active gaps](../../../docs/rollups/NEXT-STAGE-GAP-LEDGER.md): verify implemented capability against source and tests; a research draft is not implementation evidence.
- [Agent specification](../../../docs/specs/AGENT-SPEC.md) and applicable `AGENTS.md` files: implementation authority and repository boundaries.
- [Performance runbook](../../../docs/teams/PERF-STABILITY-RUNBOOK.md) and [measurement owner](../../../docs/design/performance-testing.md): current commands, probes, cache controls, and approved contracts.
- In the selected benchmark checkout: `README.md`, `docs/COVERAGE-MATRIX.md`, the chosen route's documentation and contract, and existing reports/regressions for this question.

The registered [workflow](../../PERFORMANCE-RESEARCH-WORKFLOW.md) defines sequencing. Use [research topics](references/research-topics.md) when selecting a direction or designing that topic's experiment. Dispatch [Benchmark](assets/deep-performance-research.prompt.md) for investigation or independent candidate evaluation, and [Modification](assets/performance-modification.prompt.md) for an authorized implementation handoff. Do not combine these prompts into one agent assignment.

## Benchmark: Select One Verifiable Slice

Recover previous results before launching experiments. Keep hypotheses, measured findings, rejected approaches, and pending decisions distinct. Reopen a rejected approach only when code, workload, or contrary evidence changes its premise.

Choose one scenario or module that can be accepted independently. State the question, affected users, accepted semantic boundary, baseline, primary metric, material tradeoffs, and what result would disprove the hypothesis. Rank candidates by representative workload cost, frequency of use, scaling behavior, and plausible end-to-end benefit. Begin with a small representative corpus when no credible cost map exists; narrow the scope after the first measurements.

Prefer questions about natural source programs: repeated container access, snapshot/history use, stream composition, inferred callables, mixed task workloads, edit-to-diagnostic latency, and explaining slow code. Treat existing copies, locks, caches, and helper calls as investigation entrypoints rather than proof of waste.

## Benchmark: Establish Comparable Evidence

1. Reuse benchmark-owned workload generation, runners, result schemas, and comparison tools. Extend only the missing case or counter needed by this question. Do not build a new harness, cache, hash scheme, or permanent gate for each study.
2. Pin the baseline and candidate source/build identities, inputs, optimization mode, target class, worker count, and cache state in the allowed evidence fields. Include uncommitted changes and all sources consumed by the native build, including runtime inputs; a commit ID alone does not describe a dirty build. Measure an identified snapshot, not a checkout Modification is still editing. Use an isolated snapshot or keep the selected inputs unchanged through evaluation.
3. Prove equivalent useful work before timing: outputs, numerical order where required, errors, ownership, snapshot/commit boundaries, and concurrency semantics. Keep different algorithms or semantics in separate experiments. A changed source spelling is not necessarily an equivalent program.
4. Separate `compile-and-run`, `native-build`, and `native-run`; distinguish cold, warm, and cache-disabled behavior when relevant. Attribute total compilation to actual compiler phases. Do not report a runtime gain from faster compilation or a default-path gain from an opt-in cache.
5. Start with small, representative, and amplified inputs using the route's scales. Sweep a few meaningful dimensions, then expand around a reproducible anomaly. Avoid an exhaustive Cartesian product of every size, type, thread count, and flag.
6. Run formal measurements without competing builds or benchmarks on the same machine. Directly compare baseline/candidate with the route's paired/interleaved repetitions, uncertainty calculation, correctness checks, and memory replay. Report absolute times as well as ratios; a changing C++ reference ratio alone does not establish a Styio gain. Keep instrumentation in separate diagnostic runs.
7. Save raw samples and their build/workload association in existing benchmark-owned artifacts before reporting aggregates. Keep warm-ups distinguishable, preserve slow samples, and explain any contract-permitted exclusion. A terminal summary or a planned report path is not an archived measurement. Disclose noise, missing capabilities, and incomparable results.
8. Read thresholds, sample counts, and reference-run requirements from the owning benchmark contract. Do not weaken them, invent a universal speedup target, or promote a development result to official parity. If the result is inconclusive, resolve the identified source of uncertainty before collecting more samples. Benchmark may repair a faulty workload or measurement method, but must identify the change and re-establish affected comparisons; do not tune the evaluation contract to make a candidate pass.

## Benchmark: Explain The Cost And Hand Off

Move from whole-program cost to phase, function, and the relevant mechanism. Use the existing frontend/native profiler, CPU or allocation profiling, queue/cache counters, or LLVM optimization remarks according to the suspected cause. Inspect generated IR or machine code when a source abstraction fails to disappear. Use the smallest extra instrumentation that distinguishes competing explanations.

Reduce a representative failure or scaling anomaly to a small reproducible program. Connect source expression -> compiler/runtime path -> observed operation counts -> end-to-end effect. Specify a falsifiable prediction, such as copied bytes growing with nested payload size, repeated type visits growing with dependency fanout, or queue wait dominating short tasks. Distinguish theoretical complexity and projected benefit from measured results.

For algorithm, data structure, cache, or scheduling changes, inspect a suitable maintained open-source implementation and primary documentation. Record the relevant source file or function, revision, algorithm, and the semantic/workload differences that affect transfer to Styio. Compare the simplest applicable candidate with the current implementation; mature source is a reference, not an instruction to add a dependency. The topic reference links provide starting points, not frozen recommendations.

Before Modification starts an optimization candidate, publish the baseline finding in the dossier and report it visibly in the task. Provide the smallest useful implementation handoff:

- The question, affected user scenario, accepted semantics, and authorized implementation surface.
- An identified baseline, reproducible workload, saved samples, uncertainty, and the measured cost; distinguish an observed mechanism from a suspected cause.
- One proposed change or diagnostic need, its expected mechanism, and what would disprove it.
- Benchmark-owned evaluation cases, relevant tradeoffs, and acceptance criteria from the existing contract.

A missing diagnostic capability can justify an instrumentation handoff with the measurement gap stated explicitly. It does not justify presenting an unmeasured optimization as necessary. A rejected hypothesis or inconclusive experiment can close one investigation; code changes are not required. In continuous mode, record that outcome and select the next supported question.

## Modification: Implement One Candidate

Recover the Benchmark handoff and existing implementation authorization before editing. An optimization candidate requires the finding, baseline, and scope; return a missing item to Benchmark instead of selecting an unmeasured optimization yourself. A diagnostic request instead requires the specific measurement gap and bounded probe to implement. Complete the smallest coherent change for that handoff. Fix ordinary in-scope implementation problems and report them within this ownership boundary. Prefer eliminating repeated work and improving layout or ownership flow over adding knobs and special cases.

Preserve observable semantics unless the user has already authorized their change. In particular, copy elimination must preserve independent values and lifetimes; stream fusion must preserve effects and frame commits; scheduler improvements must preserve settlement and progress. Do not improve measurements by removing verification, shortening useful work, changing error behavior, or adding timeouts that interrupt valid long-running programs.

If the remedy needs a developer decision, finish the authorized diagnosis and prepare a concrete proposal with alternatives, benefit, affected contracts, and focused validation. Pause only dependent work. Reuse an existing decision instead of asking again.

When replacing an implementation, migrate its callers, tests, and owner documentation completely within the authorized scope. Use the existing [cutover skill](../styio-feature-cutover/skill.toml). Check removed names and paths with a one-time migration audit; do not retain that audit as a permanent test or gate. Preserve explicitly permanent independent assets and other contributors' edits.

Review the source and complete focused correctness checks before returning a candidate. Identify the source/build inputs and uncommitted patch, changed paths, intended mechanism, tested semantics, and remaining concerns in the existing task handoff. Keep the selected candidate unchanged while Benchmark evaluates it. Propose any missing evaluation case to Benchmark; do not rewrite its tests or results. Do not stack a second optimization on a candidate awaiting evaluation.

## Benchmark: Evaluate And Report Each Candidate

Evaluate the returned candidate against the identified baseline. Confirm the mechanism changed as predicted and any benefit survives a representative end-to-end case and a relevant case not used to tune the change. Evaluate small-input overhead, memory, compile time, and tail latency where the change can affect them; do not measure unrelated dimensions by habit.

After each meaningful experiment, update the dossier with working artifact links and report the result visibly before handing off another implementation round. Distinguish measured improvement, regression, no observed benefit, inconclusive measurement, and an untested candidate. Include absolute costs, uncertainty, tradeoffs, and the next discriminating experiment. Do not let a dossier remain at "pending" after a candidate has been measured.

Recommend retaining a candidate only when its intended benefit is supported and material tradeoffs are acceptable under the assignment. Benchmark records the verdict; Modification performs any authorized revision or removal of its own experimental changes. Preserve negative results as concise evidence. Neither role may declare a performance gain from code inspection, passing correctness tests, or an inconclusive timing difference.

## Modification: Close The Delivery Unit

Use semantic tests for durable behavior and Benchmark evidence for performance. Do not add wording-matching tests for this skill, permanent migration tests, duplicated guards, or speculative regression matrices. Update the owning runbooks rather than duplicating the research dossier.

Once Benchmark has evaluated the final candidate and all changes, source review, in-scope repairs, and focused verification for the delivery unit are complete, apply the [commit-readiness self-check](../styio-functional-commit-readiness/skill.toml) and run the required final complete regression once at the owner-defined scope. Reuse passing evidence while its inputs remain unchanged. Benchmark does not run a compiler-wide regression merely to deliver a research report. A later implementation change requires evaluation of the affected candidate before delivery.

If that final complete regression fails, diagnose and report the concrete cause, proposed repair, and proposed validation. Do not automatically repair or rerun; the developer decides the follow-up. Continue independent authorized work if any remains. A failed or unverified acceptance condition is not completion.

## Preserve A Useful Research Handoff

Benchmark owns the single research dossier, workload sources, minimized performance regressions, measurements, and performance conclusions in the explicit `styio-benchmark` checkout. Modification supplies implementation and correctness facts through its task handoff and Styio owner documents. Link the two handoffs; do not add a parallel progress database or copy a performance report into the compiler tree.

Retain the following compact information in the existing benchmark research record or authorized task handoff; do not add fields to frozen machine-result schemas:

| Field | Content |
|-------|---------|
| Question and scope | Scenario, user value, semantic boundary, implementation authority, run mode, and the separate Benchmark/Modification task owners |
| Reproduction | Repository-relative workload IDs, allowed build/revision identity, parameters, baseline/candidate selection, and cache mode |
| Evidence | Correctness oracle, sample artifacts, uncertainty, allocation/copy/wait/query counters relevant to the conclusion |
| Cause and change | Minimized example, source attribution, compared open-source implementation, and the implemented or rejected remedy |
| Outcome | Measured benefit and tradeoffs, or a disproved hypothesis, inconclusive result, or concrete pending decision |
| Verification | Focused results, final regression status when required, and objective unverified surfaces |
| Next lead | One specific next question, evidence motivating it, the smallest discriminating experiment, or the pending owner/dependency and resume condition |

Reports must exclude host/account identity, private paths/endpoints, credentials, backend operating data, and raw runtime values or unsanitized subprocess output. Prefer synthetic or approved fixtures. Keep diagnostic traces local until inspected and sanitized. Use only the owning report schema's allowed fields; parameterized reproduction instructions belong in the allowed human handoff, not a structured parity result that forbids commands.

## Continuous Research Loop

1. **Recover before resuming.** Read the current dossier, latest result, role handoffs, and any active build or measurement status. Keep the assigned role and continuous mode. Resume an existing operation instead of starting a duplicate; a tool yield does not mean it finished. Reuse valid evidence and do not repeat rejected experiments without a changed premise.
2. **Choose the next useful action.** Benchmark evaluates a ready candidate, resolves an identified uncertainty, or selects the next prioritized question within the campaign's scope. Keep each experiment independently verifiable. Use the dossier's existing question table and handoffs to track ready work and external dependencies; do not add another queue service or progress database.
3. **Measure, record, report, continue.** Complete the bounded experiment, save its evidence, update the dossier, and visibly report the result plus the next action. Proceed with that action when possible. One report, a negative result, or one successful candidate does not complete a continuous campaign.
4. **Keep handoffs moving without crossing roles.** If Modification is needed, Benchmark records the concrete request and continues other independent research within scope. A missing Modification task does not grant source-write authority or end all Benchmark work. A Modification task assigned continuous mode processes its next existing authorized handoff after the prior candidate's evaluation; it does not invent optimizations or change a candidate still being measured. Serialize formal measurement windows against competing builds and tests.
5. **Checkpoint only for a real pause.** Respect an explicit user stop/pause, an assigned budget actually reached, a host interruption, or an external dependency that leaves no actionable work within scope. First record the latest result, pending owner/decision, next executable step, and the event needed to resume. Continue independent permitted work when available. Do not repeat unchanged measurements or poll unchanged state just to stay active, invent a stopping timer, or claim that the whole campaign is complete.

Define implementation delivery units independently of the campaign lifetime. Modification runs the required final complete regression once after each unit's changes, review, focused repairs, and Benchmark evaluation are complete; do not run it after every research experiment or defer all acceptance until an indefinite campaign ends. The developer decision after a final-regression failure still applies to that unit.

Continuous mode tells a running agent what to do next; it does not keep the host process alive or schedule future turns by itself. Use a host's continuation mechanism only when requested, carrying forward the same role, dossier, and next action. For Cursor dispatch, the [workflow](../../PERFORMANCE-RESEARCH-WORKFLOW.md#continuous-mode-and-host-continuation) describes `/loop`. If the host cannot resume, leave an honest checkpoint instead of creating an unrequested daemon, cron job, or second orchestration system.

## If Parallel Agents Are Explicitly Requested

Keep Benchmark and Modification in separate tasks even when a coordinator dispatches both. Divide additional independent workload construction, source attribution, or review by concrete file ownership. Tell each agent it shares the codebase and must preserve others' changes. Serialize formal measurements on each machine; concurrent builds and test runners also contaminate samples. Read-only source analysis can continue while another task owns a measurement window.

Do not use fast mode. Use an observation window of at least 10 minutes for an ordinary task and 30 minutes for a large task, split into waits allowed by the host with progress updates. A returned wait or expired observation window is not a cancellation, failure, or completion signal. Reuse agents for related work and stop only for an actual task outcome, explicit cancellation, or a demonstrated blocker.
