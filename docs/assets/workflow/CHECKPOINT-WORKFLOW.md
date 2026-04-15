# Styio 中断友好 Checkpoint 工作流

**Purpose:** 约束 Styio 在底层重构期间的 **微里程碑拆分、可中断恢复、分支寿命与合并门槛**；不替代语言语义文档（见 `../../design/Styio-Language-Design.md` / `../../design/Styio-EBNF.md`），也不重写项目级优先级顺序（见 `../../specs/PRINCIPLES-AND-OBJECTIVES.md`）。

**Last updated:** 2026-04-15

---

## 1. 强制规则

1. 单分支最长存活 72 小时；超时必须拆分或先落 feature flag 空壳。
2. 每个 Checkpoint 合并前必须满足：可编译、可测试、可回滚、可读档恢复。
3. 每个 Checkpoint 最小交付包固定为 5 项：代码、测试、文档、ADR、恢复指引。
4. 恢复指引统一写入 `docs/history/YYYY-MM-DD.md` 的 `Checkpoint` 小节。
5. 高风险重构默认双轨：`legacy` 稳定路径 + `nightly` 影子路径，默认不切主行为。
6. 每个微里程碑必须至少包含一条“先失败后修复”的测试历史（TDD 书签）。
7. 关键所有权/生命周期决策必须落 ADR（`docs/adr/ADR-*.md`）。
8. 冷启动恢复时优先执行 `./scripts/checkpoint-health.sh`（可用 `--no-asan` 快速模式）。
9. 进入双轨重构的函数必须按状态命名：
   - `*_legacy`：稳定旧实现；
   - `*_nightly`：新实现/影子实现；
   - `*_latest`：legacy/nightly 共享入口或稳定公共 helper；
   - `*_draft`：下一刀准备接管、尚未完成合并门槛的在改版本（例如 `parse_stmt_or_expr_legacy_draft`）。
10. 历史文档里出现的 `new` 自 2026-04-07 起统一视为 `nightly`；新提交禁止再引入 `new` 作为活动命名。
11. 双轨迁移进入收尾阶段后，默认 CLI、恢复脚本、five-layer pipeline 与主验证链路必须切到 `nightly-first`；`legacy` 只允许保留在 parser core 或显式 parity harness 中。
12. 恢复脚本必须能自动绕开失效的 CMake cache；若默认 `build/` 不可用，自动切换到可工作的 build 目录属于 workflow 正常要求，不得要求人工先清缓存才能恢复。
13. 每次提交与 push 前必须符合 [`REPO-HYGIENE-COMMIT-STANDARD.md`](./REPO-HYGIENE-COMMIT-STANDARD.md)：禁止提交构建产物、测试发现文件、二进制与大 blob。
14. 若 GitHub push 因 `100MB` 限制失败，必须清理**当前待推送历史**，而不是只删除工作区文件。

---

## 2. 微里程碑粒度

- 单个微里程碑目标粒度：1-3 天内可合并。
- 严禁提交“跨两周才能验证”的黑盒分支。
- 大里程碑（A-F）只作为路线图，执行与合并均按 `A.1 / A.2 / ...` 级别推进。

---

## 3. Checkpoint 交接格式（history 必填）

每次落地一个微里程碑，在 `docs/history/YYYY-MM-DD.md` 增加：

1. `状态`：已完成内容与影响范围。
2. `下一步`：明确到下一个可执行微里程碑编号。
3. `复现命令`：最小构建/测试命令（可复制运行）。
4. `风险`：未闭合风险与回滚点。

---

## 4. 分支与合并建议

1. 采用 trunk-based，小 PR 高频合并。
2. 优先“结构空壳先入主干，再逐层接管行为”。
3. 若改动涉及 ABI/诊断格式，先提交 ADR，再改代码。

---

## 5. 与现有文档关系

- 语言语义：`../../design/Styio-Language-Design.md` / `../../design/Styio-EBNF.md`
- 项目原则与目标：`../../specs/PRINCIPLES-AND-OBJECTIVES.md`
- 实现规程：`../../specs/AGENT-SPEC.md`
- 文档归档策略：`../../specs/DOCUMENTATION-POLICY.md`
- 决策记录：`docs/adr/`
- 维护执行模板：`../templates/REFACTOR-WORKFLOW-TEMPLATE.md`
- 仓库卫生与提交标准：`REPO-HYGIENE-COMMIT-STANDARD.md`
