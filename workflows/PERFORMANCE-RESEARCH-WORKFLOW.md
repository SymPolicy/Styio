# Performance Research Workflow

**Purpose:** Sequence separate Benchmark and Modification tasks through reported research findings, bounded implementation, and independent performance evaluation.

**Last updated:** 2026-09-08

**TOML:** [PERFORMANCE-RESEARCH-WORKFLOW.toml](./PERFORMANCE-RESEARCH-WORKFLOW.toml) registers this workflow and its repo-local skill.

## Entry And Scope

Read [styio-performance-research/SKILL.md](./skills/styio-performance-research/SKILL.md) to execute the workflow. [skill.toml](./skills/styio-performance-research/skill.toml) supplies repository discovery metadata; `SKILL.md` owns the execution discipline. Keep both names and descriptions aligned. This is an agent research workflow; registering it does not add performance experiments to routine CI or create a recurring job.

The assignment selects a scenario, a Styio checkout, an explicit `styio-benchmark` checkout, and a task role. Infer paths and scope from existing context; default an unspecified research assignment to **Benchmark**. Benchmark owns workload and measurement changes, the dossier, and performance conclusions. Modification owns authorized compiler/runtime changes, semantic tests, and implementation documentation. Separate agent tasks hold these roles; one agent must not switch between measuring and implementing its own optimization.

The [topic reference](./skills/styio-performance-research/references/research-topics.md) supplies optional directions. The [Benchmark prompt](./skills/styio-performance-research/assets/deep-performance-research.prompt.md) explicitly selects continuous research; add a single-run instruction when only one investigation or evaluation is wanted. Use the [Modification prompt](./skills/styio-performance-research/assets/performance-modification.prompt.md) with a concrete Benchmark handoff for implementation. This workflow does not itself create or authorize additional agents. A single-run Benchmark task finishes with findings and a handoff; continuous Benchmark work proceeds to the next supported question.

## Sequence

1. **Benchmark:** Recover accepted contracts and prior evidence; select one user scenario and a falsifiable question.
2. **Benchmark:** Establish correctness and a baseline, profile and minimize the cost, and compare applicable maintained open-source implementations. Save samples and publish the finding in the dossier and task before requesting an implementation. Hand missing compiler instrumentation to Modification as a diagnostic request.
3. **Modification:** Read that handoff and existing authorization, implement one bounded candidate, review it, and complete focused correctness checks. Return an identified source/build snapshot and verification facts without changing the benchmark contract.
4. **Benchmark:** Independently compare the stable candidate and baseline, inspect the mechanism, and evaluate relevant tradeoffs. Preserve raw samples and uncertainty; update the dossier and visibly report improvement, regression, no benefit, or inconclusive evidence before another implementation round.
5. **Modification:** Act on that evaluation within existing scope. Keep revisions separate from the evaluated candidate and return changed candidates for evaluation. After the delivery unit's implementation, review, repairs, and focused evidence are complete, run the required final complete regression once. Apply the existing [cutover](./FEATURE-CUTOVER-WORKFLOW.md) and [commit-readiness](./FUNCTIONAL-COMMIT-READINESS-WORKFLOW.md) boundaries when they apply.
6. **Both owners:** Link Benchmark's performance verdict and Modification's implementation/verification handoff. A negative finding or an implementation proposal can close one investigation; a delivered optimization requires both evidence and implementation verification. In continuous mode, report the checkpoint and return to the next actionable research or candidate-evaluation item within existing scope and budget.

The skill owns write boundaries, handoff requirements, measurement discipline, negative-result handling, privacy, conditional agent delegation, and the developer decision after a final-regression failure. Numeric budgets, benchmark catalogs, and performance report schemas remain in `styio-benchmark`; the [performance runbook](../docs/teams/PERF-STABILITY-RUNBOOK.md) remains the owner of current compiler-side tool commands.

## Continuous Mode And Host Continuation

Select `single` or `continuous` separately from the Benchmark/Modification role. Requests to keep researching, loop, or continue until stopped select continuous mode. Follow the skill's [continuous loop](./skills/styio-performance-research/SKILL.md#continuous-research-loop): recover current state, select useful work, execute one bounded experiment, save and report evidence, then continue. Keep pending Modification work in the existing dossier and pursue independent Benchmark questions while waiting. A new wake-up resumes the same assignment; it does not duplicate an active measurement or reset rejected findings.

The workflow defines continuation decisions, not host process lifetime. Cursor's built-in `/loop` can repeat prompts on a local schedule until an outcome or user stop; without a fixed interval, the agent chooses when or which event should wake it. Use that host feature when requesting repeated execution instead of relying on ordinary prompt wording to schedule another turn. See [Cursor's official `/loop` announcement](https://cursor.com/changelog/shared-canvases). Prefix the Benchmark assignment with `/loop` when using it in Cursor; preserve the role, explicit checkout, dossier, and next action on each resumption.

One completed experiment does not end continuous research. Checkpoint for a user stop, an actual assigned budget limit, host interruption, or a dependency that leaves no actionable work within scope, and record the condition needed to resume. Do not add an unrequested daemon, scheduler, or queue database. Close each implementation delivery unit with its required final complete regression; the surrounding research campaign may continue with further questions.

## Maintaining This Workflow

Use existing registry, scheduler, documentation, and privacy checks when changing the workflow or skill:

```bash
python3 scripts/workflow-scheduler.py check
python3 tests/workflow_scheduler_test.py
python3 scripts/tool-skill-registry-gate.py
python3 scripts/docs-index.py --check
python3 scripts/docs-audit.py
python3 scripts/local-info-leak-gate.py --mode worktree
git diff --check
```

These validate workflow integration; they do not prove a compiler performance improvement. Runtime/source optimizations choose their focused and final gates from the affected owner contracts, not from this documentation-maintenance list.
