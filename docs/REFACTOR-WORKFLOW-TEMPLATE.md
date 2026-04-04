# 通用重构工作流模板（可复用）

**用途：** 作为维护代码库时的通用执行模板。  
**目标：** 小步可合并、可中断恢复、每步可验证、风险可回滚。

---

## 0. 预检（开始前 10 分钟）

1. 明确本次范围：只改一个微目标（1-3 天可合并）。
2. 先写“失败测试”（TDD 书签），定义要修复的行为。
3. 记录风险边界（所有权、生命周期、ABI、并发、持久化）。
4. 选择迁移策略：
   - `兼容模式`：对外接口不变，内部先收敛；
   - `双轨模式`：`legacy` + `new` 并存，默认 legacy。

---

## 1. 微里程碑模板（每刀都按这个走）

### 1.1 目标定义

- 目标编号：`<M.X>`
- 本刀只做：`<一句话>`
- 不做：`<明确排除项>`

### 1.2 先失败后修复

1. 增加最小失败测试（先红）。
2. 实现改动（只覆盖该测试对应行为）。
3. 跑最小测试集（转绿）。

### 1.3 扩展验证

1. 跑安全回归（如 `security`）。
2. 跑关键链路（如 `pipeline`）。
3. 跑基线回归（如 `milestone`）。

### 1.4 交付包（强制五件套）

1. 代码：功能改动本体。
2. 测试：失败先行 + 回归覆盖。
3. 文档：行为变化与边界说明。
4. ADR：关键决策（所有权/生命周期/兼容策略）。
5. 恢复指引：追加到 `docs/history/YYYY-MM-DD.md`。

---

## 2. 风险控制清单（提交前逐项确认）

1. 是否引入隐式全局清理逻辑，可能与局部 RAII 冲突？
2. 是否可能出现双重释放/悬空引用/UAF？
3. 是否改变了对外行为（CLI/错误码/输出格式）？
4. 是否影响旧路径兼容（legacy、测试样例、历史数据）？
5. 是否可一键回滚（提交粒度是否足够小）？

---

## 3. 恢复模板（粘贴到 history 的 Checkpoint 段）

```md
## Checkpoint（<M.X 名称>）

### 当前状态
- <完成内容 1>
- <完成内容 2>

### 下一步
1. <下一个微目标>
2. <下一个风险点>

### 复现命令
```bash
cmake -S . -B build
cmake --build build --target <targets>
ctest --test-dir build -L <label> --output-on-failure
```
```

---

## 4. 提交信息模板

```text
checkpoint: <动词> <范围> <目的>
```

示例：

```text
checkpoint: migrate BinOp/BinComp to RAII ownership
fix: make AST tracked cleanup non-owning to avoid double free
```

---

## 5. 推荐执行顺序（每次重构默认）

1. 失败测试（红）
2. 最小实现（绿）
3. 安全测试
4. 关键链路测试
5. 全量基线测试
6. ADR + history
7. 小提交合并

---

## 6. 双轨重构附加模板（Shadow Gate）

适用场景：`legacy/new` 并存、默认不切主行为的高风险重构。

1. 加显式开关：
   - 例如 `--<domain>-engine=legacy|new`、`--<domain>-shadow-compare`。
2. 默认路径保持 `legacy`，`new` 仅在开关下运行。
3. 增加工件输出参数：
   - 例如 `--<domain>-shadow-artifact-dir`。
4. 在 CI 设置显式 gate（不要只依赖大标签间接覆盖）：
   - 全量样例 shadow compare 必须通过。
5. gate 失败时自动上传工件，保证可回放。

可直接复用的 Styio 命令模板：

```bash
tmp_dir="$(mktemp -d)"
./scripts/parser-shadow-m1-gate.sh ./build/bin/styio ./tests/milestones/m1 "$tmp_dir"
```
