# Styio Ecosystem File Governance Alignment Plan

**Purpose:** 作为 `styio-nightly`、`styio-spio`、`styio-view` 三仓文件治理对齐的权威专项计划，定义统一目录职责、文档生命周期、文件/产物治理基线、迁移顺序与门禁出口，确保治理能力保持一致，而不是只靠单仓历史习惯演化。

**Last updated:** 2026-04-17

## 1. Authority and Mirrors

本文件是三仓文件治理对齐的唯一权威副本。

镜像入口固定为：

1. `styio-spio/docs/planning/Styio-Ecosystem-File-Governance-Alignment-Plan.md`
2. `styio-view/docs/plans/Styio-Ecosystem-File-Governance-Alignment-Plan.md`

任何一次 checkpoint 只要改了目录职责、文档生命周期、忽略策略、fixture 反忽略规则、索引生成策略、repo hygiene 规则或团队治理门禁，必须按这个顺序同步：

1. 更新本文件。
2. 更新 `spio` / `view` 镜像计划。
3. 更新受影响仓库的维护模型或文档策略。
4. 更新受影响仓库的 `docs/teams/COORDINATION-RUNBOOK.md` 与相关 team runbook。
5. 更新索引、history、delivery gate 或 repo hygiene 脚本的引用面。

## 2. Goal

目标不是让三仓目录树完全同构，而是让三仓具备同一套治理能力：

1. 同样清晰的目录职责和 SSOT 边界。
2. 同样明确的 active / history / archive 生命周期。
3. 同样可验证的索引生成、docs audit 和 lifecycle 检查。
4. 同样可执行的临时文件、构建产物、fixture 与反忽略策略。
5. 同样清晰的 team ownership、review routing、handoff 和 recovery 纪律。

## 3. Current Assessment

### 3.1 `styio-nightly`

`nightly` 当前是治理参考仓：

1. `docs/README.md` 已定义 tree contract 和默认阅读顺序。
2. `docs/rollups/`、`docs/history/`、`docs/archive/` 已形成文档生命周期。
3. `scripts/docs-index.py`、`scripts/docs-audit.py`、`scripts/docs-lifecycle.py`、`scripts/team-docs-gate.py` 已形成自动化门禁。
4. `delivery-gate.sh` 已把 docs/file governance 纳入公共交付地板。

### 3.2 `styio-spio`

`spio` 当前工程门禁较强，但文档生命周期仍弱于 `nightly`：

1. `docs/governance/Docs-Maintenance-Model.md` 已明确模块职责和单一事实来源。
2. `scripts/repo-hygiene-check.py`、`scripts/delivery-gate.py`、`scripts/submit-gate.py` 已形成较强的工程治理。
3. 但缺少 `nightly` 级别的 `docs/history/`、`docs/archive/`、`docs/rollups/` 与索引/生命周期生成机制。

### 3.3 `styio-view`

`view` 当前目录形态已经不错，但自动化和生命周期治理最弱：

1. `docs/specs/DOCUMENTATION-POLICY.md` 与 `docs/teams/` 已经定义了文档和协作规则。
2. 但 docs 索引、lifecycle、archive/rollup、repo hygiene 自动化都明显弱于 `nightly`。
3. 这导致 `view` 更依赖人工维护，最容易在目录治理上漂移。

## 4. Shared Baseline

三仓最终都必须满足以下基线。

### 4.1 Tree Contract Baseline

每个仓库都必须清楚区分这些职责：

1. `docs/design/` 或等价设计 SSOT
2. `docs/specs/` 或等价规范 SSOT
3. `docs/plans/` 或 `docs/planning/`
4. `docs/teams/`
5. `docs/history/`
6. `docs/archive/`
7. `docs/rollups/` 或等价 active-summary 层
8. `docs/review/`
9. `docs/assets/` / `docs/operations/` / `docs/contracts/` / `docs/external/for-*`

允许仓库保留自身主题模块，但不允许职责混叠到无法判断“谁拥有哪类事实”。

### 4.2 Lifecycle Baseline

每个仓库都必须具备统一的文档生命周期：

1. Active summary：当前状态、gap ledger、执行摘要。
2. History：按日恢复记录。
3. Archive：原始 provenance 与已退役文档。
4. Review：未裁决冲突和风险。

不能只靠 `plans/` 承担全部状态、历史和风险。

### 4.3 Generated Inventory Baseline

每个重要文档集合都必须有：

1. `README.md` 说明范围与职责。
2. `INDEX.md` 作为入口索引。
3. 生成式或脚本校验式索引刷新路径。

`nightly` 当前是生成式标准；`spio` 和 `view` 至少要达到“索引可检查、可刷新、不可长期漂移”的水平。

### 4.4 File / Artifact Governance Baseline

三仓必须统一遵守：

1. 构建目录、缓存目录、临时文件、日志文件默认忽略。
2. 任何需要入库的 temp/build 风格 fixture，必须显式反忽略。
3. repo hygiene gate 必须能发现：
   - 二进制或构建产物误入库
   - 索引未更新
   - 必需入口缺失
   - ignore 规则误吞 tracked fixture

### 4.5 Ownership Baseline

三仓都必须保留：

1. `docs/teams/COORDINATION-RUNBOOK.md`
2. 明确的 team runbook
3. review matrix
4. checkpoint policy
5. handoff / recovery 纪律

目录治理变化视为 Docs / Governance 级变更，不得绕过 owner review。

## 5. Milestones

### FG0 Baseline Freeze

目标：

1. 冻结这份专项计划。
2. 给 `spio` 和 `view` 建立镜像入口。
3. 将专项计划接入三仓 docs 入口、维护模型/文档策略与协调 runbook。

出口：

1. 三仓都能从顶层 docs 入口找到本计划或其镜像。
2. 三仓都把“文件治理变化”纳入协调与 checkpoint 规则。

### FG1 Align `styio-spio`

目标：

1. 为 `spio` 补齐 `docs/history/`、`docs/archive/`、`docs/rollups/`。
2. 引入 `nightly` 风格的 docs index / audit / lifecycle 能力，允许是 `spio` 版实现，不要求照搬脚本文件名。
3. 把现有 `Docs-Maintenance-Model`、verification matrix、submit gate 与新文档生命周期接上。

出口：

1. `spio` 拥有与 `nightly` 同级别的文档生命周期，不再只有 planning/governance 模块而缺 active/history/archive 层。
2. docs 索引、入口和生命周期具有脚本级检查。

### FG2 Align `styio-view`

目标：

1. 为 `view` 补齐 `docs/archive/` 与轻量 `docs/rollups/`。
2. 为 `view` 增加 docs index / audit / lifecycle 路径。
3. 升级 `scripts/check_repo_hygiene.py`，让它覆盖 docs 入口、索引、临时产物和 fixture 反忽略策略。

出口：

1. `view` 的文档治理从“人工维护为主”升级到“脚本背书为主”。
2. `view` 的 docs 结构和文件治理可与 `nightly` / `spio` 一致比较。

### FG3 Unify File / Artifact Gates

目标：

1. 统一三仓 `.gitignore` 基线与 fixture negate 规则写法。
2. 统一 repo hygiene、docs audit、team docs gate、delivery gate 的最低治理地板。
3. 让跨仓 gate 能验证 docs/file governance，而不只是 CLI contract。

2026-04-17 checkpoint:

1. `spio` 的 required-pattern / negate-rule 模型继续作为对齐参考，不再复制第二套 policy。
2. `nightly` 与 `view` 必须把 shared baseline pattern 和 docs/test fixture negate rule 直接写入根 `.gitignore`。
3. `nightly` 的 `repo-hygiene-gate.py` 与 `view` 的 `check_repo_hygiene.py` 必须像 `spio` 一样显式拦截 required pattern 与文档接线漂移。

出口：

1. 三仓都能自动发现相同类型的文件治理回归。
2. temp/build/cache/log 路径的治理风险不再只靠人工 code review 发现。

### FG4 Steady State

目标：

1. 三仓进入长期稳态治理。
2. 新增目录、脚本、handoff 或 team surface 时，都走同一套治理传播顺序。

出口：

1. 任一仓库都能回答：当前状态在哪看、历史在哪看、归档在哪、谁负责、索引怎么刷、哪些产物禁止入库、tracked fixture 如何反忽略。
2. 三仓文件治理能力保持等强，而不是继续出现“强仓更强，弱仓靠人工兜底”的分化。

## 6. Rollout Order

执行顺序固定为：

1. 先完成 `FG0`，冻结规则和入口。
2. 优先做 `FG1`，因为 `spio` 离 `nightly` 最近，迁移成本低，且有现成工程 gate 可接。
3. 再做 `FG2`，把 `view` 的文档与文件治理自动化补上。
4. 最后做 `FG3` 与 `FG4` 收口。

不要先做大规模目录搬迁；正确顺序是先定规则，再补脚本，再迁移目录。

## 7. Required Reuse of Existing Pipelines

本计划明确要求复用，而不是绕过现有文档流水线：

1. `styio-nightly`
   - `scripts/docs-index.py`
   - `scripts/docs-audit.py`
   - `scripts/docs-lifecycle.py`
   - `scripts/team-docs-gate.py`
   - `scripts/delivery-gate.sh`
2. `styio-spio`
   - `scripts/repo-hygiene-check.py`
   - `scripts/delivery-gate.py`
   - `scripts/submit-gate.py`
3. `styio-view`
   - `scripts/check_repo_hygiene.py`

后续新增治理脚本时，优先移植 `nightly` 能力或扩展现有脚本，不额外建立平行 gate。

## 8. Non-Goals

本计划不要求：

1. 三仓目录名完全一致。
2. 三仓脚本文件名完全一致。
3. 一次性重写全部旧文档。

本计划要求的是治理能力一致，而不是表面结构机械一致。
