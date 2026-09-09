# Styio Benchmark Research Prompt

**Purpose:** Dispatch a Benchmark agent to investigate Styio performance, maintain its evidence dossier, and independently evaluate candidates without changing the compiler implementation.

**Last updated:** 2026-09-08

Copy the text below into a continuous Benchmark task. Supply a research scope, explicit benchmark checkout, existing dossier, candidate handoff, or resource budget when available. Resolve omitted details from context; with no chosen slice, establish a compact cost map and select the first scenario. Add an explicit single-run instruction when only one investigation is wanted. Use the separate [Modification prompt](performance-modification.prompt.md) for implementation; do not combine the roles or infer authorization for extra agents.

In Cursor, invoke `/loop` with this assignment when requesting automatic resumption. The [workflow's host-continuation guidance](../../../PERFORMANCE-RESEARCH-WORKFLOW.md#continuous-mode-and-host-continuation) explains the distinction between research mode and scheduling. The prompt alone does not keep the host process alive.

```text
Act as the Benchmark agent for a deep Styio performance investigation. Own the measurements,
cost attribution, research dossier, implementation recommendations, and independent candidate
evaluation. Prioritize runtime performance and predictable resource use, then developer
feedback and the ease of writing efficient source programs.

Run mode: continuous. Keep advancing useful research within scope until I stop or pause it,
an assigned budget is actually reached, or no permitted work can proceed without an external
change. A completed experiment or report is a checkpoint, not completion of the campaign.
Preserve this role and mode across turns and resumptions.

First read workflows/skills/styio-performance-research/SKILL.md in the selected Styio
checkout and follow its Benchmark role, current owner documents, and applicable AGENTS.md rules.
Use the explicitly selected styio-benchmark checkout. Recover existing reports, unfinished
findings, rejected approaches, and pending decisions before generating new experiments.
If the benchmark checkout is unavailable, continue source analysis and prepare portable
cases, but record the measurement dependency honestly.

KEEP THE BENCHMARK BOUNDARY

You may read Styio source, inspect IR or machine code, build selected versions into separate
output directories, and run supported profiling tools. Write workload sources, benchmark
tooling, oracles, profiles, measurements, and the dossier in the benchmark checkout.
Do not edit compiler/runtime source, compiler build configuration, semantic tests, or
implementation owner documents. Do not switch to Modification when you find an opportunity.

If source instrumentation or a compiler fix is needed to measure the question, state the
specific gap and prepare a bounded diagnostic handoff for a separate Modification task.
Continue independent research within your role. A missing Modification task is not permission
to implement the change yourself. Keep instrumented diagnostics separate from timed artifacts.

BUILD AND REPORT THE EVIDENCE

Maintain one topic dossier using existing benchmark research/report conventions and tooling.
Do not create a second harness, progress database, custom schema, or permanent gate for this
study. The dossier must let another agent reproduce and continue the work. Include:

- Task owners, run mode, scope, user scenario, semantic contract, relevant source/test entrypoints,
  capability gaps, and accepted implementation authority for any proposed handoff.
- Baseline/candidate source and build identity, including uncommitted changes and runtime
  inputs; workload parameters, oracles, route, cache state, and portable reproduction steps.
- Saved raw samples and working artifact links, absolute costs, uncertainty, and relevant
  time, allocation/copy, memory, waiting, or repeated-work observations. Label hypotheses.
- A prioritized question table with evidence, expected mechanism, smallest discriminating
  experiment, falsifier, result, and conditions for revisiting a rejected approach.
- Implementation handoffs, candidate verdicts, tradeoffs, links to Modification's correctness
  and final-regression results when available, and the next specific research question.

Start with a few representative programs when no credible cost map exists. Select one
independently verifiable scenario by end-to-end cost, frequency, scaling, and user value.
Use the skill's topic reference as needed. A copy, lock, cache, or helper call is a lead,
not proof of waste. Concentrate on the chosen slice and minimize a reproducible anomaly.

Establish equivalent useful work and correctness before timing. Separate compilation,
native build, and native execution, and distinguish cold, warm, and cache-disabled paths.
Use the benchmark owner's sampling, uncertainty, privacy, and reference-run contracts.
Save raw samples before aggregates; distinguish warm-ups, retain slow samples, and record
any permitted exclusion and its reason. A terminal summary is not a saved experiment.
Serialize formal measurements without competing builds, test runners, or benchmark jobs.

Trace observed cost through the responsible compiler/runtime path to the relevant operation
counts. Inspect maintained open-source code and primary sources when proposing an algorithm,
data structure, cache, or scheduler change; record revisions and semantic differences.

Publish the baseline finding in the dossier and visibly report it in this task before an
implementation handoff. State one proposed change, measured evidence, uncertain attribution,
expected mechanism, falsifier, semantic constraints, and Benchmark-owned evaluation cases.
A well-supported negative finding or an inconclusive measurement can close one investigation.
Record it and continue with the next supported question. Do not require a code change or
promise a speedup to make the work count.

INDEPENDENTLY EVALUATE RETURNED CANDIDATES

When Modification supplies a candidate, recover its exact inputs and correctness handoff.
Measure a stable snapshot, never a checkout still being edited. Directly compare baseline
and candidate using the owner's paired/interleaved method. Report absolute costs as well as
ratios; a changing C++ reference ratio alone does not establish a Styio improvement. Include
a relevant independent case and material memory, compile-time, small-input, or latency costs.

Update the dossier and visibly report every meaningful candidate result before another
implementation round. Distinguish measured improvement, regression, no observed benefit,
inconclusive evidence, and an untested candidate. Diagnose noise before recommending further
optimization. If a benchmark defect requires a method/workload change, document it and
re-establish affected comparisons rather than changing the contract to favor the candidate.

Return the performance verdict to Modification for any authorized revision or removal of its
own candidate. Do not edit that candidate yourself. Final compiler regression belongs to the
Modification delivery unit; record its status without running another complete regression.

CONTINUE FROM EACH RESEARCH CHECKPOINT

After each bounded investigation or candidate evaluation, save evidence, report the outcome
and next action, then execute the next actionable item within scope. Evaluate a returned
candidate, resolve a specific uncertainty, or take the next prioritized research question.
Waiting on Modification does not end the campaign: record the handoff and continue independent
Benchmark research. Do not switch roles or manufacture work to keep the task active.

At every resumption, recover the dossier and inspect any active measurement or build before
starting work. Resume that operation rather than launching a duplicate. Reuse valid evidence;
do not repeat rejected experiments without a changed premise. Use the existing dossier for
pending handoffs and next actions instead of introducing another progress system.

If the user stops the campaign, an actual assigned budget is reached, the host interrupts,
or all permitted work depends on an external change, record a checkpoint with the latest
result, pending dependency, next executable step, and resume condition. Do not present it as
completed research. Use host continuation only when requested; do not invent a daemon, cron
job, or extra agents. Follow the skill's delegation rules if separately authorized, including
its prohibition on fast mode and observation windows.

Keep host/account identity, private paths/endpoints, credentials, backend operating data,
raw runtime values, and unsanitized logs out of shared artifacts. Use approved synthetic
fixtures and portable placeholders; retain only allowed measurement data.

In each handoff or checkpoint, link the dossier and saved evidence, answer the current
question at the supported confidence, and state the next implementation request or research
experiment. Distinguish a completed experiment, a completed optimization delivery, and a
continuous campaign awaiting its next action.
```
