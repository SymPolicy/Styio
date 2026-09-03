# PLAN-001 Restore nightly CI green and migrate docs/plan to v3

Phase: completed · Revision: 2

This document is a render-only projection of `Plan.json`. Edit `Plan.json`; never edit this file.

## Intent

**Goal**: 修复 2026-08-19 深度报告列出的全部 10 个紧迫问题并完成 docs/plan v3 迁移：nightly 恢复全绿（styio_pipeline 23 项失败归零、ASan/UBSan 构建测试通过、fuzz 崩溃清零），分支保护生效，上游同步推进，测试构建模型修复，账本刷新，Windows smoke CI、徽章/Issues、coverage/benchmark CI 与卫生修复落地；docs/plan 旧代工件彻底删除且未闭环工作显式并入新计划

**In scope**
- Q1：修复 PR #10/#12 引入的 23 项 styio_pipeline 失败（证据驱动逐案判定，只向前修复，不回退已交付特性；期望失真须对照设计 SSOT 确认后更新 golden/诊断期望并记录证据）
- Q2：修复 sanitizer 构建链接失败（libstyio_frontend_core 引用 codegen 符号 typeinfo for StyioToLLVM），做恢复绿灯所需的最小分层修复
- Q3：下载三个 fuzz 失败 run（32213494185、32096745697、31862492601）的回归包，复现并修复 lexer/parser 崩溃，崩溃输入固化进 tests/fuzz/corpus/
- Q4：通过 gh api 为 nightly 分支设置 required status checks（platform-adaptation linux/macos、golden-standard），并同步 docs/specs/POST-COMMIT-CI-CHECKS.md
- Q5：按 AGENTS.md 推进上游同步——上游 PR 必须以 Unka-Malloc:nightly 为 head 且需先合并本交付 PR；若被维护者合并动作阻塞则以 blocked_by_authority 报告并给出可执行步骤
- Q6：用内部测试静态库替代 tests/CMakeLists.txt 的源码直编（STYIO_INTERNAL_PARSER_SUPPORT_SOURCES 模式），消除重复编译并锁定分层
- Q7：刷新 MIGRATION-LEDGER.md 失真数据（M-CLI-01 行数等），执行 M-AUDIT-01 四步归档（核对、archive manifest 记录、ledger 重生成、git rm 四份 2026-04-22 审计），为 main.cpp 行数加棘轮门禁
- Q8：在 CI 增加 windows-2022 smoke job（configure+构建+两个已定义的 Windows 过滤测试），验证依赖 push 后远端运行
- Q9：修复 README.md/README_zh.md 徽章（styio-org→Unka-Malloc），用 gh api 启用 Issues 并为 fuzz/sanitizer 建立追踪单
- Q10：新增 coverage report-only CI job 与 benchmark CI 联动；修复 styio-audit.yml 运行时 sed 补丁（外部仓库若无权限则阻塞报告）；清理本地残留并加固 hygiene 检查
- 迁移：docs/plan 旧代格式（Manifest 数组、roadmap/ 树、Capabilities.json、Styio-Project-Roadmap.md）彻底删除并以 v3 结构替代；引用旧结构的所有仓库脚本、门禁、CI、文档同步更新；旧计划未闭环工作（M7 slice、Topology v2、P4、W1-W8 队列、延后检查点）以显式 backlog 形式并入新计划

**Out of scope**
- 旧计划未闭环条目只登记不实现（M7、Topology v2、P4 等战略目标属后续交付）
- 巨石文件大规模重构（G5 目标；本计划 Q7 仅做账本刷新与棘轮门禁）
- Windows 完整测试矩阵（仅 smoke job）
- 覆盖率 95% 强制门禁上 CI（仅 report-only）
- 任何语法/语义扩展或新语言特性
- 删除或弱化失败测试以掩盖回归

**Success**
- ctest -L styio_pipeline 全绿且安全/核心套件无新增失败
- ASan+UBSan RelWithDebInfo 配置构建 styio_typeinfer_internal_test 成功并通过
- 三个 fuzz 回归包输入全部复现修复并固化进 corpus，fuzz smoke 标签通过
- nightly 分支保护 required checks 生效（gh api 可读回验证）
- 上游 promote 已执行或给出 blocked_by_authority 的可执行报告
- 内部测试静态库替代源码直编，架构分层门禁通过
- MIGRATION-LEDGER 数据刷新、M-AUDIT-01 四步归档完成、行数棘轮门禁生效
- windows-2022 smoke job 存在于 CI 且远端运行结果可观测
- README 徽章指向 Unka-Malloc/styio-nightly，Issues 已启用且有 fuzz/sanitizer 追踪单
- coverage report-only 与 benchmark CI job 存在；styio-audit 补丁修复或阻塞报告
- docs/plan 无旧代工件残留，全部仓库门禁（docs、hygiene、manifest 校验、language gates）在 v3 结构下通过，未闭环工作在新计划内显式登记

**Risk boundary**
- 不回退 PR #10/#12 已交付特性；实现回归与期望失真逐案以证据判定；不得删除或弱化失败测试掩盖回归；不得扩大语法/语义范围
- GitHub 管理操作仅限 Unka-Malloc/styio-nightly（分支保护、Issues）；上游 PR 遵循 AGENTS.md（head 必须为 Unka-Malloc:nightly，且先合并本交付 PR）
- 外部仓库（eBioRing/styio-audit、eBioRing/Styio）无权限或需等待维护者动作时以 blocked_by_authority 报告，不强行越权
- 旧工件删除必须一次交付内完成且可证明（一次性验证脚本，不留永久门禁）

## Decisions

Dossier status: resolved

### Q-001 本次交付覆盖哪些紧迫问题？

Context: 2026-08-19 的仓库深度报告列出 10 个紧迫问题（Q1 pipeline 红灯已本地复现 23 项失败；Q2 sanitizer 链接失败；Q3 fuzz 崩溃未闭环；Q4 红灯合并纪律；Q5 上游落后 503 提交；Q6 测试直编源码；Q7 巨石文件/账本失真；Q8 Windows 死代码；Q9 Issues 禁用/徽章错误；Q10 覆盖率/benchmark 未上 CI 与卫生回潮）。用户已裁决：新计划放入 docs/plan、旧计划未闭环工作并入、旧计划彻底删除。

Resolution: opt_all (user selection)

- [x] `opt_all` 全部 10 个问题 + 工作区迁移
  - Q1–Q10 全部纳入交付：pipeline 修复、sanitizer 链接修复、fuzz 崩溃清零、分支保护设置、上游 promote、内部测试库替代直编、账本刷新与 M-AUDIT-01 归档、Windows smoke CI、徽章修复与 Issues 启用、coverage/benchmark CI 与卫生修复。
  - 验收覆盖全部 10 项；受外部权限阻塞的条目（如上游合并等待）以 blocked_by_authority 终态报告。
- [ ] `opt_core` 仅 Q1+Q2+迁移
  - 只恢复 nightly 绿灯并完成 docs/plan v3 迁移。
  - Q3–Q10 全部留作后续。
- [ ] `opt_mid` Q1–Q4+迁移
  - 恢复绿灯、fuzz 清零、分支保护设置与迁移。
  - Q5–Q10 留作后续。

### Q-002 交付终态推进到哪一步？

Context: AGENTS.md 规定：提交必须在临时分支，push 只到 origin，合并经 PR 进 downstream nightly。当前已在 agent/nightly-ci-green-restore 分支。历史 PR (#10/#11/#12) 由维护者合并。用户已裁决：临时分支 + PR 指向 nightly。

Resolution: opt_a (user selection)

- [x] `opt_a` 临时分支提交 + push + 开 PR 指向 nightly，不自行合并
  - Worker 在临时分支提交；交付完成时 push 到 origin 并用 gh 创建指向 nightly 的 PR。
  - 合并动作留给维护者，符合 AGENTS.md 的最保守读法。
- [ ] `opt_b` 本地提交即止，不 push
  - 所有提交只留在本地临时分支。
  - push 与 PR 由维护者手动执行。

### Q-003 golden/诊断期望与实现不一致时的修复策略？

Context: 23 个失败测试中部分可能是 PR #10/#12 有意改变诊断文案/golden 输出而未同步期望，部分可能是真实实现回归。用户已裁决：不回退，追加修复即可。

Resolution: opt_a (user selection)

- [x] `opt_a` 证据驱动逐案判定，只向前修复
  - 每个失败先判定‘实现回归’还是‘期望失真’；实现回归修代码，期望失真在确认新行为符合设计 SSOT 后更新 golden/期望并记录证据。
  - 禁止回退 PR #10/#12 已交付特性；禁止仅为转绿而批量更新期望或删除失败测试。
- [ ] `opt_b` 回退优先
  - 凡不能快速定性的失败，一律回退 PR #10/#12 的相关改动。
  - 用户已明确排除此路径。

### Observed repository facts

- none

## Requirements

| Code | Statement | Sources |
| --- | --- | --- |
| REQ-001 | The styio_pipeline label runs with zero failures after every previously failing case is repaired by moving forward, without deleting or weakening a test. | user-request, ctest evidence |
| REQ-002 | Each of the 23 recorded pipeline failures carries an explicit implementation-regression or stale-expectation verdict with its evidence source. | user-request, decision Q-003 |
| REQ-003 | The ASan plus UBSan configuration links every test target that previously failed and its security label passes. | user-request, nightly-sanitizers evidence |
| REQ-004 | Every crash input from the three failed nightly fuzz runs is reproduced, fixed, and preserved in the tracked corpus so the fuzz smoke label replays it. | user-request, nightly-fuzz evidence |
| REQ-005 | Internal test binaries consume a declared library boundary instead of recompiling frontend sources into each test executable. | user-request, tests build evidence |
| REQ-006 | The downstream nightly branch ruleset requires the named status checks with strict enforcement, readable back through the repository ruleset API. | user-request, ruleset evidence |
| REQ-007 | The downstream repository has Issues enabled and carries one tracking issue each for the fuzz and sanitizer regressions. | user-request, repository settings evidence |
| REQ-008 | Public badges and the post-commit check specification name the actual downstream owner and the actual enforced checks. | user-request, README evidence |
| REQ-009 | Upstream promotion is either performed inside the authorized boundary or reported as authority-blocked with verified divergence facts and executable steps. | user-request, decision Q-002 |
| REQ-010 | External policy and benchmark-asset changes that this delivery needs are reported as authority-blocked with the exact required content and executable steps. | user-request, external repository evidence |
| REQ-011 | The migration ledger reports measured current values instead of stale counts and states its refresh date. | user-request, ledger evidence |
| REQ-012 | The four dated audit reports are closed through the documented four-step archive path and no longer exist in the current tree. | user-request, ledger row M-AUDIT-01 |
| REQ-013 | A registered gate fails when the compiler entry file grows beyond its recorded ceiling. | user-request |
| REQ-014 | A native Windows smoke job exists in CI, builds the compiler and test binaries, and runs the two Windows-only filtered tests. | user-request, tests build evidence |
| REQ-015 | A report-only coverage job exists in CI, publishes its report, and is excluded from required status checks. | user-request |
| REQ-016 | A report-only benchmark job exists in CI, resolves the external benchmark checkout explicitly, and reports the exact missing-asset list when the external repository cannot satisfy the integration contract. | user-request, external repository evidence |
| REQ-017 | The released audit policy normalization step fails with the unmatched pattern instead of silently becoming a no-op. | user-request, workflow evidence |
| REQ-018 | The hygiene gate detects forbidden build and profiling residue that ignore rules currently hide, and the local tree carries none. | user-request |
| REQ-019 | The plan workspace contains only the v3 structure, and every legacy artifact of the previous generation is gone. | user-request |
| REQ-020 | Every repository script, registry, gate, and document that referenced the legacy plan structure now references the v3 structure and passes its own gate. | user-request |
| REQ-021 | Every unclosed item of the deleted plan is registered explicitly in the surviving ledger and plan index without being implemented. | user-request |
| REQ-022 | A one-time targeted command proves that no legacy plan artifact or reference remains, without leaving a permanent gate behind. | user-request |

## Architecture

One delivery, five mutually parallel Tasks split by real write ownership rather than by problem number, because the compiler source tree, the CI workflow tree, the documentation and gate tree, the downstream repository settings, and the external repositories are five surfaces that never write each other. All causally coupled repairs stay inside one Task, and every Task exposes its own internal concurrency through a minimal Node DAG.

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

## Tasks

Every Task belongs to the same mutually independent parallel frontier.

### TASK-001 compiler-signal-green

Tier: complex · Workload: heavy · Verification: code · Frontier: parallel

Outcome: Every compiler-side red signal on nightly turns green in one forward-only repair: the pipeline label passes, the sanitizer configuration links and passes, the fuzz corpus replays without a crash, and internal test binaries stop recompiling frontend sources.

Risks: quality, schema, concurrency, shared_resource
Requirements: REQ-001, REQ-002, REQ-003, REQ-004, REQ-005
Writes: src, tests, cmake, CMakeLists.txt
Exclusive resources: local default build directory build/default, local sanitizer build directory build/asan-ubsan, local fuzz build directory build/fuzz

In scope
- Classify and repair all 23 recorded styio_pipeline failures, updating an expectation only after confirming the new behavior against the design authority.
- Remove the archive-level layering cycle that breaks the sanitizer link of the internal type-inference test.
- Replace source-direct compilation in the test build with a declared internal library boundary.
- Download, reproduce, and fix the crash inputs from the three failed nightly fuzz runs and preserve them in the tracked corpus.
- Record one verdict per failing test and one regression note per fuzz crash inside the tests tree.

Out of scope
- Reverting any feature delivered by the two suspect merges.
- Deleting, skipping, or relaxing a failing test to obtain a green result.
- Any syntax or semantic extension, including new diagnostics beyond the confirmed current contract.
- Splitting the compiler entry file or any other monolith refactor.
- Editing documentation, gate scripts, CI workflows, or repository settings.

Outputs
- OUT-001 Per-test verdict record for the recorded pipeline failures (`tests/PIPELINE-FAILURE-VERDICTS.md`): Each repair is traceable to a verdict and an evidence source instead of an unexplained expectation change.
- OUT-002 Frontend sources free of backend run-time type dependencies (`src/StyioIR`): The frontend library no longer requires backend symbols, so single-pass linkers close the link.
- OUT-003 Declared internal library boundary for test binaries (`tests/CMakeLists.txt`): Internal tests link one compiled boundary instead of duplicating frontend translation units.
- OUT-004 Crash inputs and regression notes preserved in the tracked corpus (`tests/fuzz`): Every fixed crash is replayed by the fuzz smoke label on every future run.

Internal Node graph
- NODE-001 reproduce-pipeline-failures · after: none · Reproduce the full failure set in the default build and capture each failure's exact diagnostic, golden diff, or verifier message.
- NODE-002 reproduce-sanitizer-link · after: none · Reconfigure and build the sanitizer directory with the nightly compiler and flag set, reproduce the undefined run-time type reference, and name the producing and consuming objects with a symbol listing.
- NODE-003 collect-fuzz-regressions · after: none · Download the regression packs of the three failed fuzz runs, configure and build the local fuzz targets, and replay every artifact until each crash is reproduced.
- NODE-004 inventory-test-build-model · after: none · Inventory every test target that compiles frontend sources directly, and record which sources each target actually needs.
- NODE-005 classify-pipeline-failures · after: NODE-001 · Judge every failure as implementation regression or stale expectation by comparing the emitted behavior against the design authority and the two suspect merges.
- NODE-006 repair-codegen-dominance-family · after: NODE-005 · Fix the resource-effect and slice families whose emitted module fails verification because a release call does not dominate its allocation.
- NODE-007 realign-confirmed-expectations · after: NODE-005 · Update only the expectations whose current behavior was confirmed intended, including the broadened match scrutinee diagnostic and the five-layer golden lines.
- NODE-008 rebuild-internal-test-boundary · after: NODE-002, NODE-004 · Remove the frontend-to-backend header dependency where it is achievable without changing semantics, otherwise declare the true library boundary, and replace source-direct test compilation with that boundary.
- NODE-009 repair-frontend-fuzz-crashes · after: NODE-003 · Fix each reproduced lexer and parser crash at its root cause instead of guarding the fuzz entry point.
- NODE-010 preserve-fuzz-corpus · after: NODE-009 · Add each fixed crash input to the tracked corpus and write its regression note from the existing template.
- NODE-011 record-failure-verdicts · after: NODE-006, NODE-007 · Write the per-test verdict record with the verdict, root cause, and evidence source for every recorded failure.
- NODE-012 integrate-compiler-signals · after: NODE-011, NODE-008, NODE-010 · Rebuild the default, sanitizer, and fuzz directories and confirm the pipeline, security, and fuzz smoke labels together with the architecture layer gate.

Design
- approach
  - Diagnose before repairing. The verdict record is written from measured behavior, so a green result can never be reached by editing an expectation that nobody confirmed.
  - Treat the non-dominating release call as one shared defect of the cleanup emission path rather than as fifteen separate test failures, and confirm the shared root cause before touching individual cases.
  - Prefer removing the frontend dependency on backend run-time type information over teaching the linker to tolerate a cycle. Reject linker grouping flags and interface-multiplicity bumps, because they preserve the violation as a permanent compatibility layer.
  - If the dependency cannot be removed without changing IR or codegen semantics, declare the real boundary in the library definitions, keep the architecture layer gate passing, and record the residual layering debt as an out-of-scope finding rather than widening this Task.
  - Make the internal test boundary a compiled library that the test targets link, so the duplicated translation units disappear and the sanitizer link path matches the production link path.
  - Use a platform-independent symbol oracle in addition to a successful link, because the failing linker behavior is single-pass and the local linker is not.
  - Keep every fixed fuzz input in the tracked corpus so the existing fuzz smoke replay becomes the permanent guard, and add no new gate.
- patterns
  - pattern_catalog: refactoring-guru-catalog-22-v1
  - candidate: Visitor
  - decision: reject
  - pressure: The IR dispatch already uses double dispatch through a codegen visitor, and that visitor's complete type leaks into a frontend header, which creates the archive cycle behind the sanitizer failure.
  - expected_benefit: none for a redesign; the existing double dispatch is correct and a reshaped visitor would not remove the cycle by itself.
  - simpler_alternative: Keep the visitor and cut the header dependency to a forward declaration, or declare the real library boundary. Either is sufficient and far smaller.
  - application: Limit the change to the declaration seam and the library definitions; do not restructure element or visitor roles.
  - costs_and_rejections: Reshaping the visitor would touch every IR node class and every codegen entry point during a red-to-green repair, so it is rejected together with adjacent Adapter and Bridge layers over the same seam.

Acceptance
- AC-001 covers REQ-001
  - Given The default build directory is configured from the delivery branch
  - When The pipeline label runs after a full build
  - Then No pipeline test fails and none was removed or relaxed
  - Oracle: ctest --test-dir build/default -L styio_pipeline --output-on-failure --no-tests=error exits zero and reports 352 tests passing
  - Evidence: command from ctest pipeline label
- AC-002 covers REQ-002, OUT-001
  - Given The verdict record exists
  - When The record is inspected
  - Then Every recorded failing test carries one verdict and one evidence source
  - Oracle: rg -c "^- \[x\] " tests/PIPELINE-FAILURE-VERDICTS.md reports 23, and every entry names one failing test with the verdict implementation-regression or stale-expectation plus its evidence source
  - Evidence: command from verdict record count
- AC-003 covers REQ-003, OUT-002
  - Given The sanitizer directory is configured with the nightly sanitizer flags
  - When The previously failing internal test target is built and the security label runs
  - Then The link succeeds and the label passes
  - Oracle: cmake --build build/asan-ubsan --target styio styio_test styio_security_test styio_typeinfer_internal_test exits zero, then ctest --test-dir build/asan-ubsan -L security --output-on-failure --no-tests=error exits zero
  - Evidence: command from sanitizer build and security label
- AC-004 covers REQ-005, OUT-003
  - Given The test build no longer compiles frontend sources directly
  - When The build model and the layer gate are inspected
  - Then The direct-source list is gone and the layer gate still passes
  - Oracle: rg -n "STYIO_INTERNAL_PARSER_SUPPORT_SOURCES" tests/CMakeLists.txt finds no match and python3 scripts/architecture-layer-gate.py exits zero
  - Evidence: command from build model and layer gate
- AC-005 covers REQ-004, OUT-004
  - Given The fuzz directory is configured with fuzzing enabled and the crash inputs are in the tracked corpus
  - When The fuzz smoke label replays the corpus
  - Then No target crashes and every downloaded artifact is represented
  - Oracle: cmake --build build/fuzz --target styio_fuzz_lexer styio_fuzz_parser exits zero, then ctest --test-dir build/fuzz -L fuzz_smoke --output-on-failure --no-tests=error exits zero
  - Evidence: command from fuzz smoke replay

Focused regression
- `cmake --build build/default --target styio styio_test styio_security_test`
- `ctest --test-dir build/default -L styio_pipeline --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -L security --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -L golden_standard --output-on-failure --no-tests=error`
- `ctest --test-dir build/fuzz -L fuzz_smoke --output-on-failure --no-tests=error`
- `python3 scripts/architecture-layer-gate.py`
- `python3 scripts/runtime-surface-gate.py`
- paths: src, tests, cmake, CMakeLists.txt

### TASK-002 ci-workflow-signals

Tier: complex · Workload: medium · Verification: code · Frontier: parallel

Outcome: The workflow tree gains the three missing observability jobs and loses its silent external-policy patch, so native Windows, coverage, and benchmark signals exist and the always-on audit workflow can no longer normalize nothing without saying so.

Risks: operations, observability, release
Requirements: REQ-014, REQ-015, REQ-016, REQ-017
Writes: .github/workflows
Exclusive resources: repository workflow definitions

In scope
- Add a native Windows smoke job that configures, builds, and runs the two Windows-only filtered tests using the documented native Windows recipe.
- Add a report-only coverage job that publishes its report and stays out of required checks.
- Add a report-only benchmark job that resolves the external benchmark checkout explicitly and reports the exact missing-asset list.
- Make the released audit policy normalization assert each declared replacement and fail with the unmatched pattern.

Out of scope
- A full Windows matrix or Windows-specific source and build-system changes.
- Enforcing any coverage threshold in CI.
- Publishing or editing anything in an external repository.
- Adding a new job to the required status checks.
- Editing gate scripts, documentation, or repository settings.

Outputs
- OUT-005 Native Windows smoke job (`.github/workflows/windows-smoke.yml`): The Windows-only build and test branches stop being unreachable code and produce an observable remote result.
- OUT-006 Report-only coverage job (`.github/workflows/nightly-coverage.yml`): Coverage is measured and published without becoming a merge gate.
- OUT-007 Report-only external benchmark linkage job (`.github/workflows/nightly-benchmark.yml`): The external benchmark integration seam is exercised and its missing assets are named instead of silently unused.
- OUT-008 Fail-loud normalization of the released audit policy (`.github/workflows/styio-audit.yml`): Policy drift in the external repository surfaces as an explicit failure instead of a vacuous audit scope.

Internal Node graph
- NODE-013 inventory-workflow-contracts · after: none · Record every current trigger, job name, check name, and reusable step so new jobs match the existing contract and no existing check name changes.
- NODE-014 add-windows-smoke-job · after: NODE-013 · Add the Windows smoke workflow that installs a Windows LLVM development package per the repository build guide, configures, builds the compiler and test binaries, and runs the two Windows-only filtered tests.
- NODE-015 add-coverage-report-job · after: NODE-013 · Add the scheduled report-only coverage workflow that runs the existing coverage gate script, uploads the report, and never fails the branch.
- NODE-016 add-benchmark-linkage-job · after: NODE-013 · Add the scheduled report-only benchmark workflow that checks out the external benchmark repository at its default branch, reports which required integration assets are present or missing, then attempts the required-mode configure and probe build.
- NODE-017 harden-audit-normalization · after: NODE-013 · Replace the silent policy rewrite with one that asserts each declared replacement applied and fails with the exact unmatched pattern.
- NODE-018 validate-workflow-definitions · after: NODE-014, NODE-015, NODE-016, NODE-017 · Parse every workflow definition and assert the new job identities, runner images, report-only settings, and preserved check names.

Design
- approach
  - Put the expensive coverage and benchmark work on a schedule with manual dispatch, matching how the sanitizer and fuzz workflows already isolate long jobs, so pull-request feedback stays fast.
  - Trigger the Windows smoke job on push so the branch produces a remote result before merge, and keep it out of required checks until it has a green history.
  - Keep report-only literally report-only: continue on error and upload the artifact, rather than lowering a threshold to zero and losing the signal.
  - Reuse the existing coverage gate script and the existing external benchmark integration contract instead of writing new measurement logic in YAML.
  - Verify the external benchmark asset list in the job itself, because the external default branch currently publishes only part of the contract and a bare configure failure would hide which assets are missing.
  - Make the audit normalization fail loudly rather than tolerant, because a silent no-op leaves the audit scoped over paths that no longer exist and turns a required check into a vacuous one.
  - Do not touch existing job names. They are the identities the branch ruleset will require.

Acceptance
- AC-006 covers REQ-014, REQ-015, OUT-005, OUT-006
  - Given The workflow tree contains the new definitions
  - When Every workflow definition is parsed
  - Then All definitions load and the new jobs declare their intended runners and report-only behavior
  - Oracle: python3 -c "import glob,yaml;[yaml.safe_load(open(p)) for p in glob.glob('.github/workflows/*.yml')]" exits zero, rg -n "windows-2022" .github/workflows/windows-smoke.yml matches, and rg -n "continue-on-error" .github/workflows/nightly-coverage.yml .github/workflows/nightly-benchmark.yml matches both files
  - Evidence: command from workflow definition validation
- AC-007 covers REQ-014
  - Given The Windows smoke job exists
  - When Its test step is inspected
  - Then It runs exactly the two Windows-only filtered tests already defined by the test build
  - Oracle: rg -n "styio_test_windows_five_layer_pipeline" .github/workflows/windows-smoke.yml and rg -n "ClassifiersCoverFallbackAndPhaseFamilies" .github/workflows/windows-smoke.yml both match
  - Evidence: command from Windows job test selection
- AC-008 covers REQ-016, OUT-007
  - Given The benchmark job exists
  - When Its asset precondition step is inspected
  - Then It names the required integration assets and reports the missing ones before configuring
  - Oracle: rg -n "styio-benchmark" .github/workflows/nightly-benchmark.yml matches and rg -n "STYIO_REQUIRE_EXTERNAL_BENCHMARK" .github/workflows/nightly-benchmark.yml matches
  - Evidence: command from benchmark job contract
- AC-009 covers REQ-017, OUT-008
  - Given The audit workflow normalizes the released policy
  - When A declared replacement does not apply
  - Then The step fails and prints the unmatched pattern
  - Oracle: rg -n "unmatched" .github/workflows/styio-audit.yml matches and the normalization step raises on a non-applied replacement instead of writing the file unchanged
  - Evidence: command from audit normalization guard

Focused regression
- `python3 -c "import glob,yaml;[yaml.safe_load(open(p)) for p in glob.glob('.github/workflows/*.yml')]"`
- `rg -n "name: platform-adaptation / linux-ci-gate|name: platform-adaptation / macos-ci-gate|name: test / smoke|name: test / golden-standard|name: styio-audit" .github/workflows`
- paths: .github/workflows

### TASK-003 plan-workspace-and-gate-migration

Tier: complex · Workload: heavy · Verification: code · Frontier: parallel

Outcome: The plan workspace exists only in its v3 form with every legacy artifact removed, every dependent script, registry, gate, and document retargeted, the migration ledger and audit archive back to measured truth, a line ratchet on the compiler entry file, hygiene residue detected and cleared, and unclosed legacy work registered rather than implemented.

Risks: migration, removal, schema, operations, quality
Requirements: REQ-011, REQ-012, REQ-013, REQ-018, REQ-019, REQ-020, REQ-021, REQ-022
Writes: docs/plan, docs/rollups, docs/teams, docs/audit, docs/archive, docs/history, docs/specs, docs/INDEX.md, docs/README.md, docs/design/README.md, docs/external/README.md, docs/external/for-pafio/README.md, scripts, workflows, .gitignore
Exclusive resources: generated documentation index projections, documentation lifecycle archive state, repo-local tool and workflow registries

In scope
- Replace the legacy plan generation with the v3 workspace and delete the legacy capability catalog, roadmap document, and roadmap tree.
- Retarget every repository reference to the legacy plan structure, including the repo-local plan validator, the tool registry, the runbooks, and the documents that link into the deleted tree.
- Exempt the generated plan workspace from the document metadata rule that its generated files cannot satisfy.
- Refresh the migration ledger with measured values and complete the four-step closure of the four dated audit reports, extending the lifecycle tool with the audit family it currently lacks.
- Add a registered line ratchet gate for the compiler entry file and wire it into the existing scheduler profiles so it runs without touching CI definitions.
- Detect forbidden ignored build and profiling residue in the hygiene gate and clear the residue that exists locally.
- Register every unclosed item of the deleted plan in the surviving gap ledger and the plan index.
- Correct the post-commit check specification owner reference and document the checks the downstream ruleset enforces.
- Refresh the runbooks and generated documentation projections this delivery requires.

Out of scope
- Implementing any registered backlog item.
- Restoring or preserving any legacy plan artifact for compatibility.
- Adding a permanent gate for legacy-artifact absence.
- Editing compiler sources, test sources, CI workflow definitions, the public readme files, or repository settings.

Outputs
- OUT-009 Plan workspace in v3 form only (`docs/plan`): One current plan structure remains, with no legacy generation to interpret or maintain.
- OUT-010 Migration ledger with measured values (`docs/rollups/MIGRATION-LEDGER.md`): Ledger rows can be trusted as current evidence rather than stale claims.
- OUT-011 Closure rows for the four dated audit reports (`docs/archive/ARCHIVE-MANIFEST.json`): Removed audit reports keep provenance without retaining the reports.
- OUT-012 Registered line ratchet gate for the compiler entry file (`scripts/monolith-line-ratchet-gate.py`): The entry file cannot grow further while its split remains future work.
- OUT-013 Repo-local plan workspace validator for the v3 structure (`scripts/manifest_tool.py`): The registered repo-local gate validates the structure that actually exists.
- OUT-014 Hygiene detection for forbidden ignored artifacts (`scripts/repo-hygiene-gate.py`): Residue hidden by ignore rules is reported instead of accumulating.
- OUT-015 Explicit register of unclosed legacy work (`docs/rollups/NEXT-STAGE-GAP-LEDGER.md`): Nothing the deleted plan left open is lost, and nothing is silently implemented.

Internal Node graph
- NODE-019 inventory-legacy-plan-surface · after: none · List every legacy plan artifact, every document that links into it, and every unclosed item the deleted plan still owns.
- NODE-020 inventory-gate-coupling · after: none · List every script, registry, profile, and gate that reads the plan workspace or constrains new files, including the document metadata rule that the generated plan files cannot satisfy and the registration rules for a new script.
- NODE-021 verify-audit-findings-closed · after: none · Verify each finding of the four dated audit reports against the implemented-decision record and its matching inventory, and record which evidence closes it.
- NODE-022 measure-ledger-drift · after: none · Measure the current values the ledger misreports, including the compiler entry file length, and record the measured baseline for the ratchet.
- NODE-023 write-v3-plan-workspace · after: NODE-019 · Write the v3 plan workspace, including the index and boundary documents that describe the current structure and its state model.
- NODE-024 remove-legacy-plan-artifacts · after: NODE-023 · Delete the legacy capability catalog, roadmap document, and roadmap tree.
- NODE-025 register-open-work · after: NODE-019 · Register every unclosed legacy item in the surviving gap ledger and the plan index, without implementing any of them.
- NODE-026 retarget-legacy-references · after: NODE-019, NODE-020 · Retarget every remaining reference to the legacy structure in documents, runbook tool lists, and registries.
- NODE-027 replace-plan-workspace-validator · after: NODE-020 · Replace the stale array-manifest validator with a v3 workspace structure validator at the registered path, and refresh its registry entry.
- NODE-028 exempt-generated-plan-workspace · after: NODE-020 · Exempt the generated plan workspace files from the document metadata rule and from the runbook trigger set, so the framework's own generated documents cannot fail the documentation audit.
- NODE-029 close-audit-archive · after: NODE-021, NODE-020 · Extend the lifecycle tool with the audit family, record the closure rows, regenerate the archive ledger, and remove the four reports from the current tree.
- NODE-030 refresh-migration-ledger · after: NODE-022 · Rewrite the drifted ledger rows with measured values and record the refresh date.
- NODE-031 add-line-ratchet-gate · after: NODE-022 · Add the line ratchet gate at the measured ceiling, register it in the tool registry, and add it to the scheduler profiles that already run in CI.
- NODE-032 harden-hygiene-residue · after: NODE-020 · Add detection for forbidden ignored artifacts to the hygiene gate without changing its existing modes.
- NODE-033 clear-local-residue · after: none · Remove the stray configured build root, profiling output, and test-discovery residue from the local tree.
- NODE-034 update-team-runbooks · after: NODE-024, NODE-025, NODE-026, NODE-027, NODE-028, NODE-029, NODE-030, NODE-031, NODE-032 · Refresh every runbook this delivery touches and the documentation statistics page.
- NODE-035 regenerate-doc-projections · after: NODE-034 · Regenerate the documentation indexes, validate the lifecycle state, and confirm the documentation audit, team gate, and registry gates.
- NODE-036 prove-legacy-removal · after: NODE-035 · Run the one-time targeted search that proves no legacy plan artifact or reference remains, and record its result without adding a permanent gate.
- NODE-037 verify-hygiene-state · after: NODE-032, NODE-033 · Confirm the hardened hygiene gate reports a clean tracked tree and no remaining residue.

Design
- approach
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

Acceptance
- AC-010 covers REQ-019, REQ-022, OUT-009
  - Given The migration is complete
  - When The one-time proof command runs over the tracked tree
  - Then No legacy plan artifact and no reference to one remains
  - Oracle: git ls-files docs/plan lists no legacy capability catalog, roadmap document, or roadmap directory entry, and rg -n "Capabilities.json|Styio-Project-Roadmap|docs/plan/roadmap" over tracked files finds no match outside deleted history
  - Evidence: command from one-time legacy removal proof
- AC-011 covers REQ-020, OUT-013
  - Given Every dependent script, registry, and document is retargeted
  - When The documentation and registry gates run
  - Then All of them pass against the v3 structure
  - Oracle: python3 scripts/docs-index.py --check, python3 scripts/docs-audit.py, python3 scripts/docs-lifecycle.py validate, python3 scripts/team-docs-gate.py, python3 scripts/tool-skill-registry-gate.py, python3 scripts/workflow-scheduler.py check, and python3 scripts/manifest_tool.py validate docs/plan all exit zero
  - Evidence: command from documentation and registry gate set
- AC-012 covers REQ-011, REQ-012, OUT-010, OUT-011
  - Given The ledger has been refreshed and the audit reports closed
  - When The ledger and archive state are inspected
  - Then The ledger reports measured values with a current refresh date and the four dated reports are gone with provenance recorded
  - Oracle: rg -n "src/main.cpp" docs/rollups/MIGRATION-LEDGER.md reports the measured current length, the four dated audit report paths no longer exist, and python3 scripts/docs-lifecycle.py validate exits zero with their closure rows present
  - Evidence: command from ledger and archive verification
- AC-013 covers REQ-013, OUT-012
  - Given The line ratchet gate is registered
  - When The gate runs and the entry file is inspected
  - Then The gate passes at the recorded ceiling and would fail above it
  - Oracle: python3 scripts/monolith-line-ratchet-gate.py exits zero, the recorded ceiling equals the measured current length, and rg -n "monolith-line-ratchet" workflows/TOOL-SKILL-REGISTRY-GATE.toml scripts/workflow-scheduler.py matches both files
  - Evidence: command from line ratchet gate
- AC-014 covers REQ-018, OUT-014
  - Given The hygiene gate detects ignored residue and the local tree was cleared
  - When The hygiene modes run
  - Then The tracked tree is clean and no forbidden residue remains
  - Oracle: python3 scripts/repo-hygiene-gate.py --mode tracked exits zero and the new residue mode exits zero on the cleared tree while reporting a seeded forbidden artifact
  - Evidence: command from hygiene gate modes
- AC-015 covers REQ-021, OUT-015
  - Given The deleted plan had unclosed work
  - When The surviving ledger and plan index are inspected
  - Then Every unclosed item is registered and none is implemented
  - Oracle: rg -n "M7|Topology v2|W1|W8|callable" docs/rollups/NEXT-STAGE-GAP-LEDGER.md matches every registered item and the delivery changes no compiler source
  - Evidence: command from open work register

Focused regression
- `python3 scripts/manifest_tool.py validate docs/plan`
- `python3 scripts/docs-index.py --write`
- `python3 scripts/docs-audit.py`
- `python3 scripts/docs-lifecycle.py validate`
- `python3 scripts/team-docs-gate.py`
- `python3 scripts/tool-skill-registry-gate.py`
- `python3 scripts/workflow-scheduler.py check`
- `python3 scripts/repo-hygiene-gate.py --mode tracked`
- `python3 scripts/local-info-leak-gate.py --mode worktree`
- `python3 scripts/monolith-line-ratchet-gate.py`
- paths: docs/plan, docs/rollups, docs/teams, docs/audit, docs/archive, docs/specs, scripts, workflows

### TASK-004 downstream-governance-alignment

Tier: standard · Workload: light · Verification: code · Frontier: parallel

Outcome: The downstream repository enforces the named status checks on its protected integration branch, accepts issues and carries the two regression tracking issues, and its public badges point at the repository that actually runs the checks.

Risks: operations, public_interface, persistent_state
Requirements: REQ-006, REQ-007, REQ-008
Writes: README.md, README_zh.md
Exclusive resources: downstream repository branch ruleset, downstream repository issue settings

In scope
- Add a required status check rule with strict enforcement to the existing branch ruleset for the integration branch, using check names verified against real check runs.
- Enable issues on the downstream repository and open one tracking issue each for the fuzz and sanitizer regressions.
- Correct the owner reference in the public badges.

Out of scope
- Administering any repository other than the downstream one.
- Merging, closing, or approving any pull request.
- Adding the new report-only jobs to the required checks.
- Editing workflow definitions, gate scripts, or documentation.

Outputs
- OUT-016 Public badges pointing at the downstream repository (`README.md`): The advertised build and license state reflects the repository that actually runs the checks.

Internal Node graph
- NODE-038 verify-check-identities · after: none · Read the actual check-run names produced by the required workflows on a recent commit and confirm each intended required check exists under that exact name.
- NODE-039 read-current-ruleset · after: none · Read the current ruleset for the integration branch and record which rules it already enforces.
- NODE-040 correct-public-badges · after: none · Correct the owner reference in the public badges and confirm no other owner mismatch remains in the public readme files.
- NODE-041 enable-issue-tracking · after: none · Enable issues on the downstream repository.
- NODE-042 add-required-status-checks · after: NODE-038, NODE-039 · Add the strict required status check rule with the verified check names, preserving the existing pull-request, deletion, and non-fast-forward rules.
- NODE-043 open-regression-tracking-issues · after: NODE-041 · Open one tracking issue each for the fuzz and sanitizer regressions, naming the failed runs and the owning surface.
- NODE-044 confirm-governance-state · after: NODE-042, NODE-043, NODE-040 · Read back the ruleset and repository settings and confirm the enforced rules, the required check names, the issue setting, and the two open tracking issues.

Design
- approach
  - Extend the existing ruleset instead of creating a second one or reviving classic branch protection, because the repository specification names rulesets as the only authority and warns that the classic endpoint misreports state.
  - Require only checks that already exist and already run on pull requests: the two platform adaptation gates, the smoke job, the golden-standard job, the released audit gate, and the hygiene gate. Verify each name against real check runs before writing it, because a required name that never reports blocks every merge.
  - Keep the new report-only Windows, coverage, and benchmark jobs out of the required set until they have a green history, so observability does not become an unproven merge gate.
  - Enable strict up-to-date enforcement, which the repository specification already requires for protected branches.
  - Prove every outcome by reading state back through the repository API, since none of these outcomes is a file.
  - Do not change the purpose or title line of any document, so no generated index becomes stale in another Task's surface.

Acceptance
- AC-016 covers REQ-006
  - Given The integration branch has an active ruleset
  - When The ruleset is read back
  - Then It enforces strict required status checks with the verified check names alongside the existing rules
  - Oracle: gh api repos/Unka-Malloc/styio-nightly/rulesets returns an active branch ruleset for the integration branch whose rule types include required_status_checks with strict enforcement and whose contexts match the verified check names
  - Evidence: command from ruleset read-back
- AC-017 covers REQ-007
  - Given Issue tracking was disabled
  - When The repository settings and issue list are read back
  - Then Issues are enabled and both tracking issues are open
  - Oracle: gh api repos/Unka-Malloc/styio-nightly reports has_issues true and gh issue list returns one open fuzz regression issue and one open sanitizer regression issue
  - Evidence: command from repository settings and issue read-back
- AC-018 covers REQ-008, OUT-016
  - Given The public badges referenced a non-existent owner
  - When The public readme files are inspected
  - Then No stale owner reference remains
  - Oracle: rg -n "styio-org" README.md README_zh.md finds no match and the badge targets name the downstream repository
  - Evidence: command from badge owner check

Focused regression
- `rg -n "styio-org" README.md README_zh.md`
- `gh api repos/Unka-Malloc/styio-nightly/rulesets`
- paths: README.md, README_zh.md

### TASK-005 external-authority-handoff

Tier: standard · Workload: light · Verification: code · Frontier: parallel

Outcome: Every action this delivery needs in a repository outside the authorized administration boundary is recorded with verified current state, the exact required content, and an executable step sequence, and none of them is performed outside that boundary.

Risks: release, operations
Requirements: REQ-009, REQ-010
Writes: docs/external/upstream
Exclusive resources: upstream repository promotion, released audit policy repository, external benchmark repository assets

In scope
- Verify and record the current upstream divergence and the branch relationship the repository contract requires for promotion.
- Record the executable promotion sequence, including the ordering constraint that the downstream integration merge must land first.
- Record the exact released audit policy change that would remove the need for runtime normalization.
- Record the exact benchmark integration assets the external benchmark repository must publish for the report-only benchmark job to configure.

Out of scope
- Performing any write action in an external repository.
- Merging the downstream integration pull request.
- Weakening the branch or ordering contract to make promotion possible sooner.
- Editing compiler sources, workflow definitions, gate scripts, or repository settings.

Outputs
- OUT-017 Handoff document for external-repository actions (`docs/external/upstream/EXTERNAL-AUTHORITY-HANDOFF.md`): The maintainer can execute each blocked action without rediscovering its state, ordering, or exact content.

Internal Node graph
- NODE-045 verify-upstream-divergence · after: none · Read and record the current divergence between the downstream integration branch and each upstream managed branch.
- NODE-046 verify-audit-policy-drift · after: none · Read the released audit policy and record which scope entries the workflow currently rewrites at runtime and what the corrected policy content is.
- NODE-047 verify-benchmark-asset-gap · after: none · Read the external benchmark repository default branch and record exactly which required integration assets are present and which are missing.
- NODE-048 write-authority-handoff · after: NODE-045, NODE-046, NODE-047 · Write the handoff document with verified state, required content, executable steps, and the ordering constraint for each blocked action.

Design
- approach
  - Treat the blocked terminal state as a real outcome with a real artifact, not as a failure. The authority boundary limits administration to the downstream repository, and the repository contract additionally requires the downstream integration merge before any upstream promotion, so promotion cannot complete inside this delivery.
  - Verify state before recording it, so the handoff carries measured divergence and a measured asset list instead of restated assumptions.
  - Record the corrected policy content precisely enough to apply without rediscovery, and keep the local fail-loud normalization as the interim guard owned by the workflow Task.
  - Place the document in a new external handoff subdirectory, which keeps it out of every generated index and away from every other Task's owned paths.
  - Keep the document free of machine identity, credentials, and permission detail; it records repository state and steps only.

Acceptance
- AC-019 covers REQ-009, REQ-010, OUT-017
  - Given Actions are required in repositories outside the authorized boundary
  - When The handoff document is inspected
  - Then Each blocked action carries verified current state, required content, executable steps, and its ordering constraint, and no external write was performed
  - Oracle: rg -n "upstream|audit policy|benchmark" docs/external/upstream/EXTERNAL-AUTHORITY-HANDOFF.md matches all three blocked actions, each section names a verified current state and an executable step sequence, and gh api reports the external repositories unchanged by this delivery
  - Evidence: command from handoff document and external state check

Focused regression
- `rg -n "^## " docs/external/upstream/EXTERNAL-AUTHORITY-HANDOFF.md`
- `python3 scripts/local-info-leak-gate.py --mode worktree`
- paths: docs/external/upstream

## Full regression

Run inside the sole Reviewer session after every repair is integrated.

- `cmake --build build/default --target styio styio_lspd styio_test styio_security_test`
- `ctest --test-dir build/default -L docs --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -L security --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -L ide --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -L styio_pipeline --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -L language_feature --output-on-failure --no-tests=error`
- `ctest --test-dir build/default -L golden_standard --output-on-failure --no-tests=error`
- `cmake --build build/fuzz --target styio_fuzz_lexer styio_fuzz_parser`
- `ctest --test-dir build/fuzz -L fuzz_smoke --output-on-failure --no-tests=error`
- `cmake --build build/asan-ubsan --target styio styio_test styio_security_test styio_typeinfer_internal_test`
- `ctest --test-dir build/asan-ubsan -L security --output-on-failure --no-tests=error`
- `ctest --test-dir build/asan-ubsan -L styio_pipeline --output-on-failure --no-tests=error`
- `python3 scripts/architecture-layer-gate.py`
- `python3 scripts/runtime-surface-gate.py`
- `python3 scripts/repo-hygiene-gate.py --mode tracked`
- `python3 scripts/local-info-leak-gate.py --mode tracked`
- `python3 scripts/team-docs-gate.py`
- `python3 scripts/docs-index.py --write`
- `python3 scripts/docs-audit.py`
- `python3 scripts/docs-lifecycle.py validate`
- `python3 scripts/syntax-feature-state-gate.py`
- `python3 scripts/tool-skill-registry-gate.py`
- `python3 scripts/workflow-scheduler.py check`
- `python3 scripts/manifest_tool.py validate docs/plan`
- `python3 scripts/monolith-line-ratchet-gate.py`
- `python3 -c "import glob,yaml;[yaml.safe_load(open(p)) for p in glob.glob('.github/workflows/*.yml')]"`
- `gh api repos/Unka-Malloc/styio-nightly/rulesets`
- paths: src, tests, cmake, CMakeLists.txt, .github/workflows, docs, scripts, workflows, README.md, README_zh.md, .gitignore
