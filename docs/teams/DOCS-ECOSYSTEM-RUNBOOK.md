# Docs / Ecosystem Runbook

**Purpose:** Provide the daily-work entrypoint for maintainers of repository documentation, generated indexes, archive/rollup lifecycle, templates, and external Styio ecosystem handoff material.

**Last updated:** 2026-07-20

## Mission

Own documentation structure and cross-repository clarity. This team protects SSOT discipline, generated indexes, archive provenance, external repository boundaries, handoff notes, and reusable templates. It does not redefine language semantics, accepted tests, or package-manager ownership.

## Owned Surface

Primary paths:

1. `docs/`
2. `docs/assets/`
3. `docs/rollups/`
4. `docs/archive/`
5. `workflows/`
6. `templates/`
7. `scripts/docs-index.py`
8. `scripts/docs-audit.py`
9. `scripts/docs-lifecycle.py`
10. `scripts/team-docs-gate.py`
11. `scripts/workflow-scheduler.py`
12. `scripts/delivery-gate.sh`

Key SSOTs:

1. [../specs/DOCUMENTATION-POLICY.md](../specs/DOCUMENTATION-POLICY.md)
2. [../specs/REPOSITORY-MAP.md](../specs/REPOSITORY-MAP.md)
3. [../../workflows/DOCS-MAINTENANCE-WORKFLOW.md](../../workflows/DOCS-MAINTENANCE-WORKFLOW.md)

## Daily Workflow

1. Check whether the content already has an owning SSOT before adding a new file.
2. Prefer linking and short summaries over copying rules across documents.
3. Add `Purpose` and `Last updated` metadata to every active docs file.
4. Use [../assets/templates/TEAM-RUNBOOK-TEMPLATE.md](../assets/templates/TEAM-RUNBOOK-TEMPLATE.md) for team runbook structure changes.
5. Regenerate `INDEX.md` files after collection changes.
6. Use `python3 scripts/docs-scaffold.py ...` for new docs files or new docs collection directories so metadata, collection registration, and generated indexes are created together.
7. Keep repository-level bootstrap/build entrypoints under `docs/BUILD-AND-DEV-ENV.md`, and push subsystem-only details back down into the owning docs collection instead of overloading `README.md` or `docs/external/for-ide/`.
8. Run the team runbook maintenance gate before delivery so source/test/docs folder changes cannot land without the mapped runbook update or required runbook format.
9. Use archive lifecycle tooling for raw history/review metadata and cleanup rather than manually moving provenance.
10. Use the default unified delivery gate for docs/process deliveries so worktree hygiene, branch-range hygiene, runbook maintenance, docs audit, and external audit stay coupled behind one command.
11. When refactoring `scripts/delivery-gate.sh`, keep literal scheduler profile invocations visible for `delivery-checkpoint` and `delivery-push` so released `styio-audit` can verify that the unified entrypoint still delegates to the approved scheduler profiles.
12. Keep the ecosystem CLI contract mirror and cross-repo doc gate aligned whenever `styio-pafio` or `styio-view` handoff docs change.
13. When a compiler-side machine contract grows, update the owner SSOT and both consumer handoff docs in the same checkpoint instead of leaving one side on preview wording.
14. Keep generated `INDEX.md` files deterministic for empty collections by deriving fallback timestamps from collection metadata instead of local wall-clock date.
15. When CI validates sibling ecosystem repositories, resolve the sibling checkout to the PR target branch when it is `release`, `stable`, or `nightly`; otherwise use `nightly` as the shared temporary-branch baseline.
16. When syntax-delivery rules change, update the workflow asset, gate scripts, and delivery entrypoints in the same checkpoint; workflow-only prose is not enough.
17. Keep `workflows/WORKFLOW-ORCHESTRATION.md` and `scripts/workflow-scheduler.py` as the registry for workflow separation; new workflow assets must be registered and pass scheduler validation before delivery.
18. Keep repository-level build bootstrap docs aligned with the standardized shared baseline: Debian 13, LLVM 18.1.x, CMake/CTest 3.31.6, Python 3.13.5, and Node.js v24.15.0 LTS where Node-backed tooling exists; when CI mirrors differ by host OS, document the mirror explicitly instead of drifting the toolchain version text.
19. Keep `docs/external/for-pafio/` aligned with the current `binary` vs `build` split: `--machine-info=json` remains the binary handshake, while `--source-build-info=json` owns the official source-layout contract for `pafio build`.
20. When a compiler/runtime/contract adjustment spans multiple checkpoints, add or update an explicit `docs/plan/` implementation plan instead of leaving the execution order only in handoff or runbook prose.
21. When the compiler-side source-build helper changes, keep `scripts/source-build-minimal.sh`, `docs/BUILD-AND-DEV-ENV.md`, and the `--source-build-info=json` handoff wording aligned in the same checkpoint.
22. When a plan remains in `docs/plan/` after one stage closes, make the file say whether it is still `Active`, `Repo-local baseline completed`, or ready for deletion after promotion; do not force readers to infer status from scattered stage tables.
23. Keep repository entry docs honest about maturity: if repo-local baselines are complete but ecosystem closure is still open, say that explicitly instead of leaving stale `early stage` wording in top-level entrypoints.
24. When an evidence-backed closure retires a gap, move it out of the open gap table, add the smallest closed-evidence note, update the matching checkpoint tree entry, and regenerate affected generated indexes.
25. Keep [../specs/POST-COMMIT-CI-CHECKS.md](../specs/POST-COMMIT-CI-CHECKS.md) aligned with actual GitHub Actions monitoring practice whenever commit, push, or CI handoff rules change.
26. Keep GitHub Actions sibling checkouts for `styio-pafio` and `styio-view` pinned to the same branch ref as `styio-nightly` when a workflow runs cross-repository gates.
27. Keep compact syntax references under `docs/design/syntax/` short and defer semantic detail to the owning design SSOT.
28. When syntax tokens change, update the compact syntax page, EBNF, and symbol reference together before regenerating indexes.
29. When standard-stream or lexical Block-completion declarations change, keep `src/StyioPrelude/resources.styio`, `docs/design/syntax/RESOURCE_IDENTIFIERS.md`, `docs/design/syntax/BLOCK_COMPLETION.md`, EBNF, symbol reference, and the language design aligned on accepted source forms. Track parser/backend availability separately from design acceptance.
30. Keep root `workflows/`, `workflows/skills/`, `workflows/workflows.toml`, generated workflow indexes, and `workflows/` mirrors aligned whenever reusable workflows or repo-local skills are added.
31. Keep repo-local skills concise: workflow docs own sequencing, while `skill.toml` owns reusable execution discipline and references.
32. Workflow and skill machine-readable definitions must use TOML (`*.toml`, `skill.toml`, and `agents/openai.toml`); Markdown remains explanatory only.
33. When test coverage changes require `workflows/TEST-CATALOG.md`, keep the catalog as an evidence index and point behavior ownership back to the implementation or test-quality runbook instead of embedding new language semantics there.
34. When language SSOT docs change token-count semantics, update the design page, EBNF, symbol reference, and test catalog in one checkpoint so docs readers do not see conflicting operator depth rules.
35. When syntax is retired, document the cutover in active SSOT docs and tests. Do not keep retired examples as runnable acceptance cases or recreate old milestone pages in the current tree.
36. When the SymPolicy Styio repository set changes, refresh the inventory with `gh repo list SymPolicy --limit 200`, then update root `README.md`, [../specs/REPOSITORY-MAP.md](../specs/REPOSITORY-MAP.md), and any active ecosystem plan that names the old repository set.
37. Keep [../specs/TECHNOLOGY-COMPONENT-INVENTORY.md](../specs/TECHNOLOGY-COMPONENT-INVENTORY.md) aligned with `styio-audit` whenever the technology stack, internal components, open-source components, dependency manifests, Apache-2.0 evidence, or commercial-risk boundaries change.
38. Keep external `styio-audit` execution wired through the repository delivery gate and dedicated GitHub Actions workflow whenever audit policy or cross-repo CI ownership changes.
39. Maintain GitHub merge gates through Rulesets rather than legacy classic branch protection; audit effective branch rules when required status-check governance changes.
40. Keep `docs/audit/defects/` ignored and out of tracked commits; promote audit findings into an approved docs collection or issue before they become repository-owned records.
41. When compiler source directories are renamed or split, update active path references across `AGENT-SPEC`, technology inventory, workflow mirrors, rollups, audit findings, and team-runbook mappings in the same checkpoint; old paths remain recoverable through Git history.
42. When C++ reference equivalence tests add algorithm cases, keep `workflows/TEST-CATALOG.md` as the concise evidence index and leave case layout details in `tests/algorithms/README.md` plus the Test Quality runbook.
43. When a user-driven syntax correction exposes a compiler/spec mismatch, record the reusable closure path in the workflow directory and make the workflow entrypoint tell agents to read the applicable workflow before editing.
44. Keep CMake build output conventions under `build/<variant>` across scripts, GitHub Actions, workflow docs, and external handoff docs; root `build-*` directories are legacy generated artifacts only and should not be introduced by new commands.
45. When a typed syntax addition changes design SSOTs and test catalogs together, keep the docs update concise: record the accepted source form, point catalog entries to evidence coverage, refresh generated indexes, and update team stats in the same delivery.
46. When source spellings are declared canonically equivalent, keep the ADR, design SSOT, workflow mirror, and test catalog aligned while leaving behavior ownership in parser, Sema / IR, Codegen / Runtime, and Test Quality runbooks.
47. When a milestone test catalog changes for new resource-management syntax, update only the evidence index there and keep lifecycle rules in the owning team runbooks; then refresh `DOC-STATS.md` before rerunning the team-docs gate.
48. Keep the Styio / `styio-benchmark` boundary explicit: Styio docs may describe probes and compatibility wrappers, but benchmark workloads, runners, baselines, reports, and regression records must point to `styio-benchmark`.
49. When compiler-owned resource topology code changes, keep the design SSOT, test catalog, Sema / IR runbook, Test Quality runbook, and `DOC-STATS.md` aligned in the same delivery so source, evidence, and ownership do not drift.
50. When retired syntax is removed from active compiler examples, keep active design docs, EBNF, symbol reference, language design, standard-library notes, implementation plans, and test catalogs aligned on the same retired/negative-test wording.
51. When the compiler-side native artifact command changes, keep root README usage, the ecosystem CLI contract matrix, and the `styio-benchmark` boundary language aligned: `styio build <file_path> -o <artifact_name>` owns artifact production, while benchmark workloads and scoreboards remain in `styio-benchmark`.
52. Public resource-management statements must cite primary language or platform sources and map each borrowed practice to Styio's actual compiler evidence. Keep the wording at "modern resource-management language" unless formal proof, async/task stress evidence, and driver-family validation justify a stronger statement.
53. Keep public wording concise and evidence-scoped. Do not speculate about companies, organizations, individuals, projects, products, or their capabilities; do not use absolute marketing superlatives or unsupported superiority language in repository docs.
54. README showcase examples must point to repository-local Styio source that can be run from the repository root, and the documented command/output pair must be verified before publication.
55. Root community files such as `CONTRIBUTING.md`, `SECURITY.md`, `SUPPORT.md`, `CODE_OF_CONDUCT.md`, `CHANGELOG.md`, and release-policy documents are approved public docs only when `scripts/docs-audit.py` classifies them explicitly and their wording stays evidence-scoped.
56. Active `example/` and `tests/` directories must not carry non-runnable language drafts. Delete drafts from the current tree after durable rules are promoted, and keep active README links pointed at CTest-covered examples.
57. Do not describe Styio as equivalent to another language's resource model. State the exact compiler evidence Styio has, then list external practices only as references.
58. Historical deprecated syntax belongs in Git history, not active docs; do not recreate old syntax catalogs or copy historical syntax into active examples without checking the active SSOT.
59. When local divergent history is reintroduced after an upstream governance update, preserve the old branch or stash reference in a rollup ledger and migrate only slices that fit current repository ownership. Do not restore deleted plan trees, old service paths, or superseded ecosystem names as active docs.
60. Keep rollup ledgers and migration notes free of developer-machine absolute paths; describe portability behavior generically unless an owned contract explicitly requires a literal path form.
61. Track valid generated-index support files and installed shared assets even when broad ignore patterns match their directories. When `docs/plan/reports/` or `share/styio/prelude/` files become required by docs audit, install layout, or source-build metadata, force-track the owned files and refresh `DOC-STATS.md` in the same change.
62. When `>>` language docs distinguish pulse transfer from standalone continue, keep EBNF, language design, active syntax, resource docs, symbol reference, rollups, agent specs, examples, and the owning implementation runbooks aligned in the same checkpoint.
63. When writable-resource iterable writes change between whole-value serialization and per-item pulse emission, keep resource identifier docs, EBNF, language design, active syntax, symbol reference, handle capability wording, Sema / IR ownership, and security evidence aligned in the same checkpoint.
64. Keep `docs/design/syntax/SYNTAX-CONVERGENCE-MATRIX.json` aligned with the current feature-test layout. The matrix and `scripts/syntax-convergence-gate.py` are durable gate inputs, so do not point them back at deleted milestone paths or archived syntax drafts.
65. Callable binding docs must teach the unified binding model consistently across active syntax, EBNF, language design, symbol reference, and the convergence matrix: `=` is mutable, `:=` is final, `#` marks a callable/operation-channel binding, and direct resource atoms such as `# sink = @stdout` stay invalid because resources remain in the visible `@` family.
66. Range docs must keep naked `start..end` as the expression-level range form, bracketed `[start..end]` as the canonical materialized range source, and `[start..end..step]` as removed/rejected wording across active syntax, EBNF, language design, symbol reference, examples, test catalog, and editor grammar notes. The old bracket stride mode (`[>>, 2]`) is also removed and must not be reintroduced.
67. Reserved-symbol wording is explicit: `<~` and `~>` participate in no syntax feature until the language design declares an activation; the infix apply-pipe spelling (`f <| a <| b`) is removed in favor of ordinary call syntax `f(a)(b)`; and Styio has no ordinary value fallback/coalescing operator. A single `|` is consumed only by an already-open type-union production (`? | T`), guard else production (`?(cond) => ... | ...`), or leading `?| operation | fallback` settlement production. General `a | b` and `a ?? b` are rejected before Sema; effect, purity, truthiness, or inferred operand types never reinterpret them. `??` has no accepted role and its orphan token/diagnostic-extract paths are deletion targets. `source -> destination` is one visual, directional core operation: data really flows from the left position to the right slot. Endpoint types may choose validation and lowering, but documentation must not rename those endpoint cases into assignment, export, redirect, resource-write, or task-binding meanings. `?|` is orthogonal and settles its complete operation; canonical value capture is `answer = ?| operation | fallback`, while `?| (operation -> destination) | fallback` is ordinary composition. Task-specific typed-target `?| job -> answer: T` and targetless `?| -> answer: T` paths are deletion debt, not aliases or reserved syntax. Keep EBNF, symbol reference, language design, active syntax, Block-completion docs, team runbooks, rollups, and test catalog aligned on these decisions.
68. Zip `&` is an event-arrival barrier: the first-arriving value waits until the other side delivers, no staleness/timeout/tolerance-window policy exists at the operator, and the removed `&[expr]` spelling stays rejected. Lexical Block completion does not activate a continuation system. Capture, resume, discontinue, implicit scope behavior, and continuation failure typing remain behind the continuation admission decision and must not be written as accepted syntax before that decision lands.
69. Selector docs teach the bracket family as a pure-symbol selection algebra ("brackets select, functions compute"): no identifier appears inside `[]`. The word-mode selector spellings `[avg, n]` / `[max, n]` and the deferred `[min/std/ema/rsi, n]` family are removed in favor of ordinary call syntax `avg(series, n)` recognized in Sema (matrix-helper model), with the parser bracket path documented as compatibility debt. The stride selector `x[%n]` (keep index ≡ 0 mod n; `[%1]` identity; `[%0]` rejected) is the design-accepted stride surface and stays documented as parser-pending and fail-closed until implementation evidence lands.
70. Derived bindings: the accepted surface is `name := $(deps) => expr` (frame-committed derived slot); the removed head spelling `name $(deps) := expr` is not syntax. Docs must teach the four frame-commit contracts (frame-edge topological recomputation at most once per frame, frame constancy under the frame-lock invariant, compile-time static graph with static cycle rejection, value decay without signal types) together with the fail-closed usage whitelist of Language Design §5.3 (module-scope declaration only, `:=` only, single pure effect-classified expression body, module-scope value dependencies only, no empty/duplicate/unused/identity captures, frame-context-only rewrites of captured variables, no task-block reads), and keep the surface documented as parser-pending and fail-closed until implementation evidence lands.
71. Three control-flow/operator decisions are settled and closed; docs must state them as decisions, not open questions. (a) Break `^...` and continue `>>...` are single-level only: token length never encodes nesting depth, multi-level jump spellings are permanently rejected, and the documented rationale is goto-hell prevention. (b) `>>` serving multiple roles (pipe / iterate, resource-write shorthand, standalone continue) is an explicit design requirement; disambiguation stays compiler-owned context logic and the roles will not be split across symbols. (c) `<<` type-directed unification is deliberately low priority: current copy/snapshot/compatibility-pull behavior stands, and `<<` redesign is not re-raised until the priority is explicitly changed.
72. Resource sessions and topology scope: topology `@name` stays root-only with the standing diagnostic (`The global resource cannot be initialized in a local block`), including inside sessions. The settled session surface is `|?| { ... }` with no `session` keyword (Resource Topology §4.2 / Language Design §6.10): mid-transfer `|?|` only when execution symbols stand before and after; statement-start settlement opens with `?|`; body whitelist is handles and anchors only; default Close; `|!|(cleanup)` special exit; `|>` deferred settlement; structured escape for accepted owned tasks; effect-first settlement obligation; deterministic bounded preservation of primary and secondary completion families; and static propagation of every unhandled family into the enclosing operation/callable. Do not describe a Java-style exception object, Erlang supervisor, ambient program-level failure channel, or dynamic handler search. Keep language design, EBNF (`TOK_SESSION_MARK` / `TOK_SESSION_EXIT` / `TOK_SETTLE_FWD`), symbol reference, active syntax, Block-completion boundary (`|>` activated; `|<-` reserved), and Sema/IR wording aligned. This surface does not activate continuation semantics. Scoped subtopology remains a separate fail-closed reserve. Surface is parser-pending until implementation evidence lands.
73. Pressure observer docs teach the accepted observer protocol: `channel.pressure >> #(p) => { ... }` delivers through a single-slot conflated latest-wins level sensor (readings never queue, so pressure streams cannot themselves develop pressure), pulses fire only on hysteresis state transitions (enter/exit/escalate) with watermarks owned by family policy, the payload is a prelude-declared read-only struct (`pending`/`limit`/`peak`) that depends on the general struct story landing first, observer bodies run off the writer path, and observer-write cycles (including cross-resource cycles), duplicate observers, non-module-scope observers, and zip use of pressure streams are rejected. `Q01-A` already fixes that pressure is not a failure by itself, catch-all does not capture it, nominal completion arms use ordinary identifier-based `family` / `family(binding)` syntax, and settlement never retries implicitly. Do not state that the observer contract chooses whether or how a resource family escalates pressure; `Q09` owns that family policy and payload declaration, while dropping, waiting, replacing, scheduling, or explicit retry remain separately owned protocol/feature choices. Keep the surface fail-closed under `STYIO_SEMA_RESOURCE_PRESSURE_OBSERVER_UNSUPPORTED` until a family activates.
74. Callable completion signatures use `T ?| {family, family}` only. The braces/comma list is a compile-time nominal upper bound, never a runtime set/dict or value union; `: T` alone is the sole empty-bound spelling, while an eligible local/private callable must omit the entire `: T` contract to request whole-summary inference. Keep the removed bare-line form `T ?| family | family`, `?| {}`, duplicates, non-family identifiers, and trailing commas out of positive docs. `Q02-SIG` and `Q02-INF` are design-approved and implementation-pending: only a capture-safe, final `:=`, non-recursive, non-boundary local/private callable value receives a definition-site principal constrained rank-1 scheme; each use is freshly instantiated, `# f = ...` only implements an established stable scheme, and required boundaries remain explicit. Link detailed prose to `Styio-Callable-Principal-Inference.md`; never present compiler displays such as `forall`, `Literal`, or `Add` as new source syntax. The approved exact-literal/scalar-Add subset is documented separately by item 75; Q05 as a whole is not closed.
75. `Q05-LIT-ADD` is design-approved and implementation-pending; link normative prose to `Styio-Exact-Literals-and-Builtin-Add.md`. Teach all ten approved parts together: exact integer/decimal terms; failure-closed contextual materialization and decimal `-0.0`; the finite `i8`–`i128`/`f32`/`f64` scalar Add table; no mixed-concrete promotion; selected-row result type; late ordinary-boundary `i64`/`f64` defaults but no scheme default or magnitude widening; checked signed addition with no-payload `overflow` and fixed strict IEEE float behavior; constant/runtime parity; conservative finite completion unions for generalized Add (`add_five` exposes `{overflow}`); and constant-known overflow as the same completion edge rather than a separate syntax/type rule. Label internal `Literal`/`Add` terms as compiler metadata, never source syntax. Keep explicit conversion, other operators, aliases/unsigned types, string/container/matrix relations, and NaN payload/equality/order visibly pending under active Q05. Operand readiness, completion stop, and publication now link to the approved Q03-F owner rather than an open question.
76. `Q03-F` is design-approved and implementation-pending; [Styio Functional Evaluation and Effect Ordering](../design/Styio-Functional-Evaluation-and-Effect-Ordering.md) is its unique semantic owner and its dedicated Better Plan is the implementation owner. Keep Language Design, EBNF, Symbol Reference, Active Syntax, Block Completion, Operation Completion, exact-literal/Add, Resource Topology, the decision ledger, review queue, all affected runbooks, rollups, and the test catalog synchronized. Teach strict values, dependency-only safe-pure siblings, explicit Block/effect/completion/resource edges, fail-closed unordered sensitive siblings, independent `->` prerequisites, lazy control edges, no rollback, mandatory exits/publication, and four separate optimizer rights. Never turn parser precedence, AST visitation, stable topological tie-break, LLVM order, or physical pending-write order into source semantics, and never introduce a runtime exception/handler/transaction model in prose.

## Change Classes

1. Small: typo, link fix, or local README wording. Run docs audit.
2. Medium: new docs collection, generated index config, SSOT table change, external handoff doc, or CLI/runtime contract matrix update. Update policy and run generated-index checks.
3. High: repository boundary, archive lifecycle, docs audit rule, or ecosystem ownership change. Use checkpoint workflow and coordinate affected implementation teams.

## Required Gates

Documentation gates:

```bash
python3 scripts/docs-index.py --write
python3 scripts/workflow-scheduler.py check
python3 scripts/team-docs-gate.py
python3 scripts/docs-lifecycle.py validate
python3 scripts/ecosystem-cli-doc-gate.py
python3 scripts/docs-audit.py
```

Unified docs/process delivery floor:

```bash
./scripts/delivery-gate.sh --skip-health
```

Optional inventory commands:

```bash
python3 scripts/docs-audit.py --manifest valid --format tree
python3 scripts/docs-audit.py --manifest invalid --format list
python3 scripts/docs-lifecycle.py candidates --family all --format tree
```

Checkpoint-grade:

```bash
./scripts/checkpoint-health.sh --no-asan --no-fuzz
```

## Cross-Team Dependencies

1. Frontend, Sema / IR, Codegen / Runtime, IDE / LSP, and CLI / Nano must review docs that describe their behavior.
2. Test Quality must review test catalog, workflow, and oracle documentation changes.
3. Perf / Stability must review benchmark, soak, and report lifecycle docs.
4. Coordination owner must review repository-boundary and external ecosystem handoff changes.

## Handoff / Recovery

Record unfinished docs/ecosystem work with:

1. Owning SSOT and files changed.
2. Generated indexes that still need refresh.
3. Link or metadata audit failures.
4. Team runbook gate failures, required runbook paths, and template/format violations.
5. External repository or handoff owner affected.
6. Archive/rollup lifecycle action still pending.
